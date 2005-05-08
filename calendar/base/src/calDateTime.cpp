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
#include "calIICSService.h"

#include "jsdate.h"

extern "C" {
    #include "ical.h"
}

static NS_DEFINE_CID(kCalICSService, CAL_ICSSERVICE_CID);

NS_IMPL_ISUPPORTS2(calDateTime, calIDateTime, nsIXPCScriptable)

calDateTime::calDateTime()
    : mImmutable(PR_FALSE),
      mValid(PR_FALSE)
{
    Reset();
}

calDateTime::calDateTime(struct icaltimetype *atimeptr)
    : mImmutable(PR_FALSE)
{
    FromIcalTime(atimeptr);
    mValid = PR_TRUE;
}

calDateTime::calDateTime(const calDateTime& cdt)
{
    // bitwise copy everything
    mValid = cdt.mValid;
    mNativeTime = cdt.mNativeTime;
    mYear = cdt.mYear;
    mMonth = cdt.mMonth;
    mDay = cdt.mDay;
    mHour = cdt.mHour;
    mMinute = cdt.mMinute;
    mSecond = cdt.mSecond;
    mIsUtc = cdt.mIsUtc;
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
        return NS_ERROR_CALENDAR_IMMUTABLE;

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
    mIsUtc = PR_FALSE;
    mWeekday = 0;
    mYearday = 0;
    mIsDate = PR_FALSE;

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
CAL_VALUETYPE_ATTR(calDateTime, PRBool, IsDate)

CAL_VALUETYPE_ATTR_GETTER(calDateTime, PRInt16, Weekday)
CAL_VALUETYPE_ATTR_GETTER(calDateTime, PRInt16, Yearday)

CAL_STRINGTYPE_ATTR_GETTER(calDateTime, nsACString, Timezone)

NS_IMETHODIMP
calDateTime::SetTimezone(const nsACString& aTimezone)
{
    mTimezone.Assign(aTimezone);
    if (aTimezone.EqualsLiteral("UTC") ||
        aTimezone.EqualsLiteral("utc"))
    {
        mTimezone.Truncate();
        mIsUtc = PR_TRUE;
    } else if (!aTimezone.IsEmpty()) {
        mIsUtc = PR_FALSE;
    }

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
    return SetTimeInTimezone (aNativeTime, NULL);
}

NS_IMETHODIMP
calDateTime::Normalize()
{
    struct icaltimetype icalt;

    ToIcalTime(&icalt);

    icalt = icaltime_normalize(icalt);

    FromIcalTime(&icalt);
    
    mValid = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::AddDuration(calIDateTime *aDuration)
{
    NS_ENSURE_ARG_POINTER(aDuration);

    PRTime nativeDur;
    nsresult rv = aDuration->GetNativeTime(&nativeDur);
    if (NS_FAILED(rv))
        return rv;

    mLastModified = PR_Now();

    return SetNativeTime(mNativeTime + nativeDur);
}

NS_IMETHODIMP
calDateTime::ToString(nsACString& aResult)
{
    aResult.Assign(nsPrintfCString(100,
                                   "%04d/%02d/%02d %02d:%02d:%02d%s%s",
                                   mYear, mMonth + 1, mDay,
                                   mHour, mMinute, mSecond,
                                   ((mTimezone.IsEmpty()) ? "" : " "),
                                   ((mTimezone.IsEmpty()) ? "" : mTimezone.get())));
    return NS_OK;
}

NS_IMETHODIMP
calDateTime::SetTimeInTimezone(PRTime aTime, const char *aTimezone)
{
    struct icaltimetype icalt;
    time_t tt;

    icaltimezone *zone = icaltimezone_get_utc_timezone();

    if (aTimezone && strcmp(aTimezone, "UTC") != 0 && strcmp(aTimezone, "utc") != 0) {
        nsCOMPtr<calIICSService> ics = do_GetService(kCalICSService);
        nsCOMPtr<calIIcalComponent> tz;
        ics->GetTimezone(nsDependentCString(aTimezone), getter_AddRefs(tz));
        if (!tz)
            return NS_ERROR_FAILURE;

        icalcomponent *zonecomp = tz->GetIcalComponent();
        zone = icalcomponent_get_timezone(zonecomp, aTimezone);
    }

    PRInt64 temp, million;
    LL_I2L(million, PR_USEC_PER_SEC);
    LL_DIV(temp, aTime, million);
    PRInt32 sectime;
    LL_L2I(sectime, temp);

    tt = sectime;
    if (zone) {
        icalt = icaltime_from_timet_with_zone(tt, 0, zone);
    } else {
        icalt = icaltime_from_timet(tt, 0);
    }
    FromIcalTime(&icalt);

    mValid = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetInTimezone(const char *aTimezone, calIDateTime **aResult)
{
    calDateTime *cdt = nsnull;

    if (mIsDate) {
        // if it's a date, we really just want to make a copy of this
        // and set the timezone.
        cdt = new calDateTime(*this);
        if (aTimezone) {
            cdt->mTimezone.Assign(aTimezone);
            cdt->mIsUtc = PR_FALSE;
        } else {
            cdt->mTimezone.Truncate();
            cdt->mIsUtc = PR_TRUE;
        }
    } else {
        struct icaltimetype icalt;
        icaltimezone *destzone;

        ToIcalTime(&icalt);

        if (!aTimezone || strcmp(aTimezone, "UTC") == 0 || strcmp(aTimezone, "utc") == 0) {
            destzone = icaltimezone_get_utc_timezone();
        } else {
            nsCOMPtr<calIICSService> ics = do_GetService(kCalICSService);
            nsCOMPtr<calIIcalComponent> tz;

            ics->GetTimezone(nsDependentCString(aTimezone), getter_AddRefs(tz));
            if (!tz)
                return NS_ERROR_FAILURE;

            icalcomponent *zonecomp = tz->GetIcalComponent();
            destzone = icalcomponent_get_timezone(zonecomp, aTimezone);
        }

        /* If there's a zone, we need to convert; otherwise, we just
         * assign, since this item is floating */
        if (icalt.zone) {
            icaltimezone_convert_time(&icalt, (icaltimezone*) icalt.zone, destzone);
            icalt.zone = destzone;
        } else {
            icalt.zone = destzone;
        }

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
    icalt.hour = 0;
    icalt.minute = 0;
    icalt.second = 0;

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
    icalt.hour = 23;
    icalt.minute = 59;
    icalt.second = 59;

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
    icalt.hour = 0;
    icalt.minute = 0;
    icalt.second = 0;
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
    icalt.hour = 23;
    icalt.minute = 59;
    icalt.second = 59;
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
    icalt.hour = 0;
    icalt.minute = 0;
    icalt.second = 0;
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
    icalt.hour = 23;
    icalt.minute = 59;
    icalt.second = 59;
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

    icalt->is_utc = mIsUtc ? 1 : 0;
    icalt->is_date = mIsDate ? 1 : 0;
    icalt->is_daylight = 0;

    if (icalt->is_utc) {
        icalt->zone = icaltimezone_get_utc_timezone();
    } else if (mTimezone.IsEmpty()) {
        icalt->zone = nsnull;
    } else {
        nsCOMPtr<calIICSService> ics = do_GetService(kCalICSService);
        nsCOMPtr<calIIcalComponent> tz;
        ics->GetTimezone(mTimezone, getter_AddRefs(tz));
        if (tz) {
            icalcomponent *zonecomp = tz->GetIcalComponent();
            icalt->zone = icalcomponent_get_timezone(zonecomp, mTimezone.get());
        } else {
            NS_WARNING("Specified timezone not found; generating floating time!");
            icalt->zone = nsnull;
        }
    }
}

void
calDateTime::FromIcalTime(icaltimetype *icalt)
{
    icaltimetype t = *icalt;

    mYear = t.year;
    mMonth = t.month - 1;
    mDay = t.day;
    mHour = t.hour;
    mMinute = t.minute;
    mSecond = t.second;

    mIsUtc = (t.zone == icaltimezone_get_utc_timezone());
    mIsDate = t.is_date ? PR_TRUE : PR_FALSE;

    mTimezone.Assign(icaltimezone_get_tzid((icaltimezone*)t.zone));

    // reconstruct nativetime
    time_t tt = icaltime_as_timet_with_zone(*icalt, icaltimezone_get_utc_timezone());
    PRInt64 temp, million;
    LL_I2L(million, PR_USEC_PER_SEC);
    LL_I2L(temp, tt);
    LL_MUL(temp, temp, million);
    mNativeTime = temp;

    fprintf (stderr, "++ tt: %lld mNativeTime: %lld\n", tt, mNativeTime);
    // reconstruct weekday/yearday
    mWeekday = icaltime_day_of_week(*icalt) - 1;
    mYearday = icaltime_day_of_year(*icalt);
}

NS_IMETHODIMP
calDateTime::Compare(calIDateTime *aOther, PRInt32 *aResult)
{
    PRBool otherIsDate = PR_FALSE;
    aOther->GetIsDate(&otherIsDate);

    icaltimetype a, b;
    ToIcalTime(&a);
    aOther->ToIcalTime(&b);

    if (mIsDate || otherIsDate)
        *aResult = icaltime_compare_date_only(a,b);
    else
        *aResult = icaltime_compare(a, b);

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

            JSObject *obj = ::js_NewDateObjectMsec(cx, msec);

            *vp = OBJECT_TO_JSVAL(obj);
            *_retval = PR_TRUE;
            return NS_OK;
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
                mValid = PR_FALSE;
            } else {
                jsdouble utcMsec = js_DateGetMsecSinceEpoch(cx, dobj);
                PRTime utcTime, thousands;
                LL_F2L(utcTime, utcMsec);
                LL_I2L(thousands, 1000);
                LL_MUL(utcTime, utcTime, thousands);

                nsresult rv = SetNativeTime(utcTime);
                if (NS_SUCCEEDED(rv)) {
                    mIsUtc = PR_TRUE;
                    mTimezone.Truncate();
                    mValid = PR_TRUE;
                } else {
                    mValid = PR_FALSE;
                }
            }

            *_retval = PR_TRUE;
            return NS_OK;
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
