/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGPOINTLIST_H__
#define MOZILLA_SVGPOINTLIST_H__

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsIWeakReferenceUtils.h"
#include "nsSVGElement.h"
#include "nsTArray.h"
#include "SVGPoint.h"

#include <string.h>

namespace mozilla {
class nsISVGPoint;

/**
 * ATTENTION! WARNING! WATCH OUT!!
 *
 * Consumers that modify objects of this type absolutely MUST keep the DOM
 * wrappers for those lists (if any) in sync!! That's why this class is so
 * locked down.
 *
 * The DOM wrapper class for this class is DOMSVGPointList.
 */
class SVGPointList
{
  friend class mozilla::nsISVGPoint;
  friend class SVGAnimatedPointList;
  friend class DOMSVGPointList;
  friend class DOMSVGPoint;

public:

  SVGPointList(){}
  ~SVGPointList(){}

  // Only methods that don't make/permit modification to this list are public.
  // Only our friend classes can access methods that may change us.

  /// This may return an incomplete string on OOM, but that's acceptable.
  void GetValueAsString(nsAString& aValue) const;

  bool IsEmpty() const {
    return mItems.IsEmpty();
  }

  uint32_t Length() const {
    return mItems.Length();
  }

  const SVGPoint& operator[](uint32_t aIndex) const {
    return mItems[aIndex];
  }

  bool operator==(const SVGPointList& rhs) const {
    // memcmp can be faster than |mItems == rhs.mItems|
    return mItems.Length() == rhs.mItems.Length() &&
           memcmp(mItems.Elements(), rhs.mItems.Elements(),
                  mItems.Length() * sizeof(SVGPoint)) == 0;
  }

  bool SetCapacity(uint32_t aSize) {
    return mItems.SetCapacity(aSize);
  }

  void Compact() {
    mItems.Compact();
  }

  // Access to methods that can modify objects of this type is deliberately
  // limited. This is to reduce the chances of someone modifying objects of
  // this type without taking the necessary steps to keep DOM wrappers in sync.
  // If you need wider access to these methods, consider adding a method to
  // SVGAnimatedPointList and having that class act as an intermediary so it
  // can take care of keeping DOM wrappers in sync.

protected:

  /**
   * This may fail on OOM if the internal capacity needs to be increased, in
   * which case the list will be left unmodified.
   */
  nsresult CopyFrom(const SVGPointList& rhs);

  SVGPoint& operator[](uint32_t aIndex) {
    return mItems[aIndex];
  }

  /**
   * This may fail (return false) on OOM if the internal capacity is being
   * increased, in which case the list will be left unmodified.
   */
  bool SetLength(uint32_t aNumberOfItems) {
    return mItems.SetLength(aNumberOfItems);
  }

private:

  // Marking the following private only serves to show which methods are only
  // used by our friend classes (as opposed to our subclasses) - it doesn't
  // really provide additional safety.

  nsresult SetValueFromString(const nsAString& aValue);

  void Clear() {
    mItems.Clear();
  }

  bool InsertItem(uint32_t aIndex, const SVGPoint &aPoint) {
    if (aIndex >= mItems.Length()) {
      aIndex = mItems.Length();
    }
    return !!mItems.InsertElementAt(aIndex, aPoint);
  }

  void ReplaceItem(uint32_t aIndex, const SVGPoint &aPoint) {
    MOZ_ASSERT(aIndex < mItems.Length(),
               "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mItems[aIndex] = aPoint;
  }

  void RemoveItem(uint32_t aIndex) {
    MOZ_ASSERT(aIndex < mItems.Length(),
               "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mItems.RemoveElementAt(aIndex);
  }

  bool AppendItem(SVGPoint aPoint) {
    return !!mItems.AppendElement(aPoint);
  }

protected:

  /* See SVGLengthList for the rationale for using FallibleTArray<SVGPoint> instead
   * of FallibleTArray<SVGPoint, 1>.
   */
  FallibleTArray<SVGPoint> mItems;
};


/**
 * This SVGPointList subclass is for SVGPointListSMILType which needs a
 * mutable version of SVGPointList. Instances of this class do not have
 * DOM wrappers that need to be kept in sync, so we can safely expose any
 * protected base class methods required by the SMIL code.
 *
 * This class contains a strong reference to the element that instances of
 * this class are being used to animate. This is because the SMIL code stores
 * instances of this class in nsSMILValue objects, some of which are cached.
 * Holding a strong reference to the element here prevents the element from
 * disappearing out from under the SMIL code unexpectedly.
 */
class SVGPointListAndInfo : public SVGPointList
{
public:

  explicit SVGPointListAndInfo(nsSVGElement *aElement = nullptr)
    : mElement(do_GetWeakReference(static_cast<nsINode*>(aElement)))
  {}

  void SetInfo(nsSVGElement *aElement) {
    mElement = do_GetWeakReference(static_cast<nsINode*>(aElement));
  }

  nsSVGElement* Element() const {
    nsCOMPtr<nsIContent> e = do_QueryReferent(mElement);
    return static_cast<nsSVGElement*>(e.get());
  }

  /**
   * Returns true if this object is an "identity" value, from the perspective
   * of SMIL. In other words, returns true until the initial value set up in
   * SVGPointListSMILType::Init() has been changed with a SetInfo() call.
   */
  bool IsIdentity() const {
    if (!mElement) {
      MOZ_ASSERT(IsEmpty(), "target element propagation failure");
      return true;
    } 
    return false;
  }

  nsresult CopyFrom(const SVGPointListAndInfo& rhs) {
    mElement = rhs.mElement;
    return SVGPointList::CopyFrom(rhs);
  }

  /**
   * Exposed so that SVGPointList baseVals can be copied to
   * SVGPointListAndInfo objects. Note that callers should also call
   * SetElement() when using this method!
   */
  nsresult CopyFrom(const SVGPointList& rhs) {
    return SVGPointList::CopyFrom(rhs);
  }
  const SVGPoint& operator[](uint32_t aIndex) const {
    return SVGPointList::operator[](aIndex);
  }
  SVGPoint& operator[](uint32_t aIndex) {
    return SVGPointList::operator[](aIndex);
  }
  bool SetLength(uint32_t aNumberOfItems) {
    return SVGPointList::SetLength(aNumberOfItems);
  }

private:
  // We must keep a weak reference to our element because we may belong to a
  // cached baseVal nsSMILValue. See the comments starting at:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=515116#c15
  // See also https://bugzilla.mozilla.org/show_bug.cgi?id=653497
  nsWeakPtr mElement;
};

} // namespace mozilla

#endif // MOZILLA_SVGPOINTLIST_H__
