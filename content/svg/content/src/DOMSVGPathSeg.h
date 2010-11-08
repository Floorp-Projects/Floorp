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

#ifndef MOZILLA_DOMSVGPATHSEG_H__
#define MOZILLA_DOMSVGPATHSEG_H__

#include "nsIDOMSVGPathSeg.h"
#include "DOMSVGPathSegList.h"
#include "SVGPathSegUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"

class nsSVGElement;

// We make DOMSVGPathSeg a pseudo-interface to allow us to QI to it in order to
// check that the objects that scripts pass to DOMSVGPathSegList methods are
// our *native* path seg objects.
//
// {494A7566-DC26-40C8-9122-52ABD76870C4}
#define MOZILLA_DOMSVGPATHSEG_IID \
  { 0x494A7566, 0xDC26, 0x40C8, { 0x91, 0x22, 0x52, 0xAB, 0xD7, 0x68, 0x70, 0xC4 } }

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
   * This convenient factory method creates instances of the correct sub-class.
   */
  static DOMSVGPathSeg *CreateFor(DOMSVGPathSegList *aList,
                                  PRUint32 aListIndex,
                                  PRBool aIsAnimValItem);

  /**
   * Create an unowned copy of this object. The caller is responsible for the
   * first AddRef()!
   */
  virtual DOMSVGPathSeg* Clone() = 0;

  PRBool IsInList() const {
    return !!mList;
  }

  /**
   * In future, if this class is used for non-list segments, this will be
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
   * the necessary notifications) is located elsewhere (in DOMSVGPathSegList).)
   */
  void InsertingIntoList(DOMSVGPathSegList *aList,
                         PRUint32 aListIndex,
                         PRBool aIsAnimValItem);

  /// This method is called to notify this object that its list index changed.
  void UpdateListIndex(PRUint8 aListIndex) {
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
                PRBool aIsAnimValItem);

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
      mList->ItemAt(mListIndex) = nsnull;
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
  PRBool IndexIsValid();
#endif

  nsRefPtr<DOMSVGPathSegList> mList;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  PRUint32 mListIndex:31;
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
                       PRBool largeArcFlag, PRBool sweepFlag);

nsIDOMSVGPathSeg*
NS_NewSVGPathSegArcRel(float x, float y,
                       float r1, float r2, float angle,
                       PRBool largeArcFlag, PRBool sweepFlag);

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

#endif // MOZILLA_DOMSVGPATHSEG_H__
