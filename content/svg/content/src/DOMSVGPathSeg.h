/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGPATHSEG_H__
#define MOZILLA_DOMSVGPATHSEG_H__

#include "DOMSVGPathSegList.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMSVGPathSeg.h"

class nsSVGElement;

// We make DOMSVGPathSeg a pseudo-interface to allow us to QI to it in order to
// check that the objects that scripts pass to DOMSVGPathSegList methods are
// our *native* path seg objects.
//
// {494A7566-DC26-40C8-9122-52ABD76870C4}
#define MOZILLA_DOMSVGPATHSEG_IID \
  { 0x494A7566, 0xDC26, 0x40C8, { 0x91, 0x22, 0x52, 0xAB, 0xD7, 0x68, 0x70, 0xC4 } }

#define MOZ_SVG_LIST_INDEX_BIT_COUNT 31

namespace mozilla {

/**
 * Class DOMSVGPathSeg
 *
 * This class is the base class of the classes that create the DOM objects that
 * wrap the internal path segments that are encoded in an SVGPathData. Its
 * sub-classes are also used to create the objects returned by
 * SVGPathElement.createSVGPathSegXxx().
 *
 * See the architecture comment in DOMSVGPathSegList.h for an overview of the
 * important points regarding these DOM wrapper structures.
 *
 * See the architecture comment in DOMSVGLength.h (yes, LENGTH) for an overview
 * of the important points regarding how this specific class works.
 *
 * The main differences between this class and DOMSVGLength is that we have
 * sub-classes (it does not), and the "internal counterpart" that we provide a
 * DOM wrapper for is a list of floats, not an instance of an internal class.
 */
class DOMSVGPathSeg : public nsIDOMSVGPathSeg
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOMSVGPATHSEG_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGPathSeg)
  NS_DECL_NSIDOMSVGPATHSEG

  /**
   * Unlike the other list classes, we hide our ctor (because no one should be
   * creating instances of this class directly). This factory method in exposed
   * instead to take care of creating instances of the correct sub-class.
   */
  static DOMSVGPathSeg *CreateFor(DOMSVGPathSegList *aList,
                                  PRUint32 aListIndex,
                                  bool aIsAnimValItem);

  /**
   * Create an unowned copy of this object. The caller is responsible for the
   * first AddRef()!
   */
  virtual DOMSVGPathSeg* Clone() = 0;

  bool IsInList() const {
    return !!mList;
  }

  /**
   * In future, if this class is used for non-list segments, this will be
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
   * the necessary notifications) is located elsewhere (in DOMSVGPathSegList).)
   */
  void InsertingIntoList(DOMSVGPathSegList *aList,
                         PRUint32 aListIndex,
                         bool aIsAnimValItem);

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

  /**
   * This method converts the segment to a string of floats as found in
   * SVGPathData (i.e. the first float contains the type of the segment,
   * encoded into a float, followed by its arguments in the same order as they
   * are given in the <path> element's 'd' attribute).
   */
  void ToSVGPathSegEncodedData(float *aData);

  /**
   * The type of this path segment.
   */
  virtual PRUint32 Type() const = 0;

protected:

  /**
   * Generic ctor for DOMSVGPathSeg objects that are created for an attribute.
   */
  DOMSVGPathSeg(DOMSVGPathSegList *aList,
                PRUint32 aListIndex,
                bool aIsAnimValItem);

  /**
   * Ctor for creating the objects returned by
   * SVGPathElement.createSVGPathSegXxx(), which do not initially belong to an
   * attribute.
   */
  DOMSVGPathSeg();

  virtual ~DOMSVGPathSeg() {
    // Our mList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mList is null.
    if (mList) {
      mList->ItemAt(mListIndex) = nullptr;
    }
  }

  nsSVGElement* Element() {
    return mList->Element();
  }

  /**
   * Get a reference to the internal SVGPathSeg list item that this DOM wrapper
   * object currently wraps.
   *
   * To simplify the code we just have this one method for obtaining both
   * baseVal and animVal internal items. This means that animVal items don't
   * get const protection, but then our setter methods guard against changing
   * animVal items.
   */
  float* InternalItem();

  virtual float* PtrToMemberArgs() = 0;

#ifdef DEBUG
  bool IndexIsValid();
#endif

  nsRefPtr<DOMSVGPathSegList> mList;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  PRUint32 mListIndex:MOZ_SVG_LIST_INDEX_BIT_COUNT;
  PRUint32 mIsAnimValItem:1; // PRUint32 because MSVC won't pack otherwise
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMSVGPathSeg, MOZILLA_DOMSVGPATHSEG_IID)

} // namespace mozilla

nsIDOMSVGPathSeg*
NS_NewSVGPathSegClosePath();

nsIDOMSVGPathSeg*
NS_NewSVGPathSegMovetoAbs(float x, float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegMovetoRel(float x, float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoAbs(float x, float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoRel(float x, float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicAbs(float x, float y,
                                float x1, float y1,
                                float x2, float y2);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicRel(float x, float y,
                                float x1, float y1,
                                float x2, float y2);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticAbs(float x, float y,
                                    float x1, float y1);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticRel(float x, float y,
                                    float x1, float y1);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegArcAbs(float x, float y,
                       float r1, float r2, float angle,
                       bool largeArcFlag, bool sweepFlag);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegArcRel(float x, float y,
                       float r1, float r2, float angle,
                       bool largeArcFlag, bool sweepFlag);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoHorizontalAbs(float x);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoHorizontalRel(float x);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoVerticalAbs(float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoVerticalRel(float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicSmoothAbs(float x, float y,
                                      float x2, float y2);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicSmoothRel(float x, float y,
                                      float x2, float y2);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticSmoothRel(float x, float y);

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT

#endif // MOZILLA_DOMSVGPATHSEG_H__
