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

#include "calRecurrenceDate.h"

#include "calDateTime.h"
#include "calIItemBase.h"
#include "calIEvent.h"
#include "calIErrors.h"
#include "nsServiceManagerUtils.h"

#include "calICSService.h"
#include "nsIClassInfoImpl.h"
#include "calBaseCID.h"

static NS_DEFINE_CID(kCalICSService, CAL_ICSSERVICE_CID);

extern "C" {
    #include "ical.h"
}

NS_IMPL_ISUPPORTS2_CI(calRecurrenceDate, calIRecurrenceItem, calIRecurrenceDate)

calRecurrenceDate::calRecurrenceDate()
    : mImmutable(PR_FALSE),
      mIsNegative(PR_FALSE)
{
}

NS_IMETHODIMP
calRecurrenceDate::GetIsMutable(PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = !mImmutable;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceDate::MakeImmutable()
{
    if (mImmutable)
        return NS_ERROR_FAILURE; // XXX another error code

    mImmutable = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceDate::Clone(calIRecurrenceItem **_retval)
{
    calRecurrenceDate *crd = new calRecurrenceDate;
    if (!crd)
        return NS_ERROR_OUT_OF_MEMORY;

    crd->mIsNegative = mIsNegative;
    if (mDate)
        mDate->Clone(getter_AddRefs(crd->mDate));
    else
        crd->mDate = nsnull;

    NS_ADDREF(*_retval = crd);
    return NS_OK;
}

/* attribute boolean isNegative; */
NS_IMETHODIMP
calRecurrenceDate::GetIsNegative(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = mIsNegative;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceDate::SetIsNegative(PRBool aIsNegative)
{
    if (mImmutable)
        return NS_ERROR_FAILURE; // XXX CAL_ERROR_ITEM_IS_IMMUTABLE

    mIsNegative = aIsNegative;
    return NS_OK;
}

/* readonly attribute boolean isFinite; */
NS_IMETHODIMP
calRecurrenceDate::GetIsFinite(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceDate::GetDate(calIDateTime **aDate)
{
    NS_ENSURE_ARG_POINTER(aDate);

    NS_IF_ADDREF(*aDate = mDate);
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceDate::SetDate(calIDateTime *aDate)
{
    NS_ENSURE_ARG_POINTER(aDate);

    mDate = aDate;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceDate::GetNextOccurrence(calIDateTime *aStartTime,
                                     calIDateTime *aOccurrenceTime,
                                     calIDateTime **_retval)
{
    NS_ENSURE_ARG_POINTER(aStartTime);
    NS_ENSURE_ARG_POINTER(aOccurrenceTime);
    NS_ENSURE_ARG_POINTER(_retval);

    if (mDate) {
        PRInt32 result;
        if (NS_SUCCEEDED(mDate->Compare(aStartTime, &result)) && result > 0) {
            NS_ADDREF (*_retval = mDate);
            return NS_OK;
        }
    }

    *_retval = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceDate::GetOccurrences(calIDateTime *aStartTime,
                                  calIDateTime *aRangeStart,
                                  calIDateTime *aRangeEnd,
                                  PRUint32 aMaxCount,
                                  PRUint32 *aCount, calIDateTime ***aDates)
{
    NS_ENSURE_ARG_POINTER(aStartTime);
    NS_ENSURE_ARG_POINTER(aRangeStart);
    NS_ENSURE_ARG_POINTER(aRangeEnd);

    PRInt32 r1, r2;

    if (mDate) {
        if (NS_SUCCEEDED(mDate->Compare(aRangeStart, &r1)) && r1 >= 0 &&
            NS_SUCCEEDED(mDate->Compare(aRangeEnd, &r2)) && r2 < 0)
        {
            calIDateTime **dates = (calIDateTime **) nsMemory::Alloc(sizeof(calIDateTime*));
            NS_ADDREF (dates[0] = mDate);
            *aDates = dates;
            *aCount = 1;
            return NS_OK;
        }
    }

    *aDates = nsnull;
    *aCount = 0;
    return NS_OK;
}

/**
 ** ical property getting/setting
 **/
NS_IMETHODIMP
calRecurrenceDate::GetIcalProperty(calIIcalProperty **aProp)
{
    if (!mDate)
        return NS_ERROR_FAILURE;

    // we need to set the timezone of the above created property manually,
    // the only reason this is necessary is because the icalproperty_set_value()
    // has the somewhat non-intuitive behavior of not handling the TZID
    // parameter automagically. 
    nsresult rv;
    nsCAutoString tzid;
    rv = mDate->GetTimezone(tzid);
    if(NS_FAILED(rv))
      return rv;

    PRBool setTZID = PR_FALSE;
    if (!tzid.IsEmpty() && !tzid.EqualsLiteral("UTC") &&
        !tzid.EqualsLiteral("floating")) {

        nsCOMPtr<calIICSService> ics = do_GetService(kCalICSService, &rv);
        if(NS_FAILED(rv))
          return NS_ERROR_NOT_AVAILABLE;

        nsCOMPtr<calIIcalComponent> tz;
        rv = ics->GetTimezone(tzid, getter_AddRefs(tz));
        if(NS_FAILED(rv))
          return rv;

        setTZID = PR_TRUE;
    }

    icalproperty *dateprop = nsnull;

    if (mIsNegative)
        dateprop = icalproperty_new(ICAL_EXDATE_PROPERTY);
    else
        dateprop = icalproperty_new(ICAL_RDATE_PROPERTY);

    struct icaltimetype icalt;
    mDate->ToIcalTime(&icalt);

    icalvalue *v;

    if (icalt.is_date)
        v = icalvalue_new_date(icalt);
    else
        v = icalvalue_new_datetime(icalt);

    icalproperty_set_value(dateprop, v);

    calIIcalProperty *icp = new calIcalProperty(dateprop, nsnull);
    if (!icp) {
        icalproperty_free(dateprop);
        return NS_ERROR_FAILURE;
    }

    if(setTZID)
      icalproperty_set_parameter_from_string(dateprop, "TZID", nsPromiseFlatCString(tzid).get());

    NS_ADDREF(*aProp = icp);
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceDate::SetIcalProperty(calIIcalProperty *aProp)
{
    NS_ENSURE_ARG_POINTER(aProp);

    icalproperty *prop = aProp->GetIcalProperty();
    if (!prop)
        return NS_ERROR_INVALID_ARG;

    int kind = icalproperty_isa(prop);

    if (kind == ICAL_RDATE_PROPERTY) {
        mIsNegative = PR_FALSE;
    } else if (kind == ICAL_EXDATE_PROPERTY) {
        mIsNegative = PR_TRUE;
    } else {
        return NS_ERROR_INVALID_ARG;
    }

    struct icaltimetype theDate = icalvalue_get_date(icalproperty_get_value(prop));

    // we need to transfer the timezone from the icalproperty to the calIDateTime
    // object manually, the only reason this is necessary is because the
    // icalproperty_get_value() has the somewhat non-intuitive behavior of
    // not handling the TZID parameter automagically. 
    const char *tzid = icalproperty_get_parameter_as_string(prop, "TZID");
    if (tzid) {
        // We have to walk up to our parent VCALENDAR and try to find this tzid
      icalcomponent *vcalendar = aProp->GetIcalComponent();
      while (vcalendar && icalcomponent_isa(vcalendar) != ICAL_VCALENDAR_COMPONENT)
        vcalendar = icalcomponent_get_parent(vcalendar);
      if (!vcalendar) {
        NS_WARNING("VCALENDAR not found while looking for VTIMEZONE!");
        return calIErrors::ICS_ERROR_BASE + icalerrno;
      }
      icaltimezone *zone = icalcomponent_get_timezone(vcalendar, tzid);
      if (!zone) {
        NS_WARNING("Can't find specified VTIMEZONE in VCALENDAR!");
        return calIErrors::INVALID_TIMEZONE;
      }
      theDate.zone = zone;
    }

    mDate = new calDateTime(&theDate);
    if (!mDate)
        return NS_ERROR_FAILURE;
    return NS_OK;
}
