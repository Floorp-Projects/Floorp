/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EffectSet.h"
#include "mozilla/dom/Element.h" // For Element
#include "mozilla/RestyleManager.h"
#include "nsCSSPseudoElements.h" // For CSSPseudoElementType
#include "nsCycleCollectionNoteChild.h" // For CycleCollectionNoteChild
#include "nsPresContext.h"
#include "nsLayoutUtils.h"

namespace mozilla {

/* static */ void
EffectSet::PropertyDtor(void* aObject, nsAtom* aPropertyName,
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
EffectSet::GetEffectSet(const dom::Element* aElement,
                        CSSPseudoElementType aPseudoType)
{
  if (!aElement->MayHaveAnimations()) {
    return nullptr;
  }

  nsAtom* propName = GetEffectSetPropertyAtom(aPseudoType);
  return static_cast<EffectSet*>(aElement->GetProperty(propName));
}

/* static */ EffectSet*
EffectSet::GetEffectSet(const nsIFrame* aFrame)
{
  Maybe<NonOwningAnimationTarget> target =
    EffectCompositor::GetAnimationElementAndPseudoForFrame(aFrame);

  if (!target) {
    return nullptr;
  }

  return GetEffectSet(target->mElement, target->mPseudoType);
}

/* static */ EffectSet*
EffectSet::GetOrCreateEffectSet(dom::Element* aElement,
                                CSSPseudoElementType aPseudoType)
{
  EffectSet* effectSet = GetEffectSet(aElement, aPseudoType);
  if (effectSet) {
    return effectSet;
  }

  nsAtom* propName = GetEffectSetPropertyAtom(aPseudoType);
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

  aElement->SetMayHaveAnimations();

  return effectSet;
}

/* static */ void
EffectSet::DestroyEffectSet(dom::Element* aElement,
                            CSSPseudoElementType aPseudoType)
{
  nsAtom* propName = GetEffectSetPropertyAtom(aPseudoType);
  EffectSet* effectSet =
    static_cast<EffectSet*>(aElement->GetProperty(propName));
  if (!effectSet) {
    return;
  }

  MOZ_ASSERT(!effectSet->IsBeingEnumerated(),
             "Should not destroy an effect set while it is being enumerated");
  effectSet = nullptr;

  aElement->DeleteProperty(propName);
}

void
EffectSet::UpdateAnimationGeneration(nsPresContext* aPresContext)
{
  mAnimationGeneration =
    aPresContext->RestyleManager()->GetAnimationGeneration();
}

/* static */ nsAtom**
EffectSet::GetEffectSetPropertyAtoms()
{
  static nsAtom* effectSetPropertyAtoms[] =
    {
      nsGkAtoms::animationEffectsProperty,
      nsGkAtoms::animationEffectsForBeforeProperty,
      nsGkAtoms::animationEffectsForAfterProperty,
      nullptr
    };

  return effectSetPropertyAtoms;
}

/* static */ nsAtom*
EffectSet::GetEffectSetPropertyAtom(CSSPseudoElementType aPseudoType)
{
  switch (aPseudoType) {
    case CSSPseudoElementType::NotPseudo:
      return nsGkAtoms::animationEffectsProperty;

    case CSSPseudoElementType::before:
      return nsGkAtoms::animationEffectsForBeforeProperty;

    case CSSPseudoElementType::after:
      return nsGkAtoms::animationEffectsForAfterProperty;

    default:
      NS_NOTREACHED("Should not try to get animation effects for a pseudo "
                    "other that :before or :after");
      return nullptr;
  }
}

void
EffectSet::AddEffect(dom::KeyframeEffect& aEffect)
{
  if (mEffects.Contains(&aEffect)) {
    return;
  }

  mEffects.PutEntry(&aEffect);
  MarkCascadeNeedsUpdate();
}

void
EffectSet::RemoveEffect(dom::KeyframeEffect& aEffect)
{
  if (!mEffects.Contains(&aEffect)) {
    return;
  }

  mEffects.RemoveEntry(&aEffect);
  MarkCascadeNeedsUpdate();
}

} // namespace mozilla
