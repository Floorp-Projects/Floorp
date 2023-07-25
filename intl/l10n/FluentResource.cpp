/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "FluentResource.h"

using namespace mozilla::dom;

namespace mozilla {
namespace intl {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FluentResource, mParent)

FluentResource::FluentResource(nsISupports* aParent,
                               const ffi::FluentResource* aRaw)
    : mParent(aParent), mRaw(std::move(aRaw)), mHasErrors(false) {}

FluentResource::FluentResource(nsISupports* aParent, const nsACString& aSource)
    : mParent(aParent), mHasErrors(false) {
  mRaw = dont_AddRef(ffi::fluent_resource_new(&aSource, &mHasErrors));
}

already_AddRefed<FluentResource> FluentResource::Constructor(
    const GlobalObject& aGlobal, const nsACString& aSource) {
  RefPtr<FluentResource> res =
      new FluentResource(aGlobal.GetAsSupports(), aSource);

  if (res->mHasErrors) {
    nsContentUtils::LogSimpleConsoleError(
        u"Errors encountered while parsing Fluent Resource."_ns, "chrome"_ns,
        false, true /* from chrome context*/);
  }
  return res.forget();
}

void FluentResource::TextElements(
    nsTArray<dom::FluentTextElementItem>& aElements, ErrorResult& aRv) {
  if (mHasErrors) {
    aRv.ThrowInvalidStateError("textElements don't exist due to parse error");
    return;
  }

  nsTArray<ffi::TextElementInfo> elements;
  ffi::fluent_resource_get_text_elements(mRaw, &elements);

  auto maybeAssign = [](dom::Optional<nsCString>& aDest, nsCString&& aSrc) {
    if (!aSrc.IsEmpty()) {
      aDest.Construct() = std::move(aSrc);
    }
  };

  for (auto& info : elements) {
    dom::FluentTextElementItem item;
    maybeAssign(item.mId, std::move(info.id));
    maybeAssign(item.mAttr, std::move(info.attr));
    maybeAssign(item.mText, std::move(info.text));

    aElements.AppendElement(item);
  }
}

JSObject* FluentResource::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return FluentResource_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace intl
}  // namespace mozilla
