/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
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

#include "calDateTime.h"

#include "calAttributeHelpers.h"

extern "C" {
    #include "ical.h"
}

NS_IMPL_ISUPPORTS1(calDateTime, calIDateTime)

calDateTime::calDateTime()
    : mImmutable(PR_FALSE),
      mValid(PR_FALSE)
{
    mNativeTime = 0;
    mYear = 0;
    mMonth = 0;
    mDay = 0;
    mHour = 0;
    mMinute = 0;
    mSecond = 0;
    mIsUtc = PR_FALSE;
    mWeekday = 0;
    mYearday = 0;
}

calDateTime::calDateTime(struct icaltimetype *atimeptr)
{
    fromIcalTime(atimeptr);
    mValid = PR_TRUE;
}

NS_IMETHODIMP
calDateTime::GetIsMutable(PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = !mImmutable;
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::MakeImmutable()
{
    if (mImmutable)
        return NS_ERROR_FAILURE; // XXX another error code

    mImmutable = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::Clone(calIDateTime **aResult)
{
    calDateTime *cdt = new calDateTime();
    if (!cdt)
        return NS_ERROR_OUT_OF_MEMORY;

    // bitwise copy everything
    cdt->mValid = mValid;
    cdt->mNativeTime = mNativeTime;
    cdt->mYear = mYear;
    cdt->mMonth = mMonth;
    cdt->mDay = mDay;
    cdt->mHour = mHour;
    cdt->mMinute = mMinute;
    cdt->mSecond = mSecond;
    cdt->mIsUtc = mIsUtc;
    cdt->mWeekday = mWeekday;
    cdt->mYearday = mYearday;

    cdt->mLastModified = PR_Now();

    // copies are always mutable
    cdt->mImmutable = PR_FALSE;
    
    NS_ADDREF(*aResult = cdt);
    return NS_OK;
}

CAL_VALUETYPE_ATTR_GETTER(calDateTime, PRBool, Valid)

CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Year)
CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Month)
CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Day)
CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Hour)
CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Minute)
CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Second)
CAL_VALUETYPE_ATTR(calDateTime, PRBool, IsUtc)

CAL_VALUETYPE_ATTR_GETTER(calDateTime, PRInt16, Weekday)
CAL_VALUETYPE_ATTR_GETTER(calDateTime, PRInt16, Yearday)

CAL_STRINGTYPE_ATTR(calDateTime, nsACString, Timezone)

NS_IMETHODIMP
calDateTime::GetNativeTime(PRTime *aResult)
{
    *aResult = mNativeTime;
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::SetNativeTime(PRTime aNativeTime)
{
    if (mIsUtc) {
        return SetTimeInTimezone (aNativeTime, "UTC");
    } else if (!mTimezone.IsEmpty()) {
        return SetTimeInTimezone (aNativeTime, mTimezone.get());
    } else {
        return SetTimeInTimezone (aNativeTime, NULL);
    }
}

NS_IMETHODIMP
calDateTime::Normalize()
{
    struct icaltimetype icalt;

    toIcalTime(&icalt);

    icalt = icaltime_normalize(icalt);

    fromIcalTime(&icalt);
    
    mValid = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::ToString(nsACString& aResult)
{
    aResult.Assign("FOO");
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::SetTimeInTimezone(PRTime aTime, const char *aTimezone)
{
    struct icaltimetype icalt;
    time_t tt;

    PRInt64 temp, million;
    LL_I2L(million, PR_USEC_PER_SEC);
    LL_DIV(temp, aTime, million);
    PRInt32 sectime;
    LL_L2I(sectime, temp);

    tt = sectime;
    icalt = icaltime_from_timet(tt, 0);
    icalt = icaltime_as_utc(icalt, aTimezone);

    fromIcalTime(&icalt);

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetInTimezone(const char *aTimezone, calIDateTime **aResult)
{
    struct icaltimetype icalt;
    toIcalTime(&icalt);

    icalt = icaltime_as_zone(icalt, aTimezone);
    calDateTime *cdt = new calDateTime(&icalt);
    if (!cdt)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF (*aResult = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetStartOfWeek(calIDateTime **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
calDateTime::GetEndOfWeek(calIDateTime **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
calDateTime::GetStartOfMonth(calIDateTime **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
calDateTime::GetEndOfMonth(calIDateTime **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
calDateTime::GetStartOfYear(calIDateTime **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
calDateTime::GetEndOfYear(calIDateTime **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/**
 ** utility/protected methods
 **/

void
calDateTime::toIcalTime(icaltimetype *icalt)
{
    icalt->year = mYear;
    icalt->month = mMonth;
    icalt->day = mDay;
    icalt->hour = mHour;
    icalt->minute = mMinute;
    icalt->second = mSecond;

    if (mIsUtc)
        icalt->is_utc = 1;
    else
        icalt->is_utc = 0;
}

void
calDateTime::fromIcalTime(icaltimetype *icalt)
{
    mYear = icalt->year;
    mMonth = icalt->month;
    mDay = icalt->day;
    mHour = icalt->hour;
    mMinute = icalt->minute;
    mSecond = icalt->second;

    if (icalt->is_utc)
        mIsUtc = PR_TRUE;
    else
        mIsUtc = PR_FALSE;

    // reconstruct nativetime
    time_t tt = icaltime_as_timet(*icalt);
    PRInt64 temp, million;
    LL_I2L(million, PR_USEC_PER_SEC);
    LL_I2L(temp, tt);
    LL_MUL(temp, temp, million);
    mNativeTime = temp;

    // reconstruct weekday/yearday
    mWeekday = icaltime_day_of_week(*icalt);
    mYearday = icaltime_day_of_year(*icalt);
}

