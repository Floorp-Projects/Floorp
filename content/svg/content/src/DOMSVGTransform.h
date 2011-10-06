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

#ifndef MOZILLA_DOMSVGTRANSFORM_H__
#define MOZILLA_DOMSVGTRANSFORM_H__

#include "nsIDOMSVGTransform.h"
#include "DOMSVGTransformList.h"
#include "SVGTransform.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"

// We make DOMSVGTransform a pseudo-interface to allow us to QI to it in order
// to check that the objects that scripts pass in are our our *native* transform
// objects.
//
// {0A799862-9469-41FE-B4CD-2019E65D8DA6}
#define MOZILLA_DOMSVGTRANSFORM_IID \
  { 0x0A799862, 0x9469, 0x41FE, \
    { 0xB4, 0xCD, 0x20, 0x19, 0xE6, 0x5D, 0x8D, 0xA6 } }

#define MOZ_SVG_LIST_INDEX_BIT_COUNT 22 // supports > 4 million list items

namespace mozilla {

class DOMSVGMatrix;

/**
 * DOM wrapper for an SVG transform. See DOMSVGLength.h.
 */
class DOMSVGTransform : public nsIDOMSVGTransform
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOMSVGTRANSFORM_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGTransform)
  NS_DECL_NSIDOMSVGTRANSFORM

  /**
   * Generic ctor for DOMSVGTransform objects that are created for an attribute.
   */
  DOMSVGTransform(DOMSVGTransformList *aList,
                  PRUint32 aListIndex,
                  PRBool aIsAnimValItem);

  /**
   * Ctors for creating the objects returned by:
   *   SVGSVGElement.createSVGTransform(),
   *   SVGSVGElement.createSVGTransformFromMatrix(in SVGMatrix matrix),
   *   SVGTransformList.createSVGTransformFromMatrix(in SVGMatrix matrix)
   * which do not initially belong to an attribute.
   */
  DOMSVGTransform();
  DOMSVGTransform(const gfxMatrix &aMatrix);

  /**
   * Ctor for creating an unowned copy. Used with Clone().
   */
  DOMSVGTransform(const SVGTransform &aMatrix);

  ~DOMSVGTransform() {
    // Our matrix tear-off pointer should be cleared before we are destroyed
    // (since matrix tear-offs keep an owning reference to their transform, and
    // clear the tear-off pointer themselves if unlinked).
    NS_ABORT_IF_FALSE(!mMatrixTearoff, "Matrix tear-off pointer not cleared."
        " Transform being destroyed before matrix?");
    // Our mList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mList is null.
    if (mList) {
      mList->mItems[mListIndex] = nsnull;
    }
  };

  /**
   * Create an unowned copy of an owned transform. The caller is responsible for
   * the first AddRef().
   */
  DOMSVGTransform* Clone() {
    NS_ASSERTION(mList, "unexpected caller");
    return new DOMSVGTransform(InternalItem());
  }

  PRBool IsInList() const {
    return !!mList;
  }

  /**
   * In future, if this class is used for non-list transforms, this will be
   * different to IsInList().
   */
  PRBool HasOwner() const {
    return !!mList;
  }

  /**
   * This method is called to notify this DOM object that it is being inserted
   * into a list, and give it the information it needs as a result.
   *
   * This object MUST NOT already belong to a list when this method is called.
   * That's not to say that script can't move these DOM objects between
   * lists - it can - it's just that the logic to handle that (and send out
   * the necessary notifications) is located elsewhere (in
   * DOMSVGTransformList).)
   */
  void InsertingIntoList(DOMSVGTransformList *aList,
                         PRUint32 aListIndex,
                         PRBool aIsAnimValItem);

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
   * internal counterpart's values. (If it didn't do this, then it would
   * "lose" its value on being removed.)
   */
  void RemovingFromList();

  SVGTransform ToSVGTransform() const {
    return Transform();
  }

protected:
  // Interface for DOMSVGMatrix's use
  friend class DOMSVGMatrix;
  const PRBool IsAnimVal() const {
    return mIsAnimValItem;
  }
  const gfxMatrix& Matrix() const {
    return Transform().Matrix();
  }
  void SetMatrix(const gfxMatrix& aMatrix);
  void ClearMatrixTearoff(DOMSVGMatrix* aMatrix);

private:
  nsSVGElement* Element() {
    return mList->Element();
  }

  /**
   * Get a reference to the internal SVGTransform list item that this DOM
   * wrapper object currently wraps.
   */
  SVGTransform& InternalItem();
  const SVGTransform& InternalItem() const;

#ifdef DEBUG
  PRBool IndexIsValid();
#endif

  const SVGTransform& Transform() const {
    return HasOwner() ? InternalItem() : *mTransform;
  }
  SVGTransform& Transform() {
    return HasOwner() ? InternalItem() : *mTransform;
  }
  void NotifyElementOfChange();

  nsRefPtr<DOMSVGTransformList> mList;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  PRUint32 mListIndex:MOZ_SVG_LIST_INDEX_BIT_COUNT;
  PRPackedBool mIsAnimValItem:1;

  // Usually this class acts as a wrapper for an SVGTransform object which is
  // part of a list and is accessed by going via the owning Element.
  //
  // However, in some circumstances, objects of this class may not be associated
  // with any particular list and thus, no internal SVGTransform object. In
  // that case we allocate an SVGTransform object on the heap to store the data.
  nsAutoPtr<SVGTransform> mTransform;

  // Weak ref to DOMSVGMatrix tearoff. The DOMSVGMatrix object will take of
  // clearing this pointer when it is destroyed (by calling ClearMatrixTearoff).
  //
  // If this extra pointer member proves undesirable, it can be replaced with
  // a hashmap (nsSVGAttrTearoffTable) to map from DOMSVGTransform to
  // DOMSVGMatrix.
  DOMSVGMatrix* mMatrixTearoff;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMSVGTransform, MOZILLA_DOMSVGTRANSFORM_IID)

} // namespace mozilla

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT

#endif // MOZILLA_DOMSVGTRANSFORM_H__
