/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FManager_h
#define mozilla_dom_U2FManager_h

#include "U2FAuthenticator.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/PWebAuthnTransaction.h"
#include "nsIDOMEventListener.h"
#include "nsIIPCBackgroundChildCreateCallback.h"

/*
 * Content process manager for the U2F protocol. Created on calls to the
 * U2F DOM object, this manager handles establishing IPC channels
 * for U2F transactions, as well as keeping track of MozPromise objects
 * representing transactions in flight.
 *
 * The U2F spec (http://fidoalliance.org/specs/fido-u2f-v1.1-id-20160915.zip)
 * allows for two different types of transactions: registration and signing.
 * When either of these is requested via the DOM API, the following steps are
 * executed in the U2FManager:
 *
 * - Validation of the request. Return a failed promise to the caller if request
 *   does not have correct parameters.
 *
 * - If request is valid, open a new IPC channel for running the transaction. If
 *   another transaction is already running in this content process, cancel it.
 *   Return a pending promise to the caller.
 *
 * - Send transaction information to parent process (by running the Start*
 *   functions of U2FManager). Assuming another transaction is currently in
 *   flight in another content process, parent will handle canceling it.
 *
 * - On return of successful transaction information from parent process, turn
 *   information into DOM object format required by spec, and resolve promise
 *   (by running the Finish* functions of U2FManager). On cancellation request
 *   from parent, reject promise with corresponding error code. Either
 *   outcome will also close the IPC channel.
 *
 */

namespace mozilla {
namespace dom {

class ArrayBufferViewOrArrayBuffer;
class OwningArrayBufferViewOrArrayBuffer;
class Promise;
class U2FTransactionChild;
class U2FTransactionInfo;

class U2FManager final : public nsIIPCBackgroundChildCreateCallback
                       , public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK

  static U2FManager* GetOrCreate();
  static U2FManager* Get();

  already_AddRefed<U2FPromise> Register(nsPIDOMWindowInner* aParent,
              const nsCString& aRpId,
              const nsCString& aClientDataJSON,
              const uint32_t& aTimeoutMillis,
              const nsTArray<WebAuthnScopedCredentialDescriptor>& aExcludeList);
  already_AddRefed<U2FPromise> Sign(nsPIDOMWindowInner* aParent,
              const nsCString& aRpId,
              const nsCString& aClientDataJSON,
              const uint32_t& aTimeoutMillis,
              const nsTArray<WebAuthnScopedCredentialDescriptor>& aKeyList);

  void FinishRegister(nsTArray<uint8_t>& aRegBuffer);
  void FinishSign(nsTArray<uint8_t>& aCredentialId,
                  nsTArray<uint8_t>& aSigBuffer);
  void RequestAborted(const nsresult& aError);

  void Cancel(const nsresult& aError);

  void ActorDestroyed();

private:
  U2FManager();
  virtual ~U2FManager();

  void MaybeClearTransaction();
  nsresult PopulateTransactionInfo(const nsCString& aRpId,
                    const nsCString& aClientDataJSON,
                    const uint32_t& aTimeoutMillis,
                    const nsTArray<WebAuthnScopedCredentialDescriptor>& aList);

  typedef MozPromise<nsresult, nsresult, false> BackgroundActorPromise;

  RefPtr<BackgroundActorPromise> GetOrCreateBackgroundActor();

  // Promise representing transaction status.
  MozPromiseHolder<U2FPromise> mTransactionPromise;

  // IPC Channel for the current transaction.
  RefPtr<U2FTransactionChild> mChild;

  // Parent of the context we're currently running the transaction in.
  nsCOMPtr<nsPIDOMWindowInner> mCurrentParent;

  // Client data, stored on successful transaction creation, so that it can be
  // used to assemble reply objects.
  Maybe<nsCString> mClientData;

  // Holds the parameters of the current transaction, as we need them both
  // before the transaction request is sent, and on successful return.
  Maybe<WebAuthnTransactionInfo> mInfo;

  // Promise for dealing with PBackground Actor creation.
  MozPromiseHolder<BackgroundActorPromise> mPBackgroundCreationPromise;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2FManager_h
