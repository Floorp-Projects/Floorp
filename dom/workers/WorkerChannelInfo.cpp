/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerChannelInfo.h"
#include "mozilla/dom/BrowsingContext.h"

namespace mozilla::dom {

// WorkerChannelLoadInfo

NS_IMPL_ISUPPORTS(WorkerChannelLoadInfo, nsIWorkerChannelLoadInfo)

NS_IMETHODIMP
WorkerChannelLoadInfo::GetWorkerAssociatedBrowsingContextID(uint64_t* aResult) {
  *aResult = mWorkerAssociatedBrowsingContextID;
  return NS_OK;
}

NS_IMETHODIMP
WorkerChannelLoadInfo::SetWorkerAssociatedBrowsingContextID(uint64_t aID) {
  mWorkerAssociatedBrowsingContextID = aID;
  return NS_OK;
}

NS_IMETHODIMP
WorkerChannelLoadInfo::GetWorkerAssociatedBrowsingContext(
    dom::BrowsingContext** aResult) {
  *aResult = BrowsingContext::Get(mWorkerAssociatedBrowsingContextID).take();
  return NS_OK;
}

// WorkerChannelInfo

NS_IMPL_ISUPPORTS(WorkerChannelInfo, nsIWorkerChannelInfo)

WorkerChannelInfo::WorkerChannelInfo(uint64_t aChannelID,
                                     uint64_t aBrowsingContextID)
    : mChannelID(aChannelID) {
  mLoadInfo = new WorkerChannelLoadInfo();
  mLoadInfo->SetWorkerAssociatedBrowsingContextID(aBrowsingContextID);
}

NS_IMETHODIMP
WorkerChannelInfo::SetLoadInfo(nsIWorkerChannelLoadInfo* aLoadInfo) {
  MOZ_ASSERT(aLoadInfo);
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
WorkerChannelInfo::GetLoadInfo(nsIWorkerChannelLoadInfo** aLoadInfo) {
  *aLoadInfo = do_AddRef(mLoadInfo).take();
  return NS_OK;
}

NS_IMETHODIMP
WorkerChannelInfo::GetChannelId(uint64_t* aChannelID) {
  NS_ENSURE_ARG_POINTER(aChannelID);
  *aChannelID = mChannelID;
  return NS_OK;
}

}  // end of namespace mozilla::dom
