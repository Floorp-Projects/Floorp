/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSPseudoElement.h"
#include "mozilla/dom/CSSPseudoElementBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(CSSPseudoElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CSSPseudoElement, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CSSPseudoElement, Release)

JSObject*
CSSPseudoElement::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CSSPseudoElementBinding::Wrap(aCx, this, aGivenProto);
}

void
CSSPseudoElement::GetAnimations(nsTArray<RefPtr<Animation>>& aRetVal)
{
  // Bug 1234403: Implement this API.
  NS_NOTREACHED("CSSPseudoElement::GetAnimations() is not implemented yet.");
}

already_AddRefed<Animation>
CSSPseudoElement::Animate(
    JSContext* aContext,
    JS::Handle<JSObject*> aFrames,
    const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
    ErrorResult& aError)
{
  // Bug 1241784: Implement this API.
  NS_NOTREACHED("CSSPseudoElement::Animate() is not implemented yet.");
  return nullptr;
}

} // namespace dom
} // namespace mozilla
