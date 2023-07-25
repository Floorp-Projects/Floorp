/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_l10n_FluentResource_h
#define mozilla_intl_l10n_FluentResource_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/FluentBinding.h"
#include "mozilla/intl/FluentBindings.h"

namespace mozilla {

namespace dom {
struct FluentTextElementItem;
}  // namespace dom

namespace intl {

class FluentResource : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(FluentResource)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(FluentResource)

  FluentResource(nsISupports* aParent, const ffi::FluentResource* aRaw);
  FluentResource(nsISupports* aParent, const nsACString& aSource);

  static already_AddRefed<FluentResource> Constructor(
      const dom::GlobalObject& aGlobal, const nsACString& aSource);

  void TextElements(nsTArray<dom::FluentTextElementItem>& aElements,
                    ErrorResult& aRv);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() const { return mParent; }

  const ffi::FluentResource* Raw() const { return mRaw; }

 protected:
  virtual ~FluentResource() = default;

  nsCOMPtr<nsISupports> mParent;
  RefPtr<const ffi::FluentResource> mRaw;
  bool mHasErrors;
};

}  // namespace intl
}  // namespace mozilla

#endif
