/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WrapperCachedNonISupportsTestInterface_h
#define mozilla_dom_WrapperCachedNonISupportsTestInterface_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class WrapperCachedNonISupportsTestInterface final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(
      WrapperCachedNonISupportsTestInterface)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(
      WrapperCachedNonISupportsTestInterface)

 public:
  WrapperCachedNonISupportsTestInterface() = default;

  static already_AddRefed<WrapperCachedNonISupportsTestInterface> Constructor(
      const GlobalObject& aGlobalObject);

 protected:
  ~WrapperCachedNonISupportsTestInterface() = default;

 public:
  nsISupports* GetParentObject() const { return nullptr; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_WrapperCachedNonISupportsTestInterface_h
