/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationIPCService_h
#define mozilla_dom_PresentationIPCService_h

#include "nsIPresentationService.h"
#include "nsRefPtrHashtable.h"
#include "nsTObserverArray.h"

class nsIDocShell;

namespace mozilla {
namespace dom {

class PresentationIPCRequest;
class PresentationResponderLoadingCallback;

class PresentationIPCService final : public nsIPresentationService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSERVICE

  PresentationIPCService();

  nsresult NotifyAvailableChange(bool aAvailable);

  nsresult NotifySessionStateChange(const nsAString& aSessionId,
                                    uint16_t aState);

  nsresult NotifyMessage(const nsAString& aSessionId,
                         const nsACString& aData);

  nsresult NotifySessionConnect(uint64_t aWindowId,
                                const nsAString& aSessionId);

  void NotifyPresentationChildDestroyed();

  nsresult MonitorResponderLoading(const nsAString& aSessionId,
                                   nsIDocShell* aDocShell);

private:
  virtual ~PresentationIPCService();
  nsresult SendRequest(nsIPresentationServiceCallback* aCallback,
                       const PresentationIPCRequest& aRequest);

  nsTObserverArray<nsCOMPtr<nsIPresentationAvailabilityListener> > mAvailabilityListeners;
  nsRefPtrHashtable<nsStringHashKey, nsIPresentationSessionListener> mSessionListeners;
  nsRefPtrHashtable<nsUint64HashKey, nsIPresentationRespondingListener> mRespondingListeners;
  nsRefPtr<PresentationResponderLoadingCallback> mCallback;

  // Store the mapping between the window ID of the OOP page (in this process)
  // and the ID of the responding session. It's used for an OOP receiver page
  // to retrieve the correspondent session ID. Besides, also keep the mapping
  // between the responding session ID and the window ID to help look up the
  // window ID.
  nsClassHashtable<nsUint64HashKey, nsString> mRespondingSessionIds;
  nsDataHashtable<nsStringHashKey, uint64_t> mRespondingWindowIds;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationIPCService_h
