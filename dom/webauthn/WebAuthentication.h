/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthentication_h
#define mozilla_dom_WebAuthentication_h

#include "mozilla/dom/WebAuthenticationBinding.h"

namespace mozilla {
namespace dom {

struct Account;
class ArrayBufferViewOrArrayBuffer;
struct AssertionOptions;
struct ScopedCredentialOptions;
struct ScopedCredentialParameters;

} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace dom {

class WebAuthentication final : public nsISupports
                              , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WebAuthentication)

  explicit WebAuthentication(nsPIDOMWindowInner* aParent);

  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise>
  MakeCredential(JSContext* aCx, const Account& accountInformation,
                 const Sequence<ScopedCredentialParameters>& cryptoParameters,
                 const ArrayBufferViewOrArrayBuffer& attestationChallenge,
                 const ScopedCredentialOptions& options);

  already_AddRefed<Promise>
  GetAssertion(const ArrayBufferViewOrArrayBuffer& assertionChallenge,
               const AssertionOptions& options);
private:
  ~WebAuthentication();

  already_AddRefed<Promise> CreatePromise();
  nsresult GetOrigin(/*out*/ nsAString& aOrigin);

  nsCOMPtr<nsPIDOMWindowInner> mParent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebAuthentication_h
