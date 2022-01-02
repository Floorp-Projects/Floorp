/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilterNodeD2D1.h"

#include "Logging.h"

#include "SourceSurfaceD2D1.h"
#include "DrawTargetD2D1.h"
#include "ExtendInputEffectD2D1.h"

namespace mozilla {
namespace gfx {

D2D1_COLORMATRIX_ALPHA_MODE D2DAlphaMode(uint32_t aMode) {
  switch (aMode) {
    case ALPHA_MODE_PREMULTIPLIED:
      return D2D1_COLORMATRIX_ALPHA_MODE_PREMULTIPLIED;
    case ALPHA_MODE_STRAIGHT:
      return D2D1_COLORMATRIX_ALPHA_MODE_STRAIGHT;
    default:
      MOZ_CRASH("GFX: Unknown enum value D2DAlphaMode!");
  }

  return D2D1_COLORMATRIX_ALPHA_MODE_PREMULTIPLIED;
}

D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE D2DAffineTransformInterpolationMode(
    SamplingFilter aSamplingFilter) {
  switch (aSamplingFilter) {
    case SamplingFilter::GOOD:
      return D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE_LINEAR;
    case SamplingFilter::LINEAR:
      return D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE_LINEAR;
    case SamplingFilter::POINT:
      return D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    default:
      MOZ_CRASH("GFX: Unknown enum value D2DAffineTIM!");
  }

  return D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE_LINEAR;
}

D2D1_BLEND_MODE D2DBlendMode(uint32_t aMode) {
  switch (aMode) {
    case BLEND_MODE_DARKEN:
      return D2D1_BLEND_MODE_DARKEN;
    case BLEND_MODE_LIGHTEN:
      return D2D1_BLEND_MODE_LIGHTEN;
    case BLEND_MODE_MULTIPLY:
      return D2D1_BLEND_MODE_MULTIPLY;
    case BLEND_MODE_SCREEN:
      return D2D1_BLEND_MODE_SCREEN;
    case BLEND_MODE_OVERLAY:
      return D2D1_BLEND_MODE_OVERLAY;
    case BLEND_MODE_COLOR_DODGE:
      return D2D1_BLEND_MODE_COLOR_DODGE;
    case BLEND_MODE_COLOR_BURN:
      return D2D1_BLEND_MODE_COLOR_BURN;
    case BLEND_MODE_HARD_LIGHT:
      return D2D1_BLEND_MODE_HARD_LIGHT;
    case BLEND_MODE_SOFT_LIGHT:
      return D2D1_BLEND_MODE_SOFT_LIGHT;
    case BLEND_MODE_DIFFERENCE:
      return D2D1_BLEND_MODE_DIFFERENCE;
    case BLEND_MODE_EXCLUSION:
      return D2D1_BLEND_MODE_EXCLUSION;
    case BLEND_MODE_HUE:
      return D2D1_BLEND_MODE_HUE;
    case BLEND_MODE_SATURATION:
      return D2D1_BLEND_MODE_SATURATION;
    case BLEND_MODE_COLOR:
      return D2D1_BLEND_MODE_COLOR;
    case BLEND_MODE_LUMINOSITY:
      return D2D1_BLEND_MODE_LUMINOSITY;

    default:
      MOZ_CRASH("GFX: Unknown enum value D2DBlendMode!");
  }

  return D2D1_BLEND_MODE_DARKEN;
}

D2D1_MORPHOLOGY_MODE D2DMorphologyMode(uint32_t aMode) {
  switch (aMode) {
    case MORPHOLOGY_OPERATOR_DILATE:
      return D2D1_MORPHOLOGY_MODE_DILATE;
    case MORPHOLOGY_OPERATOR_ERODE:
      return D2D1_MORPHOLOGY_MODE_ERODE;
  }

  MOZ_CRASH("GFX: Unknown enum value D2DMorphologyMode!");
  return D2D1_MORPHOLOGY_MODE_DILATE;
}

D2D1_TURBULENCE_NOISE D2DTurbulenceNoise(uint32_t aMode) {
  switch (aMode) {
    case TURBULENCE_TYPE_FRACTAL_NOISE:
      return D2D1_TURBULENCE_NOISE_FRACTAL_SUM;
    case TURBULENCE_TYPE_TURBULENCE:
      return D2D1_TURBULENCE_NOISE_TURBULENCE;
  }

  MOZ_CRASH("GFX: Unknown enum value D2DTurbulenceNoise!");
  return D2D1_TURBULENCE_NOISE_TURBULENCE;
}

D2D1_COMPOSITE_MODE D2DFilterCompositionMode(uint32_t aMode) {
  switch (aMode) {
    case COMPOSITE_OPERATOR_OVER:
      return D2D1_COMPOSITE_MODE_SOURCE_OVER;
    case COMPOSITE_OPERATOR_IN:
      return D2D1_COMPOSITE_MODE_SOURCE_IN;
    case COMPOSITE_OPERATOR_OUT:
      return D2D1_COMPOSITE_MODE_SOURCE_OUT;
    case COMPOSITE_OPERATOR_ATOP:
      return D2D1_COMPOSITE_MODE_SOURCE_ATOP;
    case COMPOSITE_OPERATOR_XOR:
      return D2D1_COMPOSITE_MODE_XOR;
    case COMPOSITE_OPERATOR_LIGHTER:
      return D2D1_COMPOSITE_MODE_PLUS;
  }

  MOZ_CRASH("GFX: Unknown enum value D2DFilterCompositionMode!");
  return D2D1_COMPOSITE_MODE_SOURCE_OVER;
}

D2D1_CHANNEL_SELECTOR D2DChannelSelector(uint32_t aMode) {
  switch (aMode) {
    case COLOR_CHANNEL_R:
      return D2D1_CHANNEL_SELECTOR_R;
    case COLOR_CHANNEL_G:
      return D2D1_CHANNEL_SELECTOR_G;
    case COLOR_CHANNEL_B:
      return D2D1_CHANNEL_SELECTOR_B;
    case COLOR_CHANNEL_A:
      return D2D1_CHANNEL_SELECTOR_A;
  }

  MOZ_CRASH("GFX: Unknown enum value D2DChannelSelector!");
  return D2D1_CHANNEL_SELECTOR_R;
}

already_AddRefed<ID2D1Image> GetImageForSourceSurface(DrawTarget* aDT,
                                                      SourceSurface* aSurface) {
  if (aDT->IsTiledDrawTarget()) {
    gfxDevCrash(LogReason::FilterNodeD2D1Target)
        << "Incompatible draw target type! " << (int)aDT->IsTiledDrawTarget();
    return nullptr;
  }
  switch (aDT->GetBackendType()) {
    case BackendType::DIRECT2D1_1:
      return static_cast<DrawTargetD2D1*>(aDT)->GetImageForSurface(
          aSurface, ExtendMode::CLAMP);
    default:
      gfxDevCrash(LogReason::FilterNodeD2D1Backend)
          << "Unknown draw target type! " << (int)aDT->GetBackendType();
      return nullptr;
  }
}

uint32_t ConvertValue(FilterType aType, uint32_t aAttribute, uint32_t aValue) {
  switch (aType) {
    case FilterType::COLOR_MATRIX:
      if (aAttribute == ATT_COLOR_MATRIX_ALPHA_MODE) {
        aValue = D2DAlphaMode(aValue);
      }
      break;
    case FilterType::TRANSFORM:
      if (aAttribute == ATT_TRANSFORM_FILTER) {
        aValue = D2DAffineTransformInterpolationMode(SamplingFilter(aValue));
      }
      break;
    case FilterType::BLEND:
      if (aAttribute == ATT_BLEND_BLENDMODE) {
        aValue = D2DBlendMode(aValue);
      }
      break;
    case FilterType::MORPHOLOGY:
      if (aAttribute == ATT_MORPHOLOGY_OPERATOR) {
        aValue = D2DMorphologyMode(aValue);
      }
      break;
    case FilterType::DISPLACEMENT_MAP:
      if (aAttribute == ATT_DISPLACEMENT_MAP_X_CHANNEL ||
          aAttribute == ATT_DISPLACEMENT_MAP_Y_CHANNEL) {
        aValue = D2DChannelSelector(aValue);
      }
      break;
    case FilterType::TURBULENCE:
      if (aAttribute == ATT_TURBULENCE_TYPE) {
        aValue = D2DTurbulenceNoise(aValue);
      }
      break;
    case FilterType::COMPOSITE:
      if (aAttribute == ATT_COMPOSITE_OPERATOR) {
        aValue = D2DFilterCompositionMode(aValue);
      }
      break;
    default:
      break;
  }

  return aValue;
}

void ConvertValue(FilterType aType, uint32_t aAttribute, IntSize& aValue) {
  switch (aType) {
    case FilterType::MORPHOLOGY:
      if (aAttribute == ATT_MORPHOLOGY_RADII) {
        aValue.width *= 2;
        aValue.width += 1;
        aValue.height *= 2;
        aValue.height += 1;
      }
      break;
    default:
      break;
  }
}

UINT32
GetD2D1InputForInput(FilterType aType, uint32_t aIndex) { return aIndex; }

#define CONVERT_PROP(moz2dname, d2dname) \
  case ATT_##moz2dname:                  \
    return D2D1_##d2dname

UINT32
GetD2D1PropForAttribute(FilterType aType, uint32_t aIndex) {
  switch (aType) {
    case FilterType::COLOR_MATRIX:
      switch (aIndex) {
        CONVERT_PROP(COLOR_MATRIX_MATRIX, COLORMATRIX_PROP_COLOR_MATRIX);
        CONVERT_PROP(COLOR_MATRIX_ALPHA_MODE, COLORMATRIX_PROP_ALPHA_MODE);
      }
      break;
    case FilterType::TRANSFORM:
      switch (aIndex) {
        CONVERT_PROP(TRANSFORM_MATRIX, 2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX);
        CONVERT_PROP(TRANSFORM_FILTER,
                     2DAFFINETRANSFORM_PROP_INTERPOLATION_MODE);
      }
    case FilterType::BLEND:
      switch (aIndex) { CONVERT_PROP(BLEND_BLENDMODE, BLEND_PROP_MODE); }
      break;
    case FilterType::MORPHOLOGY:
      switch (aIndex) {
        CONVERT_PROP(MORPHOLOGY_OPERATOR, MORPHOLOGY_PROP_MODE);
      }
      break;
    case FilterType::FLOOD:
      switch (aIndex) { CONVERT_PROP(FLOOD_COLOR, FLOOD_PROP_COLOR); }
      break;
    case FilterType::TILE:
      switch (aIndex) { CONVERT_PROP(TILE_SOURCE_RECT, TILE_PROP_RECT); }
      break;
    case FilterType::TABLE_TRANSFER:
      switch (aIndex) {
        CONVERT_PROP(TABLE_TRANSFER_DISABLE_R, TABLETRANSFER_PROP_RED_DISABLE);
        CONVERT_PROP(TABLE_TRANSFER_DISABLE_G,
                     TABLETRANSFER_PROP_GREEN_DISABLE);
        CONVERT_PROP(TABLE_TRANSFER_DISABLE_B, TABLETRANSFER_PROP_BLUE_DISABLE);
        CONVERT_PROP(TABLE_TRANSFER_DISABLE_A,
                     TABLETRANSFER_PROP_ALPHA_DISABLE);
        CONVERT_PROP(TABLE_TRANSFER_TABLE_R, TABLETRANSFER_PROP_RED_TABLE);
        CONVERT_PROP(TABLE_TRANSFER_TABLE_G, TABLETRANSFER_PROP_GREEN_TABLE);
        CONVERT_PROP(TABLE_TRANSFER_TABLE_B, TABLETRANSFER_PROP_BLUE_TABLE);
        CONVERT_PROP(TABLE_TRANSFER_TABLE_A, TABLETRANSFER_PROP_ALPHA_TABLE);
      }
      break;
    case FilterType::DISCRETE_TRANSFER:
      switch (aIndex) {
        CONVERT_PROP(DISCRETE_TRANSFER_DISABLE_R,
                     DISCRETETRANSFER_PROP_RED_DISABLE);
        CONVERT_PROP(DISCRETE_TRANSFER_DISABLE_G,
                     DISCRETETRANSFER_PROP_GREEN_DISABLE);
        CONVERT_PROP(DISCRETE_TRANSFER_DISABLE_B,
                     DISCRETETRANSFER_PROP_BLUE_DISABLE);
        CONVERT_PROP(DISCRETE_TRANSFER_DISABLE_A,
                     DISCRETETRANSFER_PROP_ALPHA_DISABLE);
        CONVERT_PROP(DISCRETE_TRANSFER_TABLE_R,
                     DISCRETETRANSFER_PROP_RED_TABLE);
        CONVERT_PROP(DISCRETE_TRANSFER_TABLE_G,
                     DISCRETETRANSFER_PROP_GREEN_TABLE);
        CONVERT_PROP(DISCRETE_TRANSFER_TABLE_B,
                     DISCRETETRANSFER_PROP_BLUE_TABLE);
        CONVERT_PROP(DISCRETE_TRANSFER_TABLE_A,
                     DISCRETETRANSFER_PROP_ALPHA_TABLE);
      }
      break;
    case FilterType::LINEAR_TRANSFER:
      switch (aIndex) {
        CONVERT_PROP(LINEAR_TRANSFER_DISABLE_R,
                     LINEARTRANSFER_PROP_RED_DISABLE);
        CONVERT_PROP(LINEAR_TRANSFER_DISABLE_G,
                     LINEARTRANSFER_PROP_GREEN_DISABLE);
        CONVERT_PROP(LINEAR_TRANSFER_DISABLE_B,
                     LINEARTRANSFER_PROP_BLUE_DISABLE);
        CONVERT_PROP(LINEAR_TRANSFER_DISABLE_A,
                     LINEARTRANSFER_PROP_ALPHA_DISABLE);
        CONVERT_PROP(LINEAR_TRANSFER_INTERCEPT_R,
                     LINEARTRANSFER_PROP_RED_Y_INTERCEPT);
        CONVERT_PROP(LINEAR_TRANSFER_INTERCEPT_G,
                     LINEARTRANSFER_PROP_GREEN_Y_INTERCEPT);
        CONVERT_PROP(LINEAR_TRANSFER_INTERCEPT_B,
                     LINEARTRANSFER_PROP_BLUE_Y_INTERCEPT);
        CONVERT_PROP(LINEAR_TRANSFER_INTERCEPT_A,
                     LINEARTRANSFER_PROP_ALPHA_Y_INTERCEPT);
        CONVERT_PROP(LINEAR_TRANSFER_SLOPE_R, LINEARTRANSFER_PROP_RED_SLOPE);
        CONVERT_PROP(LINEAR_TRANSFER_SLOPE_G, LINEARTRANSFER_PROP_GREEN_SLOPE);
        CONVERT_PROP(LINEAR_TRANSFER_SLOPE_B, LINEARTRANSFER_PROP_BLUE_SLOPE);
        CONVERT_PROP(LINEAR_TRANSFER_SLOPE_A, LINEARTRANSFER_PROP_ALPHA_SLOPE);
      }
      break;
    case FilterType::GAMMA_TRANSFER:
      switch (aIndex) {
        CONVERT_PROP(GAMMA_TRANSFER_DISABLE_R, GAMMATRANSFER_PROP_RED_DISABLE);
        CONVERT_PROP(GAMMA_TRANSFER_DISABLE_G,
                     GAMMATRANSFER_PROP_GREEN_DISABLE);
        CONVERT_PROP(GAMMA_TRANSFER_DISABLE_B, GAMMATRANSFER_PROP_BLUE_DISABLE);
        CONVERT_PROP(GAMMA_TRANSFER_DISABLE_A,
                     GAMMATRANSFER_PROP_ALPHA_DISABLE);
        CONVERT_PROP(GAMMA_TRANSFER_AMPLITUDE_R,
                     GAMMATRANSFER_PROP_RED_AMPLITUDE);
        CONVERT_PROP(GAMMA_TRANSFER_AMPLITUDE_G,
                     GAMMATRANSFER_PROP_GREEN_AMPLITUDE);
        CONVERT_PROP(GAMMA_TRANSFER_AMPLITUDE_B,
                     GAMMATRANSFER_PROP_BLUE_AMPLITUDE);
        CONVERT_PROP(GAMMA_TRANSFER_AMPLITUDE_A,
                     GAMMATRANSFER_PROP_ALPHA_AMPLITUDE);
        CONVERT_PROP(GAMMA_TRANSFER_EXPONENT_R,
                     GAMMATRANSFER_PROP_RED_EXPONENT);
        CONVERT_PROP(GAMMA_TRANSFER_EXPONENT_G,
                     GAMMATRANSFER_PROP_GREEN_EXPONENT);
        CONVERT_PROP(GAMMA_TRANSFER_EXPONENT_B,
                     GAMMATRANSFER_PROP_BLUE_EXPONENT);
        CONVERT_PROP(GAMMA_TRANSFER_EXPONENT_A,
                     GAMMATRANSFER_PROP_ALPHA_EXPONENT);
        CONVERT_PROP(GAMMA_TRANSFER_OFFSET_R, GAMMATRANSFER_PROP_RED_OFFSET);
        CONVERT_PROP(GAMMA_TRANSFER_OFFSET_G, GAMMATRANSFER_PROP_GREEN_OFFSET);
        CONVERT_PROP(GAMMA_TRANSFER_OFFSET_B, GAMMATRANSFER_PROP_BLUE_OFFSET);
        CONVERT_PROP(GAMMA_TRANSFER_OFFSET_A, GAMMATRANSFER_PROP_ALPHA_OFFSET);
      }
      break;
    case FilterType::CONVOLVE_MATRIX:
      switch (aIndex) {
        CONVERT_PROP(CONVOLVE_MATRIX_BIAS, CONVOLVEMATRIX_PROP_BIAS);
        CONVERT_PROP(CONVOLVE_MATRIX_KERNEL_MATRIX,
                     CONVOLVEMATRIX_PROP_KERNEL_MATRIX);
        CONVERT_PROP(CONVOLVE_MATRIX_DIVISOR, CONVOLVEMATRIX_PROP_DIVISOR);
        CONVERT_PROP(CONVOLVE_MATRIX_KERNEL_UNIT_LENGTH,
                     CONVOLVEMATRIX_PROP_KERNEL_UNIT_LENGTH);
        CONVERT_PROP(CONVOLVE_MATRIX_PRESERVE_ALPHA,
                     CONVOLVEMATRIX_PROP_PRESERVE_ALPHA);
      }
    case FilterType::DISPLACEMENT_MAP:
      switch (aIndex) {
        CONVERT_PROP(DISPLACEMENT_MAP_SCALE, DISPLACEMENTMAP_PROP_SCALE);
        CONVERT_PROP(DISPLACEMENT_MAP_X_CHANNEL,
                     DISPLACEMENTMAP_PROP_X_CHANNEL_SELECT);
        CONVERT_PROP(DISPLACEMENT_MAP_Y_CHANNEL,
                     DISPLACEMENTMAP_PROP_Y_CHANNEL_SELECT);
      }
      break;
    case FilterType::TURBULENCE:
      switch (aIndex) {
        CONVERT_PROP(TURBULENCE_BASE_FREQUENCY, TURBULENCE_PROP_BASE_FREQUENCY);
        CONVERT_PROP(TURBULENCE_NUM_OCTAVES, TURBULENCE_PROP_NUM_OCTAVES);
        CONVERT_PROP(TURBULENCE_SEED, TURBULENCE_PROP_SEED);
        CONVERT_PROP(TURBULENCE_STITCHABLE, TURBULENCE_PROP_STITCHABLE);
        CONVERT_PROP(TURBULENCE_TYPE, TURBULENCE_PROP_NOISE);
      }
      break;
    case FilterType::ARITHMETIC_COMBINE:
      switch (aIndex) {
        CONVERT_PROP(ARITHMETIC_COMBINE_COEFFICIENTS,
                     ARITHMETICCOMPOSITE_PROP_COEFFICIENTS);
      }
      break;
    case FilterType::COMPOSITE:
      switch (aIndex) { CONVERT_PROP(COMPOSITE_OPERATOR, COMPOSITE_PROP_MODE); }
      break;
    case FilterType::GAUSSIAN_BLUR:
      switch (aIndex) {
        CONVERT_PROP(GAUSSIAN_BLUR_STD_DEVIATION,
                     GAUSSIANBLUR_PROP_STANDARD_DEVIATION);
      }
      break;
    case FilterType::DIRECTIONAL_BLUR:
      switch (aIndex) {
        CONVERT_PROP(DIRECTIONAL_BLUR_STD_DEVIATION,
                     DIRECTIONALBLUR_PROP_STANDARD_DEVIATION);
        CONVERT_PROP(DIRECTIONAL_BLUR_DIRECTION, DIRECTIONALBLUR_PROP_ANGLE);
      }
      break;
    case FilterType::POINT_DIFFUSE:
      switch (aIndex) {
        CONVERT_PROP(POINT_DIFFUSE_DIFFUSE_CONSTANT,
                     POINTDIFFUSE_PROP_DIFFUSE_CONSTANT);
        CONVERT_PROP(POINT_DIFFUSE_POSITION, POINTDIFFUSE_PROP_LIGHT_POSITION);
        CONVERT_PROP(POINT_DIFFUSE_COLOR, POINTDIFFUSE_PROP_COLOR);
        CONVERT_PROP(POINT_DIFFUSE_SURFACE_SCALE,
                     POINTDIFFUSE_PROP_SURFACE_SCALE);
        CONVERT_PROP(POINT_DIFFUSE_KERNEL_UNIT_LENGTH,
                     POINTDIFFUSE_PROP_KERNEL_UNIT_LENGTH);
      }
      break;
    case FilterType::SPOT_DIFFUSE:
      switch (aIndex) {
        CONVERT_PROP(SPOT_DIFFUSE_DIFFUSE_CONSTANT,
                     SPOTDIFFUSE_PROP_DIFFUSE_CONSTANT);
        CONVERT_PROP(SPOT_DIFFUSE_POINTS_AT, SPOTDIFFUSE_PROP_POINTS_AT);
        CONVERT_PROP(SPOT_DIFFUSE_FOCUS, SPOTDIFFUSE_PROP_FOCUS);
        CONVERT_PROP(SPOT_DIFFUSE_LIMITING_CONE_ANGLE,
                     SPOTDIFFUSE_PROP_LIMITING_CONE_ANGLE);
        CONVERT_PROP(SPOT_DIFFUSE_POSITION, SPOTDIFFUSE_PROP_LIGHT_POSITION);
        CONVERT_PROP(SPOT_DIFFUSE_COLOR, SPOTDIFFUSE_PROP_COLOR);
        CONVERT_PROP(SPOT_DIFFUSE_SURFACE_SCALE,
                     SPOTDIFFUSE_PROP_SURFACE_SCALE);
        CONVERT_PROP(SPOT_DIFFUSE_KERNEL_UNIT_LENGTH,
                     SPOTDIFFUSE_PROP_KERNEL_UNIT_LENGTH);
      }
      break;
    case FilterType::DISTANT_DIFFUSE:
      switch (aIndex) {
        CONVERT_PROP(DISTANT_DIFFUSE_DIFFUSE_CONSTANT,
                     DISTANTDIFFUSE_PROP_DIFFUSE_CONSTANT);
        CONVERT_PROP(DISTANT_DIFFUSE_AZIMUTH, DISTANTDIFFUSE_PROP_AZIMUTH);
        CONVERT_PROP(DISTANT_DIFFUSE_ELEVATION, DISTANTDIFFUSE_PROP_ELEVATION);
        CONVERT_PROP(DISTANT_DIFFUSE_COLOR, DISTANTDIFFUSE_PROP_COLOR);
        CONVERT_PROP(DISTANT_DIFFUSE_SURFACE_SCALE,
                     DISTANTDIFFUSE_PROP_SURFACE_SCALE);
        CONVERT_PROP(DISTANT_DIFFUSE_KERNEL_UNIT_LENGTH,
                     DISTANTDIFFUSE_PROP_KERNEL_UNIT_LENGTH);
      }
      break;
    case FilterType::POINT_SPECULAR:
      switch (aIndex) {
        CONVERT_PROP(POINT_SPECULAR_SPECULAR_CONSTANT,
                     POINTSPECULAR_PROP_SPECULAR_CONSTANT);
        CONVERT_PROP(POINT_SPECULAR_SPECULAR_EXPONENT,
                     POINTSPECULAR_PROP_SPECULAR_EXPONENT);
        CONVERT_PROP(POINT_SPECULAR_POSITION,
                     POINTSPECULAR_PROP_LIGHT_POSITION);
        CONVERT_PROP(POINT_SPECULAR_COLOR, POINTSPECULAR_PROP_COLOR);
        CONVERT_PROP(POINT_SPECULAR_SURFACE_SCALE,
                     POINTSPECULAR_PROP_SURFACE_SCALE);
        CONVERT_PROP(POINT_SPECULAR_KERNEL_UNIT_LENGTH,
                     POINTSPECULAR_PROP_KERNEL_UNIT_LENGTH);
      }
      break;
    case FilterType::SPOT_SPECULAR:
      switch (aIndex) {
        CONVERT_PROP(SPOT_SPECULAR_SPECULAR_CONSTANT,
                     SPOTSPECULAR_PROP_SPECULAR_CONSTANT);
        CONVERT_PROP(SPOT_SPECULAR_SPECULAR_EXPONENT,
                     SPOTSPECULAR_PROP_SPECULAR_EXPONENT);
        CONVERT_PROP(SPOT_SPECULAR_POINTS_AT, SPOTSPECULAR_PROP_POINTS_AT);
        CONVERT_PROP(SPOT_SPECULAR_FOCUS, SPOTSPECULAR_PROP_FOCUS);
        CONVERT_PROP(SPOT_SPECULAR_LIMITING_CONE_ANGLE,
                     SPOTSPECULAR_PROP_LIMITING_CONE_ANGLE);
        CONVERT_PROP(SPOT_SPECULAR_POSITION, SPOTSPECULAR_PROP_LIGHT_POSITION);
        CONVERT_PROP(SPOT_SPECULAR_COLOR, SPOTSPECULAR_PROP_COLOR);
        CONVERT_PROP(SPOT_SPECULAR_SURFACE_SCALE,
                     SPOTSPECULAR_PROP_SURFACE_SCALE);
        CONVERT_PROP(SPOT_SPECULAR_KERNEL_UNIT_LENGTH,
                     SPOTSPECULAR_PROP_KERNEL_UNIT_LENGTH);
      }
      break;
    case FilterType::DISTANT_SPECULAR:
      switch (aIndex) {
        CONVERT_PROP(DISTANT_SPECULAR_SPECULAR_CONSTANT,
                     DISTANTSPECULAR_PROP_SPECULAR_CONSTANT);
        CONVERT_PROP(DISTANT_SPECULAR_SPECULAR_EXPONENT,
                     DISTANTSPECULAR_PROP_SPECULAR_EXPONENT);
        CONVERT_PROP(DISTANT_SPECULAR_AZIMUTH, DISTANTSPECULAR_PROP_AZIMUTH);
        CONVERT_PROP(DISTANT_SPECULAR_ELEVATION,
                     DISTANTSPECULAR_PROP_ELEVATION);
        CONVERT_PROP(DISTANT_SPECULAR_COLOR, DISTANTSPECULAR_PROP_COLOR);
        CONVERT_PROP(DISTANT_SPECULAR_SURFACE_SCALE,
                     DISTANTSPECULAR_PROP_SURFACE_SCALE);
        CONVERT_PROP(DISTANT_SPECULAR_KERNEL_UNIT_LENGTH,
                     DISTANTSPECULAR_PROP_KERNEL_UNIT_LENGTH);
      }
      break;
    case FilterType::CROP:
      switch (aIndex) { CONVERT_PROP(CROP_RECT, CROP_PROP_RECT); }
      break;
    default:
      break;
  }

  return UINT32_MAX;
}

bool GetD2D1PropsForIntSize(FilterType aType, uint32_t aIndex,
                            UINT32* aPropWidth, UINT32* aPropHeight) {
  switch (aType) {
    case FilterType::MORPHOLOGY:
      if (aIndex == ATT_MORPHOLOGY_RADII) {
        *aPropWidth = D2D1_MORPHOLOGY_PROP_WIDTH;
        *aPropHeight = D2D1_MORPHOLOGY_PROP_HEIGHT;
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

static inline REFCLSID GetCLDIDForFilterType(FilterType aType) {
  switch (aType) {
    case FilterType::OPACITY:
    case FilterType::COLOR_MATRIX:
      return CLSID_D2D1ColorMatrix;
    case FilterType::TRANSFORM:
      return CLSID_D2D12DAffineTransform;
    case FilterType::BLEND:
      return CLSID_D2D1Blend;
    case FilterType::MORPHOLOGY:
      return CLSID_D2D1Morphology;
    case FilterType::FLOOD:
      return CLSID_D2D1Flood;
    case FilterType::TILE:
      return CLSID_D2D1Tile;
    case FilterType::TABLE_TRANSFER:
      return CLSID_D2D1TableTransfer;
    case FilterType::LINEAR_TRANSFER:
      return CLSID_D2D1LinearTransfer;
    case FilterType::DISCRETE_TRANSFER:
      return CLSID_D2D1DiscreteTransfer;
    case FilterType::GAMMA_TRANSFER:
      return CLSID_D2D1GammaTransfer;
    case FilterType::DISPLACEMENT_MAP:
      return CLSID_D2D1DisplacementMap;
    case FilterType::TURBULENCE:
      return CLSID_D2D1Turbulence;
    case FilterType::ARITHMETIC_COMBINE:
      return CLSID_D2D1ArithmeticComposite;
    case FilterType::COMPOSITE:
      return CLSID_D2D1Composite;
    case FilterType::GAUSSIAN_BLUR:
      return CLSID_D2D1GaussianBlur;
    case FilterType::DIRECTIONAL_BLUR:
      return CLSID_D2D1DirectionalBlur;
    case FilterType::POINT_DIFFUSE:
      return CLSID_D2D1PointDiffuse;
    case FilterType::POINT_SPECULAR:
      return CLSID_D2D1PointSpecular;
    case FilterType::SPOT_DIFFUSE:
      return CLSID_D2D1SpotDiffuse;
    case FilterType::SPOT_SPECULAR:
      return CLSID_D2D1SpotSpecular;
    case FilterType::DISTANT_DIFFUSE:
      return CLSID_D2D1DistantDiffuse;
    case FilterType::DISTANT_SPECULAR:
      return CLSID_D2D1DistantSpecular;
    case FilterType::CROP:
      return CLSID_D2D1Crop;
    case FilterType::PREMULTIPLY:
      return CLSID_D2D1Premultiply;
    case FilterType::UNPREMULTIPLY:
      return CLSID_D2D1UnPremultiply;
    default:
      break;
  }
  return GUID_NULL;
}

static bool IsTransferFilterType(FilterType aType) {
  switch (aType) {
    case FilterType::LINEAR_TRANSFER:
    case FilterType::GAMMA_TRANSFER:
    case FilterType::TABLE_TRANSFER:
    case FilterType::DISCRETE_TRANSFER:
      return true;
    default:
      return false;
  }
}

static bool HasUnboundedOutputRegion(FilterType aType) {
  if (IsTransferFilterType(aType)) {
    return true;
  }

  switch (aType) {
    case FilterType::COLOR_MATRIX:
    case FilterType::POINT_DIFFUSE:
    case FilterType::SPOT_DIFFUSE:
    case FilterType::DISTANT_DIFFUSE:
    case FilterType::POINT_SPECULAR:
    case FilterType::SPOT_SPECULAR:
    case FilterType::DISTANT_SPECULAR:
      return true;
    default:
      return false;
  }
}

/* static */
already_AddRefed<FilterNode> FilterNodeD2D1::Create(ID2D1DeviceContext* aDC,
                                                    FilterType aType) {
  if (aType == FilterType::CONVOLVE_MATRIX) {
    return MakeAndAddRef<FilterNodeConvolveD2D1>(aDC);
  }

  RefPtr<ID2D1Effect> effect;
  HRESULT hr;

  hr = aDC->CreateEffect(GetCLDIDForFilterType(aType), getter_AddRefs(effect));

  if (FAILED(hr) || !effect) {
    gfxCriticalErrorOnce() << "Failed to create effect for FilterType: "
                           << hexa(hr);
    return nullptr;
  }

  if (aType == FilterType::ARITHMETIC_COMBINE) {
    effect->SetValue(D2D1_ARITHMETICCOMPOSITE_PROP_CLAMP_OUTPUT, TRUE);
  }

  if (aType == FilterType::OPACITY) {
    return MakeAndAddRef<FilterNodeOpacityD2D1>(effect, aType);
  }

  RefPtr<FilterNodeD2D1> filter = new FilterNodeD2D1(effect, aType);

  if (HasUnboundedOutputRegion(aType)) {
    // These filters can produce non-transparent output from transparent
    // input pixels, and we want them to have an unbounded output region.
    filter = new FilterNodeExtendInputAdapterD2D1(aDC, filter, aType);
  }

  if (IsTransferFilterType(aType)) {
    // Component transfer filters should appear to apply on unpremultiplied
    // colors, but the D2D1 effects apply on premultiplied colors.
    filter = new FilterNodePremultiplyAdapterD2D1(aDC, filter, aType);
  }

  return filter.forget();
}

void FilterNodeD2D1::InitUnmappedProperties() {
  switch (mType) {
    case FilterType::COLOR_MATRIX:
      mEffect->SetValue(D2D1_COLORMATRIX_PROP_CLAMP_OUTPUT, TRUE);
      break;
    case FilterType::TRANSFORM:
      mEffect->SetValue(D2D1_2DAFFINETRANSFORM_PROP_BORDER_MODE,
                        D2D1_BORDER_MODE_HARD);
      break;
    default:
      break;
  }
}

void FilterNodeD2D1::SetInput(uint32_t aIndex, SourceSurface* aSurface) {
  UINT32 input = GetD2D1InputForInput(mType, aIndex);
  ID2D1Effect* effect = InputEffect();
  MOZ_ASSERT(input < effect->GetInputCount());

  if (mType == FilterType::COMPOSITE) {
    UINT32 inputCount = effect->GetInputCount();

    if (aIndex == inputCount - 1 && aSurface == nullptr) {
      effect->SetInputCount(inputCount - 1);
    } else if (aIndex >= inputCount && aSurface) {
      effect->SetInputCount(aIndex + 1);
    }
  }

  MOZ_ASSERT(input < effect->GetInputCount());

  mInputSurfaces.resize(effect->GetInputCount());
  mInputFilters.resize(effect->GetInputCount());

  // In order to convert aSurface into an ID2D1Image, we need to know what
  // DrawTarget we paint into. However, the same FilterNode object can be
  // used on different DrawTargets, so we need to hold on to the SourceSurface
  // objects and delay the conversion until we're actually painted and know
  // our target DrawTarget.
  // The conversion happens in WillDraw().

  mInputSurfaces[input] = aSurface;
  mInputFilters[input] = nullptr;

  // Clear the existing image from the effect.
  effect->SetInput(input, nullptr);
}

void FilterNodeD2D1::SetInput(uint32_t aIndex, FilterNode* aFilter) {
  UINT32 input = GetD2D1InputForInput(mType, aIndex);
  ID2D1Effect* effect = InputEffect();

  if (mType == FilterType::COMPOSITE) {
    UINT32 inputCount = effect->GetInputCount();

    if (aIndex == inputCount - 1 && aFilter == nullptr) {
      effect->SetInputCount(inputCount - 1);
    } else if (aIndex >= inputCount && aFilter) {
      effect->SetInputCount(aIndex + 1);
    }
  }

  MOZ_ASSERT(input < effect->GetInputCount());

  if (aFilter && aFilter->GetBackendType() != FILTER_BACKEND_DIRECT2D1_1) {
    gfxWarning() << "Unknown input FilterNode set on effect.";
    MOZ_ASSERT(0);
    return;
  }

  FilterNodeD2D1* filter = static_cast<FilterNodeD2D1*>(aFilter);

  mInputSurfaces.resize(effect->GetInputCount());
  mInputFilters.resize(effect->GetInputCount());

  // We hold on to the FilterNode object so that we can call WillDraw() on it.
  mInputSurfaces[input] = nullptr;
  mInputFilters[input] = filter;

  if (filter) {
    effect->SetInputEffect(input, filter->OutputEffect());
  }
}

void FilterNodeD2D1::WillDraw(DrawTarget* aDT) {
  // Convert input SourceSurfaces into ID2D1Images and set them on the effect.
  for (size_t inputIndex = 0; inputIndex < mInputSurfaces.size();
       inputIndex++) {
    if (mInputSurfaces[inputIndex]) {
      ID2D1Effect* effect = InputEffect();
      RefPtr<ID2D1Image> image =
          GetImageForSourceSurface(aDT, mInputSurfaces[inputIndex]);
      effect->SetInput(inputIndex, image);
    }
  }

  // Call WillDraw() on our input filters.
  for (std::vector<RefPtr<FilterNodeD2D1>>::iterator it = mInputFilters.begin();
       it != mInputFilters.end(); it++) {
    if (*it) {
      (*it)->WillDraw(aDT);
    }
  }
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, uint32_t aValue) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  if (mType == FilterType::TURBULENCE &&
      aIndex == ATT_TURBULENCE_BASE_FREQUENCY) {
    mEffect->SetValue(input, D2D1::Vector2F(FLOAT(aValue), FLOAT(aValue)));
    return;
  } else if (mType == FilterType::DIRECTIONAL_BLUR &&
             aIndex == ATT_DIRECTIONAL_BLUR_DIRECTION) {
    mEffect->SetValue(input, aValue == BLUR_DIRECTION_X ? 0 : 90.0f);
    return;
  }

  mEffect->SetValue(input, ConvertValue(mType, aIndex, aValue));
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, Float aValue) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, aValue);
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Point& aValue) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DPoint(aValue));
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Matrix5x4& aValue) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DMatrix5x4(aValue));
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Point3D& aValue) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DVector3D(aValue));
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Size& aValue) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2D1::Vector2F(aValue.width, aValue.height));
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const IntSize& aValue) {
  UINT32 widthProp, heightProp;

  if (!GetD2D1PropsForIntSize(mType, aIndex, &widthProp, &heightProp)) {
    return;
  }

  IntSize value = aValue;
  ConvertValue(mType, aIndex, value);

  mEffect->SetValue(widthProp, (UINT)value.width);
  mEffect->SetValue(heightProp, (UINT)value.height);
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const DeviceColor& aValue) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  switch (mType) {
    case FilterType::POINT_DIFFUSE:
    case FilterType::SPOT_DIFFUSE:
    case FilterType::DISTANT_DIFFUSE:
    case FilterType::POINT_SPECULAR:
    case FilterType::SPOT_SPECULAR:
    case FilterType::DISTANT_SPECULAR:
      mEffect->SetValue(input, D2D1::Vector3F(aValue.r, aValue.g, aValue.b));
      break;
    default:
      mEffect->SetValue(input,
                        D2D1::Vector4F(aValue.r * aValue.a, aValue.g * aValue.a,
                                       aValue.b * aValue.a, aValue.a));
  }
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Rect& aValue) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DRect(aValue));
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const IntRect& aValue) {
  if (mType == FilterType::TURBULENCE) {
    MOZ_ASSERT(aIndex == ATT_TURBULENCE_RECT);

    mEffect->SetValue(D2D1_TURBULENCE_PROP_OFFSET,
                      D2D1::Vector2F(Float(aValue.X()), Float(aValue.Y())));
    mEffect->SetValue(
        D2D1_TURBULENCE_PROP_SIZE,
        D2D1::Vector2F(Float(aValue.Width()), Float(aValue.Height())));
    return;
  }

  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input,
                    D2D1::RectF(Float(aValue.X()), Float(aValue.Y()),
                                Float(aValue.XMost()), Float(aValue.YMost())));
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, bool aValue) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, (BOOL)aValue);
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Float* aValues,
                                  uint32_t aSize) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, (BYTE*)aValues, sizeof(Float) * aSize);
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const IntPoint& aValue) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DPoint(aValue));
}

void FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Matrix& aMatrix) {
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DMatrix(aMatrix));
}

void FilterNodeOpacityD2D1::SetAttribute(uint32_t aIndex, Float aValue) {
  D2D1_MATRIX_5X4_F matrix =
      D2D1::Matrix5x4F(aValue, 0, 0, 0, 0, aValue, 0, 0, 0, 0, aValue, 0, 0, 0,
                       0, aValue, 0, 0, 0, 0);

  mEffect->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matrix);
  mEffect->SetValue(D2D1_COLORMATRIX_PROP_ALPHA_MODE,
                    D2D1_COLORMATRIX_ALPHA_MODE_STRAIGHT);
}

FilterNodeConvolveD2D1::FilterNodeConvolveD2D1(ID2D1DeviceContext* aDC)
    : FilterNodeD2D1(nullptr, FilterType::CONVOLVE_MATRIX),
      mEdgeMode(EDGE_MODE_DUPLICATE) {
  // Correctly handling the interaction of edge mode and source rect is a bit
  // tricky with D2D1 effects. We want the edge mode to only apply outside of
  // the source rect (as specified by the ATT_CONVOLVE_MATRIX_SOURCE_RECT
  // attribute). So if our input surface or filter is smaller than the source
  // rect, we need to add transparency around it until we reach the edges of
  // the source rect, and only then do any repeating or edge duplicating.
  // Unfortunately, the border effect does not have a source rect attribute -
  // it only looks at the output rect of its input filter or surface. So we use
  // our custom ExtendInput effect to adjust the output rect of our input.
  // All of this is only necessary when our edge mode is not EDGE_MODE_NONE, so
  // we update the filter chain dynamically in UpdateChain().

  HRESULT hr;

  hr = aDC->CreateEffect(CLSID_D2D1ConvolveMatrix, getter_AddRefs(mEffect));

  if (FAILED(hr) || !mEffect) {
    gfxWarning() << "Failed to create ConvolveMatrix filter!";
    return;
  }

  mEffect->SetValue(D2D1_CONVOLVEMATRIX_PROP_BORDER_MODE,
                    D2D1_BORDER_MODE_SOFT);

  hr = aDC->CreateEffect(CLSID_ExtendInputEffect,
                         getter_AddRefs(mExtendInputEffect));

  if (FAILED(hr) || !mExtendInputEffect) {
    gfxWarning() << "Failed to create ConvolveMatrix filter!";
    return;
  }

  hr = aDC->CreateEffect(CLSID_D2D1Border, getter_AddRefs(mBorderEffect));

  if (FAILED(hr) || !mBorderEffect) {
    gfxWarning() << "Failed to create ConvolveMatrix filter!";
    return;
  }

  mBorderEffect->SetInputEffect(0, mExtendInputEffect.get());

  UpdateChain();
  UpdateSourceRect();
}

void FilterNodeConvolveD2D1::SetInput(uint32_t aIndex, FilterNode* aFilter) {
  FilterNodeD2D1::SetInput(aIndex, aFilter);

  UpdateChain();
}

void FilterNodeConvolveD2D1::SetAttribute(uint32_t aIndex, uint32_t aValue) {
  if (aIndex != ATT_CONVOLVE_MATRIX_EDGE_MODE) {
    return FilterNodeD2D1::SetAttribute(aIndex, aValue);
  }

  mEdgeMode = (ConvolveMatrixEdgeMode)aValue;

  UpdateChain();
}

ID2D1Effect* FilterNodeConvolveD2D1::InputEffect() {
  return mEdgeMode == EDGE_MODE_NONE ? mEffect.get() : mExtendInputEffect.get();
}

void FilterNodeConvolveD2D1::UpdateChain() {
  // The shape of the filter graph:
  //
  // EDGE_MODE_NONE:
  // input --> convolvematrix
  //
  // EDGE_MODE_DUPLICATE or EDGE_MODE_WRAP:
  // input --> extendinput --> border --> convolvematrix
  //
  // mEffect is convolvematrix.

  if (mEdgeMode != EDGE_MODE_NONE) {
    mEffect->SetInputEffect(0, mBorderEffect.get());
  }

  RefPtr<ID2D1Effect> inputEffect;
  if (mInputFilters.size() > 0 && mInputFilters[0]) {
    inputEffect = mInputFilters[0]->OutputEffect();
  }
  InputEffect()->SetInputEffect(0, inputEffect);

  if (mEdgeMode == EDGE_MODE_DUPLICATE) {
    mBorderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_X,
                            D2D1_BORDER_EDGE_MODE_CLAMP);
    mBorderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_Y,
                            D2D1_BORDER_EDGE_MODE_CLAMP);
  } else if (mEdgeMode == EDGE_MODE_WRAP) {
    mBorderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_X,
                            D2D1_BORDER_EDGE_MODE_WRAP);
    mBorderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_Y,
                            D2D1_BORDER_EDGE_MODE_WRAP);
  }
}

void FilterNodeConvolveD2D1::SetAttribute(uint32_t aIndex,
                                          const IntSize& aValue) {
  if (aIndex != ATT_CONVOLVE_MATRIX_KERNEL_SIZE) {
    MOZ_ASSERT(false);
    return;
  }

  mKernelSize = aValue;

  mEffect->SetValue(D2D1_CONVOLVEMATRIX_PROP_KERNEL_SIZE_X, aValue.width);
  mEffect->SetValue(D2D1_CONVOLVEMATRIX_PROP_KERNEL_SIZE_Y, aValue.height);

  UpdateOffset();
}

void FilterNodeConvolveD2D1::SetAttribute(uint32_t aIndex,
                                          const IntPoint& aValue) {
  if (aIndex != ATT_CONVOLVE_MATRIX_TARGET) {
    MOZ_ASSERT(false);
    return;
  }

  mTarget = aValue;

  UpdateOffset();
}

void FilterNodeConvolveD2D1::SetAttribute(uint32_t aIndex,
                                          const IntRect& aValue) {
  if (aIndex != ATT_CONVOLVE_MATRIX_SOURCE_RECT) {
    MOZ_ASSERT(false);
    return;
  }

  mSourceRect = aValue;

  UpdateSourceRect();
}

void FilterNodeConvolveD2D1::UpdateOffset() {
  D2D1_VECTOR_2F vector = D2D1::Vector2F(
      (Float(mKernelSize.width) - 1.0f) / 2.0f - Float(mTarget.x),
      (Float(mKernelSize.height) - 1.0f) / 2.0f - Float(mTarget.y));

  mEffect->SetValue(D2D1_CONVOLVEMATRIX_PROP_KERNEL_OFFSET, vector);
}

void FilterNodeConvolveD2D1::UpdateSourceRect() {
  mExtendInputEffect->SetValue(
      EXTENDINPUT_PROP_OUTPUT_RECT,
      D2D1::Vector4F(Float(mSourceRect.X()), Float(mSourceRect.Y()),
                     Float(mSourceRect.XMost()), Float(mSourceRect.YMost())));
}

FilterNodeExtendInputAdapterD2D1::FilterNodeExtendInputAdapterD2D1(
    ID2D1DeviceContext* aDC, FilterNodeD2D1* aFilterNode, FilterType aType)
    : FilterNodeD2D1(aFilterNode->MainEffect(), aType),
      mWrappedFilterNode(aFilterNode) {
  // We have an mEffect that looks at the bounds of the input effect, and we
  // want mEffect to regard its input as unbounded. So we take the input,
  // pipe it through an ExtendInput effect (which has an infinite output rect
  // by default), and feed the resulting unbounded composition into mEffect.

  HRESULT hr;

  hr = aDC->CreateEffect(CLSID_ExtendInputEffect,
                         getter_AddRefs(mExtendInputEffect));

  if (FAILED(hr) || !mExtendInputEffect) {
    gfxWarning() << "Failed to create extend input effect for filter: "
                 << hexa(hr);
    return;
  }

  aFilterNode->InputEffect()->SetInputEffect(0, mExtendInputEffect.get());
}

FilterNodePremultiplyAdapterD2D1::FilterNodePremultiplyAdapterD2D1(
    ID2D1DeviceContext* aDC, FilterNodeD2D1* aFilterNode, FilterType aType)
    : FilterNodeD2D1(aFilterNode->MainEffect(), aType) {
  // D2D1 component transfer effects do strange things when it comes to
  // premultiplication.
  // For our purposes we only need the transfer filters to apply straight to
  // unpremultiplied source channels and output unpremultiplied results.
  // However, the D2D1 effects are designed differently: They can apply to both
  // premultiplied and unpremultiplied inputs, and they always premultiply
  // their result - at least in those color channels that have not been
  // disabled.
  // In order to determine whether the input needs to be unpremultiplied as
  // part of the transfer, the effect consults the alpha mode metadata of the
  // input surface or the input effect. We don't have such a concept in Moz2D,
  // and giving Moz2D users different results based on something that cannot be
  // influenced through Moz2D APIs seems like a bad idea.
  // We solve this by applying a premultiply effect to the input before feeding
  // it into the transfer effect. The premultiply effect always premultiplies
  // regardless of any alpha mode metadata on inputs, and it always marks its
  // output as premultiplied so that the transfer effect will unpremultiply
  // consistently. Feeding always-premultiplied input into the transfer effect
  // also avoids another problem that would appear when individual color
  // channels disable the transfer: In that case, the disabled channels would
  // pass through unchanged in their unpremultiplied form and the other
  // channels would be premultiplied, giving a mixed result.
  // But since we now ensure that the input is premultiplied, disabled channels
  // will pass premultiplied values through to the result, which is consistent
  // with the enabled channels.
  // We also add an unpremultiply effect that postprocesses the result of the
  // transfer effect because getting unpremultiplied results from the transfer
  // filters is part of the FilterNode API.
  HRESULT hr;

  hr = aDC->CreateEffect(CLSID_D2D1Premultiply,
                         getter_AddRefs(mPrePremultiplyEffect));

  if (FAILED(hr) || !mPrePremultiplyEffect) {
    gfxWarning() << "Failed to create ComponentTransfer filter!";
    return;
  }

  hr = aDC->CreateEffect(CLSID_D2D1UnPremultiply,
                         getter_AddRefs(mPostUnpremultiplyEffect));

  if (FAILED(hr) || !mPostUnpremultiplyEffect) {
    gfxWarning() << "Failed to create ComponentTransfer filter!";
    return;
  }

  aFilterNode->InputEffect()->SetInputEffect(0, mPrePremultiplyEffect.get());
  mPostUnpremultiplyEffect->SetInputEffect(0, aFilterNode->OutputEffect());
}

}  // namespace gfx
}  // namespace mozilla
