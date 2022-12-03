/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_DOMSVGANIMATEDNUMBERLIST_H_
#define DOM_SVG_DOMSVGANIMATEDNUMBERLIST_H_

#include "nsCycleCollectionParticipant.h"
#include "SVGElement.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class SVGAnimatedNumberList;
class SVGNumberList;

namespace dom {

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
class DOMSVGAnimatedNumberList final : public nsWrapperCache {
  friend class DOMSVGNumberList;

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMSVGAnimatedNumberList)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMSVGAnimatedNumberList)

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
  static already_AddRefed<DOMSVGAnimatedNumberList> GetDOMWrapper(
      SVGAnimatedNumberList* aList, dom::SVGElement* aElement,
      uint8_t aAttrEnum);

  /**
   * This method returns the DOMSVGAnimatedNumberList wrapper for an internal
   * SVGAnimatedNumberList object if it currently has a wrapper. If it does
   * not, then nullptr is returned.
   */
  static DOMSVGAnimatedNumberList* GetDOMWrapperIfExists(
      SVGAnimatedNumberList* aList);

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

  // WebIDL
  dom::SVGElement* GetParentObject() const { return mElement; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  // These aren't weak refs because mBaseVal and mAnimVal are weak
  already_AddRefed<DOMSVGNumberList> BaseVal();
  already_AddRefed<DOMSVGNumberList> AnimVal();

 private:
  /**
   * Only our static GetDOMWrapper() factory method may create objects of our
   * type.
   */
  DOMSVGAnimatedNumberList(dom::SVGElement* aElement, uint8_t aAttrEnum)
      : mBaseVal(nullptr),
        mAnimVal(nullptr),
        mElement(aElement),
        mAttrEnum(aAttrEnum) {}

  ~DOMSVGAnimatedNumberList();

  /// Get a reference to this DOM wrapper object's internal counterpart.
  SVGAnimatedNumberList& InternalAList();
  const SVGAnimatedNumberList& InternalAList() const;

  // Weak refs to our DOMSVGNumberList baseVal/animVal objects. These objects
  // are friends and take care of clearing these pointers when they die, making
  // these true weak references.
  DOMSVGNumberList* mBaseVal;
  DOMSVGNumberList* mAnimVal;

  // Strong ref to our element to keep it alive. We hold this not only for
  // ourself, but also for our base/animVal and all of their items.
  RefPtr<dom::SVGElement> mElement;

  uint8_t mAttrEnum;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_DOMSVGANIMATEDNUMBERLIST_H_
