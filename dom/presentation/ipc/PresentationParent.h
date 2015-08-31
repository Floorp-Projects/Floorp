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
                               , public nsIPresentationListener
                               , public nsIPresentationSessionListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONLISTENER
  NS_DECL_NSIPRESENTATIONSESSIONLISTENER

  PresentationParent();

  bool Init();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  RecvPPresentationRequestConstructor(PPresentationRequestParent* aActor,
                                      const PresentationRequest& aRequest) override;

  virtual PPresentationRequestParent*
  AllocPPresentationRequestParent(const PresentationRequest& aRequest) override;

  virtual bool
  DeallocPPresentationRequestParent(PPresentationRequestParent* aActor) override;

  virtual bool Recv__delete__() override;

  virtual bool RecvRegisterHandler() override;

  virtual bool RecvUnregisterHandler() override;

  virtual bool RecvRegisterSessionHandler(const nsString& aSessionId) override;

  virtual bool RecvUnregisterSessionHandler(const nsString& aSessionId) override;

  virtual bool RecvNotifyReceiverReady(const nsString& aSessionId) override;

private:
  virtual ~PresentationParent();

  bool mActorDestroyed;
  nsCOMPtr<nsIPresentationService> mService;
  nsTArray<nsString> mSessionIds;
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
