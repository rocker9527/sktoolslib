// sktoolslib - common files for SK tools

// Copyright (C) 2012 - Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include "stdafx.h"
#include "TextFile.h"

CTextFile::CTextFile(void) : pFileBuf(NULL)
    , filelen(0)
    , encoding(AUTOTYPE)
{
}

CTextFile::~CTextFile(void)
{
    if (pFileBuf)
        delete [] pFileBuf;
}

bool CTextFile::Save(LPCTSTR path)
{
    HANDLE hFile = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ,
                              NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;
    DWORD byteswritten;
    if (!WriteFile(hFile, pFileBuf, filelen, &byteswritten, NULL))
    {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    return true;
}

bool CTextFile::Load(LPCTSTR path)
{
    LARGE_INTEGER lint;
    if (pFileBuf)
        delete [] pFileBuf;
    pFileBuf = NULL;
    HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;
    std::wstring wpath(path);
    size_t pos = wpath.find_last_of('\\');
    filename = wpath.substr(pos + 1);
    if (!GetFileSizeEx(hFile, &lint))
    {
        CloseHandle(hFile);
        return false;
    }
    if (lint.HighPart)
    {
        // file is way too big for us!
        CloseHandle(hFile);
        return false;
    }
    DWORD bytesread;
    pFileBuf = new BYTE[lint.LowPart];
    if (!ReadFile(hFile, pFileBuf, lint.LowPart, &bytesread, NULL))
    {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    filelen = bytesread;

    // we have the file read into memory, now we have to find out what
    // kind of text file we have here.
    encoding = CheckUnicodeType(pFileBuf, bytesread);

    if (encoding == UNICODE_LE)
        textcontent = std::wstring((wchar_t*)pFileBuf, bytesread / sizeof(wchar_t));
    else if (encoding == UTF8)
    {
        int ret = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)pFileBuf, bytesread, NULL, 0);
        wchar_t * pWideBuf = new wchar_t[ret];
        int ret2 = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)pFileBuf, bytesread, pWideBuf, ret);
        if (ret2 == ret)
            textcontent = std::wstring(pWideBuf, ret);
        delete [] pWideBuf;
    }
    else if (encoding == ANSI)
    {
        int ret = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)pFileBuf, bytesread, NULL, 0);
        wchar_t * pWideBuf = new wchar_t[ret];
        int ret2 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)pFileBuf, bytesread, pWideBuf, ret);
        if (ret2 == ret)
            textcontent = std::wstring(pWideBuf, ret);
        delete [] pWideBuf;
    }
    else
        return false;
    return CalculateLines();
}

bool CTextFile::ContentsModified(LPVOID pBuf, DWORD newLen)
{
    if (pFileBuf)
        delete [] pFileBuf;
    pFileBuf = pBuf;
    filelen = newLen;
    return true;
}

CTextFile::UnicodeType CTextFile::CheckUnicodeType(LPVOID pBuffer, int cb)
{
    if (cb < 2)
        return ANSI;
    UINT16 * pVal = (UINT16 *)pBuffer;
    UINT8 * pVal2 = (UINT8 *)(pVal + 1);
    // scan the whole buffer for a 0x0000 sequence
    // if found, we assume a binary file
    bool bNull = false;
    for (int i = 0; i < (cb - 2); i = i + 2)
    {
        if (0x0000 == *pVal++)
            return BINARY;
        if (0x00 == *pVal2++)
            bNull = true;
        if (0x00 == *pVal2++)
            bNull = true;
    }
    if ((bNull) && ((cb % 2) == 0))
        return UNICODE_LE;
    pVal = (UINT16 *)pBuffer;
    pVal2 = (UINT8 *)(pVal + 1);
    if (*pVal == 0xFEFF)
        return UNICODE_LE;
    if (cb < 3)
        return ANSI;
    if (*pVal == 0xBBEF)
    {
        if (*pVal2 == 0xBF)
            return UTF8;
    }
    // check for illegal UTF8 chars
    pVal2 = (UINT8 *)pBuffer;
    for (int i = 0; i < cb; ++i)
    {
        if ((*pVal2 == 0xC0) || (*pVal2 == 0xC1) || (*pVal2 >= 0xF5))
            return ANSI;
        pVal2++;
    }
    pVal2 = (UINT8 *)pBuffer;
    bool bUTF8 = false;
    for (int i = 0; i < (cb - 3); ++i)
    {
        if ((*pVal2 & 0xE0) == 0xC0)
        {
            pVal2++;
            i++;
            if ((*pVal2 & 0xC0) != 0x80)
                return ANSI;
            bUTF8 = true;
        }
        if ((*pVal2 & 0xF0) == 0xE0)
        {
            pVal2++;
            i++;
            if ((*pVal2 & 0xC0) != 0x80)
                return ANSI;
            pVal2++;
            i++;
            if ((*pVal2 & 0xC0) != 0x80)
                return ANSI;
            bUTF8 = true;
        }
        if ((*pVal2 & 0xF8) == 0xF0)
        {
            pVal2++;
            i++;
            if ((*pVal2 & 0xC0) != 0x80)
                return ANSI;
            pVal2++;
            i++;
            if ((*pVal2 & 0xC0) != 0x80)
                return ANSI;
            pVal2++;
            i++;
            if ((*pVal2 & 0xC0) != 0x80)
                return ANSI;
            bUTF8 = true;
        }
        pVal2++;
    }
    if (bUTF8)
        return UTF8;
    return ANSI;
}

bool CTextFile::CalculateLines()
{
    // fill an array with starting positions for every line in the loaded file
    if (pFileBuf == NULL)
        return false;
    if (textcontent.empty())
        return false;
    linepositions.clear();
    size_t pos = 0;
    for (std::wstring::iterator it = textcontent.begin(); it != textcontent.end(); ++it)
    {
        if (*it == '\r')
        {
            ++it;
            ++pos;
            if (it != textcontent.end())
            {
                if (*it == '\n')
                {
                    // crlf lineending
                    linepositions.push_back(pos);
                }
                else
                {
                    // cr lineending
                    linepositions.push_back(pos - 1);
                }
            }
            else
                break;
        }
        else if (*it == '\n')
        {
            // lf lineending
            linepositions.push_back(pos);
        }
        ++pos;
    }
    return true;
}

long CTextFile::LineFromPosition(long pos) const
{
    long line = 0;
    for (std::vector<size_t>::const_iterator it = linepositions.begin(); it != linepositions.end(); ++it)
    {
        line++;
        if (pos <= long(*it))
            break;
    }
    return line;
}

std::wstring CTextFile::GetFileNameWithoutExtension()
{
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos)
        return filename.substr(0, pos);
    return filename;
}

std::wstring CTextFile::GetFileNameExtension()
{
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos)
        return filename.substr(pos);
    return L"";
}