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

#include "nsCOMArray.h"

#include "calRecurrenceRule.h"

#include "calDateTime.h"
#include "calIItemBase.h"
#include "calIEvent.h"

#include "calICSService.h"

extern "C" {
    #include "ical.h"
}

NS_IMPL_ISUPPORTS2_CI(calRecurrenceRule, calIRecurrenceItem, calIRecurrenceRule)

calRecurrenceRule::calRecurrenceRule()
    : mImmutable(PR_FALSE),
      mIsNegative(PR_FALSE),
      mIsByCount(PR_FALSE)
{
    mIcalRecur = new struct icalrecurrencetype;
    icalrecurrencetype_clear(mIcalRecur);
}

calRecurrenceRule::~calRecurrenceRule()
{
    delete mIcalRecur;
}

NS_IMETHODIMP
calRecurrenceRule::GetIsMutable(PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = !mImmutable;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceRule::MakeImmutable()
{
    if (mImmutable)
        return NS_ERROR_FAILURE; // XXX another error code

    mImmutable = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceRule::Clone(calIRecurrenceItem **aResult)
{
    calRecurrenceRule *crc = new calRecurrenceRule;
    if (!crc)
        return NS_ERROR_OUT_OF_MEMORY;

    crc->mIsNegative = mIsNegative;
    crc->mIsByCount = mIsByCount;
    *(crc->mIcalRecur) = *mIcalRecur;

    NS_ADDREF(*aResult = crc);
    return NS_OK;
}

/* attribute boolean isNegative; */
NS_IMETHODIMP
calRecurrenceRule::GetIsNegative(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = mIsNegative;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceRule::SetIsNegative(PRBool aIsNegative)
{
    if (mImmutable)
        return NS_ERROR_FAILURE; // XXX CAL_ERROR_ITEM_IS_IMMUTABLE

    mIsNegative = aIsNegative;
    return NS_OK;
}

/* readonly attribute boolean isFinite; */
NS_IMETHODIMP
calRecurrenceRule::GetIsFinite(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    if ((mIsByCount && mIcalRecur->count == 0) ||
        (!mIsByCount && icaltime_is_null_time(mIcalRecur->until)))
    {
        *_retval = PR_FALSE;
    } else {
        *_retval = PR_TRUE;
    }
    return NS_OK;
}

/* attribute long type; */
NS_IMETHODIMP
calRecurrenceRule::GetType(nsACString &aType)
{
    switch (mIcalRecur->freq) {
#define RECUR_HELPER(x) \
        case ICAL_##x##_RECURRENCE: aType.AssignLiteral( #x ); break
        RECUR_HELPER(SECONDLY);
        RECUR_HELPER(MINUTELY);
        RECUR_HELPER(HOURLY);
        RECUR_HELPER(DAILY);
        RECUR_HELPER(WEEKLY);
        RECUR_HELPER(MONTHLY);
        RECUR_HELPER(YEARLY);
#undef RECUR_HELPER
        default:
            aType.AssignLiteral("");
    }

    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceRule::SetType(const nsACString &aType)
{
#define RECUR_HELPER(x) \
    if (aType.EqualsLiteral( #x )) mIcalRecur->freq = ICAL_##x##_RECURRENCE
    RECUR_HELPER(SECONDLY);
    else RECUR_HELPER(MINUTELY);
    else RECUR_HELPER(HOURLY);
    else RECUR_HELPER(DAILY);
    else RECUR_HELPER(WEEKLY);
    else RECUR_HELPER(MONTHLY);
    else RECUR_HELPER(YEARLY);
#undef RECUR_HELPER
    else if (aType.IsEmpty() || aType.EqualsLiteral(""))
        mIcalRecur->freq = ICAL_NO_RECURRENCE;
    else
        return NS_ERROR_FAILURE;

    return NS_OK;
}

/* attribute long count; */
NS_IMETHODIMP
calRecurrenceRule::GetCount(PRInt32 *aRecurCount)
{
    NS_ENSURE_ARG_POINTER(aRecurCount);

    if (!mIsByCount)
        return NS_ERROR_FAILURE;

    if (mIcalRecur->count == 0 && icaltime_is_null_time(mIcalRecur->until)) {
        *aRecurCount = -1;
    } else if (mIcalRecur->count) {
        *aRecurCount = mIcalRecur->count;
    } else {
        // count wasn't set, so we don't know
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceRule::SetCount(PRInt32 aRecurCount)
{
    if (aRecurCount != -1) {
        mIcalRecur->count = aRecurCount;
    } else {
        mIcalRecur->count = 0;
    }

    mIcalRecur->until = icaltime_null_time();

    mIsByCount = PR_TRUE;

    return NS_OK;
}

/* attribute calIDateTime endDate; */
NS_IMETHODIMP
calRecurrenceRule::GetEndDate(calIDateTime * *aRecurEnd)
{
    NS_ENSURE_ARG_POINTER(aRecurEnd);

    if (mIsByCount)
        return NS_ERROR_FAILURE;

    if (!icaltime_is_null_time(mIcalRecur->until)) {
        calDateTime *cdt = new calDateTime(&mIcalRecur->until);
        if (!cdt)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF (*aRecurEnd = cdt);
    } else {
        // infinite recurrence
        *aRecurEnd = nsnull; 
    }
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceRule::SetEndDate(calIDateTime * aRecurEnd)
{
    if (aRecurEnd) {
        struct icaltimetype itt;
        aRecurEnd->ToIcalTime(&itt);

        mIcalRecur->until = itt;
    } else {
        mIcalRecur->until = icaltime_null_time();
    }

    mIcalRecur->count = 0;

    mIsByCount = PR_FALSE;

    return NS_OK;
}

/* readonly attribute boolean isByCount; */
NS_IMETHODIMP
calRecurrenceRule::GetIsByCount (PRBool *aIsByCount)
{
    *aIsByCount = mIsByCount;
    return NS_OK;
}

/* attribute long interval; */
NS_IMETHODIMP
calRecurrenceRule::GetInterval(PRInt32 *aInterval)
{
    NS_ENSURE_ARG_POINTER(aInterval);
    *aInterval = mIcalRecur->interval;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceRule::SetInterval(PRInt32 aInterval)
{
    mIcalRecur->interval = aInterval;
    return NS_OK;
}

/* void getComponent (in AUTF8String aComponentType, out unsigned long aCount, [array, size_is (aCount), retval] out long aValues); */
NS_IMETHODIMP
calRecurrenceRule::GetComponent(const nsACString &aComponentType, PRUint32 *aCount, PRInt16 **aValues)
{
    NS_ENSURE_ARG_POINTER(aCount);
    NS_ENSURE_ARG_POINTER(aValues);

    // This little ugly macro counts the number of real entries
    // we have in the relevant array, and then clones it to the result.
#define HANDLE_COMPONENT(_comptype,_icalvar,_icalmax)                   \
    if (aComponentType.EqualsLiteral( #_comptype )) {                    \
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

    HANDLE_COMPONENT(BYSECOND, by_second, ICAL_BY_SECOND_SIZE)
    else HANDLE_COMPONENT(BYMINUTE, by_minute, ICAL_BY_MINUTE_SIZE)
    else HANDLE_COMPONENT(BYHOUR, by_hour, ICAL_BY_HOUR_SIZE)
    else HANDLE_COMPONENT(BYDAY, by_day, ICAL_BY_DAY_SIZE) // special
    else HANDLE_COMPONENT(BYMONTHDAY, by_month_day, ICAL_BY_MONTHDAY_SIZE)
    else HANDLE_COMPONENT(BYYEARDAY, by_year_day, ICAL_BY_YEARDAY_SIZE)
    else HANDLE_COMPONENT(BYWEEKNO, by_week_no, ICAL_BY_WEEKNO_SIZE)
    else HANDLE_COMPONENT(BYMONTH, by_month, ICAL_BY_MONTH_SIZE)
    else HANDLE_COMPONENT(BYSETPOS, by_set_pos, ICAL_BY_SETPOS_SIZE)
    else {
        // invalid component; XXX - error code
        return NS_ERROR_FAILURE;
    }
#undef HANDLE_COMPONENT

    return NS_OK;
}

/* void setComponent (in AUTF8String aComponentType, in unsigned long aCount, [array, size_is (aCount)] in long aValues); */
NS_IMETHODIMP
calRecurrenceRule::SetComponent(const nsACString& aComponentType, PRUint32 aCount, PRInt16 *aValues)
{
    NS_ENSURE_ARG_POINTER(aValues);

    // Copy the passed-in array into the ical structure array
#define HANDLE_COMPONENT(_comptype,_icalvar,_icalmax)                   \
    if (aComponentType.EqualsLiteral( #_comptype )) {                    \
        if (aCount > _icalmax)                                          \
            return NS_ERROR_FAILURE;                                    \
        memcpy(mIcalRecur->_icalvar, aValues, aCount * sizeof(PRInt16)); \
        if (aCount < _icalmax)                                          \
            mIcalRecur->_icalvar[aCount] = ICAL_RECURRENCE_ARRAY_MAX;    \
    }

    HANDLE_COMPONENT(BYSECOND, by_second, ICAL_BY_SECOND_SIZE)
    else HANDLE_COMPONENT(BYMINUTE, by_minute, ICAL_BY_MINUTE_SIZE)
    else HANDLE_COMPONENT(BYHOUR, by_hour, ICAL_BY_HOUR_SIZE)
    else HANDLE_COMPONENT(BYDAY, by_day, ICAL_BY_DAY_SIZE) // special
    else HANDLE_COMPONENT(BYMONTHDAY, by_month_day, ICAL_BY_MONTHDAY_SIZE)
    else HANDLE_COMPONENT(BYYEARDAY, by_year_day, ICAL_BY_YEARDAY_SIZE)
    else HANDLE_COMPONENT(BYWEEKNO, by_week_no, ICAL_BY_WEEKNO_SIZE)
    else HANDLE_COMPONENT(BYMONTH, by_month, ICAL_BY_MONTH_SIZE)
    else HANDLE_COMPONENT(BYSETPOS, by_set_pos, ICAL_BY_SETPOS_SIZE)
    else {
        // invalid component; XXX - error code
        return NS_ERROR_FAILURE;
    }
#undef HANDLE_COMPONENT

    return NS_OK;
}

/* calIDateTime getNextOccurrence (in calIDateTime aStartTime, in calIDateTime aOccurrenceTime); */
NS_IMETHODIMP
calRecurrenceRule::GetNextOccurrence(calIDateTime *aStartTime,
                                     calIDateTime *aOccurrenceTime,
                                     calIDateTime **_retval)
{
    NS_ENSURE_ARG_POINTER(aStartTime);
    NS_ENSURE_ARG_POINTER(aOccurrenceTime);
    NS_ENSURE_ARG_POINTER(_retval);

    struct icaltimetype dtstart;
    aStartTime->ToIcalTime(&dtstart);

    struct icaltimetype occurtime;
    aOccurrenceTime->ToIcalTime(&occurtime);

    icalrecur_iterator* recur_iter;
    recur_iter = icalrecur_iterator_new (*mIcalRecur,
                                         dtstart);
    if (!recur_iter)
        return NS_ERROR_OUT_OF_MEMORY;

    struct icaltimetype next = icalrecur_iterator_next(recur_iter);
    while (!icaltime_is_null_time(next)) {
        if (icaltime_compare(next, occurtime) > 0)
            break;

        next = icalrecur_iterator_next(recur_iter);
    }

    icalrecur_iterator_free(recur_iter);

    if (icaltime_is_null_time(next)) {
        *_retval = nsnull;
        return NS_OK;
    }

    nsCOMPtr<calIDateTime> cdt = new calDateTime(&next);
    NS_ADDREF(*_retval = cdt);
    return NS_OK;
}

static int PR_CALLBACK
calDateTimeComparator (calIDateTime *aElement1,
                       calIDateTime *aElement2,
                       void *aData)
{
    PRInt32 result;
    aElement1->Compare(aElement2, &result);
    return result;
}

NS_IMETHODIMP
calRecurrenceRule::GetOccurrences(calIDateTime *aStartTime,
                                  calIDateTime *aRangeStart,
                                  calIDateTime *aRangeEnd,
                                  PRUint32 aMaxCount,
                                  PRUint32 *aCount, calIDateTime ***aDates)
{
    NS_ENSURE_ARG_POINTER(aStartTime);
    NS_ENSURE_ARG_POINTER(aRangeStart);
    NS_ENSURE_ARG_POINTER(aCount);
    NS_ENSURE_ARG_POINTER(aDates);

    // make sure the request is sane; infinite recurrence
    // with no end time is bad times.
    if (!aMaxCount && !aRangeEnd && mIcalRecur->count == 0 && icaltime_is_null_time(mIcalRecur->until))
        return NS_ERROR_INVALID_ARG;

    nsCOMArray<calIDateTime> dates;

#ifdef DEBUG_vlad
    {
        char* ss = icalrecurrencetype_as_string(mIcalRecur);
        nsCAutoString tst, tend;
        aRangeStart->ToString(tst);
        aRangeEnd->ToString(tend);
        fprintf (stderr, "RULE: [%s -> %s, %d]: %s\n", tst.get(), tend.get(), mIcalRecur->count, ss);
    }
#endif

    struct icaltimetype rangestart, dtstart, dtend;
    aRangeStart->ToIcalTime(&rangestart);
    aStartTime->ToIcalTime(&dtstart);
    if (aRangeEnd) {
        aRangeEnd->ToIcalTime(&dtend);

        // if the start of the recurrence is past the end,
        // we have no dates
        if (icaltime_compare (dtstart, dtend) >= 0) {
            *aDates = nsnull;
            *aCount = 0;
            return NS_OK;
        }
    }

    icalrecur_iterator* recur_iter;
    recur_iter = icalrecur_iterator_new (*mIcalRecur, dtstart);
    if (!recur_iter)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 count = 0;

    struct icaltimetype next = icalrecur_iterator_next(recur_iter);
    while (!icaltime_is_null_time(next)) {
        // if this thing is before the range start
        if (icaltime_compare(next, rangestart) < 0) {
            next = icalrecur_iterator_next(recur_iter);
            continue;
        }

        if (aRangeEnd && icaltime_compare(next, dtend) >= 0)
            break;

        nsCOMPtr<calIDateTime> cdt = new calDateTime(&next);
        if (!cdt) {
            icalrecur_iterator_free(recur_iter);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        dates.AppendObject(cdt);

        next = icalrecur_iterator_next(recur_iter);

        count++;

        if (aMaxCount && aMaxCount <= count)
            break;
    }

    icalrecur_iterator_free(recur_iter);

    dates.Sort(calDateTimeComparator, nsnull);

    if (count) {
        calIDateTime **dateArray = (calIDateTime **) nsMemory::Alloc(sizeof(calIDateTime*) * count);
        for (int i = 0; i < (int) count; i++) {
            NS_ADDREF(dateArray[i] = dates[i]);
        }
        *aDates = dateArray;
    } else {
        *aDates = nsnull;
    }

    *aCount = count;

    return NS_OK;
}

/**
 ** ical property getting/setting
 **/
NS_IMETHODIMP
calRecurrenceRule::GetIcalProperty(calIIcalProperty **prop)
{
    icalproperty* rrule = icalproperty_new_rrule(*mIcalRecur);
    if (!rrule)
        return NS_ERROR_OUT_OF_MEMORY; // XXX map error code
    *prop = new calIcalProperty(rrule, nsnull);
    if (!*prop) {
        icalproperty_free(rrule);
        return NS_ERROR_FAILURE;
    }

    NS_ADDREF(*prop);
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceRule::SetIcalProperty(calIIcalProperty *aProp)
{
    NS_ENSURE_ARG_POINTER(aProp);

    if (mImmutable)
        return NS_ERROR_FAILURE;

    nsresult rv;

    // XXX we really should do some CID checking here
    calIcalProperty *cprop = NS_STATIC_CAST(calIcalProperty *, aProp);
    nsCAutoString propname;

    rv = cprop->GetPropertyName(propname);
    NS_ENSURE_SUCCESS(rv, rv);
    if (propname.EqualsLiteral("RRULE"))
        mIsNegative = PR_FALSE;
    else if (propname.EqualsLiteral("EXRULE"))
        mIsNegative = PR_TRUE;
    else
        return NS_ERROR_INVALID_ARG;

    icalproperty *prop;
    struct icalrecurrencetype icalrecur;

    prop = cprop->getIcalProperty();

    icalrecur = icalproperty_get_rrule(prop);

    // XXX Note that we ignore the dtstart and use the one from the
    // event, though I realize now that we shouldn't.  Ignoring
    // dtstart makes it impossible to have multiple RRULEs on one
    // event that start at different times (e.g. every day starting on
    // jan 1 for 2 weeks, every other day starting on feb 1 for 2
    // weeks).  Neither the server nor the UI supports this now,
    // but we really ought to!
    //struct icaltimetype icaldtstart;
    //icaldtstrat = icalproperty_get_dtstart(prop);

    if (icalrecur.count != 0)
        mIsByCount = PR_TRUE;
    else
        mIsByCount = PR_FALSE;

    *mIcalRecur = icalrecur;

    return NS_OK;
}
