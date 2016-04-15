/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVIPCService_h
#define mozilla_dom_TVIPCService_h

#include "mozilla/Tuple.h"
#include "nsITVService.h"
#include "nsTArray.h"
#include "TVTypes.h"

namespace mozilla {
namespace dom {

class TVIPCRequest;

class TVIPCService final : public nsITVService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITVSERVICE

  TVIPCService();

  nsresult NotifyChannelScanned(const nsAString& aTunerId,
                                const nsAString& aSourceType,
                                nsITVChannelData* aChannelData);

  nsresult NotifyChannelScanComplete(const nsAString& aTunerId,
                                     const nsAString& aSourceType);

  nsresult NotifyChannelScanStopped(const nsAString& aTunerId,
                                    const nsAString& aSourceType);

  nsresult NotifyEITBroadcasted(const nsAString& aTunerId,
                                const nsAString& aSourceType,
                                nsITVChannelData* aChannelData,
                                nsITVProgramData** aProgramDataList,
                                uint32_t aCount);

private:
  virtual ~TVIPCService();

  void GetSourceListeners(
    const nsAString& aTunerId, const nsAString& aSourceType,
    nsTArray<nsCOMPtr<nsITVSourceListener>>& aListeners) const;

  nsresult SendRequest(nsITVServiceCallback* aCallback,
                       const TVIPCRequest& aRequest);

  nsTArray<UniquePtr<TVSourceListenerTuple>> mSourceListenerTuples;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVIPCService_h
