/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationContentSessionInfo_h
#define mozilla_dom_PresentationContentSessionInfo_h

#include "nsCOMPtr.h"
#include "nsIPresentationSessionTransport.h"

namespace mozilla {
namespace dom {

/**
 * PresentationContentSessionInfo manages nsIPresentationSessionTransport and
 * delegates the callbacks to PresentationIPCService. Only lives in content
 * process.
 */
class PresentationContentSessionInfo final : public nsIPresentationSessionTransportCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORTCALLBACK

  PresentationContentSessionInfo(const nsAString& aSessionId,
                                 uint8_t aRole,
                                 nsIPresentationSessionTransport* aTransport)
    : mSessionId(aSessionId)
    , mRole(aRole)
    , mTransport(aTransport)
  {
    MOZ_ASSERT(XRE_IsContentProcess());
    MOZ_ASSERT(!aSessionId.IsEmpty());
    MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
               aRole == nsIPresentationService::ROLE_RECEIVER);
    MOZ_ASSERT(aTransport);
  }

  nsresult Init();

  nsresult Send(const nsAString& aData);

  nsresult SendBinaryMsg(const nsACString& aData);

  nsresult SendBlob(nsIDOMBlob* aBlob);

  nsresult Close(nsresult aReason);

private:
  virtual ~PresentationContentSessionInfo() {}

  nsString mSessionId;
  uint8_t mRole;
  nsCOMPtr<nsIPresentationSessionTransport> mTransport;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationContentSessionInfo_h
