/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Dan Mosedale <dan.mosedale@oracle.com>
 *   Michiel van Leeuwen <mvl@exedo.nl>
 *   Clint Talbert <cmtalbert@myfastmail.com>
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

#include "nsPrintfCString.h"

#include "calDateTime.h"
#include "calAttributeHelpers.h"
#include "calBaseCID.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#ifndef MOZILLA_1_8_BRANCH
#include "nsIClassInfoImpl.h"
#endif

#include "calIICSService.h"
#include "calIDuration.h"
#include "calIErrors.h"

#include "jsdate.h"

extern "C" {
    #include "ical.h"
}

static NS_DEFINE_CID(kCalICSService, CAL_ICSSERVICE_CID);

NS_IMPL_ISUPPORTS2_CI(calDateTime, calIDateTime, nsIXPCScriptable)

calDateTime::calDateTime()
    : mImmutable(PR_FALSE),
      mIsValid(PR_FALSE)
{
    Reset();
}

calDateTime::calDateTime(struct icaltimetype *atimeptr)
    : mImmutable(PR_FALSE)
{
    FromIcalTime(atimeptr);
    mIsValid = PR_TRUE;
}

calDateTime::calDateTime(const calDateTime& cdt)
{
    // bitwise copy everything
    mIsValid = cdt.mIsValid;
    mNativeTime = cdt.mNativeTime;
    mYear = cdt.mYear;
    mMonth = cdt.mMonth;
    mDay = cdt.mDay;
    mHour = cdt.mHour;
    mMinute = cdt.mMinute;
    mSecond = cdt.mSecond;
    mWeekday = cdt.mWeekday;
    mYearday = cdt.mYearday;

    mIsDate = cdt.mIsDate;

    mLastModified = PR_Now();

    // copies are always mutable
    mImmutable = PR_FALSE;

    mTimezone.Assign(cdt.mTimezone);
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
        return NS_ERROR_OBJECT_IS_IMMUTABLE;

    mImmutable = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::Clone(calIDateTime **aResult)
{
    calDateTime *cdt = new calDateTime(*this);
    if (!cdt)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::Reset()
{
    if (mImmutable)
        return NS_ERROR_FAILURE;

    mNativeTime = 0;
    mYear = 0;
    mMonth = 0;
    mDay = 0;
    mHour = 0;
    mMinute = 0;
    mSecond = 0;
    mWeekday = 0;
    mYearday = 0;
    mIsDate = PR_FALSE;
    mTimezone.AssignLiteral("UTC");

    mIsValid = PR_FALSE;
    return NS_OK;
}

CAL_VALUETYPE_ATTR_GETTER(calDateTime, PRBool, IsValid)

CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Year)
CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Month)
CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Day)
CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Hour)
CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Minute)
CAL_VALUETYPE_ATTR(calDateTime, PRInt16, Second)

CAL_VALUETYPE_ATTR_GETTER(calDateTime, PRInt16, Weekday)
CAL_VALUETYPE_ATTR_GETTER(calDateTime, PRInt16, Yearday)

CAL_STRINGTYPE_ATTR_GETTER(calDateTime, nsACString, Timezone)

NS_IMETHODIMP
calDateTime::SetIsDate(PRBool aIsDate)
{
    if (mImmutable)
        return NS_ERROR_FAILURE;

    mIsDate = aIsDate;
    if (aIsDate) {
        mHour = 0;
        mMinute = 0;
        mSecond = 0;
        Normalize();
    }
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetIsDate(PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = mIsDate;
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::SetTimezone(const nsACString& aTimezone)
{
    if (aTimezone.EqualsLiteral("UTC") || aTimezone.EqualsLiteral("utc")) {
        mTimezone.AssignLiteral("UTC");
    } else if (aTimezone.EqualsLiteral("floating")) {
        mTimezone.AssignLiteral("floating");
    } else {
        icaltimezone *tz = nsnull;
        nsresult rv = GetIcalTZ(aTimezone, &tz);
        if (NS_FAILED(rv))
            return rv;

        mTimezone.Assign(aTimezone);
    }

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetTimezoneOffset(PRInt32 *aResult)
{
    struct icaltimetype icalt;
    ToIcalTime(&icalt);

    int dst;
    *aResult = 
        icaltimezone_get_utc_offset(NS_CONST_CAST(icaltimezone* ,icalt.zone),
                                    &icalt, &dst);
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetNativeTime(PRTime *aResult)
{
    *aResult = mNativeTime;
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::SetNativeTime(PRTime aNativeTime)
{
    return SetTimeInTimezone (aNativeTime, NS_LITERAL_CSTRING("UTC"));
}

NS_IMETHODIMP
calDateTime::Normalize()
{
    struct icaltimetype icalt;

    ToIcalTime(&icalt);
    icalt = icaltime_normalize(icalt);
    FromIcalTime(&icalt);
    
    mIsValid = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::AddDuration(calIDuration *aDuration)
{
    NS_ENSURE_ARG_POINTER(aDuration);

    struct icaldurationtype idt;
    aDuration->ToIcalDuration(&idt);
    
    struct icaltimetype itt;
    ToIcalTime(&itt);

    struct icaltimetype newitt;
    newitt = icaltime_add(itt, idt);
    FromIcalTime(&newitt);

    mIsValid = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::SubtractDate(calIDateTime *aDate, calIDuration **aDuration)
{
    NS_ENSURE_ARG_POINTER(aDate);

    struct icaltimetype itt1;
    struct icaltimetype itt2;

    ToIcalTime(&itt1);
    aDate->ToIcalTime(&itt2);

    // same as icaltime_subtract(), but minding timezones:
    PRTime t2t;
    aDate->GetNativeTime(&t2t);
    // for a duration, need to convert the difference in microseconds (prtime)
    // to seconds (libical), so divide by one million.
    icaldurationtype idt = 
        icaldurationtype_from_int(NS_STATIC_CAST(int, (mNativeTime - t2t) / PRInt64(PR_USEC_PER_SEC)));

    nsCOMPtr<calIDuration> result(do_CreateInstance("@mozilla.org/calendar/duration;1"));
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    result->SetIsNegative(idt.is_neg);
    result->SetWeeks(idt.weeks);
    result->SetDays(idt.days);
    result->SetHours(idt.hours);
    result->SetMinutes(idt.minutes);
    result->SetSeconds(idt.seconds);

    *aDuration = result;
    NS_ADDREF(*aDuration);

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::ToString(nsACString& aResult)
{
    aResult.Assign(nsPrintfCString(100,
                                   "%04d/%02d/%02d %02d:%02d:%02d %s",
                                   mYear, mMonth + 1, mDay,
                                   mHour, mMinute, mSecond,
                                   mTimezone.get()));
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::SetTimeInTimezone(PRTime aTime, const nsACString& aTimezone)
{
    icaltimezone *tz = nsnull;

    if (aTimezone.IsEmpty())
        return NS_ERROR_INVALID_ARG;

    nsresult rv = GetIcalTZ(aTimezone, &tz);
    NS_ENSURE_SUCCESS(rv, rv);

    struct icaltimetype icalt = icaltime_null_time();
    PRTimeToIcaltime(aTime, PR_FALSE, tz, &icalt);

    FromIcalTime(&icalt);
    mIsValid = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetInTimezone(const nsACString& aTimezone, calIDateTime **aResult)
{
    calDateTime *cdt = nsnull;

    if (mIsDate) {
        // if it's a date, we really just want to make a copy of this
        // and set the timezone.
        cdt = new calDateTime(*this);
        if (!mTimezone.EqualsLiteral("floating"))
            cdt->mTimezone.Assign(aTimezone);
    } else {
        struct icaltimetype icalt;
        icaltimezone *tz = nsnull;

        ToIcalTime(&icalt);

        // Get the latest version of aTimezone.
        nsresult rv;

        nsCOMPtr<calIICSService> icsSvc = do_GetService(kCalICSService, &rv);
        if (NS_FAILED(rv)) {
            return rv;
        }

        nsCAutoString newTimezone;
        rv = icsSvc->LatestTzId(aTimezone, newTimezone);
        if (NS_FAILED(rv)) {
            return rv;
        }

        if (newTimezone.Length() == 0) {
            newTimezone = aTimezone;
        }

        rv = GetIcalTZ(newTimezone, &tz);
        if (NS_FAILED(rv))
            return rv;

        /* If there's a zone, we need to convert; otherwise, we just
         * assign, since this item is floating */
        if (icalt.zone && tz) {
            icaltimezone_convert_time(&icalt, (icaltimezone*) icalt.zone, tz);
        }
        if (tz == icaltimezone_get_utc_timezone())
            icalt.is_utc = 1;
        else
            icalt.is_utc = 0;

        if (!mTimezone.EqualsLiteral("floating"))
            icalt.zone = tz;

        cdt = new calDateTime(&icalt);
    }

    NS_ADDREF (*aResult = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetStartOfWeek(calIDateTime **aResult)
{
    struct icaltimetype icalt;
    ToIcalTime(&icalt);
    int day_of_week = icaltime_day_of_week(icalt);
    if (day_of_week > 1)
        icaltime_adjust(&icalt, - (day_of_week - 1), 0, 0, 0);
    icalt.is_date = 1;

    calDateTime *cdt = new calDateTime(&icalt);
    NS_ADDREF(*aResult = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetEndOfWeek(calIDateTime **aResult)
{
    struct icaltimetype icalt;
    ToIcalTime(&icalt);
    int day_of_week = icaltime_day_of_week(icalt);
    if (day_of_week < 7)
        icaltime_adjust(&icalt, 7 - day_of_week, 0, 0, 0);
    icalt.is_date = 1;

    calDateTime *cdt = new calDateTime(&icalt);
    NS_ADDREF(*aResult = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetStartOfMonth(calIDateTime **aResult)
{
    struct icaltimetype icalt;
    ToIcalTime(&icalt);
    icalt.day = 1;
    icalt.is_date = 1;

    calDateTime *cdt = new calDateTime(&icalt);
    NS_ADDREF(*aResult = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetEndOfMonth(calIDateTime **aResult)
{
    struct icaltimetype icalt;
    ToIcalTime(&icalt);
    icalt.day = icaltime_days_in_month(icalt.month, icalt.year);
    icalt.is_date = 1;

    calDateTime *cdt = new calDateTime(&icalt);
    NS_ADDREF(*aResult = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetStartOfYear(calIDateTime **aResult)
{
    struct icaltimetype icalt;
    ToIcalTime(&icalt);
    icalt.month = 1;
    icalt.day = 1;
    icalt.is_date = 1;

    calDateTime *cdt = new calDateTime(&icalt);
    NS_ADDREF(*aResult = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetEndOfYear(calIDateTime **aResult)
{
    struct icaltimetype icalt;
    ToIcalTime(&icalt);
    icalt.month = 12;
    icalt.day = 31;
    icalt.is_date = 1;

    calDateTime *cdt = new calDateTime(&icalt);
    NS_ADDREF(*aResult = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetIcalString(nsACString& aResult)
{
    icaltimetype t;
    ToIcalTime(&t);

    // note that ics is owned by libical, so we don't need to free
    const char *ics = icaltime_as_ical_string(t);
    
    if (ics) {
        aResult.Assign(ics);
        return NS_OK;
    }

    return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
calDateTime::SetIcalString(const nsACString& aIcalString)
{
    struct icaltimetype icalt;
    icalt = icaltime_from_string(nsPromiseFlatCString(aIcalString).get());
    if (icaltime_is_null_time(icalt)) {
        return calIErrors::ICS_ERROR_BASE + icalerrno;
    }
    FromIcalTime(&icalt);
    return NS_OK;
}

/**
 ** utility/protected methods
 **/

NS_IMETHODIMP_(void)
calDateTime::ToIcalTime(icaltimetype *icalt)
{
    icalt->year = mYear;
    icalt->month = mMonth + 1;
    icalt->day = mDay;
    icalt->hour = mHour;
    icalt->minute = mMinute;
    icalt->second = mSecond;

    icalt->is_date = mIsDate ? 1 : 0;
    icalt->is_daylight = 0;

    icaltimezone* tz = nsnull;
    nsresult rv = GetIcalTZ(mTimezone, &tz);
    // this will always succeed, because
    // mTimezone can't be set without GetIcalTZ
    // succeeding.
    if (NS_FAILED(rv)) {
        NS_ERROR("calDateTime::ToIcalTime: GetIcalTZ failed!");
    }

    icalt->zone = tz;

    if (icalt->zone == icaltimezone_get_utc_timezone())
        icalt->is_utc = 1;
    else
        icalt->is_utc = 0;
}

void
calDateTime::FromIcalTime(icaltimetype *icalt)
{
    icaltimetype t = *icalt;

    mYear = t.year;
    mMonth = t.month - 1;
    mDay = t.day;

    mIsDate = t.is_date ? PR_TRUE : PR_FALSE;

    if (!mIsDate) {
        mHour = t.hour;
        mMinute = t.minute;
        mSecond = t.second;
    } else {
        mHour = 0;
        mMinute = 0;
        mSecond = 0;
    }

    if (t.is_utc || t.zone == icaltimezone_get_utc_timezone())
        mTimezone.AssignLiteral("UTC");
    else if (t.zone)
        mTimezone.Assign(icaltimezone_get_tzid((icaltimezone*)t.zone));
    else
        mTimezone.AssignLiteral("floating");

    mWeekday = icaltime_day_of_week(t) - 1;
    mYearday = icaltime_day_of_year(t);
    
    // mNativeTime: not moving the existing date to UTC,
    // but merely representing it a UTC-based way.
    t.is_date = 0;
    mNativeTime = IcaltimeToPRTime(&t, icaltimezone_get_utc_timezone());
}

PRTime
calDateTime::IcaltimeToPRTime(const icaltimetype *const icalt, struct _icaltimezone *const tz)
{
    struct icaltimetype tt = *icalt;
    struct PRExplodedTime et;

    /* If the time is the special null time, return 0. */
    if (icaltime_is_null_time(*icalt)) {
        return 0;
    }

    if (tz) {
        // use libical for timezone conversion, as it can handle all ics
        // timezones. having nspr do it is much harder.
        tt = icaltime_convert_to_zone(*icalt, tz);
    }

    /* Empty the destination */
    memset(&et, 0, sizeof(struct PRExplodedTime));

    /* Fill the fields */
    if (icaltime_is_date(tt)) {
        et.tm_sec = et.tm_min = et.tm_hour = 0;
    } else {
        et.tm_sec = tt.second;
        et.tm_min = tt.minute;
        et.tm_hour = tt.hour;
    }
    et.tm_mday = tt.day;
    et.tm_month = tt.month-1;
    et.tm_year = tt.year;

    return PR_ImplodeTime(&et);
}

void
calDateTime::PRTimeToIcaltime(const PRTime time, const PRBool isdate,
                              struct _icaltimezone *const tz,
                              icaltimetype *icalt)
{
    icaltimezone *utc_zone;

    struct PRExplodedTime et;
    PR_ExplodeTime(time, PR_GMTParameters, &et);

    icalt->year   = et.tm_year;
    icalt->month  = et.tm_month + 1;
    icalt->day    = et.tm_mday;

    if (isdate) { 
        icalt->is_date = 1;
        return;
    }

    icalt->hour   = et.tm_hour;
    icalt->minute = et.tm_min;
    icalt->second = et.tm_sec;

    /* Only do conversion when not in floating timezone */
    if (tz) {
        utc_zone = icaltimezone_get_utc_timezone();
        icalt->is_utc = (tz == utc_zone) ? 1 : 0;
        icalt->zone = tz;
    }
    return;
}

NS_IMETHODIMP
calDateTime::Compare(calIDateTime *aOther, PRInt32 *aResult)
{
    if (!aOther)
        return NS_ERROR_INVALID_ARG;

    PRBool otherIsDate = PR_FALSE;
    aOther->GetIsDate(&otherIsDate);

    icaltimetype a, b;
    ToIcalTime(&a);
    aOther->ToIcalTime(&b);

    if (mIsDate || otherIsDate) {
        icaltimezone *tz;
        nsresult rv = GetIcalTZ(mTimezone, &tz);
        if (NS_FAILED(rv))
            return calIErrors::INVALID_TIMEZONE;

        *aResult = icaltime_compare_date_only(a, b, tz);
    } else {
        *aResult = icaltime_compare(a, b);
    }

    return NS_OK;
}

nsresult
calDateTime::GetIcalTZ(const nsACString& tzid, struct _icaltimezone **tzp)
{
    if (tzid.IsEmpty()) {
        return NS_ERROR_INVALID_ARG;
    }

    if (tzid.EqualsLiteral("floating")) {
        *tzp = nsnull;
        return NS_OK;
    }

    if (tzid.EqualsLiteral("UTC") || tzid.EqualsLiteral("utc")) {
        *tzp = icaltimezone_get_utc_timezone();
        return NS_OK;
    }

    nsCOMPtr<calIICSService> ics = do_GetService(kCalICSService);
    nsCOMPtr<calIIcalComponent> tz;

    nsresult rv = ics->GetTimezone(tzid, getter_AddRefs(tz));
    if (NS_FAILED(rv) || !tz) {
        // No timezone was found. To prevent the app from dying,
        // pretent that there is no timezone, ie return floating.
        // See bug 335879
        *tzp = nsnull;
        return NS_OK;
        // We should return an error, but don't:
        //return NS_ERROR_INVALID_ARG;
    }

    if (tzp) {
        icalcomponent *zonecomp = tz->GetIcalComponent();
        *tzp = icalcomponent_get_timezone(zonecomp, nsPromiseFlatCString(tzid).get());
    }

    return NS_OK;
}

/*
 * nsIXPCScriptable impl
 */

/* readonly attribute string className; */
NS_IMETHODIMP
calDateTime::GetClassName(char * *aClassName)
{
    NS_ENSURE_ARG_POINTER(aClassName);
    *aClassName = (char *) nsMemory::Clone("calDateTime", 12);
    if (!*aClassName)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

/* readonly attribute PRUint32 scriptableFlags; */
NS_IMETHODIMP
calDateTime::GetScriptableFlags(PRUint32 *aScriptableFlags)
{
    *aScriptableFlags =
        nsIXPCScriptable::WANT_GETPROPERTY |
        nsIXPCScriptable::WANT_SETPROPERTY |
        nsIXPCScriptable::WANT_NEWRESOLVE |
        nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE;
    return NS_OK;
}

/* PRBool getProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
calDateTime::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    if (JSVAL_IS_STRING(id)) {
        nsDependentString jsid((PRUnichar *)::JS_GetStringChars(JSVAL_TO_STRING(id)),
                               ::JS_GetStringLength(JSVAL_TO_STRING(id)));
        if (jsid.EqualsLiteral("jsDate")) {
            PRTime tmp, thousand;
            jsdouble msec;
            LL_I2L(thousand, 1000);
            LL_DIV(tmp, mNativeTime, thousand);
            LL_L2D(msec, tmp);

            JSObject *obj;
            if (mTimezone.EqualsLiteral("floating"))
                obj = ::js_NewDateObject(cx, mYear, mMonth, mDay, mHour, mMinute, mSecond);
            else
                obj = ::js_NewDateObjectMsec(cx, msec);

            *vp = OBJECT_TO_JSVAL(obj);
            *_retval = PR_TRUE;
            return NS_SUCCESS_I_DID_SOMETHING;
        }
    }

    *_retval = PR_TRUE;
    return NS_OK;
}


/* PRBool setProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
calDateTime::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    if (JSVAL_IS_STRING(id)) {
        nsDependentString jsid((PRUnichar *)::JS_GetStringChars(JSVAL_TO_STRING(id)),
                               ::JS_GetStringLength(JSVAL_TO_STRING(id)));
        if (jsid.EqualsLiteral("jsDate") && vp) {
            JSObject *dobj;
            if (!JSVAL_IS_OBJECT(*vp) ||
                !js_DateIsValid(cx, (dobj = JSVAL_TO_OBJECT(*vp)))) {
                mIsValid = PR_FALSE;
            } else {
                jsdouble utcMsec = js_DateGetMsecSinceEpoch(cx, dobj);
                PRTime utcTime, thousands;
                LL_F2L(utcTime, utcMsec);
                LL_I2L(thousands, 1000);
                LL_MUL(utcTime, utcTime, thousands);

                nsresult rv = SetNativeTime(utcTime);
                if (NS_SUCCEEDED(rv)) {
                    mIsValid = PR_TRUE;
                } else {
                    mIsValid = PR_FALSE;
                }
            }

            *_retval = PR_TRUE;
            return NS_SUCCESS_I_DID_SOMETHING;
        }
    }
    *_retval = PR_TRUE;
    return NS_OK;
}

/* void preCreate (in nsISupports nativeObj, in JSContextPtr cx, in JSObjectPtr globalObj, out JSObjectPtr parentObj); */
NS_IMETHODIMP
calDateTime::PreCreate(nsISupports *nativeObj, JSContext * cx,
                       JSObject * globalObj, JSObject * *parentObj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void create (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
calDateTime::Create(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void postCreate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
calDateTime::PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool addProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
calDateTime::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool delProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
calDateTime::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool enumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
calDateTime::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                       JSObject * obj, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool newEnumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 enum_op, in JSValPtr statep, out JSID idp); */
NS_IMETHODIMP
calDateTime::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                          JSObject * obj, PRUint32 enum_op, jsval * statep, jsid *idp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
calDateTime::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                        JSObject * obj, jsval id, PRUint32 flags,
                        JSObject * *objp, PRBool *_retval)
{
    if (JSVAL_IS_STRING(id)) {
        JSString *str = JSVAL_TO_STRING(id);
        nsDependentString name((PRUnichar *)::JS_GetStringChars(str),
                               ::JS_GetStringLength(str));
        if (name.EqualsLiteral("jsDate")) {
            *_retval = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                             ::JS_GetStringLength(str),
                                             JSVAL_VOID,
                                             nsnull, nsnull, 0);
            *objp = obj;
            return *_retval ? NS_OK : NS_ERROR_FAILURE;
        }
    }

    *_retval = PR_TRUE;
    return NS_OK;
}

/* PRBool convert (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 type, in JSValPtr vp); */
NS_IMETHODIMP
calDateTime::Convert(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                     JSObject * obj, PRUint32 type, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void finalize (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
calDateTime::Finalize(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                      JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool checkAccess (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 mode, in JSValPtr vp); */
NS_IMETHODIMP
calDateTime::CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval id, PRUint32 mode, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool call (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
calDateTime::Call(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                  JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
calDateTime::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                       JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val, out PRBool bp); */
NS_IMETHODIMP
calDateTime::HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval val, PRBool *bp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRUint32 mark (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in voidPtr arg); */
NS_IMETHODIMP
calDateTime::Mark(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                  JSObject * obj, void * arg, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool equality(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val); */
NS_IMETHODIMP
calDateTime::Equality(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *obj, jsval val, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* JSObjectPtr outerObject(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
calDateTime::OuterObject(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, JSObject **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* JSObjectPtr innerObject(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
calDateTime::InnerObject(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, JSObject **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/**
 * We are subclassing nsCString for mTimezone so we can check the tzid of all
 * calIDateTimes as they go by.
 */
void calTzId::Assign(char* c)
{
  this->Assign(nsDependentCString(c));
}

void calTzId::Assign(const nsACString_internal& aStr)
{
    nsCString _retVal;
    nsCOMPtr<calIICSService> icsSvc = do_GetService(kCalICSService);
    icsSvc->LatestTzId(aStr, _retVal);

    if (_retVal.Length() != 0) {
        nsCString::Assign(_retVal);
    } else {
        nsCString::Assign(aStr);
    }
}
