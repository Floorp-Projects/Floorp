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

#ifndef MOZILLA_DOMSVGANIMATEDTRANSFORMLIST_H__
#define MOZILLA_DOMSVGANIMATEDTRANSFORMLIST_H__

#include "nsIDOMSVGAnimTransformList.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

class nsSVGElement;

namespace mozilla {

class SVGAnimatedTransformList;
class SVGTransformList;
class DOMSVGTransformList;

/**
 * Class DOMSVGAnimatedTransformList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * SVGAnimatedTransformList objects.
 *
 * See the architecture comment in DOMSVGAnimatedLengthList.h (that's
 * LENGTH list). The comment for that class largly applies to this one too
 * and will go a long way to helping you understand the architecture here.
 *
 * This class is strongly intertwined with DOMSVGTransformList and
 * DOMSVGTransform.
 * Our DOMSVGTransformList base and anim vals are friends and take care of
 * nulling out our pointers to them when they die (making our pointers to them
 * true weak refs).
 */
class DOMSVGAnimatedTransformList : public nsIDOMSVGAnimatedTransformList
{
  friend class DOMSVGTransformList;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGAnimatedTransformList)
  NS_DECL_NSIDOMSVGANIMATEDTRANSFORMLIST

  /**
   * Factory method to create and return a DOMSVGAnimatedTransformList wrapper
   * for a given internal SVGAnimatedTransformList object. The factory takes
   * care of caching the object that it returns so that the same object can be
   * returned for the given SVGAnimatedTransformList each time it is requested.
   * The cached object is only removed from the cache when it is destroyed due
   * to there being no more references to it or to any of its descendant
   * objects. If that happens, any subsequent call requesting the DOM wrapper
   * for the SVGAnimatedTransformList will naturally result in a new
   * DOMSVGAnimatedTransformList being returned.
   */
  static already_AddRefed<DOMSVGAnimatedTransformList>
    GetDOMWrapper(SVGAnimatedTransformList *aList, nsSVGElement *aElement);

  /**
   * This method returns the DOMSVGAnimatedTransformList wrapper for an internal
   * SVGAnimatedTransformList object if it currently has a wrapper. If it does
   * not, then nsnull is returned.
   */
  static DOMSVGAnimatedTransformList*
    GetDOMWrapperIfExists(SVGAnimatedTransformList *aList);

  /**
   * Called by internal code to notify us when we need to sync the length of
   * our baseVal DOM list with its internal list. This is called just prior to
   * the length of the internal baseVal list being changed so that any DOM list
   * items that need to be removed from the DOM list can first get their values
   * from their internal counterpart.
   *
   * The only time this method could fail is on OOM when trying to increase the
   * length of the DOM list. If that happens then this method simply clears the
   * list and returns. Callers just proceed as normal, and we simply accept
   * that the DOM list will be empty (until successfully set to a new value).
   */
  void InternalBaseValListWillChangeLengthTo(PRUint32 aNewLength);
  void InternalAnimValListWillChangeLengthTo(PRUint32 aNewLength);

  /**
   * Returns true if our attribute is animating (in which case our animVal is
   * not simply a mirror of our baseVal).
   */
  PRBool IsAnimating() const;

private:

  /**
   * Only our static GetDOMWrapper() factory method may create objects of our
   * type.
   */
  DOMSVGAnimatedTransformList(nsSVGElement *aElement)
    : mBaseVal(nsnull)
    , mAnimVal(nsnull)
    , mElement(aElement)
  {}

  ~DOMSVGAnimatedTransformList();

  /// Get a reference to this DOM wrapper object's internal counterpart.
  SVGAnimatedTransformList& InternalAList();
  const SVGAnimatedTransformList& InternalAList() const;

  // Weak refs to our DOMSVGTransformList baseVal/animVal objects. These objects
  // are friends and take care of clearing these pointers when they die, making
  // these true weak references.
  DOMSVGTransformList *mBaseVal;
  DOMSVGTransformList *mAnimVal;

  // Strong ref to our element to keep it alive. We hold this not only for
  // ourself, but also for our base/animVal and all of their items.
  nsRefPtr<nsSVGElement> mElement;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGANIMATEDTRANSFORMLIST_H__
