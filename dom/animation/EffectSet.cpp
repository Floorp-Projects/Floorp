/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EffectSet.h"
#include "mozilla/dom/Element.h" // For Element

namespace mozilla {

/* static */ void
EffectSet::PropertyDtor(void* aObject, nsIAtom* aPropertyName,
                        void* aPropertyValue, void* aData)
{
  EffectSet* effectSet = static_cast<EffectSet*>(aPropertyValue);

#ifdef DEBUG
  MOZ_ASSERT(!effectSet->mCalledPropertyDtor, "Should not call dtor twice");
  effectSet->mCalledPropertyDtor = true;
#endif

  delete effectSet;
}

/* static */ EffectSet*
EffectSet::GetEffectSet(dom::Element* aElement,
                        nsCSSPseudoElements::Type aPseudoType)
{
  nsIAtom* propName = GetEffectSetPropertyAtom(aPseudoType);
  return static_cast<EffectSet*>(aElement->GetProperty(propName));
}

/* static */ EffectSet*
EffectSet::GetOrCreateEffectSet(dom::Element* aElement,
                                nsCSSPseudoElements::Type aPseudoType)
{
  EffectSet* effectSet = GetEffectSet(aElement, aPseudoType);
  if (effectSet) {
    return effectSet;
  }

  nsIAtom* propName = GetEffectSetPropertyAtom(aPseudoType);
  effectSet = new EffectSet();

  nsresult rv = aElement->SetProperty(propName, effectSet,
                                      &EffectSet::PropertyDtor, false);
  if (NS_FAILED(rv)) {
    NS_WARNING("SetProperty failed");
    // The set must be destroyed via PropertyDtor, otherwise
    // mCalledPropertyDtor assertion is triggered in destructor.
    EffectSet::PropertyDtor(aElement, propName, effectSet, nullptr);
    return nullptr;
  }

  if (aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement) {
    aElement->SetMayHaveAnimations();
  }

  return effectSet;
}

/* static */ nsIAtom*
EffectSet::GetEffectSetPropertyAtom(nsCSSPseudoElements::Type aPseudoType)
{
  switch (aPseudoType) {
    case nsCSSPseudoElements::ePseudo_NotPseudoElement:
      return nsGkAtoms::animationEffectsProperty;

    case nsCSSPseudoElements::ePseudo_before:
      return nsGkAtoms::animationEffectsForBeforeProperty;

    case nsCSSPseudoElements::ePseudo_after:
      return nsGkAtoms::animationEffectsForAfterProperty;

    default:
      NS_NOTREACHED("Should not try to get animation effects for a pseudo "
                    "other that :before or :after");
      return nullptr;
  }
}

} // namespace mozilla
