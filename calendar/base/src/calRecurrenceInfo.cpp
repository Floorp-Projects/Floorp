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

#include "calBaseCID.h"

#include "calRecurrenceInfo.h"
#include "calDateTime.h"
#include "calIItemBase.h"
#include "calIEvent.h"

#include "calICSService.h"

#include "nsCOMArray.h"

NS_IMPL_ISUPPORTS1(calRecurrenceInfo, calIRecurrenceInfo)

calRecurrenceInfo::calRecurrenceInfo()
    : mImmutable(PR_FALSE)
{
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
    nsresult rv;

    calRecurrenceInfo *cri = new calRecurrenceInfo;
    if (!cri)
        return NS_ERROR_OUT_OF_MEMORY;

    cri->mBaseItem = mBaseItem;

    for (int i = 0; i < mRecurrenceItems.Count(); i++) {
        nsCOMPtr<calIRecurrenceItem> item;
        rv = mRecurrenceItems[i]->Clone(getter_AddRefs(item));
        NS_ENSURE_SUCCESS(rv, rv);

        cri->mRecurrenceItems.AppendObject(item);
    }

    NS_ADDREF(*aResult = cri);
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::Initialize(calIItemBase *aItem)
{
    // should it be an error to initialize an already-initialized objet?
    mBaseItem = aItem;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::GetItem(calIItemBase **aResult)
{
    NS_IF_ADDREF(*aResult = mBaseItem);
    return NS_OK;
}

//
// Recurrence Item set munging
//

NS_IMETHODIMP
calRecurrenceInfo::GetRecurrenceItems (PRUint32 *aCount, calIRecurrenceItem ***aItems)
{
    if (mRecurrenceItems.Count() == 0) {
        *aItems = nsnull;
        *aCount = 0;
        return NS_OK;
    }

    calIRecurrenceItem **items = (calIRecurrenceItem**) nsMemory::Alloc (sizeof(calIRecurrenceItem*) * mRecurrenceItems.Count());
    if (!items)
        return NS_ERROR_OUT_OF_MEMORY;

    for (PRInt32 i = 0; i < mRecurrenceItems.Count(); i++) {
        NS_IF_ADDREF (items[i] = mRecurrenceItems[i]);
    }

    *aItems = items;
    *aCount = mRecurrenceItems.Count();
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::SetRecurrenceItems (PRUint32 aCount, calIRecurrenceItem **aItems)
{
    if (mImmutable) {
        return NS_ERROR_FAILURE; // XXX different error
    }

    mRecurrenceItems.Clear();
    for (PRUint32 i = 0; i < aCount; i++)
        mRecurrenceItems.AppendObject(aItems[i]);

    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::CountRecurrenceItems (PRUint32 *aCount)
{
    *aCount = mRecurrenceItems.Count();
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::GetRecurrenceItemAt (PRUint32 aIndex, calIRecurrenceItem **aItem)
{
    NS_IF_ADDREF(*aItem = mRecurrenceItems[aIndex]);
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::AppendRecurrenceItem (calIRecurrenceItem *aItem)
{
    if (mImmutable) {
        return NS_ERROR_FAILURE; // XXX different error
    }

    mRecurrenceItems.AppendObject(aItem);
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::DeleteRecurrenceItemAt (PRUint32 aIndex)
{
    if (!mRecurrenceItems.RemoveObjectAt(aIndex))
        return NS_ERROR_INVALID_ARG;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::InsertRecurrenceItemAt (calIRecurrenceItem *aItem, PRUint32 aIndex)
{
    if (!mRecurrenceItems.InsertObjectAt(aItem, aIndex))
        return NS_ERROR_FAILURE;
    return NS_OK;
}

//
// recurrence calculation
//

NS_IMETHODIMP
calRecurrenceInfo::GetNextOccurrence (calIDateTime *aOccurrenceTime, calIItemOccurrence **aItem)
{
    NS_ENSURE_ARG_POINTER(aOccurrenceTime);
    NS_ENSURE_ARG_POINTER(aItem);

    NS_ASSERTION (mBaseItem, "RecurrenceInfo not initialized");

    PRInt32 i, j;
    PRInt32 result;
    nsresult rv;

    nsCOMArray<calIDateTime> dates;
    nsCOMPtr<calIDateTime> date;

    nsCOMPtr<calIDateTime> startDate;
    rv = mBaseItem->GetRecurrenceStartDate(getter_AddRefs(startDate));
    NS_ENSURE_SUCCESS(rv, rv);

    for (i = 0; i < mRecurrenceItems.Count(); i++) {
        rv = mRecurrenceItems[i]->GetNextOccurrence (startDate, aOccurrenceTime, getter_AddRefs(date));
        NS_ENSURE_SUCCESS(rv, rv);

        // if there is no next occurrence, continue.
        if (!date)
            continue;

        PRBool isNegative = PR_FALSE;
        rv = mRecurrenceItems[i]->GetIsNegative(&isNegative);
        NS_ENSURE_SUCCESS(rv, rv);

        if (isNegative) {
            // if this is negative, we look for this date in the existing set, and remove it if its there
            for (j = dates.Count() - 1; j >= 0; j--) {
                if (NS_SUCCEEDED(dates[j]->Compare(date, &result)) && result == 0)
                    dates.RemoveObjectAt(j);
            }
        } else {
            // if positive, we just add the date to the existing set
            dates.AppendObject(date);
        }
    }

    // no next date found
    if (dates.Count() == 0) {
        *aItem = nsnull;
        return NS_OK;
    }

    // find the earliest date in the set
    date = dates[0];
    for (i = 0; i < dates.Count(); i++) {
        if (NS_SUCCEEDED(date->Compare(dates[i], &result)) && result > 0)
            date = dates[i];
    }
    nsCOMPtr<calIDateTime> enddate;
    date->Clone(getter_AddRefs(enddate));

    nsCOMPtr<calIEvent> event = do_QueryInterface(mBaseItem);
    if (event) {
        nsCOMPtr<calIDateTime> duration;
        rv = event->GetDuration(getter_AddRefs(duration));
        NS_ENSURE_SUCCESS(rv, rv);

        enddate->AddDuration(duration);
    }

    nsCOMPtr<calIItemOccurrence> occ = do_CreateInstance(CAL_ITEM_OCCURRENCE_CONTRACTID);
    occ->Initialize(mBaseItem, date, enddate);
    NS_ADDREF(*aItem = occ);
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
calRecurrenceInfo::GetOccurrences (calIDateTime *aRangeStart,
                                   calIDateTime *aRangeEnd,
                                   PRUint32 aMaxCount,
                                   PRUint32 *aCount, calIItemOccurrence ***aItems)
{
    NS_ENSURE_ARG_POINTER(aRangeStart);
    NS_ENSURE_ARG_POINTER(aCount);
    NS_ENSURE_ARG_POINTER(aItems);

    NS_ASSERTION (mBaseItem, "RecurrenceInfo not initialized");

    PRInt32 i, j, k;
    PRInt32 result;
    nsresult rv;

    nsCOMArray<calIDateTime> dates;

    nsCOMPtr<calIDateTime> startDate;
    rv = mBaseItem->GetRecurrenceStartDate(getter_AddRefs(startDate));
    NS_ENSURE_SUCCESS(rv, rv);

    for (i = 0; i < mRecurrenceItems.Count(); i++) {
        calIDateTime **cur_dates = nsnull;
        PRUint32 num_cur_dates = 0;

        // if both range start and end are specified, we ask for all of the occurrences,
        // to make sure we catch all possible exceptions.  If aRangeEnd isn't specified,
        // then we have to ask for aMaxCount, and hope for the best.
        if (aRangeStart && aRangeEnd)
            rv = mRecurrenceItems[i]->GetOccurrences (startDate, aRangeStart, aRangeEnd, 0, &num_cur_dates, &cur_dates);
        else
            rv = mRecurrenceItems[i]->GetOccurrences (startDate, aRangeStart, aRangeEnd, aMaxCount, &num_cur_dates, &cur_dates);
        NS_ENSURE_SUCCESS(rv, rv);

        // if there are no occurrences, continue.
        if (num_cur_dates == 0)
            continue;

        PRBool isNegative = PR_FALSE;
        rv = mRecurrenceItems[i]->GetIsNegative(&isNegative);
        NS_ENSURE_SUCCESS(rv, rv);

        if (isNegative) {
            // if this is negative, we look for any of the given dates
            // in the existing set, and remove them if they're
            // present.
            for (j = dates.Count() - 1; j >= 0; j--) {
                for (k = 0; k < (int) num_cur_dates; k++) {
                    if (NS_SUCCEEDED(dates[j]->Compare(cur_dates[k], &result)) && result == 0) {
                        dates.RemoveObjectAt(j);
                        break;
                    }
                }
            }
        } else {
            // if positive, we just add these date to the existing set,
            // but only if they're not already there
            for (j = 0; j < (int) num_cur_dates; j++) {
                PRBool isFound = PR_FALSE;
                for (k = 0; k < dates.Count(); k++) {
                    if (NS_SUCCEEDED(dates[k]->Compare(cur_dates[j], &result)) && result == 0) {
                        isFound = PR_TRUE;
                        break;
                    }
                }

                if (!isFound)
                    dates.AppendObject(cur_dates[j]);
            }
        }
    }

    // now sort the resulting list
    dates.Sort(calDateTimeComparator, nsnull);

    nsCOMPtr<calIDateTime> duration;
    nsCOMPtr<calIEvent> event = do_QueryInterface(mBaseItem);
    if (event) {
        rv = event->GetDuration(getter_AddRefs(duration));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    PRUint32 count = aMaxCount;
    if (!count)
        count = dates.Count();
    if (count) {
        calIItemOccurrence **occArray = (calIItemOccurrence **) nsMemory::Alloc(sizeof(calIItemOccurrence*) * count);
        for (int i = 0; i < (int) count; i++) {
            nsCOMPtr<calIItemOccurrence> occ = do_CreateInstance (CAL_ITEM_OCCURRENCE_CONTRACTID);

            nsCOMPtr<calIDateTime> endDate;
            dates[i]->Clone(getter_AddRefs(endDate));

            if (event) {
                endDate->AddDuration(duration);
            }
                
            occ->Initialize (mBaseItem, dates[i], endDate);

            NS_ADDREF(occArray[i] = occ);
        }

        *aItems = occArray;
    } else {
        *aItems = nsnull;
    }
    *aCount = count;
    return NS_OK;
}

NS_IMETHODIMP
calRecurrenceInfo::GetIcalProperty(calIIcalProperty **aProp)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
calRecurrenceInfo::SetIcalProperty(calIIcalProperty *aProp)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
