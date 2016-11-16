/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationChild_h
#define mozilla_dom_PresentationChild_h

#include "mozilla/dom/PPresentationBuilderChild.h"
#include "mozilla/dom/PPresentationChild.h"
#include "mozilla/dom/PPresentationRequestChild.h"

class nsIPresentationServiceCallback;

namespace mozilla {
namespace dom {

class PresentationIPCService;

class PresentationChild final : public PPresentationChild
{
public:
  explicit PresentationChild(PresentationIPCService* aService);

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PPresentationRequestChild*
  AllocPPresentationRequestChild(const PresentationIPCRequest& aRequest) override;

  virtual bool
  DeallocPPresentationRequestChild(PPresentationRequestChild* aActor) override;

  mozilla::ipc::IPCResult RecvPPresentationBuilderConstructor(PPresentationBuilderChild* aActor,
                                                              const nsString& aSessionId,
                                                              const uint8_t& aRole) override;

  virtual PPresentationBuilderChild*
  AllocPPresentationBuilderChild(const nsString& aSessionId, const uint8_t& aRole) override;

  virtual bool
  DeallocPPresentationBuilderChild(PPresentationBuilderChild* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyAvailableChange(nsTArray<nsString>&& aAvailabilityUrls,
                            const bool& aAvailable) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifySessionStateChange(const nsString& aSessionId,
                               const uint16_t& aState,
                               const nsresult& aReason) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyMessage(const nsString& aSessionId,
                    const nsCString& aData,
                    const bool& aIsBinary) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifySessionConnect(const uint64_t& aWindowId,
                           const nsString& aSessionId) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyCloseSessionTransport(const nsString& aSessionId,
                                  const uint8_t& aRole,
                                  const nsresult& aReason) override;

private:
  virtual ~PresentationChild();

  bool mActorDestroyed = false;
  RefPtr<PresentationIPCService> mService;
};

class PresentationRequestChild final : public PPresentationRequestChild
{
  friend class PresentationChild;

public:
  explicit PresentationRequestChild(nsIPresentationServiceCallback* aCallback);

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult
  Recv__delete__(const nsresult& aResult) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyRequestUrlSelected(const nsString& aUrl) override;

private:
  virtual ~PresentationRequestChild();

  bool mActorDestroyed = false;
  nsCOMPtr<nsIPresentationServiceCallback> mCallback;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationChild_h
