/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationService_h
#define mozilla_dom_PresentationService_h

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "PresentationServiceBase.h"
#include "PresentationSessionInfo.h"

class nsIPresentationSessionRequest;
class nsIPresentationTerminateRequest;
class nsIURI;
class nsIPresentationSessionTransportBuilder;

namespace mozilla {
namespace dom {

class PresentationDeviceRequest;
class PresentationRespondingInfo;

class PresentationService final
                      : public nsIPresentationService
                      , public nsIObserver
                      , public PresentationServiceBase<PresentationSessionInfo>
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIPRESENTATIONSERVICE

  PresentationService();
  bool Init();

  bool IsSessionAccessible(const nsAString& aSessionId,
                           const uint8_t aRole,
                           base::ProcessId aProcessId);

private:
  friend class PresentationDeviceRequest;

  virtual ~PresentationService();
  void HandleShutdown();
  nsresult HandleDeviceAdded(nsIPresentationDevice* aDevice);
  nsresult HandleDeviceRemoved();
  nsresult HandleSessionRequest(nsIPresentationSessionRequest* aRequest);
  nsresult HandleTerminateRequest(nsIPresentationTerminateRequest* aRequest);
  nsresult HandleReconnectRequest(nsIPresentationSessionRequest* aRequest);

  // This is meant to be called by PresentationDeviceRequest.
  already_AddRefed<PresentationSessionInfo>
  CreateControllingSessionInfo(const nsAString& aUrl,
                               const nsAString& aSessionId,
                               uint64_t aWindowId);

  // Emumerate all devices to get the availability of each input Urls.
  nsresult UpdateAvailabilityUrlChange(
                                  const nsTArray<nsString>& aAvailabilityUrls);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationService_h
