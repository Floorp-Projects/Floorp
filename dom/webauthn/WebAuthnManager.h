/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnManager_h
#define mozilla_dom_WebAuthnManager_h

#include "mozilla/MozPromise.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/PWebAuthnTransaction.h"
#include "nsIDOMEventListener.h"
#include "nsIIPCBackgroundChildCreateCallback.h"

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

// Forward decl because of nsHTMLDocument.h's complex dependency on /layout/style
class nsHTMLDocument {
public:
  bool IsRegistrableDomainSuffixOfOrEqualTo(const nsAString& aHostSuffixString,
                                            const nsACString& aOrigHost);
};

namespace mozilla {
namespace dom {

struct Account;
class ArrayBufferViewOrArrayBuffer;
struct AssertionOptions;
class OwningArrayBufferViewOrArrayBuffer;
struct MakePublicKeyCredentialOptions;
class Promise;
class WebAuthnTransactionChild;

class WebAuthnTransaction
{
public:
  WebAuthnTransaction(nsPIDOMWindowInner* aParent,
                      const RefPtr<Promise>& aPromise,
                      const WebAuthnTransactionInfo&& aInfo,
                      const nsAutoCString&& aClientData)
    : mParent(aParent)
    , mPromise(aPromise)
    , mInfo(aInfo)
    , mClientData(aClientData)
  { }

  // Parent of the context we're running the transaction in.
  nsCOMPtr<nsPIDOMWindowInner> mParent;

  // JS Promise representing the transaction status.
  RefPtr<Promise> mPromise;

  // Holds the parameters of the current transaction, as we need them both
  // before the transaction request is sent, and on successful return.
  WebAuthnTransactionInfo mInfo;

  // Client data used to assemble reply objects.
  nsCString mClientData;
};

class WebAuthnManager final : public nsIIPCBackgroundChildCreateCallback,
                              public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK

  static WebAuthnManager* GetOrCreate();
  static WebAuthnManager* Get();

  already_AddRefed<Promise>
  MakeCredential(nsPIDOMWindowInner* aParent,
                 const MakePublicKeyCredentialOptions& aOptions);

  already_AddRefed<Promise>
  GetAssertion(nsPIDOMWindowInner* aParent,
               const PublicKeyCredentialRequestOptions& aOptions);

  already_AddRefed<Promise>
  Store(nsPIDOMWindowInner* aParent, const Credential& aCredential);

  void
  FinishMakeCredential(nsTArray<uint8_t>& aRegBuffer);

  void
  FinishGetAssertion(nsTArray<uint8_t>& aCredentialId,
                     nsTArray<uint8_t>& aSigBuffer);

  void
  RequestAborted(const nsresult& aError);

  void ActorDestroyed();

private:
  WebAuthnManager();
  virtual ~WebAuthnManager();

  // Clears all information we have about the current transaction.
  void ClearTransaction();
  // Rejects the current transaction and calls ClearTransaction().
  void RejectTransaction(const nsresult& aError);
  // Cancels the current transaction (by sending a Cancel message to the
  // parent) and rejects it by calling RejectTransaction().
  void CancelTransaction(const nsresult& aError);

  typedef MozPromise<nsresult, nsresult, false> BackgroundActorPromise;

  RefPtr<BackgroundActorPromise> GetOrCreateBackgroundActor();

  // IPC Channel for the current transaction.
  RefPtr<WebAuthnTransactionChild> mChild;

  // The current transaction, if any.
  Maybe<WebAuthnTransaction> mTransaction;

  // Promise for dealing with PBackground Actor creation.
  MozPromiseHolder<BackgroundActorPromise> mPBackgroundCreationPromise;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebAuthnManager_h
