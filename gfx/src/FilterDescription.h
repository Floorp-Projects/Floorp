/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FilterDescription_h
#define __FilterDescription_h

#include "FilterSupport.h"
#include "mozilla/Variant.h"
#include "mozilla/gfx/Rect.h"
#include "nsTArray.h"

namespace mozilla::gfx {
class FilterPrimitiveDescription;
}

MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::gfx::FilterPrimitiveDescription)

namespace mozilla::gfx {
typedef Variant<
    EmptyAttributes, BlendAttributes, MorphologyAttributes,
    ColorMatrixAttributes, FloodAttributes, TileAttributes,
    ComponentTransferAttributes, OpacityAttributes, ConvolveMatrixAttributes,
    OffsetAttributes, DisplacementMapAttributes, TurbulenceAttributes,
    CompositeAttributes, MergeAttributes, ImageAttributes,
    GaussianBlurAttributes, DropShadowAttributes, DiffuseLightingAttributes,
    SpecularLightingAttributes, ToAlphaAttributes>
    PrimitiveAttributes;

/**
 * A data structure to carry attributes for a given primitive that's part of a
 * filter. Will be serializable via IPDL, so it must not contain complex
 * functionality.
 * Used as part of a FilterDescription.
 */
class FilterPrimitiveDescription final {
 public:
  enum {
    kPrimitiveIndexSourceGraphic = -1,
    kPrimitiveIndexSourceAlpha = -2,
    kPrimitiveIndexFillPaint = -3,
    kPrimitiveIndexStrokePaint = -4
  };

  FilterPrimitiveDescription();
  explicit FilterPrimitiveDescription(PrimitiveAttributes&& aAttributes);
  FilterPrimitiveDescription(FilterPrimitiveDescription&& aOther) = default;
  FilterPrimitiveDescription& operator=(FilterPrimitiveDescription&& aOther) =
      default;
  FilterPrimitiveDescription(const FilterPrimitiveDescription& aOther)
      : mAttributes(aOther.mAttributes),
        mInputPrimitives(aOther.mInputPrimitives.Clone()),
        mFilterPrimitiveSubregion(aOther.mFilterPrimitiveSubregion),
        mFilterSpaceBounds(aOther.mFilterSpaceBounds),
        mInputColorSpaces(aOther.mInputColorSpaces.Clone()),
        mOutputColorSpace(aOther.mOutputColorSpace),
        mIsTainted(aOther.mIsTainted) {}

  const PrimitiveAttributes& Attributes() const { return mAttributes; }
  PrimitiveAttributes& Attributes() { return mAttributes; }

  IntRect PrimitiveSubregion() const { return mFilterPrimitiveSubregion; }
  IntRect FilterSpaceBounds() const { return mFilterSpaceBounds; }
  bool IsTainted() const { return mIsTainted; }

  size_t NumberOfInputs() const { return mInputPrimitives.Length(); }
  int32_t InputPrimitiveIndex(size_t aInputIndex) const {
    return aInputIndex < mInputPrimitives.Length()
               ? mInputPrimitives[aInputIndex]
               : 0;
  }

  ColorSpace InputColorSpace(size_t aInputIndex) const {
    return aInputIndex < mInputColorSpaces.Length()
               ? mInputColorSpaces[aInputIndex]
               : ColorSpace();
  }

  ColorSpace OutputColorSpace() const { return mOutputColorSpace; }

  void SetPrimitiveSubregion(const IntRect& aRect) {
    mFilterPrimitiveSubregion = aRect;
  }

  void SetFilterSpaceBounds(const IntRect& aRect) {
    mFilterSpaceBounds = aRect;
  }

  void SetIsTainted(bool aIsTainted) { mIsTainted = aIsTainted; }

  void SetInputPrimitive(size_t aInputIndex, int32_t aInputPrimitiveIndex) {
    mInputPrimitives.EnsureLengthAtLeast(aInputIndex + 1);
    mInputPrimitives[aInputIndex] = aInputPrimitiveIndex;
  }

  void SetInputColorSpace(size_t aInputIndex, ColorSpace aColorSpace) {
    mInputColorSpaces.EnsureLengthAtLeast(aInputIndex + 1);
    mInputColorSpaces[aInputIndex] = aColorSpace;
  }

  void SetOutputColorSpace(const ColorSpace& aColorSpace) {
    mOutputColorSpace = aColorSpace;
  }

  bool operator==(const FilterPrimitiveDescription& aOther) const;
  bool operator!=(const FilterPrimitiveDescription& aOther) const {
    return !(*this == aOther);
  }

 private:
  PrimitiveAttributes mAttributes;
  AutoTArray<int32_t, 2> mInputPrimitives;
  IntRect mFilterPrimitiveSubregion;
  IntRect mFilterSpaceBounds;
  AutoTArray<ColorSpace, 2> mInputColorSpaces;
  ColorSpace mOutputColorSpace;
  bool mIsTainted;
};

/**
 * A data structure that contains one or more FilterPrimitiveDescriptions.
 * Designed to be serializable via IPDL, so it must not contain complex
 * functionality.
 */
struct FilterDescription final {
  FilterDescription() = default;
  explicit FilterDescription(nsTArray<FilterPrimitiveDescription>&& aPrimitives)
      : mPrimitives(std::move(aPrimitives)) {}

  bool operator==(const FilterDescription& aOther) const;
  bool operator!=(const FilterDescription& aOther) const {
    return !(*this == aOther);
  }

  CopyableTArray<FilterPrimitiveDescription> mPrimitives;
};
}  // namespace mozilla::gfx

#endif  // __FilterSupport_h
