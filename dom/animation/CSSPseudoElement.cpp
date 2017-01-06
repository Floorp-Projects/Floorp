/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSPseudoElement.h"
#include "mozilla/dom/CSSPseudoElementBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/AnimationComparator.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CSSPseudoElement, mParentElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CSSPseudoElement, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CSSPseudoElement, Release)

CSSPseudoElement::CSSPseudoElement(Element* aElement,
                                   CSSPseudoElementType aType)
  : mParentElement(aElement)
  , mPseudoType(aType)
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aType == CSSPseudoElementType::after ||
             aType == CSSPseudoElementType::before,
             "Unexpected Pseudo Type");
}

CSSPseudoElement::~CSSPseudoElement()
{
  // Element might have been unlinked already, so we have to do null check.
  if (mParentElement) {
    mParentElement->DeleteProperty(
      GetCSSPseudoElementPropertyAtom(mPseudoType));
  }
}

ParentObject
CSSPseudoElement::GetParentObject() const
{
  return mParentElement->GetParentObject();
}

JSObject*
CSSPseudoElement::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CSSPseudoElementBinding::Wrap(aCx, this, aGivenProto);
}

void
CSSPseudoElement::GetAnimations(const AnimationFilter& filter,
                                nsTArray<RefPtr<Animation>>& aRetVal)
{
  nsIDocument* doc = mParentElement->GetComposedDoc();
  if (doc) {
    doc->FlushPendingNotifications(FlushType::Style);
  }

  Element::GetAnimationsUnsorted(mParentElement, mPseudoType, aRetVal);
  aRetVal.Sort(AnimationPtrComparator<RefPtr<Animation>>());
}

already_AddRefed<Animation>
CSSPseudoElement::Animate(
    JSContext* aContext,
    JS::Handle<JSObject*> aKeyframes,
    const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
    ErrorResult& aError)
{
  Nullable<ElementOrCSSPseudoElement> target;
  target.SetValue().SetAsCSSPseudoElement() = this;
  return Element::Animate(target, aContext, aKeyframes, aOptions, aError);
}

/* static */ already_AddRefed<CSSPseudoElement>
CSSPseudoElement::GetCSSPseudoElement(Element* aElement,
                                      CSSPseudoElementType aType)
{
  if (!aElement) {
    return nullptr;
  }

  nsIAtom* propName = CSSPseudoElement::GetCSSPseudoElementPropertyAtom(aType);
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

/* static */ nsIAtom*
CSSPseudoElement::GetCSSPseudoElementPropertyAtom(CSSPseudoElementType aType)
{
  switch (aType) {
    case CSSPseudoElementType::before:
      return nsGkAtoms::cssPseudoElementBeforeProperty;

    case CSSPseudoElementType::after:
      return nsGkAtoms::cssPseudoElementAfterProperty;

    default:
      NS_NOTREACHED("Should not try to get CSSPseudoElement "
                    "other than ::before or ::after");
      return nullptr;
  }
}

} // namespace dom
} // namespace mozilla
