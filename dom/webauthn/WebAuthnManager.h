/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnManager_h
#define mozilla_dom_WebAuthnManager_h

#include "mozilla/MozPromise.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/PWebAuthnTransaction.h"
#include "mozilla/dom/WebAuthnManagerBase.h"
#include "nsIDOMEventListener.h"

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
  WebAuthnTransaction(const RefPtr<Promise>& aPromise,
                      const nsTArray<uint8_t>& aRpIdHash,
                      const nsCString& aClientData,
                      AbortSignal* aSignal)
    : mPromise(aPromise)
    , mRpIdHash(aRpIdHash)
    , mClientData(aClientData)
    , mSignal(aSignal)
    , mId(NextId())
  {
    MOZ_ASSERT(mId > 0);
  }

  // JS Promise representing the transaction status.
  RefPtr<Promise> mPromise;

  // The RP ID hash.
  nsTArray<uint8_t> mRpIdHash;

  // Client data used to assemble reply objects.
  nsCString mClientData;

  // An optional AbortSignal instance.
  RefPtr<AbortSignal> mSignal;

  // Unique transaction id.
  uint64_t mId;

private:
  // Generates a unique id for new transactions. This doesn't have to be unique
  // forever, it's sufficient to differentiate between temporally close
  // transactions, where messages can intersect. Can overflow.
  static uint64_t NextId() {
    static uint64_t id = 0;
    return ++id;
  }
};

class WebAuthnManager final : public WebAuthnManagerBase
                            , public nsIDOMEventListener
                            , public AbortFollower
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  explicit WebAuthnManager(nsPIDOMWindowInner* aParent);

  already_AddRefed<Promise>
  MakeCredential(const MakePublicKeyCredentialOptions& aOptions,
                 const Optional<OwningNonNull<AbortSignal>>& aSignal);

  already_AddRefed<Promise>
  GetAssertion(const PublicKeyCredentialRequestOptions& aOptions,
               const Optional<OwningNonNull<AbortSignal>>& aSignal);

  already_AddRefed<Promise>
  Store(const Credential& aCredential);

  // WebAuthnManagerBase

  void
  FinishMakeCredential(const uint64_t& aTransactionId,
                       nsTArray<uint8_t>& aRegBuffer) override;

  void
  FinishGetAssertion(const uint64_t& aTransactionId,
                     nsTArray<uint8_t>& aCredentialId,
                     nsTArray<uint8_t>& aSigBuffer) override;

  void
  RequestAborted(const uint64_t& aTransactionId,
                 const nsresult& aError) override;

  void ActorDestroyed() override;

  // AbortFollower

  void Abort() override;

private:
  virtual ~WebAuthnManager();

  // Visibility event handling.
  void ListenForVisibilityEvents();
  void StopListeningForVisibilityEvents();

  // Clears all information we have about the current transaction.
  void ClearTransaction();
  // Rejects the current transaction and calls ClearTransaction().
  void RejectTransaction(const nsresult& aError);
  // Cancels the current transaction (by sending a Cancel message to the
  // parent) and rejects it by calling RejectTransaction().
  void CancelTransaction(const nsresult& aError);

  bool MaybeCreateBackgroundActor();

  // The parent window.
  nsCOMPtr<nsPIDOMWindowInner> mParent;

  // IPC Channel to the parent process.
  RefPtr<WebAuthnTransactionChild> mChild;

  // The current transaction, if any.
  Maybe<WebAuthnTransaction> mTransaction;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebAuthnManager_h
