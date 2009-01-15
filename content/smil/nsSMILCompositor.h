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

#include "nsIContent.h"
#include "nsTHashtable.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsSMILAnimationFunction.h"
#include "nsSMILCompositorTable.h"
#include "pldhash.h"

//----------------------------------------------------------------------
// nsSMILCompositorKey
//
// Hash Key: Animated Element, Attribute Name & Type (CSS vs. XML)
//
// NOTE: Need a nsRefPtr to the element, because nsSMILCompositors are kept
// around for 1 sample after they're used, and they need to make sure their
// target isn't deleted.

struct nsSMILCompositorKey
{
  PRBool    Equals(const nsSMILCompositorKey &aOther) const;

  nsRefPtr<nsIContent> mElement;
  nsRefPtr<nsIAtom>    mAttributeName; // XXX need to consider namespaces here
  PRPackedBool         mIsCSS;
};

//----------------------------------------------------------------------
// nsSMILCompositor
//
// Performs the composition of the animation sandwich by combining the results
// of a series animation functions according to the rules of SMIL composition
// including prioritising animations.

class nsSMILCompositor : public PLDHashEntryHdr
{
public:
  typedef const nsSMILCompositorKey& KeyType;
  typedef const nsSMILCompositorKey* KeyTypePointer;

  nsSMILCompositor(KeyTypePointer aKey) : mKey(*aKey) { }
  nsSMILCompositor(const nsSMILCompositor& toCopy)
    : mKey(toCopy.mKey),
      mAnimationFunctions(toCopy.mAnimationFunctions)
  { }
  ~nsSMILCompositor() { }

  // PLDHashEntryHdr methods
  KeyType GetKey() const { return mKey; }
  PRBool KeyEquals(KeyTypePointer aKey) const;
  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey);
  enum { ALLOW_MEMMOVE = PR_FALSE };

  // Adds the given animation function to this Compositor's list of functions
  void AddAnimationFunction(nsSMILAnimationFunction* aFunc);
  
  // Calls ComposeAttribute() on each nsSMILCompositor in the given hashset
  static void ComposeAttributes(nsSMILCompositorTable& aCompositorTable);

  // Cycle-collection support
  void Traverse(nsCycleCollectionTraversalCallback* aCallback);

 private:
  // Composes the attribute's current value with the list of animation
  // functions, and assigns the resulting value to this compositor's target
  // attribute.
  void ComposeAttribute();

  // Static callback methods
  PR_STATIC_CALLBACK(PLDHashOperator) DoComposeAttribute(
      nsSMILCompositor* aCompositor, void *aData);

  // The hash key (element, attribute name, isCSS)
  nsSMILCompositorKey  mKey;

  // Hash Value: List of animation functions that animate the specified
  // attribute
  // ---------------------------------------------------------------
  nsTArray<nsSMILAnimationFunction*> mAnimationFunctions;
};

#endif // NS_SMILCOMPOSITOR_H_
