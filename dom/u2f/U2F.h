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
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

#include "NSSToken.h"
#include "USBToken.h"

namespace mozilla {
namespace dom {

struct RegisterRequest;
struct RegisteredKey;
class U2FRegisterCallback;
class U2FSignCallback;

} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace dom {

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
  NSSToken mSoftToken;
  USBToken mUSBToken;

  static const nsString FinishEnrollment;
  static const nsString GetAssertion;

  ~U2F();

  nsresult
  AssembleClientData(const nsAString& aTyp,
                     const nsAString& aChallenge,
                     CryptoBuffer& aClientData) const;

  // ValidAppID determines whether the supplied FIDO AppID is valid for
  // the current FacetID, e.g., the current origin. If the supplied
  // aAppId param is null or empty, it will be filled in per the algorithm.
  // See https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-appid-and-facets.html
  // for a description of the algorithm.
  bool
  ValidAppID(/* in/out */ nsString& aAppId) const;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2F_h
