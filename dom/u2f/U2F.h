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
#include "mozilla/dom/Nullable.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsINSSU2FToken.h"
#include "nsNSSShutDown.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

#include "USBToken.h"

namespace mozilla {
namespace dom {

struct RegisterRequest;
struct RegisteredKey;
class U2FRegisterCallback;
class U2FSignCallback;

// These enumerations are defined in the FIDO U2F Javascript API under the
// interface "ErrorCode" as constant integers, and thus in the U2F.webidl file.
// Any changes to these must occur in both locations.
enum class ErrorCode {
  OK = 0,
  OTHER_ERROR = 1,
  BAD_REQUEST = 2,
  CONFIGURATION_UNSUPPORTED = 3,
  DEVICE_INELIGIBLE = 4,
  TIMEOUT = 5
};

typedef nsCOMPtr<nsINSSU2FToken> Authenticator;

class U2FTask : public Runnable
{
public:
  U2FTask(const nsAString& aOrigin,
          const nsAString& aAppId);

  nsString mOrigin;
  nsString mAppId;

  virtual
  void ReturnError(ErrorCode code) = 0;

protected:
  virtual ~U2FTask();
};

class U2FRegisterTask final : public nsNSSShutDownObject,
                              public U2FTask
{
public:
  U2FRegisterTask(const nsAString& aOrigin,
                  const nsAString& aAppId,
                  const Sequence<RegisterRequest>& aRegisterRequests,
                  const Sequence<RegisteredKey>& aRegisteredKeys,
                  U2FRegisterCallback* aCallback,
                  const Sequence<Authenticator>& aAuthenticators);

  // No NSS resources to release.
  virtual
  void virtualDestroyNSSReference() override {};

  void ReturnError(ErrorCode code) override;

  NS_DECL_NSIRUNNABLE
private:
  ~U2FRegisterTask();

  Sequence<RegisterRequest> mRegisterRequests;
  Sequence<RegisteredKey> mRegisteredKeys;
  RefPtr<U2FRegisterCallback> mCallback;
  Sequence<Authenticator> mAuthenticators;
};

class U2FSignTask final : public nsNSSShutDownObject,
                          public U2FTask
{
public:
  U2FSignTask(const nsAString& aOrigin,
              const nsAString& aAppId,
              const nsAString& aChallenge,
              const Sequence<RegisteredKey>& aRegisteredKeys,
              U2FSignCallback* aCallback,
              const Sequence<Authenticator>& aAuthenticators);

  // No NSS resources to release.
  virtual
  void virtualDestroyNSSReference() override {};

  void ReturnError(ErrorCode code) override;

  NS_DECL_NSIRUNNABLE
private:
  ~U2FSignTask();

  nsString mChallenge;
  Sequence<RegisteredKey> mRegisteredKeys;
  RefPtr<U2FSignCallback> mCallback;
  Sequence<Authenticator> mAuthenticators;
};

class U2F final : public nsISupports,
                  public nsWrapperCache,
                  public nsNSSShutDownObject
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

  ~U2F();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2F_h
