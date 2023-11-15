/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGPATHDATA_H_
#define DOM_SVG_SVGPATHDATA_H_

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefPtr.h"
#include "nsTArray.h"

#include <string.h>

namespace mozilla {

struct StylePathCommand;
struct SVGMark;
enum class StyleStrokeLinecap : uint8_t;

class SVGPathDataParser;  // IWYU pragma: keep

namespace dom {
class DOMSVGPathSeg;
class DOMSVGPathSegList;
}  // namespace dom

/**
 * ATTENTION! WARNING! WATCH OUT!!
 *
 * Consumers that modify objects of this type absolutely MUST keep the DOM
 * wrappers for those lists (if any) in sync!! That's why this class is so
 * locked down.
 *
 * The DOM wrapper class for this class is DOMSVGPathSegList.
 *
 * This class is not called |class SVGPathSegList| for one very good reason;
 * this class does not provide a list of "SVGPathSeg" items, it provides an
 * array of floats into which path segments are encoded. See the paragraphs
 * that follow for why. Note that the Length() method returns the number of
 * floats in our array, not the number of encoded segments, and the index
 * operator indexes floats in the array, not segments. If this class were
 * called SVGPathSegList the names of these methods would be very misleading.
 *
 * The reason this class is designed in this way is because there are many
 * different types of path segment, each taking a different numbers of
 * arguments. We want to store the segments in an nsTArray to avoid individual
 * allocations for each item, but the different size of segments means we can't
 * have one single segment type for the nsTArray (not without using a space
 * wasteful union or something similar). Since the internal code does not need
 * to index into the list (the DOM wrapper does, but it handles that itself)
 * the obvious solution is to have the items in this class take up variable
 * width and have the internal code iterate over these lists rather than index
 * into them.
 *
 * Implementing indexing to segments with O(1) performance would require us to
 * allocate and maintain a separate segment index table (keeping that table in
 * sync when items are inserted or removed from the list). So long as the
 * internal code doesn't require indexing to segments, we can avoid that
 * overhead and additional complexity.
 *
 * Segment encoding: the first float in the encoding of a segment contains the
 * segment's type. The segment's type is encoded to/decoded from this float
 * using the static methods SVGPathSegUtils::EncodeType(uint32_t)/
 * SVGPathSegUtils::DecodeType(float). If the path segment type in question
 * takes any arguments then these follow the first float, and are in the same
 * order as they are given in a <path> element's 'd' attribute (NOT in the
 * order of the createSVGPathSegXxx() methods' arguments from the SVG DOM
 * interface SVGPathElement, which are different...grr). Consumers can use
 * SVGPathSegUtils::ArgCountForType(type) to determine how many arguments
 * there are (if any), and thus where the current encoded segment ends, and
 * where the next segment (if any) begins.
 */
class SVGPathData {
  friend class SVGAnimatedPathSegList;
  friend class dom::DOMSVGPathSeg;
  friend class dom::DOMSVGPathSegList;
  friend class SVGPathDataParser;
  // SVGPathDataParser will not keep wrappers in sync, so consumers
  // are responsible for that!

  using DrawTarget = gfx::DrawTarget;
  using Path = gfx::Path;
  using PathBuilder = gfx::PathBuilder;
  using FillRule = gfx::FillRule;
  using Float = gfx::Float;
  using CapStyle = gfx::CapStyle;

 public:
  using const_iterator = const float*;

  SVGPathData() = default;
  ~SVGPathData() = default;

  SVGPathData& operator=(const SVGPathData& aOther) {
    mData.ClearAndRetainStorage();
    // Best-effort, really.
    Unused << mData.AppendElements(aOther.mData, fallible);
    return *this;
  }

  SVGPathData(const SVGPathData& aOther) { *this = aOther; }

  // Only methods that don't make/permit modification to this list are public.
  // Only our friend classes can access methods that may change us.

  /// This may return an incomplete string on OOM, but that's acceptable.
  void GetValueAsString(nsAString& aValue) const;

  bool IsEmpty() const { return mData.IsEmpty(); }

#ifdef DEBUG
  /**
   * This method iterates over the encoded segment data and counts the number
   * of segments we currently have.
   */
  uint32_t CountItems() const;
#endif

  /**
   * Returns the number of *floats* in the encoding array, and NOT the number
   * of segments encoded in this object. (For that, see CountItems() above.)
   */
  uint32_t Length() const { return mData.Length(); }

  const nsTArray<float>& RawData() const { return mData; }

  const float& operator[](uint32_t aIndex) const { return mData[aIndex]; }

  // Used by SMILCompositor to check if the cached base val is out of date
  bool operator==(const SVGPathData& rhs) const {
    // We use memcmp so that we don't need to worry that the data encoded in
    // the first float may have the same bit pattern as a NaN.
    return mData.Length() == rhs.mData.Length() &&
           memcmp(mData.Elements(), rhs.mData.Elements(),
                  mData.Length() * sizeof(float)) == 0;
  }

  bool SetCapacity(uint32_t aSize) {
    return mData.SetCapacity(aSize, fallible);
  }

  void Compact() { mData.Compact(); }

  float GetPathLength() const;

  uint32_t GetPathSegAtLength(float aDistance) const;

  static uint32_t GetPathSegAtLength(Span<const StylePathCommand> aPath,
                                     float aDistance);

  void GetMarkerPositioningData(nsTArray<SVGMark>* aMarks) const;

  static void GetMarkerPositioningData(Span<const StylePathCommand> aPath,
                                       nsTArray<SVGMark>* aMarks);

  /**
   * Returns true, except on OOM, in which case returns false.
   */
  bool GetDistancesFromOriginToEndsOfVisibleSegments(
      FallibleTArray<double>* aOutput) const;

  /**
   * This is identical to the above one but accepts StylePathCommand.
   */
  static bool GetDistancesFromOriginToEndsOfVisibleSegments(
      Span<const StylePathCommand> aPath, FallibleTArray<double>* aOutput);

  /**
   * This returns a path without the extra little line segments that
   * ApproximateZeroLengthSubpathSquareCaps can insert if we have square-caps.
   * See the comment for that function for more info on that.
   */
  already_AddRefed<Path> BuildPathForMeasuring() const;

  already_AddRefed<Path> BuildPath(PathBuilder* aBuilder,
                                   StyleStrokeLinecap aStrokeLineCap,
                                   Float aStrokeWidth) const;

  static already_AddRefed<Path> BuildPathForMeasuring(
      Span<const StylePathCommand> aPath);

  /**
   * This function tries to build the path from an array of StylePathCommand,
   * which is generated by cbindgen from Rust (see ServoStyleConsts.h).
   * Basically, this is a variant of the above BuildPath() functions.
   */
  static already_AddRefed<Path> BuildPath(
      Span<const StylePathCommand> aPath, PathBuilder* aBuilder,
      StyleStrokeLinecap aStrokeLineCap, Float aStrokeWidth,
      const gfx::Point& aOffset = gfx::Point(), float aZoomFactor = 1.0);

  const_iterator begin() const { return mData.Elements(); }
  const_iterator end() const { return mData.Elements() + mData.Length(); }

  // memory reporting methods
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Access to methods that can modify objects of this type is deliberately
  // limited. This is to reduce the chances of someone modifying objects of
  // this type without taking the necessary steps to keep DOM wrappers in sync.
  // If you need wider access to these methods, consider adding a method to
  // SVGAnimatedPathSegList and having that class act as an intermediary so it
  // can take care of keeping DOM wrappers in sync.

 protected:
  using iterator = float*;

  /**
   * This may fail on OOM if the internal capacity needs to be increased, in
   * which case the list will be left unmodified.
   */
  nsresult CopyFrom(const SVGPathData& rhs);
  void SwapWith(SVGPathData& aRhs) { mData.SwapElements(aRhs.mData); }

  float& operator[](uint32_t aIndex) { return mData[aIndex]; }

  /**
   * This may fail (return false) on OOM if the internal capacity is being
   * increased, in which case the list will be left unmodified.
   */
  bool SetLength(uint32_t aLength) {
    return mData.SetLength(aLength, fallible);
  }

  nsresult SetValueFromString(const nsAString& aValue);

  void Clear() { mData.Clear(); }

  // Our DOM wrappers have direct access to our mData, so they directly
  // manipulate it rather than us implementing:
  //
  // * InsertItem(uint32_t aDataIndex, uint32_t aType, const float *aArgs);
  // * ReplaceItem(uint32_t aDataIndex, uint32_t aType, const float *aArgs);
  // * RemoveItem(uint32_t aDataIndex);
  // * bool AppendItem(uint32_t aType, const float *aArgs);

  nsresult AppendSeg(uint32_t aType, ...);  // variable number of float args

  iterator begin() { return mData.Elements(); }
  iterator end() { return mData.Elements() + mData.Length(); }

  FallibleTArray<float> mData;
};

/**
 * This SVGPathData subclass is for SVGPathSegListSMILType which needs to
 * have write access to the lists it works with.
 *
 * Instances of this class do not have DOM wrappers that need to be kept in
 * sync, so we can safely expose any protected base class methods required by
 * the SMIL code.
 */
class SVGPathDataAndInfo final : public SVGPathData {
 public:
  explicit SVGPathDataAndInfo(dom::SVGElement* aElement = nullptr)
      : mElement(do_GetWeakReference(static_cast<nsINode*>(aElement))) {}

  void SetElement(dom::SVGElement* aElement) {
    mElement = do_GetWeakReference(static_cast<nsINode*>(aElement));
  }

  dom::SVGElement* Element() const {
    nsCOMPtr<nsIContent> e = do_QueryReferent(mElement);
    return static_cast<dom::SVGElement*>(e.get());
  }

  nsresult CopyFrom(const SVGPathDataAndInfo& rhs) {
    mElement = rhs.mElement;
    return SVGPathData::CopyFrom(rhs);
  }

  /**
   * Returns true if this object is an "identity" value, from the perspective
   * of SMIL. In other words, returns true until the initial value set up in
   * SVGPathSegListSMILType::Init() has been changed with a SetElement() call.
   */
  bool IsIdentity() const {
    if (!mElement) {
      MOZ_ASSERT(IsEmpty(), "target element propagation failure");
      return true;
    }
    return false;
  }

  /**
   * Exposed so that SVGPathData baseVals can be copied to
   * SVGPathDataAndInfo objects. Note that callers should also call
   * SetElement() when using this method!
   */
  using SVGPathData::CopyFrom;

  // Exposed since SVGPathData objects can be modified.
  using SVGPathData::iterator;
  using SVGPathData::operator[];
  using SVGPathData::begin;
  using SVGPathData::end;
  using SVGPathData::SetLength;

 private:
  // We must keep a weak reference to our element because we may belong to a
  // cached baseVal SMILValue. See the comments starting at:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=515116#c15
  // See also https://bugzilla.mozilla.org/show_bug.cgi?id=653497
  nsWeakPtr mElement;
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGPATHDATA_H_
