/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WorkerChannel_h
#define mozilla_dom_WorkerChannel_h

#include "nsIWorkerChannelInfo.h"
#include "nsILoadInfo.h"
#include "nsIChannel.h"
#include "nsTArray.h"
#include "nsIURI.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/OriginAttributes.h"

namespace mozilla::dom {

class WorkerChannelLoadInfo final : public nsIWorkerChannelLoadInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS;
  NS_DECL_NSIWORKERCHANNELLOADINFO;

 private:
  ~WorkerChannelLoadInfo() = default;

  uint64_t mWorkerAssociatedBrowsingContextID;
};

class WorkerChannelInfo final : public nsIWorkerChannelInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS;
  NS_DECL_NSIWORKERCHANNELINFO;

  WorkerChannelInfo(uint64_t aChannelID,
                    uint64_t aWorkerAssociatedBrowsingContextID);
  WorkerChannelInfo() = delete;

 private:
  ~WorkerChannelInfo() = default;

  nsCOMPtr<nsIWorkerChannelLoadInfo> mLoadInfo;
  uint64_t mChannelID;
};

}  // end of namespace mozilla::dom

#endif
