/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code, released
 * Jan 28, 2003.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Garrett Arch Blythe, 28-January-2003
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozce_internal.h"

#include <stdarg.h>

extern "C" {
#if 0
}
#endif


MOZCE_SHUNT_API int mozce_access(const char *path, int mode)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- mozce_access called\n");
#endif

    return 0;
}

MOZCE_SHUNT_API void mozce_rewind(FILE* inStream)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_rewind called\n");
#endif

    fseek(inStream, 0, SEEK_SET);
}


MOZCE_SHUNT_API FILE* mozce_fdopen(int inFD, const char* inMode)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_fdopen called\n");
#endif

    FILE* retval = NULL;

    if(NULL != inMode)
    {
        WCHAR buffer[16];

        int convRes = a2w_buffer(inMode, -1, buffer, sizeof(buffer) / sizeof(WCHAR));
        if(0 != convRes)
        {
            retval = _wfdopen((void*)inFD, buffer);
        }
    }

    return retval;
}


MOZCE_SHUNT_API void mozce_perror(const char* inString)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_perror called\n");
#endif

    fprintf(stderr, "%s", inString);
}


MOZCE_SHUNT_API int mozce_remove(const char* inPath)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_remove called on %s\n", inPath);
#endif

    int retval = -1;

    if(NULL != inPath)
    {
        unsigned short wPath[MAX_PATH];

        if(0 != a2w_buffer(inPath, -1, wPath, sizeof(wPath) / sizeof(unsigned short)))
        {
            if(FALSE != DeleteFileW(wPath))
            {
                retval = 0;

            }
        }
    }

    return retval;
}

MOZCE_SHUNT_API char* mozce_getcwd(char* buff, size_t size)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_getcwd called.\n");
#endif

	unsigned short dir[MAX_PATH];
	GetModuleFileName(GetModuleHandle (NULL), dir, MAX_PATH);
	for (int i = _tcslen(dir); i && dir[i] != TEXT('\\'); i--) {}
	dir[i + 1] = TCHAR('\0');

	w2a_buffer(dir, -1, buff, size);

    return buff;
}

MOZCE_SHUNT_API int mozce_printf(const char * format, ...)
{
    return 0;
}


MOZCE_SHUNT_API int mozce_open(const char *pathname, int flags, int mode)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_open called\n");
#endif
    if (mode | O_RDONLY)
    {
        return (int) fopen(pathname, "r");
    }

    if (mode | O_RDWR && mode | O_CREAT)
    {
        return (int) fopen(pathname, "w+");
    }

    if (mode | O_RDWR && mode | O_APPEND)
    {
        return (int) fopen(pathname, "a+");
    }

    if (mode | O_RDWR)
    {
        return (int) fopen(pathname, "r+");
    }

    if (mode | O_WRONLY && mode | O_CREAT)
    {
        return (int) fopen(pathname, "w+");
    }

    if (mode | O_WRONLY && mode | O_APPEND)
    {
        return (int) fopen(pathname, "a");
    }

    if (mode | O_WRONLY)
    {
        return (int) fopen(pathname, "w");
    }

#ifdef DEBUG
    mozce_printf("-- Unsupported mode!\n");
#endif
    return 0;
}

MOZCE_SHUNT_API int mozce_close(int fp)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_close called\n");
#endif
    return fclose((FILE*) fp);
}

MOZCE_SHUNT_API size_t mozce_read(int fp, void* buffer, size_t count)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_read called\n");
#endif
    return fread(buffer, count, 1, (void*)fp);
}


MOZCE_SHUNT_API size_t mozce_write(int fp, const void* buffer, size_t count)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_write called\n");
#endif
    return fwrite(buffer, count, 1, (void*)fp);
}


MOZCE_SHUNT_API int mozce_unlink(const char *pathname)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_unlink called\n");
#endif
    return mozce_remove(pathname);
}


MOZCE_SHUNT_API int mozce_lseek(int fildes, int offset, int whence)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_lseek called\n");
#endif

    return fseek((FILE*) fildes, offset, whence);
}

#if 0
{
#endif
} /* extern "C" */

