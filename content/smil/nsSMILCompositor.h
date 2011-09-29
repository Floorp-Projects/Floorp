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

#ifndef NS_SMILCOMPOSITOR_H_
#define NS_SMILCOMPOSITOR_H_

#include "nsTHashtable.h"
#include "nsString.h"
#include "nsSMILAnimationFunction.h"
#include "nsSMILTargetIdentifier.h"
#include "nsSMILCompositorTable.h"
#include "pldhash.h"

//----------------------------------------------------------------------
// nsSMILCompositor
//
// Performs the composition of the animation sandwich by combining the results
// of a series animation functions according to the rules of SMIL composition
// including prioritising animations.

class nsSMILCompositor : public PLDHashEntryHdr
{
public:
  typedef nsSMILTargetIdentifier KeyType;
  typedef const KeyType& KeyTypeRef;
  typedef const KeyType* KeyTypePointer;

  explicit nsSMILCompositor(KeyTypePointer aKey)
   : mKey(*aKey),
     mForceCompositing(PR_FALSE)
  { }
  nsSMILCompositor(const nsSMILCompositor& toCopy)
    : mKey(toCopy.mKey),
      mAnimationFunctions(toCopy.mAnimationFunctions),
      mForceCompositing(PR_FALSE)
  { }
  ~nsSMILCompositor() { }

  // PLDHashEntryHdr methods
  KeyTypeRef GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const;
  static KeyTypePointer KeyToPointer(KeyTypeRef aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey);
  enum { ALLOW_MEMMOVE = PR_FALSE };

  // Adds the given animation function to this Compositor's list of functions
  void AddAnimationFunction(nsSMILAnimationFunction* aFunc);

  // Composes the attribute's current value with the list of animation
  // functions, and assigns the resulting value to this compositor's target
  // attribute
  void ComposeAttribute();

  // Clears animation effects on my target attribute
  void ClearAnimationEffects();

  // Cycle-collection support
  void Traverse(nsCycleCollectionTraversalCallback* aCallback);

  // Toggles a bit that will force us to composite (bypassing early-return
  // optimizations) when we hit ComposeAttribute.
  void ToggleForceCompositing() { mForceCompositing = PR_TRUE; }

  // Transfers |aOther|'s mCachedBaseValue to |this|
  void StealCachedBaseValue(nsSMILCompositor* aOther) {
    mCachedBaseValue = aOther->mCachedBaseValue;
  }

 private:
  // Create a nsISMILAttr for my target, on the heap.  Caller is responsible
  // for deallocating the returned object.
  nsISMILAttr* CreateSMILAttr();
  
  // Finds the index of the first function that will affect our animation
  // sandwich. Also toggles the 'mForceCompositing' flag if it finds that any
  // (used) functions have changed.
  PRUint32 GetFirstFuncToAffectSandwich();

  // If the passed-in base value differs from our cached base value, this
  // method updates the cached value (and toggles the 'mForceCompositing' flag)
  void UpdateCachedBaseValue(const nsSMILValue& aBaseValue);

  // Static callback methods
  PR_STATIC_CALLBACK(PLDHashOperator) DoComposeAttribute(
      nsSMILCompositor* aCompositor, void *aData);

  // The hash key (tuple of element/attributeName/attributeType)
  KeyType mKey;

  // Hash Value: List of animation functions that animate the specified attr
  nsTArray<nsSMILAnimationFunction*> mAnimationFunctions;

  // Member data for detecting when we need to force-recompose
  // ---------------------------------------------------------
  // Flag for tracking whether we need to compose. Initialized to PR_FALSE, but
  // gets flipped to PR_TRUE if we detect that something has changed.
  bool mForceCompositing;

  // Cached base value, so we can detect & force-recompose when it changes
  // from one sample to the next.  (nsSMILAnimationController copies this
  // forward from the previous sample's compositor.)
  nsAutoPtr<nsSMILValue> mCachedBaseValue;
};

#endif // NS_SMILCOMPOSITOR_H_
