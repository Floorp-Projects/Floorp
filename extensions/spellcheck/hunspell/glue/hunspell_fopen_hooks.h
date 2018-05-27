/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef fopen_hooks_h__
#define fopen_hooks_h__

/**
 * This file is force-included in hunspell code. Its purpose is to add
 * readahead to fopen() calls in hunspell without modifying its code, in order
 * to ease future upgrades.
 */

#include "mozilla/FileUtils.h"
#include <stdio.h>
#include <string.h>

#if defined(XP_WIN)
#include "nsNativeCharsetUtils.h"
#include "nsString.h"

#include <fcntl.h>
#include <windows.h>
// Hunspell defines a function named near. Windef.h #defines near.
#undef near
// mozHunspell defines a function named RemoveDirectory.
#undef RemoveDirectory
#endif /* defined(XP_WIN) */

inline FILE*
hunspell_fopen_readahead(const char* filename, const char* mode)
{
  if (!filename || !mode) {
    return nullptr;
  }
  // Fall back to libc's fopen for modes not supported by ReadAheadFile
  if (!strchr(mode, 'r') || strchr(mode, '+')) {
    return fopen(filename, mode);
  }
  int fd = -1;
#if defined(XP_WIN)
  // filename is obtained via the nsIFile::nativePath attribute, so
  // it is using the Windows ANSI code page, NOT UTF-8!
  nsAutoString utf16Filename;
  nsresult rv = NS_CopyNativeToUnicode(nsDependentCString(filename),
                                       utf16Filename);
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  HANDLE handle = INVALID_HANDLE_VALUE;
  mozilla::ReadAheadFile(utf16Filename.get(), 0, SIZE_MAX, &handle);
  if (handle == INVALID_HANDLE_VALUE) {
    return nullptr;
  }
  int flags = _O_RDONLY;
  // MSVC CRT's _open_osfhandle only supports adding _O_TEXT, not _O_BINARY
  if (strchr(mode, 't')) {
    // Force translated mode
    flags |= _O_TEXT;
  }
  // Import the Win32 fd into the CRT
  fd = _open_osfhandle((intptr_t)handle, flags);
  if (fd < 0) {
    CloseHandle(handle);
    return nullptr;
  }
#else
  mozilla::ReadAheadFile(filename, 0, SIZE_MAX, &fd);
  if (fd < 0) {
    return nullptr;
  }
#endif /* defined(XP_WIN) */

  FILE* file = fdopen(fd, mode);
  if (!file) {
    close(fd);
  }
  return file;
}

#define fopen(filename, mode) hunspell_fopen_readahead(filename, mode)

#endif /* fopen_hooks_h__ */

