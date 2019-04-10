/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGTRANSFORMLIST_H__
#define MOZILLA_SVGTRANSFORMLIST_H__

#include "gfxMatrix.h"
#include "SVGTransform.h"
#include "nsTArray.h"

namespace mozilla {

namespace dom {
class DOMSVGTransform;
}  // namespace dom

/**
 * ATTENTION! WARNING! WATCH OUT!!
 *
 * Consumers that modify objects of this type absolutely MUST keep the DOM
 * wrappers for those lists (if any) in sync!! That's why this class is so
 * locked down.
 *
 * The DOM wrapper class for this class is DOMSVGTransformList.
 */
class SVGTransformList {
  friend class SVGAnimatedTransformList;
  friend class DOMSVGTransformList;
  friend class dom::DOMSVGTransform;

 public:
  SVGTransformList() = default;
  ~SVGTransformList() = default;

  // Only methods that don't make/permit modification to this list are public.
  // Only our friend classes can access methods that may change us.

  /// This may return an incomplete string on OOM, but that's acceptable.
  void GetValueAsString(nsAString& aValue) const;

  bool IsEmpty() const { return mItems.IsEmpty(); }

  uint32_t Length() const { return mItems.Length(); }

  const SVGTransform& operator[](uint32_t aIndex) const {
    return mItems[aIndex];
  }

  bool operator==(const SVGTransformList& rhs) const {
    return mItems == rhs.mItems;
  }

  bool SetCapacity(uint32_t size) { return mItems.SetCapacity(size, fallible); }

  void Compact() { mItems.Compact(); }

  gfxMatrix GetConsolidationMatrix() const;

  // Access to methods that can modify objects of this type is deliberately
  // limited. This is to reduce the chances of someone modifying objects of
  // this type without taking the necessary steps to keep DOM wrappers in sync.
  // If you need wider access to these methods, consider adding a method to
  // DOMSVGAnimatedTransformList and having that class act as an intermediary so
  // it can take care of keeping DOM wrappers in sync.

 protected:
  /**
   * These may fail on OOM if the internal capacity needs to be increased, in
   * which case the list will be left unmodified.
   */
  nsresult CopyFrom(const SVGTransformList& rhs);
  nsresult CopyFrom(const nsTArray<SVGTransform>& aTransformArray);

  SVGTransform& operator[](uint32_t aIndex) { return mItems[aIndex]; }

  /**
   * This may fail (return false) on OOM if the internal capacity is being
   * increased, in which case the list will be left unmodified.
   */
  bool SetLength(uint32_t aNumberOfItems) {
    return mItems.SetLength(aNumberOfItems, fallible);
  }

 private:
  // Marking the following private only serves to show which methods are only
  // used by our friend classes (as opposed to our subclasses) - it doesn't
  // really provide additional safety.

  nsresult SetValueFromString(const nsAString& aValue);

  void Clear() { mItems.Clear(); }

  bool InsertItem(uint32_t aIndex, const SVGTransform& aTransform) {
    if (aIndex >= mItems.Length()) {
      aIndex = mItems.Length();
    }
    return !!mItems.InsertElementAt(aIndex, aTransform, fallible);
  }

  void ReplaceItem(uint32_t aIndex, const SVGTransform& aTransform) {
    MOZ_ASSERT(aIndex < mItems.Length(),
               "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mItems[aIndex] = aTransform;
  }

  void RemoveItem(uint32_t aIndex) {
    MOZ_ASSERT(aIndex < mItems.Length(),
               "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mItems.RemoveElementAt(aIndex);
  }

  bool AppendItem(const SVGTransform& aTransform) {
    return !!mItems.AppendElement(aTransform, fallible);
  }

 protected:
  /*
   * See SVGLengthList for the rationale for using
   * FallibleTArray<SVGTransform> instead of FallibleTArray<SVGTransform,
   * 1>.
   */
  FallibleTArray<SVGTransform> mItems;
};

}  // namespace mozilla

#endif  // MOZILLA_SVGTRANSFORMLIST_H__
