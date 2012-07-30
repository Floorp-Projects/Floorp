/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGLENGTH_H__
#define MOZILLA_DOMSVGLENGTH_H__

#include "DOMSVGLengthList.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsIDOMSVGLength.h"
#include "nsTArray.h"
#include "SVGLength.h"
#include "mozilla/Attributes.h"

class nsSVGElement;

// We make DOMSVGLength a pseudo-interface to allow us to QI to it in order to
// check that the objects that scripts pass to DOMSVGLengthList methods are our
// *native* length objects.
//
// {A8468350-7F7B-4976-9A7E-3765A1DADF9A}
#define MOZILLA_DOMSVGLENGTH_IID \
  { 0xA8468350, 0x7F7B, 0x4976, { 0x9A, 0x7E, 0x37, 0x65, 0xA1, 0xDA, 0xDF, 0x9A } }

#define MOZ_SVG_LIST_INDEX_BIT_COUNT 22 // supports > 4 million list items

namespace mozilla {

/**
 * Class DOMSVGLength
 *
 * This class creates the DOM objects that wrap internal SVGLength objects that
 * are in an SVGLengthList. It is also used to create the objects returned by
 * SVGSVGElement.createSVGLength().
 *
 * For the DOM wrapper classes for non-list SVGLength, see nsSVGLength2.h.
 *
 * See the architecture comment in DOMSVGAnimatedLengthList.h.
 *
 * This class is strongly intertwined with DOMSVGAnimatedLengthList and
 * DOMSVGLengthList. We are a friend of DOMSVGLengthList, and are responsible
 * for nulling out our DOMSVGLengthList's pointer to us when we die, making it
 * a real weak pointer.
 *
 * When objects of this type are in a DOMSVGLengthList they belong to an
 * attribute. While they belong to an attribute, the objects' values come from
 * their corresponding internal SVGLength objects in the internal SVGLengthList
 * objects for the attribute. Getting and setting values of a DOMSVGLength
 * requires reading and writing to its internal SVGLength. However, if the
 * DOMSVGLength is detached from its DOMSVGLengthList then it first makes a
 * copy of its internal SVGLength's value and unit so that it doesn't appear to
 * "lose" its value from script's perspective on being removed from the list.
 * This means that these DOM tearoffs have space to store these values, even
 * though they're not used in the common case.
 *
 * This class also stores its current list index, attribute enum, and whether
 * it belongs to a baseVal or animVal list. This is so that objects of this
 * type can find their corresponding internal SVGLength.
 *
 * To use these classes for <length> attributes as well as <list-of-length>
 * attributes, we would need to take a bit from mListIndex and use that to
 * indicate whether the object belongs to a list or non-list attribute, then
 * if-else as appropriate. The bug for doing that work is:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=571734
 */
class DOMSVGLength MOZ_FINAL : public nsIDOMSVGLength
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOMSVGLENGTH_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGLength)
  NS_DECL_NSIDOMSVGLENGTH

  /**
   * Generic ctor for DOMSVGLength objects that are created for an attribute.
   */
  DOMSVGLength(DOMSVGLengthList *aList,
               PRUint8 aAttrEnum,
               PRUint32 aListIndex,
               PRUint8 aIsAnimValItem);

  /**
   * Ctor for creating the objects returned by SVGSVGElement.createSVGLength(),
   * which do not initially belong to an attribute.
   */
  DOMSVGLength();

  ~DOMSVGLength() {
    // Our mList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mList is null.
    if (mList) {
      mList->mItems[mListIndex] = nullptr;
    }
  };

  /**
   * Create an unowned copy of an owned length. The caller is responsible for
   * the first AddRef().
   */
  DOMSVGLength* Copy() {
    NS_ASSERTION(mList, "unexpected caller");
    DOMSVGLength *copy = new DOMSVGLength();
    SVGLength &length = InternalItem();
    copy->NewValueSpecifiedUnits(length.GetUnit(), length.GetValueInCurrentUnits());
    return copy;
  }

  bool IsInList() const {
    return !!mList;
  }

  /**
   * In future, if this class is used for non-list lengths, this will be
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
   * the necessary notifications) is located elsewhere (in DOMSVGLengthList).)
   */
  void InsertingIntoList(DOMSVGLengthList *aList,
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
   * internal counterpart's values. (If it didn't do this, then it would
   * "lose" its value on being removed.)
   */
  void RemovingFromList();

  SVGLength ToSVGLength();

private:

  nsSVGElement* Element() {
    return mList->Element();
  }

  PRUint8 AttrEnum() const {
    return mAttrEnum;
  }

  /**
   * Get the axis that this length lies along. This method must only be called
   * when this object is associated with an element (HasOwner() returns true).
   */
  PRUint8 Axis() const {
    return mList->Axis();
  }

  /**
   * Get a reference to the internal SVGLength list item that this DOM wrapper
   * object currently wraps.
   *
   * To simplify the code we just have this one method for obtaining both
   * baseVal and animVal internal items. This means that animVal items don't
   * get const protection, but then our setter methods guard against changing
   * animVal items.
   */
  SVGLength& InternalItem();

#ifdef DEBUG
  bool IndexIsValid();
#endif

  nsRefPtr<DOMSVGLengthList> mList;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  PRUint32 mListIndex:MOZ_SVG_LIST_INDEX_BIT_COUNT;
  PRUint32 mAttrEnum:4; // supports up to 16 attributes
  PRUint32 mIsAnimValItem:1;

  // The following members are only used when we're not in a list:
  PRUint32 mUnit:5; // can handle 31 units (the 10 SVG 1.1 units + rem, vw, vh, wm, calc + future additions)
  float mValue;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMSVGLength, MOZILLA_DOMSVGLENGTH_IID)

} // namespace mozilla

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT

#endif // MOZILLA_DOMSVGLENGTH_H__
