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
#include "nsHashKeys.h"

// nsSMILCompositorKey methods
inline PRBool
nsSMILCompositorKey::Equals(const nsSMILCompositorKey &aOther) const
{
  return (aOther.mElement       == mElement &&
          aOther.mAttributeName == mAttributeName &&
          aOther.mIsCSS         == mIsCSS);
}

// PLDHashEntryHdr methods
PRBool
nsSMILCompositor::KeyEquals(KeyTypePointer aKey) const
{
  return aKey && aKey->Equals(mKey);
}

/*static*/ PLDHashNumber
nsSMILCompositor::HashKey(KeyTypePointer aKey)
{
  // Combine the 3 values into one numeric value, which will be hashed
  const char *attrName = nsnull;
  aKey->mAttributeName->GetUTF8String(&attrName);
  return NS_PTR_TO_UINT32(aKey->mElement.get()) +
    HashString(attrName) +
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
  nsAutoPtr<nsISMILAttr> smilAttr;
  if (mKey.mIsCSS) {
    // XXX Look up style system for the CSS property. The set of CSS properties
    // should be the same for all elements so we don't need to query the element
    // itself.
  } else {
    smilAttr = mKey.mElement->GetAnimatedAttr(mKey.mAttributeName);
  }

  if (!smilAttr) {
    // Target attribute not found
    return;
  }

  // SECOND: Sort the animationFunctions, to prepare for compositing.
  nsSMILAnimationFunction::Comparator comparator;
  mAnimationFunctions.Sort(comparator);

  // THIRD: Step backwards through animation functions to find out
  // which ones we actually care about.
  //  PRBool changed = PR_FALSE; // XXXdholbert removing until we have
                                 //  HasChangedTarget
  PRUint32 length = mAnimationFunctions.Length();
  PRUint32 i;
  for (i = length; i > 0; --i) {
    nsSMILAnimationFunction* curAnimFunc = mAnimationFunctions[i-1];
    // XXXdholbert we need to add another function
    // nsSMILAnimationFunction::HasChangedTarget(elem, smilAttr, isCSS) that
    // we call here (in addition to HasChanged(), because even if function
    // value hasn't changed, its target might have.
    // For this to work, the nsSMILAnimationFunction needs to cache its last
    // elem/smilAttr/isCSS values, and then check them against the new values
    // here.
    /*
    if (!changed && curAnimFunc->HasChanged()) {
      changed = PR_TRUE;
    }
    */
    
    if (curAnimFunc->WillReplace()) {
      --i;
      break;
    }
  }
  // NOTE: 'i' is now the index of the first animation function that we need
  // to use in compositing.

  // if (!changed) // XXXdholbert removing until we have HasChangedTarget
  //  return;

  // FOURTH: Compose animation functions (starting with base value)
  nsSMILValue resultValue = smilAttr->GetBaseValue();
  if (resultValue.IsNull()) {
    NS_WARNING("nsISMILAttr::GetBaseValue failed");
    return;
  }
  for (; i < length; ++i) {
    nsSMILAnimationFunction* curAnimFunc = mAnimationFunctions[i];
    if (curAnimFunc) {
      curAnimFunc->ComposeResult(*smilAttr, resultValue);
    }
  }

  // FIFTH: Set the animated value to the final composited result.
  nsresult rv = smilAttr->SetAnimValue(resultValue);
  if (NS_FAILED(rv)) {
    NS_WARNING("nsISMILAttr::SetAnimValue failed");
  } 
}

/*static*/ void
nsSMILCompositor::ComposeAttributes(nsSMILCompositorTable& aCompositorTable)
{
  aCompositorTable.EnumerateEntries(DoComposeAttribute, nsnull);
}

/*static*/ PR_CALLBACK PLDHashOperator
nsSMILCompositor::DoComposeAttribute(nsSMILCompositor* aCompositor,
                                     void* /*aData*/)
{ 
  aCompositor->ComposeAttribute();
  return PL_DHASH_NEXT;
}
