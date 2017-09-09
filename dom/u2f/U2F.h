/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2F_h
#define mozilla_dom_U2F_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/U2FBinding.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/SharedThreadPool.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIU2FToken.h"
#include "nsNSSShutDown.h"
#include "nsPIDOMWindow.h"
#include "nsProxyRelease.h"
#include "nsWrapperCache.h"

#include "U2FAuthenticator.h"

class nsISerialEventTarget;

namespace mozilla {
namespace dom {

class U2FRegisterCallback;
class U2FSignCallback;

// Defined in U2FBinding.h by the U2F.webidl; their use requires a JSContext.
struct RegisterRequest;
struct RegisteredKey;

// These structs are analogs to the WebIDL versions, but can be used on worker
// threads which lack a JSContext.
struct LocalRegisterRequest
{
  nsString mChallenge;
  nsString mVersion;
  CryptoBuffer mClientData;
};

struct LocalRegisteredKey
{
  nsString mKeyHandle;
  nsString mVersion;
  Nullable<nsString> mAppId;
  // TODO: Support transport preferences
  // Nullable<nsTArray<Transport>> mTransports;
};

typedef MozPromise<nsString, ErrorCode, false> U2FPromise;
typedef MozPromise<Authenticator, ErrorCode, false> U2FPrepPromise;

// U2FPrepTasks return lists of Authenticators that are OK to
// proceed; they're useful for culling incompatible Authenticators.
// Currently, only IsRegistered is supported.
class U2FPrepTask : public Runnable
{
public:
  explicit U2FPrepTask(const Authenticator& aAuthenticator,
                       nsISerialEventTarget* aEventTarget);

  RefPtr<U2FPrepPromise> Execute();

protected:
  virtual ~U2FPrepTask();

  Authenticator mAuthenticator;
  MozPromiseHolder<U2FPrepPromise> mPromise;
  const nsCOMPtr<nsISerialEventTarget> mEventTarget;
};

// Determine whether the provided Authenticator already knows
// of the provided Registered Key.
class U2FIsRegisteredTask final : public U2FPrepTask
{
public:
  U2FIsRegisteredTask(const Authenticator& aAuthenticator,
                      const LocalRegisteredKey& aRegisteredKey,
                      const CryptoBuffer& aAppParam,
                      nsISerialEventTarget* aEventTarget);

  NS_DECL_NSIRUNNABLE
private:
  ~U2FIsRegisteredTask();

  LocalRegisteredKey mRegisteredKey;
  CryptoBuffer mAppParam;
};

class U2FTask : public Runnable
{
public:
  U2FTask(const nsAString& aOrigin,
          const nsAString& aAppId,
          const Authenticator& aAuthenticator,
          nsISerialEventTarget* aEventTarget);

  RefPtr<U2FPromise> Execute();

  nsString mOrigin;
  nsString mAppId;
  Authenticator mAuthenticator;
  const nsCOMPtr<nsISerialEventTarget> mEventTarget;

protected:
  virtual ~U2FTask();

  MozPromiseHolder<U2FPromise> mPromise;
};

// Use the provided Authenticator to Register a new scoped credential
// for the provided application.
class U2FRegisterTask final : public U2FTask
{
public:
  U2FRegisterTask(const nsAString& aOrigin,
                  const nsAString& aAppId,
                  const Authenticator& aAuthenticator,
                  const CryptoBuffer& aAppParam,
                  const CryptoBuffer& aChallengeParam,
                  const LocalRegisterRequest& aRegisterEntry,
                  nsISerialEventTarget* aEventTarget);

  NS_DECL_NSIRUNNABLE
private:
  ~U2FRegisterTask();

  CryptoBuffer mAppParam;
  CryptoBuffer mChallengeParam;
  LocalRegisterRequest mRegisterEntry;
};

// Generate an assertion using the provided Authenticator for the given origin
// and provided application to attest to ownership of a valid scoped credential.
class U2FSignTask final : public U2FTask
{
public:
  U2FSignTask(const nsAString& aOrigin,
              const nsAString& aAppId,
              const nsAString& aVersion,
              const Authenticator& aAuthenticator,
              const CryptoBuffer& aAppParam,
              const CryptoBuffer& aChallengeParam,
              const CryptoBuffer& aClientData,
              const CryptoBuffer& aKeyHandle,
              nsISerialEventTarget* aEventTarget);

  NS_DECL_NSIRUNNABLE
private:
  ~U2FSignTask();

  nsString mVersion;
  CryptoBuffer mAppParam;
  CryptoBuffer mChallengeParam;
  CryptoBuffer mClientData;
  CryptoBuffer mKeyHandle;
};

// Mediate inter-thread communication for multiple authenticators being queried
// in concert. Operates as a cyclic buffer with a stop-work method.
class U2FStatus
{
public:
  U2FStatus();
  U2FStatus(const U2FStatus&) = delete;

  void WaitGroupAdd();
  void WaitGroupDone();
  void WaitGroupWait();

  void Stop(const ErrorCode aErrorCode);
  void Stop(const ErrorCode aErrorCode, const nsAString& aResponse);
  bool IsStopped();
  ErrorCode GetErrorCode();
  nsString GetResponse();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(U2FStatus)

private:
  ~U2FStatus();

  uint16_t mCount;
  bool mIsStopped;
  nsString mResponse;
  MOZ_INIT_OUTSIDE_CTOR ErrorCode mErrorCode;
  ReentrantMonitor mReentrantMonitor;
};

// U2FRunnables run to completion, performing a single U2F operation such as
// registering, or signing.
class U2FRunnable : public Runnable
                  , public nsNSSShutDownObject
{
public:
  U2FRunnable(const nsAString& aOrigin, const nsAString& aAppId,
              nsISerialEventTarget* aEventTarget);

  // No NSS resources to release.
  virtual
  void virtualDestroyNSSReference() override {};

protected:
  virtual ~U2FRunnable();
  ErrorCode EvaluateAppID();

  nsString mOrigin;
  nsString mAppId;
  const nsCOMPtr<nsISerialEventTarget> mEventTarget;
};

// This U2FRunnable completes a single application-requested U2F Register
// operation.
class U2FRegisterRunnable : public U2FRunnable
{
public:
  U2FRegisterRunnable(const nsAString& aOrigin,
                      const nsAString& aAppId,
                      const Sequence<RegisterRequest>& aRegisterRequests,
                      const Sequence<RegisteredKey>& aRegisteredKeys,
                      const Sequence<Authenticator>& aAuthenticators,
                      U2FRegisterCallback* aCallback,
                      nsISerialEventTarget* aEventTarget);

  void SendResponse(const RegisterResponse& aResponse);
  void SetTimeout(const int32_t aTimeoutMillis);

  NS_DECL_NSIRUNNABLE

private:
  ~U2FRegisterRunnable();

  nsTArray<LocalRegisterRequest> mRegisterRequests;
  nsTArray<LocalRegisteredKey> mRegisteredKeys;
  nsTArray<Authenticator> mAuthenticators;
  nsMainThreadPtrHandle<U2FRegisterCallback> mCallback;
  Nullable<int32_t> opt_mTimeoutSeconds;
};

// This U2FRunnable completes a single application-requested U2F Sign operation.
class U2FSignRunnable : public U2FRunnable
{
public:
  U2FSignRunnable(const nsAString& aOrigin,
                  const nsAString& aAppId,
                  const nsAString& aChallenge,
                  const Sequence<RegisteredKey>& aRegisteredKeys,
                  const Sequence<Authenticator>& aAuthenticators,
                  U2FSignCallback* aCallback,
                  nsISerialEventTarget* aEventTarget);

  void SendResponse(const SignResponse& aResponse);
  void SetTimeout(const int32_t aTimeoutMillis);

  NS_DECL_NSIRUNNABLE

private:
  ~U2FSignRunnable();

  nsString mChallenge;
  CryptoBuffer mClientData;
  nsTArray<LocalRegisteredKey> mRegisteredKeys;
  nsTArray<Authenticator> mAuthenticators;
  nsMainThreadPtrHandle<U2FSignCallback> mCallback;
  Nullable<int32_t> opt_mTimeoutSeconds;
};

// The U2F Class is used by the JS engine to initiate U2F operations.
class U2F final : public nsISupports
                , public nsWrapperCache
                , public nsNSSShutDownObject
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(U2F)

  U2F();

  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return mParent;
  }

  void
  Init(nsPIDOMWindowInner* aParent, ErrorResult& aRv);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  Register(const nsAString& aAppId,
           const Sequence<RegisterRequest>& aRegisterRequests,
           const Sequence<RegisteredKey>& aRegisteredKeys,
           U2FRegisterCallback& aCallback,
           const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
           ErrorResult& aRv);

  void
  Sign(const nsAString& aAppId,
       const nsAString& aChallenge,
       const Sequence<RegisteredKey>& aRegisteredKeys,
       U2FSignCallback& aCallback,
       const Optional<Nullable<int32_t>>& opt_aTimeoutSeconds,
       ErrorResult& aRv);

  // No NSS resources to release.
  virtual
  void virtualDestroyNSSReference() override {};

private:
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  nsString mOrigin;
  Sequence<Authenticator> mAuthenticators;
  bool mInitialized;
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  ~U2F();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2F_h
