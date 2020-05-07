/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FilterSupport_h
#define __FilterSupport_h

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Rect.h"
#include "nsClassHashtable.h"
#include "nsRegion.h"
#include "nsTArray.h"

namespace mozilla {
namespace gfx {
class FilterPrimitiveDescription;
}  // namespace gfx
}  // namespace mozilla

MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::gfx::FilterPrimitiveDescription)

extern const float gsRGBToLinearRGBMap[256];

namespace mozilla {
namespace gfx {
namespace FilterWrappers {
extern already_AddRefed<FilterNode> Clear(DrawTarget* aDT);
extern already_AddRefed<FilterNode> ForSurface(
    DrawTarget* aDT, SourceSurface* aSurface, const IntPoint& aSurfacePosition);
}  // namespace FilterWrappers

// Morphology Operators
const unsigned short SVG_OPERATOR_UNKNOWN = 0;
const unsigned short SVG_OPERATOR_ERODE = 1;
const unsigned short SVG_OPERATOR_DILATE = 2;

// ColorMatrix types
const unsigned short SVG_FECOLORMATRIX_TYPE_UNKNOWN = 0;
const unsigned short SVG_FECOLORMATRIX_TYPE_MATRIX = 1;
const unsigned short SVG_FECOLORMATRIX_TYPE_SATURATE = 2;
const unsigned short SVG_FECOLORMATRIX_TYPE_HUE_ROTATE = 3;
const unsigned short SVG_FECOLORMATRIX_TYPE_LUMINANCE_TO_ALPHA = 4;
// ColorMatrix types for CSS filters
const unsigned short SVG_FECOLORMATRIX_TYPE_SEPIA = 5;

// ComponentTransfer types
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN = 0;
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY = 1;
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_TABLE = 2;
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE = 3;
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_LINEAR = 4;
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_GAMMA = 5;
const unsigned short SVG_FECOMPONENTTRANSFER_SAME_AS_R = 6;

// Blend Mode Values
const unsigned short SVG_FEBLEND_MODE_UNKNOWN = 0;
const unsigned short SVG_FEBLEND_MODE_NORMAL = 1;
const unsigned short SVG_FEBLEND_MODE_MULTIPLY = 2;
const unsigned short SVG_FEBLEND_MODE_SCREEN = 3;
const unsigned short SVG_FEBLEND_MODE_DARKEN = 4;
const unsigned short SVG_FEBLEND_MODE_LIGHTEN = 5;
const unsigned short SVG_FEBLEND_MODE_OVERLAY = 6;
const unsigned short SVG_FEBLEND_MODE_COLOR_DODGE = 7;
const unsigned short SVG_FEBLEND_MODE_COLOR_BURN = 8;
const unsigned short SVG_FEBLEND_MODE_HARD_LIGHT = 9;
const unsigned short SVG_FEBLEND_MODE_SOFT_LIGHT = 10;
const unsigned short SVG_FEBLEND_MODE_DIFFERENCE = 11;
const unsigned short SVG_FEBLEND_MODE_EXCLUSION = 12;
const unsigned short SVG_FEBLEND_MODE_HUE = 13;
const unsigned short SVG_FEBLEND_MODE_SATURATION = 14;
const unsigned short SVG_FEBLEND_MODE_COLOR = 15;
const unsigned short SVG_FEBLEND_MODE_LUMINOSITY = 16;

// Edge Mode Values
const unsigned short SVG_EDGEMODE_UNKNOWN = 0;
const unsigned short SVG_EDGEMODE_DUPLICATE = 1;
const unsigned short SVG_EDGEMODE_WRAP = 2;
const unsigned short SVG_EDGEMODE_NONE = 3;

// Channel Selectors
const unsigned short SVG_CHANNEL_UNKNOWN = 0;
const unsigned short SVG_CHANNEL_R = 1;
const unsigned short SVG_CHANNEL_G = 2;
const unsigned short SVG_CHANNEL_B = 3;
const unsigned short SVG_CHANNEL_A = 4;

// Turbulence Types
const unsigned short SVG_TURBULENCE_TYPE_UNKNOWN = 0;
const unsigned short SVG_TURBULENCE_TYPE_FRACTALNOISE = 1;
const unsigned short SVG_TURBULENCE_TYPE_TURBULENCE = 2;

// Composite Operators
const unsigned short SVG_FECOMPOSITE_OPERATOR_UNKNOWN = 0;
const unsigned short SVG_FECOMPOSITE_OPERATOR_OVER = 1;
const unsigned short SVG_FECOMPOSITE_OPERATOR_IN = 2;
const unsigned short SVG_FECOMPOSITE_OPERATOR_OUT = 3;
const unsigned short SVG_FECOMPOSITE_OPERATOR_ATOP = 4;
const unsigned short SVG_FECOMPOSITE_OPERATOR_XOR = 5;
const unsigned short SVG_FECOMPOSITE_OPERATOR_ARITHMETIC = 6;

class DrawTarget;
class SourceSurface;
struct FilterAttribute;

// Limits
const float kMaxStdDeviation = 500;

// Simple PrimitiveAttributes:

struct EmptyAttributes {
  bool operator==(const EmptyAttributes& aOther) const { return true; }
};

struct BlendAttributes {
  uint32_t mBlendMode;

  bool operator==(const BlendAttributes& aOther) const {
    return mBlendMode == aOther.mBlendMode;
  }
};

struct MorphologyAttributes {
  uint32_t mOperator;
  Size mRadii;

  bool operator==(const MorphologyAttributes& aOther) const {
    return mOperator == aOther.mOperator && mRadii == aOther.mRadii;
  }
};

struct FloodAttributes {
  sRGBColor mColor;

  bool operator==(const FloodAttributes& aOther) const {
    return mColor == aOther.mColor;
  }
};

struct TileAttributes {
  bool operator==(const TileAttributes& aOther) const { return true; }
};

struct OpacityAttributes {
  float mOpacity;

  bool operator==(const OpacityAttributes& aOther) const {
    return mOpacity == aOther.mOpacity;
  }
};

struct OffsetAttributes {
  IntPoint mValue;

  bool operator==(const OffsetAttributes& aOther) const {
    return mValue == aOther.mValue;
  }
};

struct DisplacementMapAttributes {
  float mScale;
  uint32_t mXChannel;
  uint32_t mYChannel;

  bool operator==(const DisplacementMapAttributes& aOther) const {
    return mScale == aOther.mScale && mXChannel == aOther.mXChannel &&
           mYChannel == aOther.mYChannel;
  }
};

struct TurbulenceAttributes {
  IntPoint mOffset;
  Size mBaseFrequency;
  float mSeed;
  uint32_t mOctaves;
  bool mStitchable;
  uint32_t mType;

  bool operator==(const TurbulenceAttributes& aOther) const {
    return mOffset == aOther.mOffset &&
           mBaseFrequency == aOther.mBaseFrequency && mSeed == aOther.mSeed &&
           mOctaves == aOther.mOctaves && mStitchable == aOther.mStitchable &&
           mType == aOther.mType;
  }
};

struct MergeAttributes {
  bool operator==(const MergeAttributes& aOther) const { return true; }
};

struct ImageAttributes {
  uint32_t mFilter;
  uint32_t mInputIndex;
  Matrix mTransform;

  bool operator==(const ImageAttributes& aOther) const {
    return mFilter == aOther.mFilter && mInputIndex == aOther.mInputIndex &&
           mTransform.ExactlyEquals(aOther.mTransform);
  }
};

struct GaussianBlurAttributes {
  Size mStdDeviation;

  bool operator==(const GaussianBlurAttributes& aOther) const {
    return mStdDeviation == aOther.mStdDeviation;
  }
};

struct DropShadowAttributes {
  Size mStdDeviation;
  Point mOffset;
  sRGBColor mColor;

  bool operator==(const DropShadowAttributes& aOther) const {
    return mStdDeviation == aOther.mStdDeviation && mOffset == aOther.mOffset &&
           mColor == aOther.mColor;
  }
};

struct ToAlphaAttributes {
  bool operator==(const ToAlphaAttributes& aOther) const { return true; }
};

// Complex PrimitiveAttributes:

class ImplicitlyCopyableFloatArray : public CopyableTArray<float> {
 public:
  ImplicitlyCopyableFloatArray() = default;

  ImplicitlyCopyableFloatArray(ImplicitlyCopyableFloatArray&& aOther) = default;

  ImplicitlyCopyableFloatArray& operator=(
      ImplicitlyCopyableFloatArray&& aOther) = default;

  ImplicitlyCopyableFloatArray(const ImplicitlyCopyableFloatArray& aOther) =
      default;

  ImplicitlyCopyableFloatArray& operator=(
      const ImplicitlyCopyableFloatArray& aOther) = default;
};

struct ColorMatrixAttributes {
  uint32_t mType;
  ImplicitlyCopyableFloatArray mValues;

  bool operator==(const ColorMatrixAttributes& aOther) const {
    return mType == aOther.mType && mValues == aOther.mValues;
  }
};

// If the types for G and B are SVG_FECOMPONENTTRANSFER_SAME_AS_R,
// use the R channel values - this lets us avoid copies.
const uint32_t kChannelROrRGB = 0;
const uint32_t kChannelG = 1;
const uint32_t kChannelB = 2;
const uint32_t kChannelA = 3;

const uint32_t kComponentTransferSlopeIndex = 0;
const uint32_t kComponentTransferInterceptIndex = 1;

const uint32_t kComponentTransferAmplitudeIndex = 0;
const uint32_t kComponentTransferExponentIndex = 1;
const uint32_t kComponentTransferOffsetIndex = 2;

struct ComponentTransferAttributes {
  uint8_t mTypes[4];
  ImplicitlyCopyableFloatArray mValues[4];

  bool operator==(const ComponentTransferAttributes& aOther) const {
    return mTypes[0] == aOther.mTypes[0] && mTypes[1] == aOther.mTypes[1] &&
           mTypes[2] == aOther.mTypes[2] && mTypes[3] == aOther.mTypes[3] &&
           mValues[0] == aOther.mValues[0] && mValues[1] == aOther.mValues[1] &&
           mValues[2] == aOther.mValues[2] && mValues[3] == aOther.mValues[3];
  }
};

struct ConvolveMatrixAttributes {
  IntSize mKernelSize;
  ImplicitlyCopyableFloatArray mKernelMatrix;
  float mDivisor;
  float mBias;
  IntPoint mTarget;
  uint32_t mEdgeMode;
  Size mKernelUnitLength;
  bool mPreserveAlpha;

  bool operator==(const ConvolveMatrixAttributes& aOther) const {
    return mKernelSize == aOther.mKernelSize &&
           mKernelMatrix == aOther.mKernelMatrix &&
           mDivisor == aOther.mDivisor && mBias == aOther.mBias &&
           mTarget == aOther.mTarget && mEdgeMode == aOther.mEdgeMode &&
           mKernelUnitLength == aOther.mKernelUnitLength &&
           mPreserveAlpha == aOther.mPreserveAlpha;
  }
};

struct CompositeAttributes {
  uint32_t mOperator;
  ImplicitlyCopyableFloatArray mCoefficients;

  bool operator==(const CompositeAttributes& aOther) const {
    return mOperator == aOther.mOperator &&
           mCoefficients == aOther.mCoefficients;
  }
};

enum class LightType {
  None = 0,
  Point,
  Spot,
  Distant,
  Max,
};

const uint32_t kDistantLightAzimuthIndex = 0;
const uint32_t kDistantLightElevationIndex = 1;
const uint32_t kDistantLightNumAttributes = 2;

const uint32_t kPointLightPositionXIndex = 0;
const uint32_t kPointLightPositionYIndex = 1;
const uint32_t kPointLightPositionZIndex = 2;
const uint32_t kPointLightNumAttributes = 3;

const uint32_t kSpotLightPositionXIndex = 0;
const uint32_t kSpotLightPositionYIndex = 1;
const uint32_t kSpotLightPositionZIndex = 2;
const uint32_t kSpotLightPointsAtXIndex = 3;
const uint32_t kSpotLightPointsAtYIndex = 4;
const uint32_t kSpotLightPointsAtZIndex = 5;
const uint32_t kSpotLightFocusIndex = 6;
const uint32_t kSpotLightLimitingConeAngleIndex = 7;
const uint32_t kSpotLightNumAttributes = 8;

struct DiffuseLightingAttributes {
  LightType mLightType;
  ImplicitlyCopyableFloatArray mLightValues;
  float mSurfaceScale;
  Size mKernelUnitLength;
  sRGBColor mColor;
  float mLightingConstant;
  float mSpecularExponent;

  bool operator==(const DiffuseLightingAttributes& aOther) const {
    return mLightType == aOther.mLightType &&
           mLightValues == aOther.mLightValues &&
           mSurfaceScale == aOther.mSurfaceScale &&
           mKernelUnitLength == aOther.mKernelUnitLength &&
           mColor == aOther.mColor;
  }
};

struct SpecularLightingAttributes : public DiffuseLightingAttributes {};

typedef Variant<
    EmptyAttributes, BlendAttributes, MorphologyAttributes,
    ColorMatrixAttributes, FloodAttributes, TileAttributes,
    ComponentTransferAttributes, OpacityAttributes, ConvolveMatrixAttributes,
    OffsetAttributes, DisplacementMapAttributes, TurbulenceAttributes,
    CompositeAttributes, MergeAttributes, ImageAttributes,
    GaussianBlurAttributes, DropShadowAttributes, DiffuseLightingAttributes,
    SpecularLightingAttributes, ToAlphaAttributes>
    PrimitiveAttributes;

enum class ColorSpace { SRGB, LinearRGB, Max };

enum class AlphaModel { Unpremultiplied, Premultiplied };

class ColorModel {
 public:
  static ColorModel PremulSRGB() {
    return ColorModel(ColorSpace::SRGB, AlphaModel::Premultiplied);
  }

  ColorModel(ColorSpace aColorSpace, AlphaModel aAlphaModel)
      : mColorSpace(aColorSpace), mAlphaModel(aAlphaModel) {}
  ColorModel()
      : mColorSpace(ColorSpace::SRGB), mAlphaModel(AlphaModel::Premultiplied) {}
  bool operator==(const ColorModel& aOther) const {
    return mColorSpace == aOther.mColorSpace &&
           mAlphaModel == aOther.mAlphaModel;
  }

  // Used to index FilterCachedColorModels::mFilterForColorModel.
  uint8_t ToIndex() const {
    return static_cast<uint8_t>(static_cast<uint8_t>(mColorSpace) << 1) |
           static_cast<uint8_t>(mAlphaModel);
  }

  ColorSpace mColorSpace;
  AlphaModel mAlphaModel;
};

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
  explicit FilterDescription(
      nsTArray<FilterPrimitiveDescription>&& aPrimitives) {
    mPrimitives.SwapElements(aPrimitives);
  }

  bool operator==(const FilterDescription& aOther) const;
  bool operator!=(const FilterDescription& aOther) const {
    return !(*this == aOther);
  }

  CopyableTArray<FilterPrimitiveDescription> mPrimitives;
};

already_AddRefed<FilterNode> FilterNodeGraphFromDescription(
    DrawTarget* aDT, const FilterDescription& aFilter,
    const Rect& aResultNeededRect, FilterNode* aSourceGraphic,
    const IntRect& aSourceGraphicRect, FilterNode* aFillPaint,
    FilterNode* aStrokePaint,
    nsTArray<RefPtr<SourceSurface>>& aAdditionalImages);

/**
 * The methods of this class are not on FilterDescription because
 * FilterDescription is designed as a simple value holder that can be used
 * on any thread.
 */
class FilterSupport {
 public:
  /**
   * Draw the filter described by aFilter. All rect parameters are in filter
   * space coordinates. aRenderRect specifies the part of the filter output
   * that will be drawn at (0, 0) into the draw target aDT, subject to the
   * current transform on aDT but with no additional scaling.
   * The source surfaces must match their corresponding rect in size.
   * aAdditionalImages carries the images that are referenced by the
   * eImageInputIndex attribute on any image primitives in the filter.
   */
  static void RenderFilterDescription(
      DrawTarget* aDT, const FilterDescription& aFilter,
      const Rect& aRenderRect, SourceSurface* aSourceGraphic,
      const IntRect& aSourceGraphicRect, SourceSurface* aFillPaint,
      const IntRect& aFillPaintRect, SourceSurface* aStrokePaint,
      const IntRect& aStrokePaintRect,
      nsTArray<RefPtr<SourceSurface>>& aAdditionalImages,
      const Point& aDestPoint, const DrawOptions& aOptions = DrawOptions());

  /**
   * Computes the region that changes in the filter output due to a change in
   * input.  This is primarily needed when an individual piece of content inside
   * a filtered container element changes.
   */
  static nsIntRegion ComputeResultChangeRegion(
      const FilterDescription& aFilter, const nsIntRegion& aSourceGraphicChange,
      const nsIntRegion& aFillPaintChange,
      const nsIntRegion& aStrokePaintChange);

  /**
   * Computes the regions that need to be supplied in the filter inputs when
   * painting aResultNeededRegion of the filter output.
   */
  static void ComputeSourceNeededRegions(
      const FilterDescription& aFilter, const nsIntRegion& aResultNeededRegion,
      nsIntRegion& aSourceGraphicNeededRegion,
      nsIntRegion& aFillPaintNeededRegion,
      nsIntRegion& aStrokePaintNeededRegion);

  /**
   * Computes the size of the filter output.
   */
  static nsIntRegion ComputePostFilterExtents(
      const FilterDescription& aFilter,
      const nsIntRegion& aSourceGraphicExtents);

  /**
   * Computes the size of a single FilterPrimitiveDescription's output given a
   * set of input extents.
   */
  static nsIntRegion PostFilterExtentsForPrimitive(
      const FilterPrimitiveDescription& aDescription,
      const nsTArray<nsIntRegion>& aInputExtents);
};

/**
 * Create a 4x5 color matrix for the different ways to specify color matrices
 * in SVG.
 *
 * Return false if the input is invalid or if the resulting matrix is the
 * identity.
 */
bool ComputeColorMatrix(const ColorMatrixAttributes& aMatrixAttributes,
                        float aOutMatrix[20]);

}  // namespace gfx
}  // namespace mozilla

#endif  // __FilterSupport_h
