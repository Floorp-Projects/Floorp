/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FilterSupport_h
#define __FilterSupport_h

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TypedEnum.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Matrix.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"
#include "nsRegion.h"

namespace mozilla {
namespace gfx {

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

// ComponentTransfer types
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN  = 0;
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY = 1;
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_TABLE    = 2;
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE = 3;
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_LINEAR   = 4;
const unsigned short SVG_FECOMPONENTTRANSFER_TYPE_GAMMA    = 5;

// Blend Mode Values
const unsigned short SVG_FEBLEND_MODE_UNKNOWN = 0;
const unsigned short SVG_FEBLEND_MODE_NORMAL = 1;
const unsigned short SVG_FEBLEND_MODE_MULTIPLY = 2;
const unsigned short SVG_FEBLEND_MODE_SCREEN = 3;
const unsigned short SVG_FEBLEND_MODE_DARKEN = 4;
const unsigned short SVG_FEBLEND_MODE_LIGHTEN = 5;

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

enum AttributeName {
  eBlendBlendmode = 0,
  eMorphologyRadii,
  eMorphologyOperator,
  eColorMatrixType,
  eColorMatrixValues,
  eFloodColor,
  eTileSourceRect,
  eComponentTransferFunctionR,
  eComponentTransferFunctionG,
  eComponentTransferFunctionB,
  eComponentTransferFunctionA,
  eComponentTransferFunctionType,
  eComponentTransferFunctionTableValues,
  eComponentTransferFunctionSlope,
  eComponentTransferFunctionIntercept,
  eComponentTransferFunctionAmplitude,
  eComponentTransferFunctionExponent,
  eComponentTransferFunctionOffset,
  eConvolveMatrixKernelSize,
  eConvolveMatrixKernelMatrix,
  eConvolveMatrixDivisor,
  eConvolveMatrixBias,
  eConvolveMatrixTarget,
  eConvolveMatrixEdgeMode,
  eConvolveMatrixKernelUnitLength,
  eConvolveMatrixPreserveAlpha,
  eOffsetOffset,
  eDropShadowStdDeviation,
  eDropShadowOffset,
  eDropShadowColor,
  eDisplacementMapScale,
  eDisplacementMapXChannel,
  eDisplacementMapYChannel,
  eTurbulenceOffset,
  eTurbulenceBaseFrequency,
  eTurbulenceNumOctaves,
  eTurbulenceSeed,
  eTurbulenceStitchable,
  eTurbulenceType,
  eCompositeOperator,
  eCompositeCoefficients,
  eGaussianBlurStdDeviation,
  eLightingLight,
  eLightingSurfaceScale,
  eLightingKernelUnitLength,
  eLightingColor,
  eDiffuseLightingDiffuseConstant,
  eSpecularLightingSpecularConstant,
  eSpecularLightingSpecularExponent,
  eLightType,
  eLightTypeNone,
  eLightTypePoint,
  eLightTypeSpot,
  eLightTypeDistant,
  ePointLightPosition,
  eSpotLightPosition,
  eSpotLightPointsAt,
  eSpotLightFocus,
  eSpotLightLimitingConeAngle,
  eDistantLightAzimuth,
  eDistantLightElevation,
  eImageInputIndex,
  eImageFilter,
  eImageNativeSize,
  eImageSubregion,
  eImageTransform,
  eLastAttributeName
};

class DrawTarget;
class SourceSurface;
class FilterNode;
struct FilterAttribute;

MOZ_BEGIN_ENUM_CLASS(AttributeType)
  eBool,
  eUint,
  eFloat,
  eSize,
  eIntSize,
  eIntPoint,
  eMatrix,
  eMatrix5x4,
  ePoint3D,
  eColor,
  eAttributeMap,
  eFloats,
  Max
MOZ_END_ENUM_CLASS(AttributeType)

// A class that stores values of different types, keyed by an attribute name.
// The Get*() methods assert that they're called for the same type that the
// attribute was Set() with.
// AttributeMaps can be nested because AttributeMap is a valid attribute type.
class AttributeMap MOZ_FINAL {
public:
  AttributeMap();
  AttributeMap(const AttributeMap& aOther);
  AttributeMap& operator=(const AttributeMap& aOther);
  bool operator==(const AttributeMap& aOther) const;
  bool operator!=(const AttributeMap& aOther) const
  {
    return !(*this == aOther);
  }
  ~AttributeMap();

  void Set(AttributeName aName, bool aValue);
  void Set(AttributeName aName, uint32_t aValue);
  void Set(AttributeName aName, float aValue);
  void Set(AttributeName aName, const Size& aValue);
  void Set(AttributeName aName, const IntSize& aValue);
  void Set(AttributeName aName, const IntPoint& aValue);
  void Set(AttributeName aName, const Matrix& aValue);
  void Set(AttributeName aName, const Matrix5x4& aValue);
  void Set(AttributeName aName, const Point3D& aValue);
  void Set(AttributeName aName, const Color& aValue);
  void Set(AttributeName aName, const AttributeMap& aValue);
  void Set(AttributeName aName, const float* aValues, int32_t aLength);

  bool GetBool(AttributeName aName) const;
  uint32_t GetUint(AttributeName aName) const;
  float GetFloat(AttributeName aName) const;
  Size GetSize(AttributeName aName) const;
  IntSize GetIntSize(AttributeName aName) const;
  IntPoint GetIntPoint(AttributeName aName) const;
  Matrix GetMatrix(AttributeName aName) const;
  Matrix5x4 GetMatrix5x4(AttributeName aName) const;
  Point3D GetPoint3D(AttributeName aName) const;
  Color GetColor(AttributeName aName) const;
  AttributeMap GetAttributeMap(AttributeName aName) const;
  const nsTArray<float>& GetFloats(AttributeName aName) const;

  typedef bool (*AttributeHandleCallback)(AttributeName aName, AttributeType aType, void* aUserData);
  void EnumerateRead(AttributeHandleCallback aCallback, void* aUserData) const;
  uint32_t Count() const;

private:
  mutable nsClassHashtable<nsUint32HashKey, FilterAttribute>  mMap;
};

MOZ_BEGIN_ENUM_CLASS(ColorSpace)
  SRGB,
  LinearRGB,
  Max
MOZ_END_ENUM_CLASS(ColorSpace)

MOZ_BEGIN_ENUM_CLASS(AlphaModel)
  Unpremultiplied,
  Premultiplied
MOZ_END_ENUM_CLASS(AlphaModel)

class ColorModel {
public:
  static ColorModel PremulSRGB()
  {
    return ColorModel(ColorSpace::SRGB, AlphaModel::Premultiplied);
  }

  ColorModel(ColorSpace aColorSpace, AlphaModel aAlphaModel) :
    mColorSpace(aColorSpace), mAlphaModel(aAlphaModel) {}
  ColorModel() :
    mColorSpace(ColorSpace::SRGB), mAlphaModel(AlphaModel::Premultiplied) {}
  bool operator==(const ColorModel& aOther) const {
    return mColorSpace == aOther.mColorSpace &&
           mAlphaModel == aOther.mAlphaModel;
  }

  // Used to index FilterCachedColorModels::mFilterForColorModel.
  uint8_t ToIndex() const
  {
    return (uint8_t(mColorSpace) << 1) + uint8_t(mAlphaModel);
  }

  ColorSpace mColorSpace;
  AlphaModel mAlphaModel;
};

MOZ_BEGIN_ENUM_CLASS(PrimitiveType)
  Empty = 0,
  Blend,
  Morphology,
  ColorMatrix,
  Flood,
  Tile,
  ComponentTransfer,
  ConvolveMatrix,
  Offset,
  DisplacementMap,
  Turbulence,
  Composite,
  Merge,
  Image,
  GaussianBlur,
  DropShadow,
  DiffuseLighting,
  SpecularLighting,
  Max
MOZ_END_ENUM_CLASS(PrimitiveType)

/**
 * A data structure to carry attributes for a given primitive that's part of a
 * filter. Will be serializable via IPDL, so it must not contain complex
 * functionality.
 * Used as part of a FilterDescription.
 */
class FilterPrimitiveDescription MOZ_FINAL {
public:
  enum {
    kPrimitiveIndexSourceGraphic = -1,
    kPrimitiveIndexSourceAlpha = -2,
    kPrimitiveIndexFillPaint = -3,
    kPrimitiveIndexStrokePaint = -4
  };

  FilterPrimitiveDescription(PrimitiveType aType);
  FilterPrimitiveDescription(const FilterPrimitiveDescription& aOther);
  FilterPrimitiveDescription& operator=(const FilterPrimitiveDescription& aOther);

  PrimitiveType Type() const { return mType; }
  const AttributeMap& Attributes() const { return mAttributes; }
  AttributeMap& Attributes() { return mAttributes; }

  IntRect PrimitiveSubregion() const { return mFilterPrimitiveSubregion; }
  bool IsTainted() const { return mIsTainted; }

  size_t NumberOfInputs() const { return mInputPrimitives.Length(); }
  int32_t InputPrimitiveIndex(size_t aInputIndex) const
  {
    return aInputIndex < mInputPrimitives.Length() ?
      mInputPrimitives[aInputIndex] : 0;
  }

  ColorSpace InputColorSpace(size_t aInputIndex) const
  {
    return aInputIndex < mInputColorSpaces.Length() ?
      mInputColorSpaces[aInputIndex] : ColorSpace();
  }

  ColorSpace OutputColorSpace() const { return mOutputColorSpace; }

  void SetPrimitiveSubregion(const IntRect& aRect)
  {
    mFilterPrimitiveSubregion = aRect;
  }

  void SetIsTainted(bool aIsTainted)
  {
    mIsTainted = aIsTainted;
  }

  void SetInputPrimitive(size_t aInputIndex, int32_t aInputPrimitiveIndex)
  {
    mInputPrimitives.EnsureLengthAtLeast(aInputIndex + 1);
    mInputPrimitives[aInputIndex] = aInputPrimitiveIndex;
  }

  void SetInputColorSpace(size_t aInputIndex, ColorSpace aColorSpace)
  {
    mInputColorSpaces.EnsureLengthAtLeast(aInputIndex + 1);
    mInputColorSpaces[aInputIndex] = aColorSpace;
  }

  void SetOutputColorSpace(const ColorSpace& aColorSpace)
  {
    mOutputColorSpace = aColorSpace;
  }

  bool operator==(const FilterPrimitiveDescription& aOther) const;
  bool operator!=(const FilterPrimitiveDescription& aOther) const
  {
    return !(*this == aOther);
  }

private:
  PrimitiveType mType;
  AttributeMap mAttributes;
  nsTArray<int32_t> mInputPrimitives;
  IntRect mFilterPrimitiveSubregion;
  nsTArray<ColorSpace> mInputColorSpaces;
  ColorSpace mOutputColorSpace;
  bool mIsTainted;
};

/**
 * A data structure that contains one or more FilterPrimitiveDescriptions.
 * Designed to be serializable via IPDL, so it must not contain complex
 * functionality.
 */
struct FilterDescription MOZ_FINAL {
  FilterDescription() {}
  FilterDescription(const nsTArray<FilterPrimitiveDescription>& aPrimitives,
                    const IntRect& aFilterSpaceBounds)
   : mPrimitives(aPrimitives)
   , mFilterSpaceBounds(aFilterSpaceBounds)
  {}

  bool operator==(const FilterDescription& aOther) const;
  bool operator!=(const FilterDescription& aOther) const
  {
    return !(*this == aOther);
  }

  nsTArray<FilterPrimitiveDescription> mPrimitives;
  IntRect mFilterSpaceBounds;
};

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
  static void
  RenderFilterDescription(DrawTarget* aDT,
                          const FilterDescription& aFilter,
                          const Rect& aRenderRect,
                          SourceSurface* aSourceGraphic,
                          const IntRect& aSourceGraphicRect,
                          SourceSurface* aFillPaint,
                          const IntRect& aFillPaintRect,
                          SourceSurface* aStrokePaint,
                          const IntRect& aStrokePaintRect,
                          nsTArray<RefPtr<SourceSurface>>& aAdditionalImages);

  /**
   * Computes the region that changes in the filter output due to a change in
   * input.
   */
  static nsIntRegion
  ComputeResultChangeRegion(const FilterDescription& aFilter,
                            const nsIntRegion& aSourceGraphicChange,
                            const nsIntRegion& aFillPaintChange,
                            const nsIntRegion& aStrokePaintChange);

  /**
   * Computes the regions that need to be supplied in the filter inputs when
   * painting aResultNeededRegion of the filter output.
   */
  static void
  ComputeSourceNeededRegions(const FilterDescription& aFilter,
                             const nsIntRegion& aResultNeededRegion,
                             nsIntRegion& aSourceGraphicNeededRegion,
                             nsIntRegion& aFillPaintNeededRegion,
                             nsIntRegion& aStrokePaintNeededRegion);

  /**
   * Computes the size of the filter output.
   */
  static nsIntRegion
  ComputePostFilterExtents(const FilterDescription& aFilter,
                           const nsIntRegion& aSourceGraphicExtents);

};

}
}

#endif // __FilterSupport_h
