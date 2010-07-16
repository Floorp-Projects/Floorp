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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef MOZILLA_DOMSVGLENGTHLIST_H__
#define MOZILLA_DOMSVGLENGTHLIST_H__

#include "nsIDOMSVGLengthList.h"
#include "SVGLengthList.h"
#include "SVGLength.h"
#include "DOMSVGAnimatedLengthList.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"

class nsSVGElement;

namespace mozilla {

class DOMSVGLength;

/**
 * Class DOMSVGLengthList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * SVGLengthList objects.
 *
 * See the architecture comment in DOMSVGAnimatedLengthList.h.
 *
 * This class is strongly intertwined with DOMSVGAnimatedLengthList and
 * DOMSVGLength. We are a friend of DOMSVGAnimatedLengthList, and are
 * responsible for nulling out our DOMSVGAnimatedLengthList's pointer to us
 * when we die, essentially making its pointer to us a weak pointer. Similarly,
 * our DOMSVGLength items are friends of us and responsible for nulling out our
 * pointers to them.
 *
 * Our DOM items are created lazily on demand as and when script requests them.
 */
class DOMSVGLengthList : public nsIDOMSVGLengthList
{
  friend class DOMSVGLength;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGLengthList)
  NS_DECL_NSIDOMSVGLENGTHLIST

  DOMSVGLengthList(DOMSVGAnimatedLengthList *aAList)
    : mAList(aAList)
  {
    // We silently ignore SetLength OOM failure since being out of sync is safe
    // so long as we have *fewer* items than our internal list.

    mItems.SetLength(InternalList().Length());
    for (PRUint32 i = 0; i < Length(); ++i) {
      // null out all the pointers - items are created on-demand
      mItems[i] = nsnull;
    }
  }

  ~DOMSVGLengthList() {
    // Our mAList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mAList is null.
    if (mAList) {
      ( IsAnimValList() ? mAList->mAnimVal : mAList->mBaseVal ) = nsnull;
    }
  };

  /**
   * This will normally be the same as InternalList().Length(), except if we've
   * hit OOM in which case our length will be zero.
   */
  PRUint32 Length() const {
    NS_ABORT_IF_FALSE(mItems.Length() == 0 ||
                      mItems.Length() ==
                        const_cast<DOMSVGLengthList*>(this)->InternalList().Length(),
                      "DOM wrapper's list length is out of sync");
    return mItems.Length();
  }

  /// Called to notify us to syncronize our length and detach excess items.
  void InternalListLengthWillChange(PRUint32 aNewLength);

private:

  nsSVGElement* Element() {
    return mAList->mElement;
  }

  PRUint8 AttrEnum() const {
    return mAList->mAttrEnum;
  }

  PRUint8 Axis() const {
    return mAList->mAxis;
  }

  /// Used to determine if this list is the baseVal or animVal list.
  PRBool IsAnimValList() const {
    return this == mAList->mAnimVal;
  }

  /**
   * Get a reference to this object's corresponding internal SVGLengthList.
   *
   * To simplyfy the code we just have this one method for obtaining both
   * baseVal and animVal internal lists. This means that animVal lists don't
   * get const protection, but our setter methods guard against changing
   * animVal lists.
   */
  SVGLengthList& InternalList();

  /// Creates a DOMSVGLength for aIndex, if it doesn't already exist.
  void EnsureItemAt(PRUint32 aIndex);

  // Weak refs to our DOMSVGLength items. The items are friends and take care
  // of clearing our pointer to them when they die.
  nsTArray<DOMSVGLength*> mItems;

  nsRefPtr<DOMSVGAnimatedLengthList> mAList;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGLENGTHLIST_H__
