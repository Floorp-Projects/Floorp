/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2007 Adrian Johnson
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Adrian Johnson.
 *
 * Contributor(s):
 *      Adrian Johnson <ajohnson@redneon.com>
 */

#define WIN32_LEAN_AND_MEAN
/* We require Windows 2000 features such as ETO_PDY */
#if !defined(WINVER) || (WINVER < 0x0500)
# define WINVER 0x0500
#endif
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0500)
# define _WIN32_WINNT 0x0500
#endif

#include "cairoint.h"

#include <windows.h>
#include <io.h>

/* tmpfile() replacment for Windows.
 *
 * On Windows tmpfile() creates the file in the root directory. This
 * may fail due to unsufficient privileges.
 */
FILE *
_cairo_win32_tmpfile (void)
{
    DWORD path_len;
    WCHAR path_name[MAX_PATH + 1];
    WCHAR file_name[MAX_PATH + 1];
    HANDLE handle;
    int fd;
    FILE *fp;

    path_len = GetTempPathW (MAX_PATH, path_name);
    if (path_len <= 0 || path_len >= MAX_PATH)
	return NULL;

    if (GetTempFileNameW (path_name, L"ps_", 0, file_name) == 0)
	return NULL;

    handle = CreateFileW (file_name,
			 GENERIC_READ | GENERIC_WRITE,
			 0,
			 NULL,
			 CREATE_ALWAYS,
			 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
			 NULL);
    if (handle == INVALID_HANDLE_VALUE) {
	DeleteFileW (file_name);
	return NULL;
    }

    fd = _open_osfhandle((intptr_t) handle, 0);
    if (fd < 0) {
	CloseHandle (handle);
	return NULL;
    }

    fp = fdopen(fd, "w+b");
    if (fp == NULL) {
	_close(fd);
	return NULL;
    }

    return fp;
}
