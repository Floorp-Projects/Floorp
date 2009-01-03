/* -*- Mode: C; tab-width: 4 -*- */
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


 #include "stdlib.h"
 #include "Windows.h"
 
#include "mozce_shunt.h"
#include "time_conversions.h"

////////////////////////////////////////////////////////
//  Environment Variable Stuff
////////////////////////////////////////////////////////

// Convert to registry soon!

struct mapping{
char* key;
char* value;
mapping* next;
};

static int init_i =1;
static mapping initial_map[] = {
#ifdef DEBUG_NSPR_ALL
    {"NSPR_LOG_MODULES", "all:5",initial_map + (init_i++)},
    {"NSPR_LOG_FILE","nspr.log",initial_map + (init_i++)},
#endif  
#ifdef TIMELINE
    {"NS_TIMELINE_LOG_FILE","\\bin\\timeline.log",initial_map + (init_i++)},
    {"NS_TIMELINE_ENABLE", "1",initial_map + (init_i++)},
#endif
    {"tmp", "/Temp",initial_map + (init_i++)},
    {"GRE_HOME",".",initial_map + (init_i++)},
    {"NSS_DEFAULT_DB_TYPE", "sql",initial_map + (init_i++)},
    {"NSPR_FD_CACHE_SIZE_LOW", "10",initial_map + (init_i++)},              
    {"NSPR_FD_CACHE_SIZE_HIGH", "30",initial_map + (init_i++)},
    {"XRE_PROFILE_PATH", "\\Application Data\\Mozilla\\Profiles",initial_map + (init_i++)},
    {"XRE_PROFILE_LOCAL_PATH","./profile",initial_map + (init_i++)},
    {"XRE_PROFILE_NAME","default",0}
};

static mapping* head = initial_map;

 
mapping* getMapping(const char* key)
{
  mapping* cur = head;
  while(cur != NULL){
    if(!strcmp(cur->key,key))
      return cur;
    cur = cur->next;
  }
  return NULL;
}

int map_put(const char* key,const char* val)
{
  mapping* map = getMapping(key);
  if(map){
    if(!((map > initial_map) && 
         (map < (initial_map + init_i))))
      free( map->value);
  }else{        
    map = (mapping*)malloc(sizeof(mapping));
    map->key = (char*)malloc((strlen(key)+1)*sizeof(char));
    strcpy(map->key,key);
    map->next = head;
    head = map;
  }
  map->value = (char*)malloc((strlen(val)+1)*sizeof(char));
  strcpy(map->value,val);
  return 0;
}

char*  map_get(const char* key)
{
  mapping* map = getMapping(key);
  if(map)
    return map->value;
  return NULL;
}
 
MOZCE_SHUNT_API char* getenv(const char* inName)
{
  return map_get(inName);
}
 
MOZCE_SHUNT_API int putenv(const char *a)
{
  int len = strlen(a);
  char* key = (char*) malloc(len*sizeof(char));
  strcpy(key,a);
  char* val = strchr(key,'=');
  val[0] = '\0';
  int rv;
  val++;
  rv = map_put(key,val);
  free(key);
  return rv;
}

MOZCE_SHUNT_API char GetEnvironmentVariableW(const unsigned short * lpName, unsigned short* lpBuffer, unsigned long nSize)
{
  char key[256];
  int rv = WideCharToMultiByte(CP_ACP,
                               0,
                               lpName,
                               -1,
                               key,
                               256,
                               NULL,
                               NULL);
  if(rv < 0)
    return rv;
  
  char* val = map_get(key);
  
  if(val) 
    {
      MultiByteToWideChar(CP_ACP,
                          0,
                          val,
                          strlen(val)+1,
                          lpBuffer,
                          nSize );
      return ERROR_SUCCESS;
    }
  return -1;
}

MOZCE_SHUNT_API char SetEnvironmentVariableW( const unsigned short * name, const unsigned short * value )
{
  char key[256];
  char val[256];
  int rv = WideCharToMultiByte(CP_ACP,
                               0,
                               name,
                               -1,
                               key,
                               256,
                               NULL,
                               NULL);
  if(rv < 0)
    return rv;
  
  rv = WideCharToMultiByte(CP_ACP,
                           0,
                           value,
                           -1,
                           val,
                           256,
                           NULL,
                           NULL);
  if(rv < 0)
    return rv;
  
  return map_put(key,val);
}

////////////////////////////////////////////////////////
//  errno
////////////////////////////////////////////////////////

MOZCE_SHUNT_API char* strerror(int inErrno)
{
  return "Unknown Error";
}

MOZCE_SHUNT_API int errno = 0;


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
  _wgetcwd( absPath, maxLength);
  unsigned long len = wcslen(absPath);
  if(!(absPath[len-1] == TCHAR('/') || absPath[len-1] == TCHAR('\\'))&& len< maxLength){
    absPath[len] = TCHAR('\\');
    absPath[++len] = TCHAR('\0');
  }
  if(len+wcslen(relPath) < maxLength){
    return wcscat(absPath,relPath);
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
#if 0
}
#endif

static const int sDaysOfYear[12] = {
  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};
static struct tm tmStorage;

#ifdef strftime
#undef strftime
#endif

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
 
 #if 0
 {
 #endif
 } /* extern "C" */
