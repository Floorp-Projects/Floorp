/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *   Brian Birtles <birtles@gmail.com>
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

#ifndef MOZILLA_DOMSVGTRANSFORMLIST_H__
#define MOZILLA_DOMSVGTRANSFORMLIST_H__

#include "DOMSVGAnimatedTransformList.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsIDOMSVGTransformList.h"
#include "nsTArray.h"
#include "SVGTransformList.h"

class nsIDOMSVGTransform;
class nsSVGElement;

namespace mozilla {

class DOMSVGTransform;

/**
 * Class DOMSVGTransformList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * SVGTransformList objects.
 *
 * See the architecture comment in DOMSVGAnimatedTransformList.h.
 */
class DOMSVGTransformList : public nsIDOMSVGTransformList
{
  friend class DOMSVGTransform;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGTransformList)
  NS_DECL_NSIDOMSVGTRANSFORMLIST

  DOMSVGTransformList(DOMSVGAnimatedTransformList *aAList,
                      const SVGTransformList &aInternalList)
    : mAList(aAList)
  {
    // aInternalList must be passed in explicitly because we can't use
    // InternalList() here. (Because it depends on IsAnimValList, which depends
    // on this object having been assigned to aAList's mBaseVal or mAnimVal,
    // which hasn't happend yet.)

    InternalListLengthWillChange(aInternalList.Length()); // Sync mItems
  }

  ~DOMSVGTransformList() {
    // Our mAList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mAList is null.
    if (mAList) {
      ( IsAnimValList() ? mAList->mAnimVal : mAList->mBaseVal ) = nsnull;
    }
  }

  /**
   * This will normally be the same as InternalList().Length(), except if we've
   * hit OOM in which case our length will be zero.
   */
  PRUint32 Length() const {
    NS_ABORT_IF_FALSE(mItems.IsEmpty() ||
      mItems.Length() == InternalList().Length(),
      "DOM wrapper's list length is out of sync");
    return mItems.Length();
  }

  nsIDOMSVGTransform* GetItemWithoutAddRef(PRUint32 aIndex);

  /// Called to notify us to synchronize our length and detach excess items.
  void InternalListLengthWillChange(PRUint32 aNewLength);

private:

  nsSVGElement* Element() const {
    return mAList->mElement;
  }

  /// Used to determine if this list is the baseVal or animVal list.
  bool IsAnimValList() const {
    NS_ABORT_IF_FALSE(this == mAList->mBaseVal || this == mAList->mAnimVal,
                      "Calling IsAnimValList() too early?!");
    return this == mAList->mAnimVal;
  }

  /**
   * Get a reference to this object's corresponding internal SVGTransformList.
   *
   * To simplify the code we just have this one method for obtaining both
   * baseVal and animVal internal lists. This means that animVal lists don't
   * get const protection, but our setter methods guard against changing
   * animVal lists.
   */
  SVGTransformList& InternalList() const;

  /// Creates a DOMSVGTransform for aIndex, if it doesn't already exist.
  void EnsureItemAt(PRUint32 aIndex);

  void MaybeInsertNullInAnimValListAt(PRUint32 aIndex);
  void MaybeRemoveItemFromAnimValListAt(PRUint32 aIndex);

  // Weak refs to our DOMSVGTransform items. The items are friends and take care
  // of clearing our pointer to them when they die.
  nsTArray<DOMSVGTransform*> mItems;

  nsRefPtr<DOMSVGAnimatedTransformList> mAList;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGTRANSFORMLIST_H__
