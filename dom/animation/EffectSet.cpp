/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EffectSet.h"
#include "mozilla/dom/Element.h" // For Element
#include "nsCycleCollectionNoteChild.h" // For CycleCollectionNoteChild

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

void
EffectSet::Traverse(nsCycleCollectionTraversalCallback& aCallback)
{
  for (auto iter = mEffects.Iter(); !iter.Done(); iter.Next()) {
    CycleCollectionNoteChild(aCallback, iter.Get()->GetKey(),
                             "EffectSet::mEffects[]", aCallback.Flags());
  }
}

/* static */ EffectSet*
EffectSet::GetEffectSet(dom::Element* aElement,
                        nsCSSPseudoElements::Type aPseudoType)
{
  nsIAtom* propName = GetEffectSetPropertyAtom(aPseudoType);
  return static_cast<EffectSet*>(aElement->GetProperty(propName));
}

/* static */ EffectSet*
EffectSet::GetEffectSet(const nsIFrame* aFrame)
{
  nsIContent* content = aFrame->GetContent();
  if (!content) {
    return nullptr;
  }

  nsIAtom* propName;
  if (aFrame->IsGeneratedContentFrame()) {
    nsIFrame* parent = aFrame->GetParent();
    if (parent->IsGeneratedContentFrame()) {
      return nullptr;
    }
    nsIAtom* name = content->NodeInfo()->NameAtom();
    if (name == nsGkAtoms::mozgeneratedcontentbefore) {
      propName = nsGkAtoms::animationEffectsForBeforeProperty;
    } else if (name == nsGkAtoms::mozgeneratedcontentafter) {
      propName = nsGkAtoms::animationEffectsForAfterProperty;
    } else {
      return nullptr;
    }
    content = content->GetParent();
    if (!content) {
      return nullptr;
    }
  } else {
    if (!content->MayHaveAnimations()) {
      return nullptr;
    }
    propName = nsGkAtoms::animationEffectsProperty;
  }

  return static_cast<EffectSet*>(content->GetProperty(propName));
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
                                      &EffectSet::PropertyDtor, true);
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

/* static */ nsIAtom**
EffectSet::GetEffectSetPropertyAtoms()
{
  static nsIAtom* effectSetPropertyAtoms[] =
    {
      nsGkAtoms::animationEffectsProperty,
      nsGkAtoms::animationEffectsForBeforeProperty,
      nsGkAtoms::animationEffectsForAfterProperty,
      nullptr
    };

  return effectSetPropertyAtoms;
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

void
EffectSet::AddEffect(dom::KeyframeEffectReadOnly& aEffect)
{
  mEffects.PutEntry(&aEffect);
}

void
EffectSet::RemoveEffect(dom::KeyframeEffectReadOnly& aEffect)
{
  mEffects.RemoveEntry(&aEffect);
}

} // namespace mozilla
