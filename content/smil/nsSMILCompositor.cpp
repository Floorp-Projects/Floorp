/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
      nsCSSProps::LookupProperty(nsDependentAtomString(mKey.mAttributeName));
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
    if (curAnimFunc->UpdateCachedTarget(mKey) ||
        (!mForceCompositing && curAnimFunc->HasChanged())) {
      mForceCompositing = PR_TRUE;
    }

    if (curAnimFunc->WillReplace()) {
      --i;
      break;
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
    mForceCompositing = PR_TRUE;
  } else if (*mCachedBaseValue != aBaseValue) {
    // Base value has changed since last sample.
    *mCachedBaseValue = aBaseValue;
    mForceCompositing = PR_TRUE;
  }
}
