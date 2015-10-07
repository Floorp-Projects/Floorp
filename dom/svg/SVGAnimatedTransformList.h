/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAnimatedTransformList_h
#define mozilla_dom_SVGAnimatedTransformList_h

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsSVGElement.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"

namespace mozilla {

class DOMSVGTransformList;
class nsSVGAnimatedTransformList;

namespace dom {

/**
 * Class SVGAnimatedTransformList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * nsSVGAnimatedTransformList objects.
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
class SVGAnimatedTransformList final : public nsWrapperCache
{
  friend class mozilla::DOMSVGTransformList;

public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(SVGAnimatedTransformList)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(SVGAnimatedTransformList)

  /**
   * Factory method to create and return a SVGAnimatedTransformList wrapper
   * for a given internal nsSVGAnimatedTransformList object. The factory takes
   * care of caching the object that it returns so that the same object can be
   * returned for the given nsSVGAnimatedTransformList each time it is requested.
   * The cached object is only removed from the cache when it is destroyed due
   * to there being no more references to it or to any of its descendant
   * objects. If that happens, any subsequent call requesting the DOM wrapper
   * for the nsSVGAnimatedTransformList will naturally result in a new
   * SVGAnimatedTransformList being returned.
   */
  static already_AddRefed<SVGAnimatedTransformList>
    GetDOMWrapper(nsSVGAnimatedTransformList *aList, nsSVGElement *aElement);

  /**
   * This method returns the SVGAnimatedTransformList wrapper for an internal
   * nsSVGAnimatedTransformList object if it currently has a wrapper. If it does
   * not, then nullptr is returned.
   */
  static SVGAnimatedTransformList*
    GetDOMWrapperIfExists(nsSVGAnimatedTransformList *aList);

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
  void InternalBaseValListWillChangeLengthTo(uint32_t aNewLength);
  void InternalAnimValListWillChangeLengthTo(uint32_t aNewLength);

  /**
   * Returns true if our attribute is animating (in which case our animVal is
   * not simply a mirror of our baseVal).
   */
  bool IsAnimating() const;

  // WebIDL
  nsSVGElement* GetParentObject() const { return mElement; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  // These aren't weak refs because mBaseVal and mAnimVal are weak
  already_AddRefed<DOMSVGTransformList> BaseVal();
  already_AddRefed<DOMSVGTransformList> AnimVal();

private:

  /**
   * Only our static GetDOMWrapper() factory method may create objects of our
   * type.
   */
  explicit SVGAnimatedTransformList(nsSVGElement *aElement)
    : mBaseVal(nullptr)
    , mAnimVal(nullptr)
    , mElement(aElement)
  {
  }

  ~SVGAnimatedTransformList();

  /// Get a reference to this DOM wrapper object's internal counterpart.
  nsSVGAnimatedTransformList& InternalAList();
  const nsSVGAnimatedTransformList& InternalAList() const;

  // Weak refs to our DOMSVGTransformList baseVal/animVal objects. These objects
  // are friends and take care of clearing these pointers when they die, making
  // these true weak references.
  DOMSVGTransformList *mBaseVal;
  DOMSVGTransformList *mAnimVal;

  // Strong ref to our element to keep it alive. We hold this not only for
  // ourself, but also for our base/animVal and all of their items.
  nsRefPtr<nsSVGElement> mElement;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGAnimatedTransformList_h
