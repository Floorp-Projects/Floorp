/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnManagerBase_h
#define mozilla_dom_WebAuthnManagerBase_h

#include "nsIDOMEventListener.h"

/*
 * A base class used by WebAuthn and U2F implementations, providing shared
 * functionality and requiring an interface used by the IPC child actors.
 */

namespace mozilla {
namespace dom {

class WebAuthnTransactionChild;
class WebAuthnMakeCredentialResult;
class WebAuthnGetAssertionResult;

class WebAuthnManagerBase : public nsIDOMEventListener {
 public:
  NS_DECL_NSIDOMEVENTLISTENER

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(WebAuthnManagerBase)

  explicit WebAuthnManagerBase(nsPIDOMWindowInner* aParent);

  MOZ_CAN_RUN_SCRIPT
  virtual void FinishMakeCredential(
      const uint64_t& aTransactionId,
      const WebAuthnMakeCredentialResult& aResult) = 0;

  MOZ_CAN_RUN_SCRIPT
  virtual void FinishGetAssertion(
      const uint64_t& aTransactionId,
      const WebAuthnGetAssertionResult& aResult) = 0;

  MOZ_CAN_RUN_SCRIPT
  virtual void RequestAborted(const uint64_t& aTransactionId,
                              const nsresult& aError) = 0;

  void ActorDestroyed();

 protected:
  MOZ_CAN_RUN_SCRIPT virtual ~WebAuthnManagerBase();

  // Needed by HandleEvent() to track visibilty changes.
  MOZ_CAN_RUN_SCRIPT virtual void HandleVisibilityChange() = 0;

  // Visibility event handling.
  void ListenForVisibilityEvents();
  void StopListeningForVisibilityEvents();

  bool MaybeCreateBackgroundActor();

  // The parent window.
  nsCOMPtr<nsPIDOMWindowInner> mParent;

  // IPC Channel to the parent process.
  RefPtr<WebAuthnTransactionChild> mChild;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_WebAuthnManagerBase_h
