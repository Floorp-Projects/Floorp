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
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
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

#include "calDuration.h"
#include "calBaseCID.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#define SECONDS_PER_WEEK   604800
#define SECONDS_PER_DAY     86400
#define SECONDS_PER_HOUR     3600
#define SECONDS_PER_MINUTE     60

NS_IMPL_ISUPPORTS1_CI(calDuration, calIDuration)

calDuration::calDuration()
    : mImmutable(PR_FALSE)
{
    Reset();
}

calDuration::calDuration(const calDuration& cdt)
{
    mDuration.is_neg = cdt.mDuration.is_neg;
    mDuration.weeks = cdt.mDuration.weeks;
    mDuration.days = cdt.mDuration.days;
    mDuration.hours = cdt.mDuration.hours;
    mDuration.minutes = cdt.mDuration.minutes;
    mDuration.seconds = cdt.mDuration.seconds;

    // copies are always mutable
    mImmutable = PR_FALSE;
}

NS_IMETHODIMP
calDuration::GetIsMutable(PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = !mImmutable;
    return NS_OK;
}

NS_IMETHODIMP
calDuration::MakeImmutable()
{
    if (mImmutable)
        return NS_ERROR_OBJECT_IS_IMMUTABLE;

    mImmutable = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
calDuration::Clone(calIDuration **aResult)
{
    calDuration *cdt = new calDuration(*this);
    if (!cdt)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calDuration::Reset()
{
    if (mImmutable)
        return NS_ERROR_FAILURE;

    mDuration.is_neg = 0;
    mDuration.weeks = 0;
    mDuration.days = 0;
    mDuration.hours = 0;
    mDuration.minutes = 0;
    mDuration.seconds = 0;

    return NS_OK;
}

NS_IMETHODIMP calDuration::GetIsNegative(PRBool *_retval)
{
    *_retval = mDuration.is_neg;
    return NS_OK;
}
NS_IMETHODIMP calDuration::SetIsNegative(PRBool aValue)
{
    if (mImmutable) return NS_ERROR_CALENDAR_IMMUTABLE;
    mDuration.is_neg = aValue;
    return NS_OK;
}

NS_IMETHODIMP calDuration::GetWeeks(PRInt16 *_retval)
{
    *_retval = (PRInt16)mDuration.weeks;
    return NS_OK;
}
NS_IMETHODIMP calDuration::SetWeeks(PRInt16 aValue)
{
    if (mImmutable) return NS_ERROR_CALENDAR_IMMUTABLE;
    mDuration.weeks = aValue;
    return NS_OK;
}

NS_IMETHODIMP calDuration::GetDays(PRInt16 *_retval)
{
    *_retval = (PRInt16)mDuration.days;
    return NS_OK;
}
NS_IMETHODIMP calDuration::SetDays(PRInt16 aValue)
{
    if (mImmutable) return NS_ERROR_CALENDAR_IMMUTABLE;
    mDuration.days = aValue;
    return NS_OK;
}

NS_IMETHODIMP calDuration::GetHours(PRInt16 *_retval)
{
    *_retval = (PRInt16)mDuration.hours;
    return NS_OK;
}
NS_IMETHODIMP calDuration::SetHours(PRInt16 aValue)
{
    if (mImmutable) return NS_ERROR_CALENDAR_IMMUTABLE;
    mDuration.hours = aValue;
    return NS_OK;
}

NS_IMETHODIMP calDuration::GetMinutes(PRInt16 *_retval)
{
    *_retval = (PRInt16)mDuration.minutes;
    return NS_OK;
}
NS_IMETHODIMP calDuration::SetMinutes(PRInt16 aValue)
{
    if (mImmutable) return NS_ERROR_CALENDAR_IMMUTABLE;
    mDuration.minutes = aValue;
    return NS_OK;
}

NS_IMETHODIMP calDuration::GetSeconds(PRInt16 *_retval)
{
    *_retval = (PRInt16)mDuration.seconds;
    return NS_OK;
}
NS_IMETHODIMP calDuration::SetSeconds(PRInt16 aValue)
{
    if (mImmutable) return NS_ERROR_CALENDAR_IMMUTABLE;
    mDuration.seconds = aValue;
    return NS_OK;
}


NS_IMETHODIMP calDuration::GetInSeconds(PRInt32 *_retval)
{
    *_retval = 
        (((PRInt32)((PRInt16)mDuration.weeks   * SECONDS_PER_WEEK)) + 
         ((PRInt32)((PRInt16)mDuration.days    * SECONDS_PER_DAY)) +
         ((PRInt32)((PRInt16)mDuration.hours   * SECONDS_PER_HOUR)) +
         ((PRInt32)((PRInt16)mDuration.minutes * SECONDS_PER_MINUTE)) +
         ((PRInt32)((PRInt16)mDuration.seconds)));

    return NS_OK;
}
NS_IMETHODIMP calDuration::SetInSeconds(PRInt32 aValue)
{
    if (mImmutable) return NS_ERROR_CALENDAR_IMMUTABLE;

    mDuration.is_neg = (aValue < 0);
    if (mDuration.is_neg)
        aValue = -aValue;

    mDuration.weeks = aValue / SECONDS_PER_WEEK;
    aValue -= (mDuration.weeks * SECONDS_PER_WEEK);

    mDuration.days = aValue / SECONDS_PER_DAY;
    aValue -= (mDuration.days * SECONDS_PER_DAY);

    mDuration.hours = aValue / SECONDS_PER_HOUR;
    aValue -= (mDuration.hours * SECONDS_PER_HOUR);

    mDuration.minutes = aValue / SECONDS_PER_MINUTE;
    aValue -= (mDuration.minutes * SECONDS_PER_MINUTE);

    mDuration.seconds = aValue;

    return NS_OK;
}

NS_IMETHODIMP calDuration::AddDuration(calIDuration *aDuration)
{
    if (mImmutable)
        return NS_ERROR_CALENDAR_IMMUTABLE;

    struct icaldurationtype idt;
    aDuration->ToIcalDuration(&idt);

    if (!idt.is_neg) {
        mDuration.weeks   += idt.weeks;
        mDuration.days    += idt.days;
        mDuration.hours   += idt.hours;
        mDuration.minutes += idt.minutes;
        mDuration.seconds += idt.seconds;
    } else {
        mDuration.weeks   -= idt.weeks;
        mDuration.days    -= idt.days;
        mDuration.hours   -= idt.hours;
        mDuration.minutes -= idt.minutes;
        mDuration.seconds -= idt.seconds;
    }

    Normalize();

    return NS_OK;
}

NS_IMETHODIMP
calDuration::Normalize()
{
    if (mImmutable)
        return NS_ERROR_CALENDAR_IMMUTABLE;

    PRInt32 totalInSeconds;
    GetInSeconds(&totalInSeconds);
    SetInSeconds(totalInSeconds);

    return NS_OK;
}

NS_IMETHODIMP
calDuration::ToString(nsACString& aResult)
{
    return GetIcalString(aResult);
}

NS_IMETHODIMP_(void)
calDuration::ToIcalDuration(struct icaldurationtype *icald)
{
    icald->is_neg  = mDuration.is_neg;
    icald->weeks   = mDuration.weeks;
    icald->days    = mDuration.days;
    icald->hours   = mDuration.hours;
    icald->minutes = mDuration.minutes;
    icald->seconds = mDuration.seconds;
}

NS_IMETHODIMP
calDuration::GetIcalString(nsACString& aResult)
{
    // note that ics is owned by libical, so we don't need to free
    const char *ics = icaldurationtype_as_ical_string(mDuration);
    
    if (ics) {
        aResult.Assign(ics);
        return NS_OK;
    }

    return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
calDuration::SetIcalString(const nsACString& aIcalString)
{
    mDuration = icaldurationtype_from_string(nsPromiseFlatCString(aIcalString).get());
    return NS_OK;
}
