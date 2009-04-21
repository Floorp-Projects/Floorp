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

#ifdef MOZ_MEMORY
void * operator new(size_t _Size)
{
   void *p =  moz_malloc(_Size);
   return (p);
}

void operator delete(void * ptr)
{
  moz_free(ptr);  
}
void *operator new[](size_t size)
{
  void* p = moz_malloc(size);
  return (p);
}
void operator delete[](void *ptr)
{
  moz_free(ptr);
}

MOZCE_SHUNT_API char*
mozce_strndup( const char *src, size_t len ) {
  char* dst = (char*)moz_malloc(len + 1);
  if(dst)
    strncpy(dst, src, len + 1);
  return dst;
}


MOZCE_SHUNT_API char*
mozce_strdup(const char *src ) {
  size_t len = strlen(src);
  return mozce_strndup(src, len );
}

MOZCE_SHUNT_API unsigned short* 
mozce_wcsndup( const unsigned short *src, size_t len ) {
  wchar_t* dst = (wchar_t*)moz_malloc(sizeof(wchar_t) * (len + 1));
  if(dst)
    wcsncpy(dst, src, len + 1);
  return dst;
}

MOZCE_SHUNT_API unsigned short* 
mozce_wcsdup( const unsigned short *src ) {
  size_t len = wcslen(src);
  return mozce_wcsndup(src, len);
}
MOZCE_SHUNT_API void* __cdecl malloc(size_t size) {
  return moz_malloc(size);
}
MOZCE_SHUNT_API void* __cdecl valloc(size_t size) {
  return moz_valloc(size);
}
MOZCE_SHUNT_API void* __cdecl  calloc(size_t size, size_t num) {
  return moz_calloc(size, num);
}
MOZCE_SHUNT_API void* __cdecl  realloc(void* ptr, size_t size) {
  return moz_realloc(ptr, size);
}
MOZCE_SHUNT_API void __cdecl  free(void* ptr) {
  return moz_free(ptr);
}

#endif

////////////////////////////////////////////////////////
//  Environment Variable Stuff
////////////////////////////////////////////////////////

typedef struct env_entry env_entry;

#define ENV_IS_STATIC 0
#define ENV_FREE_KEY 1
#define ENV_FREE_BOTH 2

struct env_entry {
  char *key;
  char *value;
  int flag;
  env_entry *next;
};

static bool env_ready = false;
static env_entry *env_head = NULL;

static env_entry *
env_find_key(const char *key)
{
  env_entry *entry = env_head;
  while (entry) {
    if (strcmp(entry->key, key) == 0)
      return entry;

    entry = entry->next;
  }

  return NULL;
}

static void
putenv_internal(char *key, char *value, int flag)
{
  env_entry *entry = env_find_key(key);
  if (entry) {
    // get rid of old key/value; if flag is set, then
    // they're static strings and we don't touch them
    if (entry->flag == ENV_FREE_BOTH)
      free (entry->value);
    if (entry->flag == ENV_FREE_KEY)
      free (entry->key);
  } else {
    entry = new env_entry;
    entry->next = env_head;
    env_head = entry;
  }

  entry->key = key;
  entry->value = value;
  entry->flag = flag;
}

static void
init_initial_env() {
  env_ready = true;

  putenv_internal("NSS_DEFAULT_DB_TYPE", "sql", ENV_IS_STATIC);
  putenv_internal("NSPR_FD_CACHE_SIZE_LOW", "10", ENV_IS_STATIC);
  putenv_internal("NSPR_FD_CACHE_SIZE_HIGH", "30", ENV_IS_STATIC);
  putenv_internal("XRE_PROFILE_NAME", "default", ENV_IS_STATIC);
  putenv_internal("tmp", "/Temp", ENV_IS_STATIC);
}

static void
putenv_copy(const char *k, const char *v)
{
  if (!env_ready)
    init_initial_env();

  putenv_internal(strdup(k), strdup(v), ENV_FREE_BOTH);
}

MOZCE_SHUNT_API int
putenv(const char *envstr)
{
  if (!env_ready)
    init_initial_env();

  char *key = strdup(envstr);
  char *value = strchr(key, '=');
  if (!value) {
    free(key);
    return -1;
  }

  *value++ = '\0';

  putenv_internal(key, value, ENV_FREE_KEY);

  return 0;
}

MOZCE_SHUNT_API char *
getenv(const char* name)
{
  if (!env_ready)
    init_initial_env();

  env_entry *entry = env_find_key(name);
  if (entry && entry->value[0] != 0) {
    return entry->value;
  }

  return NULL;
}

MOZCE_SHUNT_API char
GetEnvironmentVariableW(const unsigned short* lpName,
                        unsigned short* lpBuffer,
                        unsigned long nSize)
{
  char key[256];
  int rv = WideCharToMultiByte(CP_ACP, 0, lpName, -1, key, 255, NULL, NULL);
  if (rv <= 0)
    return 0;

  key[rv] = 0;
  
  char* val = getenv(key);
  
  if (!val)
    return 0;

  // strlen(val)+1, let MBTWC convert the nul byte for us
  return MultiByteToWideChar(CP_ACP, 0, val, strlen(val)+1, lpBuffer, nSize);
}

MOZCE_SHUNT_API char
SetEnvironmentVariableW(const unsigned short* name,
                        const unsigned short* value)
{
  char key[256];
  char val[256];
  int rv;

  rv = WideCharToMultiByte(CP_ACP, 0, name, -1, key, 255, NULL, NULL);
  if (rv < 0)
    return rv;

  key[rv] = 0;
  
  rv = WideCharToMultiByte(CP_ACP, 0, value, -1, val, 255, NULL, NULL);
  if (rv < 0)
    return rv;

  val[rv] = 0;

  putenv_copy(key, val);
  return 0;
}


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



MOZCE_SHUNT_API unsigned int ExpandEnvironmentStringsW(const unsigned short* lpSrc,
                                                       unsigned short* lpDst,
                                                       unsigned int nSize)
{
  if ( NULL == lpDst )
    return 0;
  
  unsigned int size = 0;
  unsigned int index = 0;
  unsigned int origLen = wcslen(lpSrc);

  const unsigned short *pIn = lpSrc;
  unsigned short *pOut = lpDst;
  
  while ( index < origLen ) {
    
    if (*pIn != L'%') {  // Regular char, copy over
      if ( size < nSize ) *pOut = *pIn, pOut++;
      index++, size++, pIn++;
      continue;
    }
    
    // Have a starting '%' - look for matching '%'
    int envlen = 0;
    const unsigned short *pTmp = ++pIn;                    // Move past original '%'
    while ( L'%' != *pTmp ) {
      envlen++, pTmp++;
      if ( origLen < index + envlen ) {    // Ran past end of original
        SetLastError(ERROR_INVALID_PARAMETER); // buffer without matching '%'
        return -1;
      }
    }
    
    if ( 0 == envlen ) {  // Encountered a "%%" - mapping to "%"
      size++;
      if ( size < nSize ) *pOut = *pIn, pOut++;
      pIn++;
      index += 2;
    } else {
      // Encountered a "%something%" - mapping "something"
      char key[256];
      int k = WideCharToMultiByte(CP_ACP, 0, pIn, envlen, key, 255, NULL, NULL);
      key[k] = 0;
      char *pC = getenv(key);
      if ( NULL != pC ) {
        int n = MultiByteToWideChar( CP_ACP, 0, pC, -1, pOut, nSize - size );
        if ( n > 0 ) {
          size += n - 1;  // Account for trailing zero
          pOut += n - 1;
        }
      }
      index += envlen + 2;
      pIn = ++pTmp;
    }
  }
  
  if ( size < nSize ) lpDst[size] = 0;
  return size;
}




////////////////////////////////////////////////////////
//  errno
////////////////////////////////////////////////////////

MOZCE_SHUNT_IMPORT_API char* strerror(int inErrno)
{
  return "Unknown Error";
}

MOZCE_SHUNT_IMPORT_API int errno = 0;


////////////////////////////////////////////////////////
//  File System Stuff
////////////////////////////////////////////////////////

MOZCE_SHUNT_API unsigned short * _wgetcwd(unsigned short * dir, unsigned long size)
{
  unsigned long i;
  GetModuleFileName(GetModuleHandle (NULL), dir, MAX_PATH);
  for (i = _tcslen(dir); i && dir[i] != TEXT('\\'); i--) {}
  dir[i + 1] = TCHAR('\0');
  return dir;
}

MOZCE_SHUNT_API unsigned short *_wfullpath( unsigned short *absPath, const unsigned short *relPath, unsigned long maxLength )
{
  if(absPath == NULL){
    absPath = (unsigned short *)malloc(maxLength*sizeof(unsigned short));
  }
  unsigned short cwd[MAX_PATH];
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

MOZCE_SHUNT_API int _unlink(const char *filename)
{
  unsigned short wname[MAX_PATH];
  
  MultiByteToWideChar(CP_ACP,
                      0,
                      filename,
                      strlen(filename)+1,
                      wname,
                      MAX_PATH );
  return DeleteFileW(wname);
}

MOZCE_SHUNT_API void abort(void)
{
#if defined(DEBUG)
  DebugBreak();
#endif
  TerminateProcess((HANDLE) GetCurrentProcessId(), 3);
}

////////////////////////////////////////////////////////
//  Time Stuff
////////////////////////////////////////////////////////

// This is the kind of crap that makes me hate microsoft.  defined in their system headers, but not implemented anywhere.
#define strftime __not_supported_on_device_strftime
#define localtime __not_supported_on_device_localtime
#define mktime __not_supported_on_device_mktime
#define gmtime __not_supported_on_device_gmtime
#define time __not_supported_on_device_time
#define clock __not_supported_on_device_clock
#include <time.h>
#undef strftime
#undef localtime
#undef mktime
#undef gmtime
#undef time
#undef clock

extern "C" {
  MOZCE_SHUNT_API size_t strftime(char *, size_t, const char *, const struct tm *);
  MOZCE_SHUNT_API struct tm* localtime(const time_t* inTimeT);
  MOZCE_SHUNT_API time_t mktime(struct tm* inTM);
  MOZCE_SHUNT_API struct tm* gmtime(const time_t* inTimeT);
  MOZCE_SHUNT_API time_t time(time_t *);
  MOZCE_SHUNT_API clock_t clock();
}

static const int sDaysOfYear[12] = {
  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};
static struct tm tmStorage;

MOZCE_SHUNT_API size_t strftime(char *, size_t, const char *, const struct tm *)
{
  return 0;
}

static struct tm* mozce_gmtime_r(const time_t* inTimeT, struct tm* outRetval)
{
  struct tm* retval = NULL;
  
  if(NULL != inTimeT) {
    SYSTEMTIME winGMTime;
    
    time_t_2_SYSTEMTIME(winGMTime, *inTimeT);
    
    outRetval->tm_sec = (int)winGMTime.wSecond;
    outRetval->tm_min = (int)winGMTime.wMinute;
    outRetval->tm_hour = (int)winGMTime.wHour;
    outRetval->tm_mday = (int)winGMTime.wDay;
    outRetval->tm_mon = (int)(winGMTime.wMonth - 1);
    outRetval->tm_year = (int)(winGMTime.wYear - 1900);
    outRetval->tm_wday = (int)winGMTime.wDayOfWeek;
    outRetval->tm_isdst = -1;
    
    outRetval->tm_yday = (int)winGMTime.wDay + sDaysOfYear[outRetval->tm_mon];
    if(0 == (winGMTime.wYear & 3)) {
      if(2 < winGMTime.wMonth) {
        if(0 == winGMTime.wYear % 100) {
          if(0 == winGMTime.wYear % 400) {
            outRetval->tm_yday++;
          }
        }else {
          outRetval->tm_yday++;
        }
      }
    }
    retval = outRetval;
  }
  return retval;
}

static struct tm* mozce_localtime_r(const time_t* inTimeT,struct tm* outRetval)
{
  struct tm* retval = NULL;
  
  if(NULL != inTimeT && NULL != outRetval) {
    SYSTEMTIME winLocalTime;
    
    time_t_2_LOCALSYSTEMTIME(winLocalTime, *inTimeT);
    
    outRetval->tm_sec = (int)winLocalTime.wSecond;
    outRetval->tm_min = (int)winLocalTime.wMinute;
    outRetval->tm_hour = (int)winLocalTime.wHour;
    outRetval->tm_mday = (int)winLocalTime.wDay;
    outRetval->tm_mon = (int)(winLocalTime.wMonth - 1);
    outRetval->tm_year = (int)(winLocalTime.wYear - 1900);
    outRetval->tm_wday = (int)winLocalTime.wDayOfWeek;
    outRetval->tm_isdst = -1;
    
    outRetval->tm_yday = (int)winLocalTime.wDay + sDaysOfYear[outRetval->tm_mon];
    if(0 == (winLocalTime.wYear & 3)) {
      if(2 < winLocalTime.wMonth) {
        if(0 == winLocalTime.wYear % 100) {
          if(0 == winLocalTime.wYear % 400) {
            outRetval->tm_yday++;
          }
        } else {
          outRetval->tm_yday++;
        }
      }
    }
    retval = outRetval;
  }
  return retval;
}


MOZCE_SHUNT_API struct tm* localtime(const time_t* inTimeT)
{
  return mozce_localtime_r(inTimeT, &tmStorage);
}

MOZCE_SHUNT_API struct tm* gmtime(const time_t* inTimeT)
{
  return mozce_gmtime_r(inTimeT, &tmStorage);
}


MOZCE_SHUNT_API time_t mktime(struct tm* inTM)
{
  time_t retval = (time_t)-1;
  
  if(NULL != inTM) {
    SYSTEMTIME winTime;
    struct tm* gmTime = NULL;
    
    memset(&winTime, 0, sizeof(winTime));
    
    /*
     * Ignore tm_wday and tm_yday.
     * We likely have some problems with dst.
     */
    winTime.wSecond = inTM->tm_sec;
    winTime.wMinute = inTM->tm_min;
    winTime.wHour = inTM->tm_hour;
    winTime.wDay = inTM->tm_mday;
    winTime.wMonth = inTM->tm_mon + 1;
    winTime.wYear = inTM->tm_year + 1900;
    
    /*
     * First get our time_t.
     */
    SYSTEMTIME_2_time_t(retval, winTime);
    
    /*
     * Now overwrite the struct passed in with what we believe it should be.
     */
    gmTime = mozce_gmtime_r(&retval, inTM);
  }
  return retval;
}

MOZCE_SHUNT_API time_t time(time_t *)
{
  time_t retval;
  SYSTEMTIME winTime;
  ::GetSystemTime(&winTime);
  SYSTEMTIME_2_time_t(retval, winTime);
  return retval;
}

MOZCE_SHUNT_API clock_t clock() 
{
  return -1;
}

////////////////////////////////////////////////////////
//  Locale Stuff
////////////////////////////////////////////////////////

#define localeconv __not_supported_on_device_localeconv
#include <locale.h>
#undef localeconv

extern "C" {
  MOZCE_SHUNT_API struct lconv * localeconv(void);
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

MOZCE_SHUNT_API struct lconv * localeconv(void)
{
  return &s_locale_conv;
}

MOZCE_SHUNT_API unsigned short *
mozce_GetEnvironmentCL()
{
  env_entry *entry = env_head;
  int len = 0;
  while (entry) {
    if (entry->flag == ENV_IS_STATIC) {
      entry = entry->next;
      continue;
    }

    len += strlen(entry->key);
    len += strlen(entry->value);

    // for each env var, 11 chars of " --environ:", 3 chars of '"="', and a null at the end
    len += 11 + 3 + 1;

    entry = entry->next;
  }

  if (len == 0) {
    return wcsdup(L"");
  }

  wchar_t *env = (wchar_t*) malloc(sizeof(wchar_t) * (len+1));
  if (!env)
    return NULL;

  entry = env_head;

  int pos = 0;
  while (entry) {
    if (entry->flag == ENV_IS_STATIC) {
      entry = entry->next;
      continue;
    }

    if (strchr(entry->key, '"') || strchr(entry->value, '"')) {
      // argh, we don't have a good way of encoding the ", so let's just
      // ignore this var for now
      RETAILMSG(1, (L"Skipping environment variable with quote marks in key or value! %S -> %s\r\n", entry->key, entry->value));
      entry = entry->next;
      continue;
    }

    wcscpy (env+pos, L" --environ:\"");
    pos += 12;
    pos += MultiByteToWideChar(CP_ACP, 0, entry->key, strlen(entry->key), env+pos, len-pos);
    env[pos++] = '=';
    pos += MultiByteToWideChar(CP_ACP, 0, entry->value, strlen(entry->value), env+pos, len-pos);
    env[pos++] = '\"';

    entry = entry->next;
  } 

  env[pos] = '\0';

  return env;
  
}

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
