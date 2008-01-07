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
#include "time_conversions.h"
#include <time.h>
extern "C" {
#if 0
}
#endif


static const int sDaysOfYear[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};
static struct tm tmStorage;

MOZCE_SHUNT_API size_t strftime(char *, size_t, const char *, const struct tm *)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_strftime called\n");
#endif

    return 0;
}


MOZCE_SHUNT_API struct tm* mozce_localtime_r(const time_t* inTimeT,struct tm* outRetval)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("tm* mozce_localtime_r called\n");
#endif

    struct tm* retval = NULL;

    if(NULL != inTimeT && NULL != outRetval)
    {
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
        if(0 == (winLocalTime.wYear & 3))
        {
            if(2 < winLocalTime.wMonth)
            {
                if(0 == winLocalTime.wYear % 100)
                {
                    if(0 == winLocalTime.wYear % 400)
                    {
                        outRetval->tm_yday++;
                    }
                }
                else
                {
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
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("tm* mozce_localtime called\n");
#endif

    return mozce_localtime_r(inTimeT, &tmStorage);
}


MOZCE_SHUNT_API struct tm* mozce_gmtime_r(const time_t* inTimeT, struct tm* outRetval)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("tm* mozce_gmtime_r called\n");
#endif

    struct tm* retval = NULL;

    if(NULL != inTimeT)
    {
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
        if(0 == (winGMTime.wYear & 3))
        {
            if(2 < winGMTime.wMonth)
            {
                if(0 == winGMTime.wYear % 100)
                {
                    if(0 == winGMTime.wYear % 400)
                    {
                        outRetval->tm_yday++;
                    }
                }
                else
                {
                    outRetval->tm_yday++;
                }
            }
        }

        retval = outRetval;
    }

    return retval;
}


MOZCE_SHUNT_API struct tm* gmtime(const time_t* inTimeT)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("tm* mozce_gmtime called\n");
#endif

    return mozce_gmtime_r(inTimeT, &tmStorage);
}


MOZCE_SHUNT_API time_t mktime(struct tm* inTM)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("mozce_mktime called\n");
#endif

    time_t retval = (time_t)-1;

    if(NULL != inTM)
    {
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
#if 0
{
#endif
} /* extern "C" */

