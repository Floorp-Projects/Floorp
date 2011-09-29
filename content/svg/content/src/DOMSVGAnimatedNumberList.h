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

#ifndef MOZILLA_DOMSVGANIMATEDNUMBERLIST_H__
#define MOZILLA_DOMSVGANIMATEDNUMBERLIST_H__

#include "nsIDOMSVGAnimatedNumberList.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

class nsSVGElement;

namespace mozilla {

class SVGAnimatedNumberList;
class SVGNumberList;
class DOMSVGNumberList;

/**
 * Class DOMSVGAnimatedNumberList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * SVGAnimatedNumberList objects.
 *
 * See the architecture comment in DOMSVGAnimatedLengthList.h (that's
 * LENGTH list). The comment for that class largly applies to this one too
 * and will go a long way to helping you understand the architecture here.
 *
 * This class is strongly intertwined with DOMSVGNumberList and DOMSVGNumber.
 * Our DOMSVGNumberList base and anim vals are friends and take care of nulling
 * out our pointers to them when they die (making our pointers to them true
 * weak refs).
 */
class DOMSVGAnimatedNumberList : public nsIDOMSVGAnimatedNumberList
{
  friend class DOMSVGNumberList;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGAnimatedNumberList)
  NS_DECL_NSIDOMSVGANIMATEDNUMBERLIST

  /**
   * Factory method to create and return a DOMSVGAnimatedNumberList wrapper
   * for a given internal SVGAnimatedNumberList object. The factory takes care
   * of caching the object that it returns so that the same object can be
   * returned for the given SVGAnimatedNumberList each time it is requested.
   * The cached object is only removed from the cache when it is destroyed due
   * to there being no more references to it or to any of its descendant
   * objects. If that happens, any subsequent call requesting the DOM wrapper
   * for the SVGAnimatedNumberList will naturally result in a new
   * DOMSVGAnimatedNumberList being returned.
   */
  static already_AddRefed<DOMSVGAnimatedNumberList>
    GetDOMWrapper(SVGAnimatedNumberList *aList,
                  nsSVGElement *aElement,
                  PRUint8 aAttrEnum);

  /**
   * This method returns the DOMSVGAnimatedNumberList wrapper for an internal
   * SVGAnimatedNumberList object if it currently has a wrapper. If it does
   * not, then nsnull is returned.
   */
  static DOMSVGAnimatedNumberList*
    GetDOMWrapperIfExists(SVGAnimatedNumberList *aList);

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
  void InternalBaseValListWillChangeTo(const SVGNumberList& aNewValue);
  void InternalAnimValListWillChangeTo(const SVGNumberList& aNewValue);

  /**
   * Returns true if our attribute is animating (in which case our animVal is
   * not simply a mirror of our baseVal).
   */
  bool IsAnimating() const;

private:

  /**
   * Only our static GetDOMWrapper() factory method may create objects of our
   * type.
   */
  DOMSVGAnimatedNumberList(nsSVGElement *aElement, PRUint8 aAttrEnum)
    : mBaseVal(nsnull)
    , mAnimVal(nsnull)
    , mElement(aElement)
    , mAttrEnum(aAttrEnum)
  {}

  ~DOMSVGAnimatedNumberList();

  /// Get a reference to this DOM wrapper object's internal counterpart.
  SVGAnimatedNumberList& InternalAList();
  const SVGAnimatedNumberList& InternalAList() const;

  // Weak refs to our DOMSVGNumberList baseVal/animVal objects. These objects
  // are friends and take care of clearing these pointers when they die, making
  // these true weak references.
  DOMSVGNumberList *mBaseVal;
  DOMSVGNumberList *mAnimVal;

  // Strong ref to our element to keep it alive. We hold this not only for
  // ourself, but also for our base/animVal and all of their items.
  nsRefPtr<nsSVGElement> mElement;

  PRUint8 mAttrEnum;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGANIMATEDNUMBERLIST_H__
