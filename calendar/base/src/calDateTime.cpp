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

#include "nsPrintfCString.h"

#include "calDateTime.h"
#include "calAttributeHelpers.h"

#include "jsdate.h"


extern "C" {
    #include "ical.h"
}

NS_IMPL_ISUPPORTS2(calDateTime, calIDateTime, nsIXPCScriptable)

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
    FromIcalTime(atimeptr);
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

    ToIcalTime(&icalt);

    icalt = icaltime_normalize(icalt);

    FromIcalTime(&icalt);
    
    mValid = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::ToString(nsACString& aResult)
{
    aResult.Assign(nsPrintfCString(100,
                                   "%04d/%02d/%02d %02d:%02d:%02d%s%s",
                                   mYear, mMonth, mDay,
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

    PRInt64 temp, million;
    LL_I2L(million, PR_USEC_PER_SEC);
    LL_DIV(temp, aTime, million);
    PRInt32 sectime;
    LL_L2I(sectime, temp);

    tt = sectime;
    icalt = icaltime_from_timet(tt, 0);
    icalt = icaltime_as_utc(icalt, aTimezone);

    FromIcalTime(&icalt);

    return NS_OK;
}

NS_IMETHODIMP
calDateTime::GetInTimezone(const char *aTimezone, calIDateTime **aResult)
{
    struct icaltimetype icalt;
    ToIcalTime(&icalt);

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

NS_IMETHODIMP_(void)
calDateTime::ToIcalTime(icaltimetype *icalt)
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
calDateTime::FromIcalTime(icaltimetype *icalt)
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

/*
 * nsIXPCScriptable ipl
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
            JSObject *obj = ::js_NewDateObject(cx, mYear, mMonth, mDay,
                                               mHour, mMinute, mSecond);
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

                mIsUtc = PR_TRUE;
                mTimezone.AssignLiteral("");

                nsresult rv = SetNativeTime(utcTime);
                if (NS_FAILED(rv)) {
                    mValid = PR_FALSE;
                } else {
                    mValid = PR_TRUE;
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
