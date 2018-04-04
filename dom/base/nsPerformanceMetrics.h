/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPerformanceMetrics_h___
#define nsPerformanceMetrics_h___

#include "nsCOMArray.h"
#include "nsIPerformanceMetrics.h"
#include "nsString.h"


class PerformanceMetricsDispatchCategory final : public nsIPerformanceMetricsDispatchCategory
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERFORMANCEMETRICSDISPATCHCATEGORY
  PerformanceMetricsDispatchCategory(uint32_t aCategory, uint32_t aCount);
private:
  ~PerformanceMetricsDispatchCategory() = default;

  uint32_t mCategory;
  uint32_t mCount;
};


class PerformanceMetricsData final : public nsIPerformanceMetricsData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERFORMANCEMETRICSDATA
  PerformanceMetricsData(uint32_t aPid, uint64_t aWid, uint64_t aPwid, const nsCString& aHost,
                         uint64_t aDuration, bool aWorker, nsIArray* aItems);
private:
  ~PerformanceMetricsData() = default;

  uint32_t mPid;
  uint64_t mWid;
  uint64_t mPwid;
  nsCString mHost;
  uint64_t mDuration;
  bool mWorker;
  nsCOMArray<nsIPerformanceMetricsDispatchCategory> mItems;
};

#endif  // end nsPerformanceMetrics_h__
