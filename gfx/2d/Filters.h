/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FILTERS_H_
#define MOZILLA_GFX_FILTERS_H_

#include "Types.h"
#include "mozilla/RefPtr.h"

#include "Point.h"
#include "Matrix.h"
#include <vector>

namespace mozilla {
namespace gfx {

class SourceSurface;

enum FilterBackend {
  FILTER_BACKEND_SOFTWARE = 0,
  FILTER_BACKEND_DIRECT2D1_1,
  FILTER_BACKEND_RECORDING,
  FILTER_BACKEND_CAPTURE
};

enum TransformFilterAtts
{
  ATT_TRANSFORM_MATRIX = 0,                 // Matrix
  ATT_TRANSFORM_FILTER                      // Filter
};

enum TransformFilterInputs
{
  IN_TRANSFORM_IN = 0
};

enum BlendFilterAtts
{
  ATT_BLEND_BLENDMODE = 0                   // uint32_t
};

enum BlendMode
{
  BLEND_MODE_MULTIPLY = 0,
  BLEND_MODE_SCREEN,
  BLEND_MODE_DARKEN,
  BLEND_MODE_LIGHTEN,
  BLEND_MODE_OVERLAY,
  BLEND_MODE_COLOR_DODGE,
  BLEND_MODE_COLOR_BURN,
  BLEND_MODE_HARD_LIGHT,
  BLEND_MODE_SOFT_LIGHT,
  BLEND_MODE_DIFFERENCE,
  BLEND_MODE_EXCLUSION,
  BLEND_MODE_HUE,
  BLEND_MODE_SATURATION,
  BLEND_MODE_COLOR,
  BLEND_MODE_LUMINOSITY
};

enum BlendFilterInputs
{
  IN_BLEND_IN = 0,
  IN_BLEND_IN2
};

enum MorphologyFilterAtts
{
  ATT_MORPHOLOGY_RADII = 0,                 // IntSize
  ATT_MORPHOLOGY_OPERATOR                   // MorphologyOperator
};

enum MorphologyOperator
{
  MORPHOLOGY_OPERATOR_ERODE = 0,
  MORPHOLOGY_OPERATOR_DILATE
};

enum MorphologyFilterInputs
{
  IN_MORPHOLOGY_IN = 0
};

enum AlphaMode
{
  ALPHA_MODE_PREMULTIPLIED = 0,
  ALPHA_MODE_STRAIGHT
};

enum ColorMatrixFilterAtts
{
  ATT_COLOR_MATRIX_MATRIX = 0,              // Matrix5x4
  ATT_COLOR_MATRIX_ALPHA_MODE               // AlphaMode
};

enum ColorMatrixFilterInputs
{
  IN_COLOR_MATRIX_IN = 0
};

enum FloodFilterAtts
{
  ATT_FLOOD_COLOR = 0                       // Color
};

enum FloodFilterInputs
{
  IN_FLOOD_IN = 0
};

enum TileFilterAtts
{
  ATT_TILE_SOURCE_RECT = 0                  // IntRect
};

enum TileFilterInputs
{
  IN_TILE_IN = 0
};

enum TransferAtts
{
  ATT_TRANSFER_DISABLE_R = 0,               // bool
  ATT_TRANSFER_DISABLE_G,                   // bool
  ATT_TRANSFER_DISABLE_B,                   // bool
  ATT_TRANSFER_DISABLE_A                    // bool
};

enum TransferInputs
{
  IN_TRANSFER_IN = 0
};

enum TableTransferAtts
{
  ATT_TABLE_TRANSFER_DISABLE_R = ATT_TRANSFER_DISABLE_R,
  ATT_TABLE_TRANSFER_DISABLE_G = ATT_TRANSFER_DISABLE_G,
  ATT_TABLE_TRANSFER_DISABLE_B = ATT_TRANSFER_DISABLE_B,
  ATT_TABLE_TRANSFER_DISABLE_A = ATT_TRANSFER_DISABLE_A,
  ATT_TABLE_TRANSFER_TABLE_R,               // Float[]
  ATT_TABLE_TRANSFER_TABLE_G,               // Float[]
  ATT_TABLE_TRANSFER_TABLE_B,               // Float[]
  ATT_TABLE_TRANSFER_TABLE_A                // Float[]
};

enum TableTransferInputs
{
  IN_TABLE_TRANSFER_IN = IN_TRANSFER_IN
};

enum DiscreteTransferAtts
{
  ATT_DISCRETE_TRANSFER_DISABLE_R = ATT_TRANSFER_DISABLE_R,
  ATT_DISCRETE_TRANSFER_DISABLE_G = ATT_TRANSFER_DISABLE_G,
  ATT_DISCRETE_TRANSFER_DISABLE_B = ATT_TRANSFER_DISABLE_B,
  ATT_DISCRETE_TRANSFER_DISABLE_A = ATT_TRANSFER_DISABLE_A,
  ATT_DISCRETE_TRANSFER_TABLE_R,            // Float[]
  ATT_DISCRETE_TRANSFER_TABLE_G,            // Float[]
  ATT_DISCRETE_TRANSFER_TABLE_B,            // Float[]
  ATT_DISCRETE_TRANSFER_TABLE_A             // Float[]
};

enum DiscreteTransferInputs
{
  IN_DISCRETE_TRANSFER_IN = IN_TRANSFER_IN
};

enum LinearTransferAtts
{
  ATT_LINEAR_TRANSFER_DISABLE_R = ATT_TRANSFER_DISABLE_R,
  ATT_LINEAR_TRANSFER_DISABLE_G = ATT_TRANSFER_DISABLE_G,
  ATT_LINEAR_TRANSFER_DISABLE_B = ATT_TRANSFER_DISABLE_B,
  ATT_LINEAR_TRANSFER_DISABLE_A = ATT_TRANSFER_DISABLE_A,
  ATT_LINEAR_TRANSFER_SLOPE_R,              // Float
  ATT_LINEAR_TRANSFER_SLOPE_G,              // Float
  ATT_LINEAR_TRANSFER_SLOPE_B,              // Float
  ATT_LINEAR_TRANSFER_SLOPE_A,              // Float
  ATT_LINEAR_TRANSFER_INTERCEPT_R,          // Float
  ATT_LINEAR_TRANSFER_INTERCEPT_G,          // Float
  ATT_LINEAR_TRANSFER_INTERCEPT_B,          // Float
  ATT_LINEAR_TRANSFER_INTERCEPT_A           // Float
};

enum LinearTransferInputs
{
  IN_LINEAR_TRANSFER_IN = IN_TRANSFER_IN
};

enum GammaTransferAtts
{
  ATT_GAMMA_TRANSFER_DISABLE_R = ATT_TRANSFER_DISABLE_R,
  ATT_GAMMA_TRANSFER_DISABLE_G = ATT_TRANSFER_DISABLE_G,
  ATT_GAMMA_TRANSFER_DISABLE_B = ATT_TRANSFER_DISABLE_B,
  ATT_GAMMA_TRANSFER_DISABLE_A = ATT_TRANSFER_DISABLE_A,
  ATT_GAMMA_TRANSFER_AMPLITUDE_R,             // Float
  ATT_GAMMA_TRANSFER_AMPLITUDE_G,             // Float
  ATT_GAMMA_TRANSFER_AMPLITUDE_B,             // Float
  ATT_GAMMA_TRANSFER_AMPLITUDE_A,             // Float
  ATT_GAMMA_TRANSFER_EXPONENT_R,              // Float
  ATT_GAMMA_TRANSFER_EXPONENT_G,              // Float
  ATT_GAMMA_TRANSFER_EXPONENT_B,              // Float
  ATT_GAMMA_TRANSFER_EXPONENT_A,              // Float
  ATT_GAMMA_TRANSFER_OFFSET_R,                // Float
  ATT_GAMMA_TRANSFER_OFFSET_G,                // Float
  ATT_GAMMA_TRANSFER_OFFSET_B,                // Float
  ATT_GAMMA_TRANSFER_OFFSET_A                 // Float
};

enum GammaTransferInputs
{
  IN_GAMMA_TRANSFER_IN = IN_TRANSFER_IN
};

enum ConvolveMatrixAtts
{
  ATT_CONVOLVE_MATRIX_KERNEL_SIZE = 0,      // IntSize
  ATT_CONVOLVE_MATRIX_KERNEL_MATRIX,        // Float[]
  ATT_CONVOLVE_MATRIX_DIVISOR,              // Float
  ATT_CONVOLVE_MATRIX_BIAS,                 // Float
  ATT_CONVOLVE_MATRIX_TARGET,               // IntPoint
  ATT_CONVOLVE_MATRIX_SOURCE_RECT,          // IntRect
  ATT_CONVOLVE_MATRIX_EDGE_MODE,            // ConvolveMatrixEdgeMode
  ATT_CONVOLVE_MATRIX_KERNEL_UNIT_LENGTH,   // Size
  ATT_CONVOLVE_MATRIX_PRESERVE_ALPHA,       // bool
};

enum ConvolveMatrixEdgeMode
{
  EDGE_MODE_DUPLICATE = 0,
  EDGE_MODE_WRAP,
  EDGE_MODE_NONE
};

enum ConvolveMatrixInputs
{
  IN_CONVOLVE_MATRIX_IN = 0
};

enum DisplacementMapAtts
{
  ATT_DISPLACEMENT_MAP_SCALE = 0,           // Float
  ATT_DISPLACEMENT_MAP_X_CHANNEL,           // ColorChannel
  ATT_DISPLACEMENT_MAP_Y_CHANNEL            // ColorChannel
};

enum ColorChannel
{
  COLOR_CHANNEL_R = 0,
  COLOR_CHANNEL_G,
  COLOR_CHANNEL_B,
  COLOR_CHANNEL_A
};

enum DisplacementMapInputs
{
  IN_DISPLACEMENT_MAP_IN = 0,
  IN_DISPLACEMENT_MAP_IN2
};

enum TurbulenceAtts
{
  ATT_TURBULENCE_BASE_FREQUENCY = 0,        // Size
  ATT_TURBULENCE_NUM_OCTAVES,               // uint32_t
  ATT_TURBULENCE_SEED,                      // uint32_t
  ATT_TURBULENCE_STITCHABLE,                // bool
  ATT_TURBULENCE_TYPE,                      // TurbulenceType
  ATT_TURBULENCE_RECT                       // IntRect
};

enum TurbulenceType
{
  TURBULENCE_TYPE_TURBULENCE = 0,
  TURBULENCE_TYPE_FRACTAL_NOISE
};

enum ArithmeticCombineAtts
{
  ATT_ARITHMETIC_COMBINE_COEFFICIENTS = 0   // Float[4]
};

enum ArithmeticCombineInputs
{
  IN_ARITHMETIC_COMBINE_IN = 0,
  IN_ARITHMETIC_COMBINE_IN2
};

enum CompositeAtts
{
  ATT_COMPOSITE_OPERATOR = 0                // CompositeOperator
};

enum CompositeOperator
{
  COMPOSITE_OPERATOR_OVER = 0,
  COMPOSITE_OPERATOR_IN,
  COMPOSITE_OPERATOR_OUT,
  COMPOSITE_OPERATOR_ATOP,
  COMPOSITE_OPERATOR_XOR
};

enum CompositeInputs
{
  // arbitrary number of inputs
  IN_COMPOSITE_IN_START = 0
};

enum GaussianBlurAtts
{
  ATT_GAUSSIAN_BLUR_STD_DEVIATION = 0       // Float
};

enum GaussianBlurInputs
{
  IN_GAUSSIAN_BLUR_IN = 0
};

enum DirectionalBlurAtts
{
  ATT_DIRECTIONAL_BLUR_STD_DEVIATION = 0,   // Float
  ATT_DIRECTIONAL_BLUR_DIRECTION            // BlurDirection
};

enum BlurDirection
{
  BLUR_DIRECTION_X = 0,
  BLUR_DIRECTION_Y
};

enum DirectionalBlurInputs
{
  IN_DIRECTIONAL_BLUR_IN = 0
};

enum LightingAtts
{
  ATT_POINT_LIGHT_POSITION = 0,             // Point3D

  ATT_SPOT_LIGHT_POSITION,                  // Point3D
  ATT_SPOT_LIGHT_POINTS_AT,                 // Point3D
  ATT_SPOT_LIGHT_FOCUS,                     // Float
  ATT_SPOT_LIGHT_LIMITING_CONE_ANGLE,       // Float

  ATT_DISTANT_LIGHT_AZIMUTH,                // Float
  ATT_DISTANT_LIGHT_ELEVATION,              // Float

  ATT_LIGHTING_COLOR,                       // Color
  ATT_LIGHTING_SURFACE_SCALE,               // Float
  ATT_LIGHTING_KERNEL_UNIT_LENGTH,          // Size

  ATT_DIFFUSE_LIGHTING_DIFFUSE_CONSTANT,    // Float

  ATT_SPECULAR_LIGHTING_SPECULAR_CONSTANT,  // Float
  ATT_SPECULAR_LIGHTING_SPECULAR_EXPONENT   // Float
};

enum LightingInputs
{
  IN_LIGHTING_IN = 0
};

enum PointDiffuseAtts
{
  ATT_POINT_DIFFUSE_POSITION              = ATT_POINT_LIGHT_POSITION,
  ATT_POINT_DIFFUSE_COLOR                 = ATT_LIGHTING_COLOR,
  ATT_POINT_DIFFUSE_SURFACE_SCALE         = ATT_LIGHTING_SURFACE_SCALE,
  ATT_POINT_DIFFUSE_KERNEL_UNIT_LENGTH    = ATT_LIGHTING_KERNEL_UNIT_LENGTH,
  ATT_POINT_DIFFUSE_DIFFUSE_CONSTANT      = ATT_DIFFUSE_LIGHTING_DIFFUSE_CONSTANT
};

enum PointDiffuseInputs
{
  IN_POINT_DIFFUSE_IN = IN_LIGHTING_IN
};

enum SpotDiffuseAtts
{
  ATT_SPOT_DIFFUSE_POSITION               = ATT_SPOT_LIGHT_POSITION,
  ATT_SPOT_DIFFUSE_POINTS_AT              = ATT_SPOT_LIGHT_POINTS_AT,
  ATT_SPOT_DIFFUSE_FOCUS                  = ATT_SPOT_LIGHT_FOCUS,
  ATT_SPOT_DIFFUSE_LIMITING_CONE_ANGLE    = ATT_SPOT_LIGHT_LIMITING_CONE_ANGLE,
  ATT_SPOT_DIFFUSE_COLOR                  = ATT_LIGHTING_COLOR,
  ATT_SPOT_DIFFUSE_SURFACE_SCALE          = ATT_LIGHTING_SURFACE_SCALE,
  ATT_SPOT_DIFFUSE_KERNEL_UNIT_LENGTH     = ATT_LIGHTING_KERNEL_UNIT_LENGTH,
  ATT_SPOT_DIFFUSE_DIFFUSE_CONSTANT       = ATT_DIFFUSE_LIGHTING_DIFFUSE_CONSTANT
};

enum SpotDiffuseInputs
{
  IN_SPOT_DIFFUSE_IN = IN_LIGHTING_IN
};

enum DistantDiffuseAtts
{
  ATT_DISTANT_DIFFUSE_AZIMUTH             = ATT_DISTANT_LIGHT_AZIMUTH,
  ATT_DISTANT_DIFFUSE_ELEVATION           = ATT_DISTANT_LIGHT_ELEVATION,
  ATT_DISTANT_DIFFUSE_COLOR               = ATT_LIGHTING_COLOR,
  ATT_DISTANT_DIFFUSE_SURFACE_SCALE       = ATT_LIGHTING_SURFACE_SCALE,
  ATT_DISTANT_DIFFUSE_KERNEL_UNIT_LENGTH  = ATT_LIGHTING_KERNEL_UNIT_LENGTH,
  ATT_DISTANT_DIFFUSE_DIFFUSE_CONSTANT    = ATT_DIFFUSE_LIGHTING_DIFFUSE_CONSTANT
};

enum DistantDiffuseInputs
{
  IN_DISTANT_DIFFUSE_IN = IN_LIGHTING_IN
};

enum PointSpecularAtts
{
  ATT_POINT_SPECULAR_POSITION             = ATT_POINT_LIGHT_POSITION,
  ATT_POINT_SPECULAR_COLOR                = ATT_LIGHTING_COLOR,
  ATT_POINT_SPECULAR_SURFACE_SCALE        = ATT_LIGHTING_SURFACE_SCALE,
  ATT_POINT_SPECULAR_KERNEL_UNIT_LENGTH   = ATT_LIGHTING_KERNEL_UNIT_LENGTH,
  ATT_POINT_SPECULAR_SPECULAR_CONSTANT    = ATT_SPECULAR_LIGHTING_SPECULAR_CONSTANT,
  ATT_POINT_SPECULAR_SPECULAR_EXPONENT    = ATT_SPECULAR_LIGHTING_SPECULAR_EXPONENT
};

enum PointSpecularInputs
{
  IN_POINT_SPECULAR_IN = IN_LIGHTING_IN
};

enum SpotSpecularAtts
{
  ATT_SPOT_SPECULAR_POSITION              = ATT_SPOT_LIGHT_POSITION,
  ATT_SPOT_SPECULAR_POINTS_AT             = ATT_SPOT_LIGHT_POINTS_AT,
  ATT_SPOT_SPECULAR_FOCUS                 = ATT_SPOT_LIGHT_FOCUS,
  ATT_SPOT_SPECULAR_LIMITING_CONE_ANGLE   = ATT_SPOT_LIGHT_LIMITING_CONE_ANGLE,
  ATT_SPOT_SPECULAR_COLOR                 = ATT_LIGHTING_COLOR,
  ATT_SPOT_SPECULAR_SURFACE_SCALE         = ATT_LIGHTING_SURFACE_SCALE,
  ATT_SPOT_SPECULAR_KERNEL_UNIT_LENGTH    = ATT_LIGHTING_KERNEL_UNIT_LENGTH,
  ATT_SPOT_SPECULAR_SPECULAR_CONSTANT     = ATT_SPECULAR_LIGHTING_SPECULAR_CONSTANT,
  ATT_SPOT_SPECULAR_SPECULAR_EXPONENT     = ATT_SPECULAR_LIGHTING_SPECULAR_EXPONENT
};

enum SpotSpecularInputs
{
  IN_SPOT_SPECULAR_IN = IN_LIGHTING_IN
};

enum DistantSpecularAtts
{
  ATT_DISTANT_SPECULAR_AZIMUTH            = ATT_DISTANT_LIGHT_AZIMUTH,
  ATT_DISTANT_SPECULAR_ELEVATION          = ATT_DISTANT_LIGHT_ELEVATION,
  ATT_DISTANT_SPECULAR_COLOR              = ATT_LIGHTING_COLOR,
  ATT_DISTANT_SPECULAR_SURFACE_SCALE      = ATT_LIGHTING_SURFACE_SCALE,
  ATT_DISTANT_SPECULAR_KERNEL_UNIT_LENGTH = ATT_LIGHTING_KERNEL_UNIT_LENGTH,
  ATT_DISTANT_SPECULAR_SPECULAR_CONSTANT  = ATT_SPECULAR_LIGHTING_SPECULAR_CONSTANT,
  ATT_DISTANT_SPECULAR_SPECULAR_EXPONENT  = ATT_SPECULAR_LIGHTING_SPECULAR_EXPONENT
};

enum DistantSpecularInputs
{
  IN_DISTANT_SPECULAR_IN = IN_LIGHTING_IN
};

enum CropAtts
{
  ATT_CROP_RECT = 0                         // Rect
};

enum CropInputs
{
  IN_CROP_IN = 0
};

enum PremultiplyInputs
{
  IN_PREMULTIPLY_IN = 0
};

enum UnpremultiplyInputs
{
  IN_UNPREMULTIPLY_IN = 0
};

enum OpacityAtts
{
  ATT_OPACITY_VALUE = 0
};

enum OpacityInputs
{
  IN_OPACITY_IN = 0
};

class FilterNode : public external::AtomicRefCounted<FilterNode>
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNode)
  virtual ~FilterNode() {}

  virtual FilterBackend GetBackendType() = 0;

  virtual void SetInput(uint32_t aIndex, SourceSurface *aSurface) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetInput(uint32_t aIndex, FilterNode *aFilter) { MOZ_CRASH("GFX: FilterNode"); }

  virtual void SetAttribute(uint32_t aIndex, bool) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, uint32_t) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, Float) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const Size &) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const IntSize &) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const IntPoint &) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const Rect &) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const IntRect &) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const Point &) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const Matrix &) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const Matrix5x4 &) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const Point3D &) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const Color &) { MOZ_CRASH("GFX: FilterNode"); }
  virtual void SetAttribute(uint32_t aIndex, const Float* aFloat, uint32_t aSize) { MOZ_CRASH("GFX: FilterNode"); }

protected:
  friend class Factory;

  FilterNode() {}
};

} // namespace gfx
} // namespace mozilla

#endif
