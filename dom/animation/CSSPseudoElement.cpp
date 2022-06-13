/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSPseudoElement.h"
#include "mozilla/dom/CSSPseudoElementBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/AnimationComparator.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CSSPseudoElement, mOriginatingElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CSSPseudoElement, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CSSPseudoElement, Release)

CSSPseudoElement::CSSPseudoElement(dom::Element* aElement,
                                   PseudoStyleType aType)
    : mOriginatingElement(aElement), mPseudoType(aType) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(AnimationUtils::IsSupportedPseudoForAnimations(aType),
             "Unexpected Pseudo Type");
}

CSSPseudoElement::~CSSPseudoElement() {
  // Element might have been unlinked already, so we have to do null check.
  if (mOriginatingElement) {
    mOriginatingElement->RemoveProperty(
        GetCSSPseudoElementPropertyAtom(mPseudoType));
  }
}

ParentObject CSSPseudoElement::GetParentObject() const {
  return mOriginatingElement->GetParentObject();
}

JSObject* CSSPseudoElement::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return CSSPseudoElement_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<CSSPseudoElement> CSSPseudoElement::GetCSSPseudoElement(
    dom::Element* aElement, PseudoStyleType aType) {
  if (!aElement) {
    return nullptr;
  }

  nsAtom* propName = CSSPseudoElement::GetCSSPseudoElementPropertyAtom(aType);
  RefPtr<CSSPseudoElement> pseudo =
      static_cast<CSSPseudoElement*>(aElement->GetProperty(propName));
  if (pseudo) {
    return pseudo.forget();
  }

  // CSSPseudoElement is a purely external interface created on-demand, and
  // when all references from script to the pseudo are dropped, we can drop the
  // CSSPseudoElement object, so use a non-owning reference from Element to
  // CSSPseudoElement.
  pseudo = new CSSPseudoElement(aElement, aType);
  nsresult rv = aElement->SetProperty(propName, pseudo, nullptr, true);
  if (NS_FAILED(rv)) {
    NS_WARNING("SetProperty failed");
    return nullptr;
  }
  return pseudo.forget();
}

/* static */
nsAtom* CSSPseudoElement::GetCSSPseudoElementPropertyAtom(
    PseudoStyleType aType) {
  switch (aType) {
    case PseudoStyleType::before:
      return nsGkAtoms::cssPseudoElementBeforeProperty;

    case PseudoStyleType::after:
      return nsGkAtoms::cssPseudoElementAfterProperty;

    case PseudoStyleType::marker:
      return nsGkAtoms::cssPseudoElementMarkerProperty;

    default:
      MOZ_ASSERT_UNREACHABLE(
          "Should not try to get CSSPseudoElement "
          "other than ::before, ::after or ::marker");
      return nullptr;
  }
}

}  // namespace mozilla::dom
