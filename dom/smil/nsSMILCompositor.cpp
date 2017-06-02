/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILCompositor.h"

#include "nsComputedDOMStyle.h"
#include "nsCSSProps.h"
#include "nsHashKeys.h"
#include "nsSMILCSSProperty.h"

// PLDHashEntryHdr methods
bool
nsSMILCompositor::KeyEquals(KeyTypePointer aKey) const
{
  return aKey && aKey->Equals(mKey);
}

/*static*/ PLDHashNumber
nsSMILCompositor::HashKey(KeyTypePointer aKey)
{
  // Combine the 3 values into one numeric value, which will be hashed.
  // NOTE: We right-shift one of the pointers by 2 to get some randomness in
  // its 2 lowest-order bits. (Those shifted-off bits will always be 0 since
  // our pointers will be word-aligned.)
  return (NS_PTR_TO_UINT32(aKey->mElement.get()) >> 2) +
    NS_PTR_TO_UINT32(aKey->mAttributeName.get());
}

// Cycle-collection support
void
nsSMILCompositor::Traverse(nsCycleCollectionTraversalCallback* aCallback)
{
  if (!mKey.mElement)
    return;

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback, "Compositor mKey.mElement");
  aCallback->NoteXPCOMChild(mKey.mElement);
}

// Other methods
void
nsSMILCompositor::AddAnimationFunction(nsSMILAnimationFunction* aFunc)
{
  if (aFunc) {
    mAnimationFunctions.AppendElement(aFunc);
  }
}

void
nsSMILCompositor::ComposeAttribute(bool& aMightHavePendingStyleUpdates)
{
  if (!mKey.mElement)
    return;

  // If we might need to resolve base styles, grab a suitable style context
  // for initializing our nsISMILAttr with.
  RefPtr<nsStyleContext> baseStyleContext;
  if (MightNeedBaseStyle()) {
    baseStyleContext =
      nsComputedDOMStyle::GetUnanimatedStyleContextNoFlush(mKey.mElement,
                                                           nullptr, nullptr);
  }

  // FIRST: Get the nsISMILAttr (to grab base value from, and to eventually
  // give animated value to)
  UniquePtr<nsISMILAttr> smilAttr = CreateSMILAttr(baseStyleContext);
  if (!smilAttr) {
    // Target attribute not found (or, out of memory)
    return;
  }
  if (mAnimationFunctions.IsEmpty()) {
    // No active animation functions. (We can still have a nsSMILCompositor in
    // that case if an animation function has *just* become inactive)
    smilAttr->ClearAnimValue();
    // Removing the animation effect may require a style update.
    aMightHavePendingStyleUpdates = true;
    return;
  }

  // SECOND: Sort the animationFunctions, to prepare for compositing.
  nsSMILAnimationFunction::Comparator comparator;
  mAnimationFunctions.Sort(comparator);

  // THIRD: Step backwards through animation functions to find out
  // which ones we actually care about.
  uint32_t firstFuncToCompose = GetFirstFuncToAffectSandwich();

  // FOURTH: Get & cache base value
  nsSMILValue sandwichResultValue;
  if (!mAnimationFunctions[firstFuncToCompose]->WillReplace()) {
    sandwichResultValue = smilAttr->GetBaseValue();
    MOZ_ASSERT(!sandwichResultValue.IsNull(),
               "Result of GetBaseValue should not be null");
  }
  UpdateCachedBaseValue(sandwichResultValue);

  if (!mForceCompositing) {
    return;
  }

  // FIFTH: Compose animation functions
  aMightHavePendingStyleUpdates = true;
  uint32_t length = mAnimationFunctions.Length();
  for (uint32_t i = firstFuncToCompose; i < length; ++i) {
    mAnimationFunctions[i]->ComposeResult(*smilAttr, sandwichResultValue);
  }
  if (sandwichResultValue.IsNull()) {
    smilAttr->ClearAnimValue();
    return;
  }

  // SIXTH: Set the animated value to the final composited result.
  nsresult rv = smilAttr->SetAnimValue(sandwichResultValue);
  if (NS_FAILED(rv)) {
    NS_WARNING("nsISMILAttr::SetAnimValue failed");
  }
}

void
nsSMILCompositor::ClearAnimationEffects()
{
  if (!mKey.mElement || !mKey.mAttributeName)
    return;

  UniquePtr<nsISMILAttr> smilAttr = CreateSMILAttr(nullptr);
  if (!smilAttr) {
    // Target attribute not found (or, out of memory)
    return;
  }
  smilAttr->ClearAnimValue();
}

// Protected Helper Functions
// --------------------------
UniquePtr<nsISMILAttr>
nsSMILCompositor::CreateSMILAttr(nsStyleContext* aBaseStyleContext)
{
  nsCSSPropertyID propID = GetCSSPropertyToAnimate();

  if (propID != eCSSProperty_UNKNOWN) {
    return MakeUnique<nsSMILCSSProperty>(propID, mKey.mElement.get(),
                                         aBaseStyleContext);
  }

  return mKey.mElement->GetAnimatedAttr(mKey.mAttributeNamespaceID,
                                        mKey.mAttributeName);
}

nsCSSPropertyID
nsSMILCompositor::GetCSSPropertyToAnimate() const
{
  if (mKey.mAttributeNamespaceID != kNameSpaceID_None) {
    return eCSSProperty_UNKNOWN;
  }

  nsCSSPropertyID propID =
    nsCSSProps::LookupProperty(nsDependentAtomString(mKey.mAttributeName),
                               CSSEnabledState::eForAllContent);

  if (!nsSMILCSSProperty::IsPropertyAnimatable(propID,
        mKey.mElement->OwnerDoc()->GetStyleBackendType())) {
    return eCSSProperty_UNKNOWN;
  }

  // If we are animating the 'width' or 'height' of an outer SVG
  // element we should animate it as a CSS property, but for other elements
  // (e.g. <rect>) we should animate it as a length attribute.
  // The easiest way to test for an outer SVG element, is to see if it is an
  // SVG-namespace element mapping its width/height attribute to style.
  //
  // If we have animation of 'width' or 'height' on an SVG element that is
  // NOT mapping that attributes to style then it must not be an outermost SVG
  // element so we should return eCSSProperty_UNKNOWN to indicate that we
  // should animate as an attribute instead.
  if ((mKey.mAttributeName == nsGkAtoms::width ||
       mKey.mAttributeName == nsGkAtoms::height) &&
      mKey.mElement->GetNameSpaceID() == kNameSpaceID_SVG &&
      !mKey.mElement->IsAttributeMapped(mKey.mAttributeName)) {
    return eCSSProperty_UNKNOWN;
  }

  return propID;
}

bool
nsSMILCompositor::MightNeedBaseStyle() const
{
  if (GetCSSPropertyToAnimate() == eCSSProperty_UNKNOWN) {
    return false;
  }

  // We should return true if at least one animation function might build on
  // the base value.
  for (const nsSMILAnimationFunction* func : mAnimationFunctions) {
    if (!func->WillReplace()) {
      return true;
    }
  }

  return false;
}

uint32_t
nsSMILCompositor::GetFirstFuncToAffectSandwich()
{
  // For performance reasons, we throttle most animations on elements in
  // display:none subtrees. (We can't throttle animations that target the
  // "display" property itself, though -- if we did, display:none elements
  // could never be dynamically displayed via animations.)
  // To determine whether we're in a display:none subtree, we will check the
  // element's primary frame since element in display:none subtree doesn't have
  // a primary frame. Before this process, we will construct frame when we
  // append an element to subtree. So we will not need to worry about pending
  // frame construction in this step.
  bool canThrottle = mKey.mAttributeName != nsGkAtoms::display &&
                     !mKey.mElement->GetPrimaryFrame();

  uint32_t i;
  for (i = mAnimationFunctions.Length(); i > 0; --i) {
    nsSMILAnimationFunction* curAnimFunc = mAnimationFunctions[i-1];
    // In the following, the lack of short-circuit behavior of |= means that we
    // will ALWAYS run UpdateCachedTarget (even if mForceCompositing is true)
    // but only call HasChanged and WasSkippedInPrevSample if necessary.  This
    // is important since we need UpdateCachedTarget to run in order to detect
    // changes to the target in subsequent samples.
    mForceCompositing |=
      curAnimFunc->UpdateCachedTarget(mKey) ||
      (curAnimFunc->HasChanged() && !canThrottle) ||
      curAnimFunc->WasSkippedInPrevSample();

    if (curAnimFunc->WillReplace()) {
      --i;
      break;
    }
  }

  // Mark remaining animation functions as having been skipped so if we later
  // use them we'll know to force compositing.
  // Note that we only really need to do this if something has changed
  // (otherwise we would have set the flag on a previous sample) and if
  // something has changed mForceCompositing will be true.
  if (mForceCompositing) {
    for (uint32_t j = i; j > 0; --j) {
      mAnimationFunctions[j-1]->SetWasSkipped();
    }
  }
  return i;
}

void
nsSMILCompositor::UpdateCachedBaseValue(const nsSMILValue& aBaseValue)
{
  if (mCachedBaseValue != aBaseValue) {
    // Base value has changed since last sample.
    mCachedBaseValue = aBaseValue;
    mForceCompositing = true;
  }
}
