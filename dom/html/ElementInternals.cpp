/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ElementInternals.h"
#include "mozilla/dom/ElementInternalsBinding.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ElementInternals, mTarget)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ElementInternals)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ElementInternals)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ElementInternals)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ElementInternals::ElementInternals(nsGenericHTMLElement* aTarget)
    : mTarget(aTarget) {}

nsISupports* ElementInternals::GetParentObject() { return ToSupports(mTarget); }

JSObject* ElementInternals::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return ElementInternals_Binding::Wrap(aCx, this, aGivenProto);
}

// https://html.spec.whatwg.org/#dom-elementinternals-shadowroot
ShadowRoot* ElementInternals::GetShadowRoot() const {
  MOZ_ASSERT(mTarget);

  ShadowRoot* shadowRoot = mTarget->GetShadowRoot();
  if (shadowRoot && !shadowRoot->IsAvailableToElementInternals()) {
    return nullptr;
  }

  return shadowRoot;
}

}  // namespace mozilla::dom
