/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationParent_h__
#define mozilla_dom_PresentationParent_h__

#include "mozilla/dom/PPresentationParent.h"
#include "mozilla/dom/PPresentationRequestParent.h"
#include "nsIPresentationListener.h"
#include "nsIPresentationService.h"

namespace mozilla {
namespace dom {

class PresentationParent final : public PPresentationParent
                               , public nsIPresentationAvailabilityListener
                               , public nsIPresentationSessionListener
                               , public nsIPresentationRespondingListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONAVAILABILITYLISTENER
  NS_DECL_NSIPRESENTATIONSESSIONLISTENER
  NS_DECL_NSIPRESENTATIONRESPONDINGLISTENER

  PresentationParent();

  bool Init();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  RecvPPresentationRequestConstructor(PPresentationRequestParent* aActor,
                                      const PresentationIPCRequest& aRequest) override;

  virtual PPresentationRequestParent*
  AllocPPresentationRequestParent(const PresentationIPCRequest& aRequest) override;

  virtual bool
  DeallocPPresentationRequestParent(PPresentationRequestParent* aActor) override;

  virtual bool Recv__delete__() override;

  virtual bool RecvRegisterAvailabilityHandler() override;

  virtual bool RecvUnregisterAvailabilityHandler() override;

  virtual bool RecvRegisterSessionHandler(const nsString& aSessionId) override;

  virtual bool RecvUnregisterSessionHandler(const nsString& aSessionId) override;

  virtual bool RecvRegisterRespondingHandler(const uint64_t& aWindowId) override;

  virtual bool RecvUnregisterRespondingHandler(const uint64_t& aWindowId) override;

  virtual bool RecvNotifyReceiverReady(const nsString& aSessionId) override;

private:
  virtual ~PresentationParent();

  bool mActorDestroyed;
  nsCOMPtr<nsIPresentationService> mService;
  nsTArray<nsString> mSessionIds;
  nsTArray<uint64_t> mWindowIds;
};

class PresentationRequestParent final : public PPresentationRequestParent
                                      , public nsIPresentationServiceCallback
{
  friend class PresentationParent;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSERVICECALLBACK

  explicit PresentationRequestParent(nsIPresentationService* aService);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  virtual ~PresentationRequestParent();

  nsresult SendResponse(nsresult aResult);

  nsresult DoRequest(const StartSessionRequest& aRequest);

  nsresult DoRequest(const SendSessionMessageRequest& aRequest);

  nsresult DoRequest(const TerminateRequest& aRequest);

  bool mActorDestroyed;
  nsCOMPtr<nsIPresentationService> mService;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationParent_h__
