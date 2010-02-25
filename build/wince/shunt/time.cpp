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
#include <Windows.h>
#include <altcecrt.h>

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
  size_t strftime(char *, size_t, const char *, const struct tm *);
  struct tm* localtime(const time_t* inTimeT);
  time_t mktime(struct tm* inTM);
  struct tm* gmtime(const time_t* inTimeT);
  time_t time(time_t *);
  clock_t clock();
}

static const int sDaysOfYear[12] = {
  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};
static struct tm tmStorage;

size_t strftime(char *out, size_t maxsize, const char *pat, const struct tm *time)
{
  size_t ret = 0;
  size_t wpatlen = MultiByteToWideChar(CP_ACP, 0, pat, -1, 0, 0);
  if (wpatlen) {
    wchar_t* wpat = (wchar_t*)calloc(wpatlen, sizeof(wchar_t));
    if (wpat) {
      wchar_t* tmpBuf = (wchar_t*)calloc(maxsize, sizeof(wchar_t));
      if (tmpBuf) {
        if (MultiByteToWideChar(CP_ACP, 0, pat, -1, wpat, wpatlen)) {
          if (wcsftime(tmpBuf, maxsize, wpat, time))
            ret = ::WideCharToMultiByte(CP_ACP, 0, tmpBuf, -1, out, maxsize, 0, 0);
        }
        free(tmpBuf);
      }
      free(wpat);
    }
  }
  return ret;
}

struct tm* gmtime_r(const time_t* inTimeT, struct tm* outRetval)
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

struct tm* localtime_r(const time_t* inTimeT,struct tm* outRetval)
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


struct tm* localtime(const time_t* inTimeT)
{
  return localtime_r(inTimeT, &tmStorage);
}

struct tm* gmtime(const time_t* inTimeT)
{
  return gmtime_r(inTimeT, &tmStorage);
}


time_t mktime(struct tm* inTM)
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
    gmTime = gmtime_r(&retval, inTM);
  }
  return retval;
}

time_t time(time_t *)
{
  time_t retval;
  SYSTEMTIME winTime;
  ::GetSystemTime(&winTime);
  SYSTEMTIME_2_time_t(retval, winTime);
  return retval;
}

clock_t clock() 
{
  return -1;
}

