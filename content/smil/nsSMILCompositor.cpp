/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILCompositor.h"
#include "nsSMILCSSProperty.h"
#include "nsCSSProps.h"
#include "nsHashKeys.h"

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
    NS_PTR_TO_UINT32(aKey->mAttributeName.get()) +
    (aKey->mIsCSS ? 1 : 0);
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
nsSMILCompositor::ComposeAttribute()
{
  if (!mKey.mElement)
    return;

  // FIRST: Get the nsISMILAttr (to grab base value from, and to eventually
  // give animated value to)
  nsAutoPtr<nsISMILAttr> smilAttr(CreateSMILAttr());
  if (!smilAttr) {
    // Target attribute not found (or, out of memory)
    return;
  }
  if (mAnimationFunctions.IsEmpty()) {
    // No active animation functions. (We can still have a nsSMILCompositor in
    // that case if an animation function has *just* become inactive)
    smilAttr->ClearAnimValue();
    return;
  }

  // SECOND: Sort the animationFunctions, to prepare for compositing.
  nsSMILAnimationFunction::Comparator comparator;
  mAnimationFunctions.Sort(comparator);

  // THIRD: Step backwards through animation functions to find out
  // which ones we actually care about.
  PRUint32 firstFuncToCompose = GetFirstFuncToAffectSandwich();

  // FOURTH: Get & cache base value
  nsSMILValue sandwichResultValue;
  if (!mAnimationFunctions[firstFuncToCompose]->WillReplace()) {
    sandwichResultValue = smilAttr->GetBaseValue();
  }
  UpdateCachedBaseValue(sandwichResultValue);

  if (!mForceCompositing) {
    return;
  }

  // FIFTH: Compose animation functions
  PRUint32 length = mAnimationFunctions.Length();
  for (PRUint32 i = firstFuncToCompose; i < length; ++i) {
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

  nsAutoPtr<nsISMILAttr> smilAttr(CreateSMILAttr());
  if (!smilAttr) {
    // Target attribute not found (or, out of memory)
    return;
  }
  smilAttr->ClearAnimValue();
}

// Protected Helper Functions
// --------------------------
nsISMILAttr*
nsSMILCompositor::CreateSMILAttr()
{
  if (mKey.mIsCSS) {
    nsCSSProperty propId =
      nsCSSProps::LookupProperty(nsDependentAtomString(mKey.mAttributeName),
                                 nsCSSProps::eEnabled);
    if (nsSMILCSSProperty::IsPropertyAnimatable(propId)) {
      return new nsSMILCSSProperty(propId, mKey.mElement.get());
    }
  } else {
    return mKey.mElement->GetAnimatedAttr(mKey.mAttributeNamespaceID,
                                          mKey.mAttributeName);
  }
  return nsnull;
}

PRUint32
nsSMILCompositor::GetFirstFuncToAffectSandwich()
{
  PRUint32 i;
  for (i = mAnimationFunctions.Length(); i > 0; --i) {
    nsSMILAnimationFunction* curAnimFunc = mAnimationFunctions[i-1];
    // In the following, the lack of short-circuit behavior of |= means that we
    // will ALWAYS run UpdateCachedTarget (even if mForceCompositing is true)
    // but only call HasChanged and WasSkippedInPrevSample if necessary.  This
    // is important since we need UpdateCachedTarget to run in order to detect
    // changes to the target in subsequent samples.
    mForceCompositing |=
      curAnimFunc->UpdateCachedTarget(mKey) ||
      curAnimFunc->HasChanged() ||
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
    for (PRUint32 j = i; j > 0; --j) {
      mAnimationFunctions[j-1]->SetWasSkipped();
    }
  }
  return i;
}

void
nsSMILCompositor::UpdateCachedBaseValue(const nsSMILValue& aBaseValue)
{
  if (!mCachedBaseValue) {
    // We don't have last sample's base value cached. Assume it's changed.
    mCachedBaseValue = new nsSMILValue(aBaseValue);
    NS_WARN_IF_FALSE(mCachedBaseValue, "failed to cache base value (OOM?)");
    mForceCompositing = true;
  } else if (*mCachedBaseValue != aBaseValue) {
    // Base value has changed since last sample.
    *mCachedBaseValue = aBaseValue;
    mForceCompositing = true;
  }
}
