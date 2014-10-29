/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVServiceRunnables_h
#define mozilla_dom_TVServiceRunnables_h

#include "nsITVService.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

/*
 * NOTE: The callbacks passing to |nsITVService| must be called asynchronously.
 * In the implementation, actual runnable objects may need to be created and
 * call the callback off of the runnables, after the function returns. Here are
 * some ready-made runnables and could be used in the following way.
 *
 * nsCOMPtr<nsIRunnable> runnable =
 *   new TVServiceNotifyRunnable(callback, dataList, optional errorCode);
 * return NS_DispatchToCurrentThread(runnable);
 */
class TVServiceNotifyRunnable MOZ_FINAL : public nsRunnable
{
public:
  TVServiceNotifyRunnable(nsITVServiceCallback* aCallback,
                          nsIArray* aDataList,
                          uint16_t aErrorCode = nsITVServiceCallback::TV_ERROR_OK)
    : mCallback(aCallback)
    , mDataList(aDataList)
    , mErrorCode(aErrorCode)
  {}

  NS_IMETHOD Run()
  {
    if (mErrorCode == nsITVServiceCallback::TV_ERROR_OK) {
      return mCallback->NotifySuccess(mDataList);
    } else {
      return mCallback->NotifyError(mErrorCode);
    }
  }

private:
  nsCOMPtr<nsITVServiceCallback> mCallback;
  nsCOMPtr<nsIArray> mDataList;
  uint16_t mErrorCode;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVServiceRunnables_h
