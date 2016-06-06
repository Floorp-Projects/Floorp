/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationService_h
#define mozilla_dom_PresentationService_h

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsTObserverArray.h"
#include "PresentationServiceBase.h"
#include "PresentationSessionInfo.h"

class nsIPresentationSessionRequest;
class nsIURI;
class nsIPresentationSessionTransportBuilder;

namespace mozilla {
namespace dom {

class PresentationDeviceRequest;
class PresentationRespondingInfo;

class PresentationService final : public nsIPresentationService
                                , public nsIObserver
                                , public PresentationServiceBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIPRESENTATIONSERVICE

  PresentationService();
  bool Init();

  already_AddRefed<PresentationSessionInfo>
  GetSessionInfo(const nsAString& aSessionId, const uint8_t aRole)
  {
    MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
               aRole == nsIPresentationService::ROLE_RECEIVER);

    RefPtr<PresentationSessionInfo> info;
    if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
      return mSessionInfoAtController.Get(aSessionId, getter_AddRefs(info)) ?
             info.forget() : nullptr;
    } else {
      return mSessionInfoAtReceiver.Get(aSessionId, getter_AddRefs(info)) ?
             info.forget() : nullptr;
    }
  }

  bool IsSessionAccessible(const nsAString& aSessionId,
                           const uint8_t aRole,
                           base::ProcessId aProcessId);

  nsresult RegisterTransportBuilder(const nsAString& aSessionId,
                                    uint8_t aRole,
                                    nsIPresentationSessionTransportBuilder* aBuilder);

private:
  friend class PresentationDeviceRequest;

  virtual ~PresentationService();
  void HandleShutdown();
  nsresult HandleDeviceChange();
  nsresult HandleSessionRequest(nsIPresentationSessionRequest* aRequest);
  void NotifyAvailableChange(bool aIsAvailable);
  bool IsAppInstalled(nsIURI* aUri);

  // This is meant to be called by PresentationDeviceRequest.
  already_AddRefed<PresentationSessionInfo>
  CreateControllingSessionInfo(const nsAString& aUrl,
                               const nsAString& aSessionId,
                               uint64_t aWindowId);

  bool mIsAvailable;
  nsTObserverArray<nsCOMPtr<nsIPresentationAvailabilityListener>> mAvailabilityListeners;
  nsRefPtrHashtable<nsStringHashKey, PresentationSessionInfo> mSessionInfoAtController;
  nsRefPtrHashtable<nsStringHashKey, PresentationSessionInfo> mSessionInfoAtReceiver;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationService_h
