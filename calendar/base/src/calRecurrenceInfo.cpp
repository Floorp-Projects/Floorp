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

#include "calRecurrenceInfo.h"
#include "calDateTime.h"
#include "calIItemBase.h"

#include "nsCOMArray.h"

extern "C" {
    #include "ical.h"
}


NS_IMPL_ISUPPORTS1(calRecurrenceInfo, calIRecurrenceInfo)

calRecurrenceInfo::calRecurrenceInfo()
    : mImmutable(PR_FALSE)
{
    mIcalRecur = new struct icalrecurrencetype;

    icalrecurrencetype_clear(mIcalRecur);
}

calRecurrenceInfo::~calRecurrenceInfo()
{
    if (mIcalRecur)
        delete mIcalRecur;
}

NS_IMETHODIMP
calRecurrenceInfo::GetIsMutable(PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = !mImmutable;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::MakeImmutable()
{
    if (mImmutable)
        return NS_ERROR_FAILURE; // XXX another error code

    mImmutable = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::Clone(calIRecurrenceInfo **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long recurType; */
NS_IMETHODIMP
calRecurrenceInfo::GetRecurType(PRInt32 *aRecurType)
{
    NS_ENSURE_ARG_POINTER(aRecurType);

    switch (mIcalRecur->freq) {
#define RECUR_HELPER(x) \
        case ICAL_##x##_RECURRENCE: *aRecurType = CAL_RECUR_##x; break
        RECUR_HELPER(SECONDLY);
        RECUR_HELPER(MINUTELY);
        RECUR_HELPER(HOURLY);
        RECUR_HELPER(DAILY);
        RECUR_HELPER(WEEKLY);
        RECUR_HELPER(MONTHLY);
        RECUR_HELPER(YEARLY);
#undef RECUR_HELPER
        default:
            *aRecurType = CAL_RECUR_INVALID;
    }

    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::SetRecurType(PRInt32 aRecurType)
{
    switch (aRecurType) {
#define RECUR_HELPER(x) \
        case CAL_RECUR_##x: mIcalRecur->freq = ICAL_##x##_RECURRENCE; break
        RECUR_HELPER(SECONDLY);
        RECUR_HELPER(MINUTELY);
        RECUR_HELPER(HOURLY);
        RECUR_HELPER(DAILY);
        RECUR_HELPER(WEEKLY);
        RECUR_HELPER(MONTHLY);
        RECUR_HELPER(YEARLY);
#undef RECUR_HELPER
        case CAL_RECUR_INVALID:
            mIcalRecur->freq = ICAL_NO_RECURRENCE;
            break;
        default:
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/* attribute long recurCount; */
NS_IMETHODIMP
calRecurrenceInfo::GetRecurCount(PRInt32 *aRecurCount)
{
    NS_ENSURE_ARG_POINTER(aRecurCount);
    *aRecurCount = mIcalRecur->count;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::SetRecurCount(PRInt32 aRecurCount)
{
    mIcalRecur->count = aRecurCount;
    mIcalRecur->until = icaltime_null_time();

    return NS_OK;
}

/* attribute calIDateTime recurEnd; */
NS_IMETHODIMP
calRecurrenceInfo::GetRecurEnd(calIDateTime * *aRecurEnd)
{
    NS_ENSURE_ARG_POINTER(aRecurEnd);

    calDateTime *cdt = new calDateTime(&mIcalRecur->until);
    if (!cdt)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF (*aRecurEnd = cdt);
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::SetRecurEnd(calIDateTime * aRecurEnd)
{
    struct icaltimetype itt;
    aRecurEnd->ToIcalTime(&itt);

    mIcalRecur->until = itt;
    mIcalRecur->count = 0;

    return NS_OK;
}

/* attribute long interval; */
NS_IMETHODIMP
calRecurrenceInfo::GetInterval(PRInt32 *aInterval)
{
    NS_ENSURE_ARG_POINTER(aInterval);
    *aInterval = mIcalRecur->interval;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::SetInterval(PRInt32 aInterval)
{
    mIcalRecur->interval = aInterval;
    return NS_OK;
}

/* void getComponent (in long aComponentType, out unsigned long aCount, [array, size_is (aCount), retval] out long aValues); */
NS_IMETHODIMP
calRecurrenceInfo::GetComponent(PRInt32 aComponentType, PRUint32 *aCount, PRInt16 **aValues)
{
    NS_ENSURE_ARG_POINTER(aCount);
    NS_ENSURE_ARG_POINTER(aValues);

    // This little ugly macro counts the number of real entries
    // we have in the relevant array, and then clones it to the result.
#define HANDLE_COMPONENT(_comptype,_icalvar,_icalmax)                   \
    if (aComponentType == _comptype) {                                  \
        int count;                                                      \
        for (count = 0; count < _icalmax; count++) {                    \
            if (mIcalRecur->_icalvar[count] == ICAL_RECURRENCE_ARRAY_MAX) \
                break;                                                  \
        }                                                               \
        if (count) {                                                    \
            *aValues = (PRInt16*) nsMemory::Clone(mIcalRecur->_icalvar, \
                                                  count * sizeof(PRInt16)); \
            if (!*aValues) return NS_ERROR_OUT_OF_MEMORY;               \
        } else {                                                        \
            *aValues = nsnull;                                          \
        }                                                               \
        *aCount = count;                                                \
    }

    HANDLE_COMPONENT(CAL_RECUR_BYSECOND, by_second, ICAL_BY_SECOND_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYMINUTE, by_minute, ICAL_BY_MINUTE_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYHOUR, by_hour, ICAL_BY_HOUR_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYDAY, by_day, ICAL_BY_DAY_SIZE) // special
    else HANDLE_COMPONENT(CAL_RECUR_BYMONTHDAY, by_month_day, ICAL_BY_MONTHDAY_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYYEARDAY, by_year_day, ICAL_BY_YEARDAY_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYWEEKNO, by_week_no, ICAL_BY_WEEKNO_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYMONTH, by_month, ICAL_BY_MONTH_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYSETPOS, by_set_pos, ICAL_BY_SETPOS_SIZE)
    else {
        // invalid component; XXX - error code
        return NS_ERROR_FAILURE;
    }
#undef HANDLE_COMPONENT

    return NS_OK;
}

/* void setComponent (in long aComponentType, in unsigned long aCount, [array, size_is (aCount)] in long aValues); */
NS_IMETHODIMP
calRecurrenceInfo::SetComponent(PRInt32 aComponentType, PRUint32 aCount, PRInt16 *aValues)
{
    NS_ENSURE_ARG_POINTER(aValues);

    // Copy the passed-in array into the ical structure array
#define HANDLE_COMPONENT(_comptype,_icalvar,_icalmax)                   \
    if (aComponentType == _comptype) {                                  \
        if (aCount > _icalmax)                                          \
            return NS_ERROR_FAILURE;                                    \
        memcpy(mIcalRecur->_icalvar, aValues, aCount * sizeof(PRInt16)); \
        if (aCount < _icalmax)                                          \
            mIcalRecur->_icalvar[aCount] = ICAL_RECURRENCE_ARRAY_MAX;    \
    }

    HANDLE_COMPONENT(CAL_RECUR_BYSECOND, by_second, ICAL_BY_SECOND_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYMINUTE, by_minute, ICAL_BY_MINUTE_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYHOUR, by_hour, ICAL_BY_HOUR_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYDAY, by_day, ICAL_BY_DAY_SIZE) // special
    else HANDLE_COMPONENT(CAL_RECUR_BYMONTHDAY, by_month_day, ICAL_BY_MONTHDAY_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYYEARDAY, by_year_day, ICAL_BY_YEARDAY_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYWEEKNO, by_week_no, ICAL_BY_WEEKNO_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYMONTH, by_month, ICAL_BY_MONTH_SIZE)
    else HANDLE_COMPONENT(CAL_RECUR_BYSETPOS, by_set_pos, ICAL_BY_SETPOS_SIZE)
    else {
        // invalid component; XXX - error code
        return NS_ERROR_FAILURE;
    }
#undef HANDLE_COMPONENT

    return NS_OK;
}

/* void getExceptions (out unsigned long aCount, [array, size_is (aCount), retval] out calIDateTime aDates); */
NS_IMETHODIMP
calRecurrenceInfo::GetExceptions(PRUint32 *aCount, calIDateTime ***aDates)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setExceptions (in unsigned long aCount, [array, size_is (aCount)] in calIDateTime aDates); */
NS_IMETHODIMP
calRecurrenceInfo::SetExceptions(PRUint32 aCount, calIDateTime **aDates)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* calIItemOccurrence getNextOccurrence (in calIItemBase aItem, in calIDateTime aStartTime); */
NS_IMETHODIMP
calRecurrenceInfo::GetNextOccurrence(calIItemBase *aItem, calIDateTime *aStartTime, calIItemOccurrence **_retval)
{
    NS_ENSURE_ARG_POINTER(aStartTime);
    NS_ENSURE_ARG_POINTER(_retval);

    nsresult rv;

    struct icaltimetype dtstart;
    aStartTime->ToIcalTime(&dtstart);

    icalrecur_iterator* recur_iter;
    recur_iter = icalrecur_iterator_new (*mIcalRecur,
                                         dtstart);
    if (!recur_iter)
        return NS_ERROR_OUT_OF_MEMORY;

    struct icaltimetype next = icalrecur_iterator_next(recur_iter);
    if (!icaltime_is_null_time(next)) {
        calDateTime *cdt = new calDateTime(&next);
        if (!cdt) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
            nsCOMPtr<calIItemOccurrence> item = do_CreateInstance("@mozilla.org/calendar/item-occurrence;1");
            if (!item) {
                rv = NS_ERROR_FAILURE;
            } else {
                rv = item->Initialize(aItem, cdt, cdt); // XXX Fixme duration!
                if (NS_SUCCEEDED(rv))
                    NS_ADDREF (*_retval = item);
            }
        }
    } else {
        *_retval = nsnull;
        rv = NS_OK;
    }

    icalrecur_iterator_free(recur_iter);

    return rv;
}

/* void getOccurrencesBetween (in calIItemBase aItem, in calIDateTime aStartTime, in calIDateTime aEndTime, out unsigned long aCount, [array, size_is (aCount), retval] out calIItemOccurrence aItems); */
NS_IMETHODIMP
calRecurrenceInfo::GetOccurrencesBetween(calIItemBase *aItem,
                                         calIDateTime *aStartTime,
                                         calIDateTime *aEndTime,
                                         PRUint32 *aCount, calIItemOccurrence ***aItems)
{
    NS_ENSURE_ARG_POINTER(aStartTime);
    NS_ENSURE_ARG_POINTER(aCount);
    NS_ENSURE_ARG_POINTER(aItems);

    // make sure there's sanity
    //if (!aEndTime && 

    nsCOMArray<calIItemOccurrence> items;

    struct icaltimetype dtstart, dtend;
    aStartTime->ToIcalTime(&dtstart);
    if (aEndTime)
        aEndTime->ToIcalTime(&dtend);

    icalrecur_iterator* recur_iter;
    recur_iter = icalrecur_iterator_new (*mIcalRecur,
                                         dtstart);
    if (!recur_iter)
        return NS_ERROR_OUT_OF_MEMORY;

    int count = 0;

    struct icaltimetype next = icalrecur_iterator_next(recur_iter);
    while (!icaltime_is_null_time(next)) {
        if (*aCount && *aCount < count)
            break;

        if (aEndTime && icaltime_compare(next, dtend) > 0)
            break;

        nsresult rv = NS_OK;
        calDateTime *cdt = new calDateTime(&next);
        if (!cdt) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
            nsCOMPtr<calIItemOccurrence> item = do_CreateInstance("@mozilla.org/calendar/item-occurrence;1");
            if (!item) {
                rv = NS_ERROR_FAILURE;
            } else {
                rv = item->Initialize(aItem, cdt, cdt); // XXX Fixme duration!
                if (NS_SUCCEEDED(rv)) {
                    items.AppendObject(item);
                }
            }
        }

        if (!NS_SUCCEEDED(rv)) {
            icalrecur_iterator_free(recur_iter);
            return rv;
        }

        next = icalrecur_iterator_next(recur_iter);
    }

    icalrecur_iterator_free(recur_iter);

    *aCount = items.Count();
    if (*aCount) {
        calIItemOccurrence **itemArray = (calIItemOccurrence **) nsMemory::Alloc(sizeof(calIItemOccurrence*) * items.Count());
        for (int i = 0; i < items.Count(); i++) {
            itemArray[i] = items[i];
            NS_ADDREF(itemArray[i]);
        }
        *aItems = itemArray;
    } else {
        *aItems = nsnull;
    }

    return NS_OK;
}

/* void getOccurrences (in calIItemBase aItem, in calIDateTime aStartTime, in long aMaxCount, out unsigned long aCount, [array,size_is(aCount),retval] out calIItemOccurrence aItems); */
NS_IMETHODIMP
calRecurrenceInfo::GetOccurrences(calIItemBase *aItem, calIDateTime *aStartTime, PRUint32 aMaxCount,
                                  PRUint32 *aCount, calIItemOccurrence ***aItems)
{
    return NS_OK;
}
