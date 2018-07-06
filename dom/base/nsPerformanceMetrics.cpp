/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- *//* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <nsIMutableArray.h>
#include <nsArrayUtils.h>
#include <nsPerformanceMetrics.h>
#include "nsComponentManagerUtils.h"

/* ------------------------------------------------------
 *
 * class PerformanceMetricsDispatchCategory
 *
 */

PerformanceMetricsDispatchCategory::PerformanceMetricsDispatchCategory(uint32_t aCategory, uint32_t aCount)
  : mCategory(aCategory), mCount(aCount)
{
}

NS_IMPL_ISUPPORTS(PerformanceMetricsDispatchCategory,
                  nsIPerformanceMetricsDispatchCategory);


NS_IMETHODIMP
PerformanceMetricsDispatchCategory::GetCategory(uint32_t* aCategory)
{
  *aCategory = mCategory;
  return NS_OK;
};

NS_IMETHODIMP
PerformanceMetricsDispatchCategory::GetCount(uint32_t* aCount)
{
  *aCount = mCount;
  return NS_OK;
};

/* ------------------------------------------------------
 *
 * class PerformanceMetricsData
 *
 */

PerformanceMetricsData::PerformanceMetricsData(uint32_t aPid, uint64_t aWid,
                                               uint64_t aPwid, const nsCString& aHost,
                                               uint64_t aDuration, bool aWorker,
                                               nsIArray* aItems)
  : mPid(aPid), mWid(aWid), mPwid(aPwid), mHost(aHost)
  , mDuration(aDuration), mWorker(aWorker)
{
    uint32_t len;
    nsresult rv = aItems->GetLength(&len);
    if (NS_FAILED(rv)) {
      NS_ASSERTION(rv == NS_OK, "Failed to ge the length");
    }
    for (uint32_t i = 0; i < len; i++) {
        nsCOMPtr<nsIPerformanceMetricsDispatchCategory> item = do_QueryElementAt(aItems, i);
        mItems.AppendElement(item);
    }
};

NS_IMPL_ISUPPORTS(PerformanceMetricsData, nsIPerformanceMetricsData);

NS_IMETHODIMP
PerformanceMetricsData::GetHost(nsACString& aHost)
{
  aHost = mHost;
  return NS_OK;
};

NS_IMETHODIMP
PerformanceMetricsData::GetWorker(bool* aWorker)
{
  *aWorker = mWorker;
  return NS_OK;
};

NS_IMETHODIMP
PerformanceMetricsData::GetPid(uint32_t* aPid)
{
  *aPid = mPid;
  return NS_OK;
};

NS_IMETHODIMP
PerformanceMetricsData::GetWid(uint64_t* aWid)
{
  *aWid = mWid;
  return NS_OK;
};

NS_IMETHODIMP
PerformanceMetricsData::GetDuration(uint64_t* aDuration)
{
  *aDuration = mDuration;
  return NS_OK;
};

NS_IMETHODIMP
PerformanceMetricsData::GetPwid(uint64_t* aPwid)
{
  *aPwid = mPwid;
  return NS_OK;
};

NS_IMETHODIMP
PerformanceMetricsData::GetItems(nsIArray** aItems)
{
  NS_ENSURE_ARG_POINTER(aItems);
  *aItems = nullptr;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> items =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t len = mItems.Length();
  for (uint32_t i = 0; i < len; i++) {
    items->AppendElement(mItems[i]);
  }

  items.forget(aItems);
  return NS_OK;
}
