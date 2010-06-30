/* -*- Mode: C;  c-basic-offset: 2; tab-width: 4; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is MOZCE Lib.
 *
 * The Initial Developer of the Original Code is Doug Turner <dougt@meer.net>.
 
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  John Wolfe <wolfe@lobo.us>
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

 
#include "include/mozce_shunt.h"
#include "time_conversions.h"
#include <stdlib.h>
#include "Windows.h"

// from environment.cpp
void putenv_copy(const char *k, const char *v);

////////////////////////////////////////////////////////
//  special folder stuff
////////////////////////////////////////////////////////

typedef struct MOZCE_SHUNT_SPECIAL_FOLDER_INFO
{
  int   nFolder;
  char *folderEnvName;
} MozceShuntSpecialFolderInfo;

// TAKEN DIRECTLY FROM MICROSOFT SHELLAPI.H HEADER FILE 
// supported SHGetSpecialFolderPath nFolder ids
#define CSIDL_DESKTOP            0x0000
#define CSIDL_PROGRAMS           0x0002      // \Windows\Start Menu\Programs
#define CSIDL_PERSONAL           0x0005
#define CSIDL_WINDOWS            0x0024      // \Windows
#define CSIDL_PROGRAM_FILES      0x0026      // \Program Files

#define CSIDL_APPDATA            0x001A      // NOT IN SHELLAPI.H header file
#define CSIDL_PROFILE            0x0028      // NOT IN SHELLAPI.H header file

MozceShuntSpecialFolderInfo mozceSpecialFoldersToEnvVars[] = {
  { CSIDL_APPDATA,  "APPDATA" },
  { CSIDL_PROGRAM_FILES, "ProgramFiles" },
  { CSIDL_WINDOWS,  "windir" },

  //  { CSIDL_PROFILE,  "HOMEPATH" },     // No return on WinMobile 6 Pro
  //  { CSIDL_PROFILE,  "USERPROFILE" },  // No return on WinMobile 6 Pro
  //  { int, "ALLUSERSPROFILE" },         // Only one profile on WinCE
  //  { int, "CommonProgramFiles" },
  //  { int, "COMPUTERNAME" },
  //  { int, "HOMEDRIVE" },
  //  { int, "SystemDrive" },
  //  { int, "SystemRoot" },
  //  { int, "TEMP" },

  { NULL, NULL }
};

static void InitializeSpecialFolderEnvVars()
{
  MozceShuntSpecialFolderInfo *p = mozceSpecialFoldersToEnvVars;
  while ( p && p->nFolder && p->folderEnvName ) {
    WCHAR wPath[MAX_PATH];
    char  cPath[MAX_PATH];
    if ( SHGetSpecialFolderPath(NULL, wPath, p->nFolder, FALSE) )
      if ( 0 != WideCharToMultiByte(CP_ACP, 0, wPath, -1, cPath, MAX_PATH, 0, 0) )
        putenv_copy(p->folderEnvName, cPath);
    p++;
  }
}

////////////////////////////////////////////////////////
//  errno
////////////////////////////////////////////////////////

char* strerror(int inErrno)
{
  return "Unknown Error";
}

int errno = 0;


////////////////////////////////////////////////////////
//  File System Stuff
////////////////////////////////////////////////////////

wchar_t * _wgetcwd(wchar_t * dir, unsigned long size)
{
  wchar_t tmp[MAX_PATH] = {0};
  GetEnvironmentVariableW(L"CWD", tmp, size);
  if (tmp && tmp[0]) {
    if (wcslen(tmp) > size)
      return 0;
    if (!dir) {
      dir = (wchar_t*)malloc(sizeof(wchar_t) * (wcslen(tmp) + 2));
      if (!dir)
        return 0;
    }
    wcscpy(dir, tmp);
  } else {
    unsigned long i;
    if (!dir) {
      dir = (wchar_t*)malloc(sizeof(wchar_t) * (MAX_PATH + 1));
      if (!dir)
        return 0;
    }
    if (!GetModuleFileNameW(GetModuleHandle (NULL), dir, MAX_PATH))
      return 0;
    for (i = wcslen(dir); i && dir[i] != '\\'; i--) {}
    dir[i + 1] = '\0';
    SetEnvironmentVariableW(L"CWD", dir);
  }
  size_t len = wcslen(dir);
  if (dir[len - 1] != '\\' && (len + 2) < size) {
    dir[len] = '\\';
    dir[len + 1] = '\0';
  }
  return dir;
}

wchar_t *_wfullpath( wchar_t *absPath, const wchar_t *relPath, unsigned long maxLength )
{
  if(absPath == NULL){
    absPath = (wchar_t *)malloc(maxLength*sizeof(wchar_t));
  }
  wchar_t cwd[MAX_PATH];
  if (NULL == _wgetcwd( cwd, MAX_PATH))
    return NULL;

  unsigned long len = wcslen(cwd);
  if(!(cwd[len-1] == TCHAR('/') || cwd[len-1] == TCHAR('\\'))&& len< maxLength){
    cwd[len] = TCHAR('\\');
    cwd[++len] = TCHAR('\0');
  }
  if(len+wcslen(relPath) < maxLength){
#if (_WIN32_WCE > 300)
    if ( 0 < CeGetCanonicalPathName(relPath[0] == L'\\'? relPath : 
                                                         wcscat(cwd,relPath), 
                                    absPath, maxLength, 0))
      return absPath;
#else
    #error Need CeGetCanonicalPathName to build.
    // NO ACTUAL CeGetCanonicalPathName function in earlier versions of WinCE
#endif
  }
  return NULL;
}

int _wchdir(const WCHAR* path) {
  return SetEnvironmentVariableW(L"CWD", path);
}

int _unlink(const char *filename)
{
  wchar_t wname[MAX_PATH];
  
  MultiByteToWideChar(CP_ACP,
                      0,
                      filename,
                      strlen(filename)+1,
                      wname,
                      MAX_PATH );
  return DeleteFileW(wname)?0:-1;
}

void abort(void)
{
#if defined(DEBUG)
  DebugBreak();
#endif
  TerminateProcess((HANDLE) GetCurrentProcessId(), 3);
}

////////////////////////////////////////////////////////
//  Locale Stuff
////////////////////////////////////////////////////////

#define localeconv __not_supported_on_device_localeconv
#include <locale.h>
#undef localeconv

extern "C" {
  struct lconv * localeconv(void);
}

static struct lconv s_locale_conv =
  {
    ".",   /* decimal_point */
    ",",   /* thousands_sep */
    "333", /* grouping */
    "$",   /* int_curr_symbol */
    "$",   /* currency_symbol */
    "",    /* mon_decimal_point */
    "",    /* mon_thousands_sep */
    "",    /* mon_grouping */
    "+",   /* positive_sign */
    "-",   /* negative_sign */
    '2',   /* int_frac_digits */
    '2',   /* frac_digits */
    1,     /* p_cs_precedes */
    1,     /* p_sep_by_space */
    1,     /* n_cs_precedes */
    1,     /* n_sep_by_space */
    1,     /* p_sign_posn */
    1,     /* n_sign_posn */
  };

struct lconv * localeconv(void)
{
  return &s_locale_conv;
}


////////////////////////////////////////////////////////
//  DllMain
////////////////////////////////////////////////////////

BOOL WINAPI DllMain(HANDLE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
  // Perform actions based on the reason for calling.
  switch( fdwReason ) 
  { 
    case DLL_PROCESS_ATTACH:
      // Initialize once for each new process.
      // Return FALSE to fail DLL load.
      InitializeSpecialFolderEnvVars();
      break;

    case DLL_THREAD_ATTACH:
      // Do thread-specific initialization.
      break;

    case DLL_THREAD_DETACH:
      // Do thread-specific cleanup.
      break;

    case DLL_PROCESS_DETACH:
      // Perform any necessary cleanup.
      break;
  }
  return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
