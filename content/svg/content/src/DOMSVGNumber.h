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

#ifndef MOZILLA_DOMSVGNUMBER_H__
#define MOZILLA_DOMSVGNUMBER_H__

#include "nsIDOMSVGNumber.h"
#include "DOMSVGNumberList.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"

class nsSVGElement;

// We make DOMSVGNumber a pseudo-interface to allow us to QI to it in order to
// check that the objects that scripts pass to DOMSVGNumberList methods are our
// *native* number objects.
//
// {2CA92412-2E1F-4DDB-A16C-52B3B582270D}
#define MOZILLA_DOMSVGNUMBER_IID \
  { 0x2CA92412, 0x2E1F, 0x4DDB, \
    { 0xA1, 0x6C, 0x52, 0xB3, 0xB5, 0x82, 0x27, 0x0D } }

#define MOZ_SVG_LIST_INDEX_BIT_COUNT 27 // supports > 134 million list items

namespace mozilla {

/**
 * Class DOMSVGNumber
 *
 * This class creates the DOM objects that wrap internal SVGNumber objects that
 * are in an SVGNumberList. It is also used to create the objects returned by
 * SVGSVGElement.createSVGNumber().
 *
 * For the DOM wrapper classes for non-list SVGNumber, see nsSVGNumber2.h.
 *
 * See the architecture comment in DOMSVGAnimatedNumberList.h.
 *
 * See the comment in DOMSVGLength.h (yes, LENGTH), which applies here too.
 */
class DOMSVGNumber : public nsIDOMSVGNumber
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOMSVGNUMBER_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGNumber)
  NS_DECL_NSIDOMSVGNUMBER

  /**
   * Generic ctor for DOMSVGNumber objects that are created for an attribute.
   */
  DOMSVGNumber(DOMSVGNumberList *aList,
               PRUint8 aAttrEnum,
               PRUint32 aListIndex,
               PRUint8 aIsAnimValItem);

  /**
   * Ctor for creating the objects returned by SVGSVGElement.createSVGNumber(),
   * which do not initially belong to an attribute.
   */
  DOMSVGNumber();

  ~DOMSVGNumber() {
    // Our mList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mList is null.
    if (mList) {
      mList->mItems[mListIndex] = nsnull;
    }
  };

  /**
   * Create an unowned copy. The caller is responsible for the first AddRef().
   */
  DOMSVGNumber* Clone() {
    DOMSVGNumber *clone = new DOMSVGNumber();
    clone->mValue = ToSVGNumber();
    return clone;
  }

  bool IsInList() const {
    return !!mList;
  }

  /**
   * In future, if this class is used for non-list numbers, this will be
   * different to IsInList().
   */
  bool HasOwner() const {
    return !!mList;
  }

  /**
   * This method is called to notify this DOM object that it is being inserted
   * into a list, and give it the information it needs as a result.
   *
   * This object MUST NOT already belong to a list when this method is called.
   * That's not to say that script can't move these DOM objects between
   * lists - it can - it's just that the logic to handle that (and send out
   * the necessary notifications) is located elsewhere (in DOMSVGNumberList).)
   */
  void InsertingIntoList(DOMSVGNumberList *aList,
                         PRUint8 aAttrEnum,
                         PRUint32 aListIndex,
                         PRUint8 aIsAnimValItem);

  static PRUint32 MaxListIndex() {
    return (1U << MOZ_SVG_LIST_INDEX_BIT_COUNT) - 1;
  }

  /// This method is called to notify this object that its list index changed.
  void UpdateListIndex(PRUint32 aListIndex) {
    mListIndex = aListIndex;
  }

  /**
   * This method is called to notify this DOM object that it is about to be
   * removed from its current DOM list so that it can first make a copy of its
   * internal counterpart's value. (If it didn't do this, then it would
   * "lose" its value on being removed.)
   */
  void RemovingFromList();

  float ToSVGNumber();

private:

  nsSVGElement* Element() {
    return mList->Element();
  }

  PRUint8 AttrEnum() const {
    return mAttrEnum;
  }

  /**
   * Get a reference to the internal SVGNumber list item that this DOM wrapper
   * object currently wraps.
   *
   * To simplify the code we just have this one method for obtaining both
   * baseVal and animVal internal items. This means that animVal items don't
   * get const protection, but then our setter methods guard against changing
   * animVal items.
   */
  float& InternalItem();

#ifdef DEBUG
  bool IndexIsValid();
#endif

  nsRefPtr<DOMSVGNumberList> mList;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  PRUint32 mListIndex:MOZ_SVG_LIST_INDEX_BIT_COUNT;
  PRUint32 mAttrEnum:4; // supports up to 16 attributes
  PRUint32 mIsAnimValItem:1;

  // The following member is only used when we're not in a list:
  float mValue;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMSVGNumber, MOZILLA_DOMSVGNUMBER_IID)

} // namespace mozilla

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT

#endif // MOZILLA_DOMSVGNUMBER_H__
