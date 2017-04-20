/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnManager_h
#define mozilla_dom_WebAuthnManager_h

#include "nsIIPCBackgroundChildCreateCallback.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/PWebAuthnTransaction.h"

/*
 * Content process manager for the WebAuthn protocol. Created on calls to the
 * WebAuthentication DOM object, this manager handles establishing IPC channels
 * for WebAuthn transactions, as well as keeping track of JS Promise objects
 * representing transactions in flight.
 *
 * The WebAuthn spec (https://www.w3.org/TR/webauthn/) allows for two different
 * types of transactions: registration and signing. When either of these is
 * requested via the DOM API, the following steps are executed in the
 * WebAuthnManager:
 *
 * - Validation of the request. Return a failed promise to js if request does
 *   not have correct parameters.
 *
 * - If request is valid, open a new IPC channel for running the transaction. If
 *   another transaction is already running in this content process, cancel it.
 *   Return a pending promise to js.
 *
 * - Send transaction information to parent process (by running the Start*
 *   functions of WebAuthnManager). Assuming another transaction is currently in
 *   flight in another content process, parent will handle canceling it.
 *
 * - On return of successful transaction information from parent process, turn
 *   information into DOM object format required by spec, and resolve promise
 *   (by running the Finish* functions of WebAuthnManager). On cancellation
 *   request from parent, reject promise with corresponding error code. Either
 *   outcome will also close the IPC channel.
 *
 */

namespace mozilla {
namespace dom {

struct Account;
class ArrayBufferViewOrArrayBuffer;
struct AssertionOptions;
class OwningArrayBufferViewOrArrayBuffer;
struct ScopedCredentialOptions;
struct ScopedCredentialParameters;
class Promise;
class WebAuthnTransactionChild;
class WebAuthnTransactionInfo;

class WebAuthnManager final : public nsIIPCBackgroundChildCreateCallback
{
public:
  NS_DECL_ISUPPORTS
  static WebAuthnManager* GetOrCreate();
  static WebAuthnManager* Get();

  void
  FinishMakeCredential(nsTArray<uint8_t>& aRegBuffer,
                       nsTArray<uint8_t>& aSigBuffer);

  void
  FinishGetAssertion(nsTArray<uint8_t>& aCredentialId,
                     nsTArray<uint8_t>& aSigBuffer);

  void
  Cancel(const nsresult& aError);

  already_AddRefed<Promise>
  MakeCredential(nsPIDOMWindowInner* aParent, JSContext* aCx,
                 const Account& aAccountInformation,
                 const Sequence<ScopedCredentialParameters>& aCryptoParameters,
                 const ArrayBufferViewOrArrayBuffer& aAttestationChallenge,
                 const ScopedCredentialOptions& aOptions);

  already_AddRefed<Promise>
  GetAssertion(nsPIDOMWindowInner* aParent,
               const ArrayBufferViewOrArrayBuffer& aAssertionChallenge,
               const AssertionOptions& aOptions);

  void StartRegister();
  void StartSign();

  // nsIIPCbackgroundChildCreateCallback methods
  void ActorCreated(PBackgroundChild* aActor) override;
  void ActorFailed() override;
  void ActorDestroyed();
private:
  WebAuthnManager();
  virtual ~WebAuthnManager();

  void MaybeClearTransaction();

  already_AddRefed<MozPromise<nsresult, nsresult, false>>
  GetOrCreateBackgroundActor();

  // JS Promise representing transaction status.
  RefPtr<Promise> mTransactionPromise;

  // IPC Channel for the current transaction.
  RefPtr<WebAuthnTransactionChild> mChild;

  // Parent of the context we're currently running the transaction in.
  nsCOMPtr<nsPIDOMWindowInner> mCurrentParent;

  // Client data, stored on successful transaction creation, so that it can be
  // used to assemble reply objects.
  Maybe<nsCString> mClientData;

  // Holds the parameters of the current transaction, as we need them both
  // before the transaction request is sent, and on successful return.
  Maybe<WebAuthnTransactionInfo> mInfo;

  // Promise for dealing with PBackground Actor creation.
  MozPromiseHolder<MozPromise<nsresult, nsresult, false>> mPBackgroundCreationPromise;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebAuthnManager_h
