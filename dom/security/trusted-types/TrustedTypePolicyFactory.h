/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEPOLICYFACTORY_H_
#define DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEPOLICYFACTORY_H_

#include "js/TypeDecls.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/TrustedHTML.h"
#include "mozilla/dom/TrustedScript.h"
#include "mozilla/dom/TrustedScriptURL.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsIGlobalObject.h"
#include "nsISupportsImpl.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

template <typename T>
struct already_AddRefed;

class DOMString;

namespace mozilla::dom {

class TrustedTypePolicy;

// https://w3c.github.io/trusted-types/dist/spec/#trusted-type-policy-factory
class TrustedTypePolicyFactory : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TrustedTypePolicyFactory)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(TrustedTypePolicyFactory)

  explicit TrustedTypePolicyFactory(nsIGlobalObject* aGlobalObject)
      : mGlobalObject{aGlobalObject} {}

  // Required for Web IDL binding.
  nsIGlobalObject* GetParentObject() const { return mGlobalObject; }

  // Required for Web IDL binding.
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicyfactory-createpolicy
  already_AddRefed<TrustedTypePolicy> CreatePolicy(
      const nsAString& aPolicyName,
      const TrustedTypePolicyOptions& aPolicyOptions);

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicyfactory-ishtml
  bool IsHTML(JSContext* aJSContext, const JS::Handle<JS::Value>& aValue) const;

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicyfactory-isscript
  bool IsScript(JSContext* aJSContext,
                const JS::Handle<JS::Value>& aValue) const;

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicyfactory-isscripturl
  bool IsScriptURL(JSContext* aJSContext,
                   const JS::Handle<JS::Value>& aValue) const;

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicyfactory-emptyhtml
  UniquePtr<TrustedHTML> EmptyHTML();

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicyfactory-emptyscript
  UniquePtr<TrustedScript> EmptyScript();

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicyfactory-getattributetype
  void GetAttributeType(const nsAString& aTagName, const nsAString& aAttribute,
                        const nsAString& aElementNs, const nsAString& aAttrNs,
                        DOMString& aResult) {
    // TODO: impl.
  }

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicyfactory-getpropertytype
  void GetPropertyType(const nsAString& aTagName, const nsAString& aProperty,
                       const nsAString& aElementNs, DOMString& aResult) {
    // TODO: impl
  }

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicyfactory-defaultpolicy
  TrustedTypePolicy* GetDefaultPolicy() const {
    // TODO: impl
    return nullptr;
  }

 private:
  // Required because this class is ref-counted.
  virtual ~TrustedTypePolicyFactory() = default;

  RefPtr<nsIGlobalObject> mGlobalObject;

  nsTArray<nsString> mCreatedPolicyNames;
};

}  // namespace mozilla::dom

#endif  // DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEPOLICYFACTORY_H_
