/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_SurfacePipeFactory_h
#define mozilla_image_SurfacePipeFactory_h

#include "SurfacePipe.h"
#include "SurfaceFilters.h"

namespace mozilla {
namespace image {

namespace detail {

/**
 * FilterPipeline is a helper template for SurfacePipeFactory that determines
 * the full type of the sequence of SurfaceFilters that a sequence of
 * configuration structs corresponds to. To make this work, all configuration
 * structs must include a typedef 'Filter' that identifies the SurfaceFilter
 * they configure.
 */
template <typename... Configs>
struct FilterPipeline;

template <typename Config, typename... Configs>
struct FilterPipeline<Config, Configs...> {
  typedef typename Config::template Filter<
      typename FilterPipeline<Configs...>::Type>
      Type;
};

template <typename Config>
struct FilterPipeline<Config> {
  typedef typename Config::Filter Type;
};

}  // namespace detail

/**
 * Flags for SurfacePipeFactory, used in conjunction with the factory functions
 * in SurfacePipeFactory to enable or disable various SurfacePipe
 * functionality.
 */
enum class SurfacePipeFlags {
  DEINTERLACE = 1 << 0,  // If set, deinterlace the image.

  ADAM7_INTERPOLATE =
      1 << 1,  // If set, the caller is deinterlacing the
               // image using ADAM7, and we may want to
               // interpolate it for better intermediate results.

  FLIP_VERTICALLY = 1 << 2,  // If set, flip the image vertically.

  PROGRESSIVE_DISPLAY = 1 << 3,  // If set, we expect the image to be displayed
                                 // progressively. This enables features that
                                 // result in a better user experience for
                                 // progressive display but which may be more
                                 // computationally expensive.

  PREMULTIPLY_ALPHA = 1 << 4,  // If set, we want to premultiply the alpha
                               // channel and the individual color channels.
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(SurfacePipeFlags)

class SurfacePipeFactory {
 public:
  /**
   * Creates and initializes a normal (i.e., non-paletted) SurfacePipe.
   *
   * @param aDecoder The decoder whose current frame the SurfacePipe will write
   *                 to.
   * @param aInputSize The original size of the image.
   * @param aOutputSize The size the SurfacePipe should output. Must be the same
   *                    as @aInputSize or smaller. If smaller, the image will be
   *                    downscaled during decoding.
   * @param aFrameRect The portion of the image that actually contains data.
   * @param aFormat The surface format of the image; generally B8G8R8A8 or
   *                B8G8R8X8.
   * @param aAnimParams Extra parameters used by animated images.
   * @param aFlags Flags enabling or disabling various functionality for the
   *               SurfacePipe; see the SurfacePipeFlags documentation for more
   *               information.
   *
   * @return A SurfacePipe if the parameters allowed one to be created
   *         successfully, or Nothing() if the SurfacePipe could not be
   *         initialized.
   */
  static Maybe<SurfacePipe> CreateSurfacePipe(
      Decoder* aDecoder, const OrientedIntSize& aInputSize,
      const OrientedIntSize& aOutputSize, const OrientedIntRect& aFrameRect,
      gfx::SurfaceFormat aInFormat, gfx::SurfaceFormat aOutFormat,
      const Maybe<AnimationParams>& aAnimParams, qcms_transform* aTransform,
      SurfacePipeFlags aFlags) {
    const bool deinterlace = bool(aFlags & SurfacePipeFlags::DEINTERLACE);
    const bool flipVertically =
        bool(aFlags & SurfacePipeFlags::FLIP_VERTICALLY);
    const bool progressiveDisplay =
        bool(aFlags & SurfacePipeFlags::PROGRESSIVE_DISPLAY);
    const bool downscale = aInputSize != aOutputSize;
    const bool removeFrameRect = !aFrameRect.IsEqualEdges(
        OrientedIntRect(OrientedIntPoint(0, 0), aInputSize));
    const bool blendAnimation = aAnimParams.isSome();
    const bool colorManagement = aTransform != nullptr;
    const bool premultiplyAlpha =
        bool(aFlags & SurfacePipeFlags::PREMULTIPLY_ALPHA);

    MOZ_ASSERT(aInFormat == gfx::SurfaceFormat::R8G8B8 ||
               aInFormat == gfx::SurfaceFormat::R8G8B8A8 ||
               aInFormat == gfx::SurfaceFormat::R8G8B8X8 ||
               aInFormat == gfx::SurfaceFormat::OS_RGBA ||
               aInFormat == gfx::SurfaceFormat::OS_RGBX);

    MOZ_ASSERT(aOutFormat == gfx::SurfaceFormat::OS_RGBA ||
               aOutFormat == gfx::SurfaceFormat::OS_RGBX);

    MOZ_ASSERT(aDecoder->GetOrientation().IsIdentity());

    const bool inFormatRgb = aInFormat == gfx::SurfaceFormat::R8G8B8;

    const bool inFormatOpaque = aInFormat == gfx::SurfaceFormat::OS_RGBX ||
                                aInFormat == gfx::SurfaceFormat::R8G8B8X8 ||
                                inFormatRgb;
    const bool outFormatOpaque = aOutFormat == gfx::SurfaceFormat::OS_RGBX;

    const bool inFormatOrder = aInFormat == gfx::SurfaceFormat::R8G8B8A8 ||
                               aInFormat == gfx::SurfaceFormat::R8G8B8X8;
    const bool outFormatOrder = aOutFormat == gfx::SurfaceFormat::R8G8B8A8 ||
                                aOutFormat == gfx::SurfaceFormat::R8G8B8X8;

    // Early swizzles are for unpacking RGB or forcing RGBA/BGRA_U32 to
    // RGBX/BGRX_U32. We should never want to premultiply in either case,
    // because the image's alpha channel will always be opaque. This must be
    // done before downscaling and color management.
    bool unpackOrMaskSwizzle =
        inFormatRgb ||
        (!inFormatOpaque && outFormatOpaque && inFormatOrder == outFormatOrder);

    // Late swizzles are for premultiplying RGBA/BGRA_U32 and/or possible
    // converting between RGBA and BGRA_U32. It must happen after color
    // management, and before downscaling.
    bool swapOrAlphaSwizzle =
        (!inFormatRgb && inFormatOrder != outFormatOrder) || premultiplyAlpha;

    if (unpackOrMaskSwizzle && swapOrAlphaSwizzle) {
      MOZ_ASSERT_UNREACHABLE("Early and late swizzles not supported");
      return Nothing();
    }

    if (!unpackOrMaskSwizzle && !swapOrAlphaSwizzle &&
        aInFormat != aOutFormat) {
      MOZ_ASSERT_UNREACHABLE("Need to swizzle, but not configured to");
      return Nothing();
    }

    // Don't interpolate if we're sure we won't show this surface to the user
    // until it's completely decoded. The final pass of an ADAM7 image doesn't
    // need interpolation, so we only need to interpolate if we'll be displaying
    // the image while it's still being decoded.
    const bool adam7Interpolate =
        bool(aFlags & SurfacePipeFlags::ADAM7_INTERPOLATE) &&
        progressiveDisplay;

    if (deinterlace && adam7Interpolate) {
      MOZ_ASSERT_UNREACHABLE("ADAM7 deinterlacing is handled by libpng");
      return Nothing();
    }

    // Construct configurations for the SurfaceFilters. Note that the order of
    // these filters is significant. We want to deinterlace or interpolate raw
    // input rows, before any other transformations, and we want to remove the
    // frame rect (which may involve adding blank rows or columns to the image)
    // before any downscaling, so that the new rows and columns are taken into
    // account.
    DeinterlacingConfig<uint32_t> deinterlacingConfig{progressiveDisplay};
    ADAM7InterpolatingConfig interpolatingConfig;
    RemoveFrameRectConfig removeFrameRectConfig{aFrameRect.ToUnknownRect()};
    BlendAnimationConfig blendAnimationConfig{aDecoder};
    DownscalingConfig downscalingConfig{aInputSize.ToUnknownSize(), aOutFormat};
    ColorManagementConfig colorManagementConfig{aTransform};
    SwizzleConfig swizzleConfig{aInFormat, aOutFormat, premultiplyAlpha};
    SurfaceConfig surfaceConfig{aDecoder, aOutputSize.ToUnknownSize(),
                                aOutFormat, flipVertically, aAnimParams};

    Maybe<SurfacePipe> pipe;

    if (unpackOrMaskSwizzle) {
      if (colorManagement) {
        if (downscale) {
          MOZ_ASSERT(!blendAnimation);
          if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              removeFrameRectConfig, downscalingConfig,
                              colorManagementConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              removeFrameRectConfig, downscalingConfig,
                              colorManagementConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(swizzleConfig, removeFrameRectConfig,
                              downscalingConfig, colorManagementConfig,
                              surfaceConfig);
            }
          } else {  // (removeFrameRect is false)
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              downscalingConfig, colorManagementConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              downscalingConfig, colorManagementConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(swizzleConfig, downscalingConfig,
                              colorManagementConfig, surfaceConfig);
            }
          }
        } else {  // (downscale is false)
          if (blendAnimation) {
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              colorManagementConfig, blendAnimationConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              colorManagementConfig, blendAnimationConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(swizzleConfig, colorManagementConfig,
                              blendAnimationConfig, surfaceConfig);
            }
          } else if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              colorManagementConfig, removeFrameRectConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              colorManagementConfig, removeFrameRectConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(swizzleConfig, colorManagementConfig,
                              removeFrameRectConfig, surfaceConfig);
            }
          } else {  // (blendAnimation and removeFrameRect is false)
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              colorManagementConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              colorManagementConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe =
                  MakePipe(swizzleConfig, colorManagementConfig, surfaceConfig);
            }
          }
        }
      } else {  // (colorManagement is false)
        if (downscale) {
          MOZ_ASSERT(!blendAnimation);
          if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              removeFrameRectConfig, downscalingConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              removeFrameRectConfig, downscalingConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(swizzleConfig, removeFrameRectConfig,
                              downscalingConfig, surfaceConfig);
            }
          } else {  // (removeFrameRect is false)
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              downscalingConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              downscalingConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(swizzleConfig, downscalingConfig, surfaceConfig);
            }
          }
        } else {  // (downscale is false)
          if (blendAnimation) {
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              blendAnimationConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              blendAnimationConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe =
                  MakePipe(swizzleConfig, blendAnimationConfig, surfaceConfig);
            }
          } else if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              removeFrameRectConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              removeFrameRectConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe =
                  MakePipe(swizzleConfig, removeFrameRectConfig, surfaceConfig);
            }
          } else {  // (blendAnimation and removeFrameRect is false)
            if (deinterlace) {
              pipe =
                  MakePipe(swizzleConfig, deinterlacingConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe =
                  MakePipe(swizzleConfig, interpolatingConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(swizzleConfig, surfaceConfig);
            }
          }
        }
      }
    } else if (swapOrAlphaSwizzle) {
      if (colorManagement) {
        if (downscale) {
          MOZ_ASSERT(!blendAnimation);
          if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              deinterlacingConfig, removeFrameRectConfig,
                              downscalingConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              interpolatingConfig, removeFrameRectConfig,
                              downscalingConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              removeFrameRectConfig, downscalingConfig,
                              surfaceConfig);
            }
          } else {  // (removeFrameRect is false)
            if (deinterlace) {
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              deinterlacingConfig, downscalingConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              interpolatingConfig, downscalingConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              downscalingConfig, surfaceConfig);
            }
          }
        } else {  // (downscale is false)
          if (blendAnimation) {
            if (deinterlace) {
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              deinterlacingConfig, blendAnimationConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              interpolatingConfig, blendAnimationConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              blendAnimationConfig, surfaceConfig);
            }
          } else if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              deinterlacingConfig, removeFrameRectConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              interpolatingConfig, removeFrameRectConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              removeFrameRectConfig, surfaceConfig);
            }
          } else {  // (blendAnimation and removeFrameRect is false)
            if (deinterlace) {
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              deinterlacingConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(colorManagementConfig, swizzleConfig,
                              interpolatingConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe =
                  MakePipe(colorManagementConfig, swizzleConfig, surfaceConfig);
            }
          }
        }
      } else {  // (colorManagement is false)
        if (downscale) {
          MOZ_ASSERT(!blendAnimation);
          if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              removeFrameRectConfig, downscalingConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              removeFrameRectConfig, downscalingConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(swizzleConfig, removeFrameRectConfig,
                              downscalingConfig, surfaceConfig);
            }
          } else {  // (removeFrameRect is false)
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              downscalingConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              downscalingConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(swizzleConfig, downscalingConfig, surfaceConfig);
            }
          }
        } else {  // (downscale is false)
          if (blendAnimation) {
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              blendAnimationConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              blendAnimationConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe =
                  MakePipe(swizzleConfig, blendAnimationConfig, surfaceConfig);
            }
          } else if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(swizzleConfig, deinterlacingConfig,
                              removeFrameRectConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(swizzleConfig, interpolatingConfig,
                              removeFrameRectConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe =
                  MakePipe(swizzleConfig, removeFrameRectConfig, surfaceConfig);
            }
          } else {  // (blendAnimation and removeFrameRect is false)
            if (deinterlace) {
              pipe =
                  MakePipe(swizzleConfig, deinterlacingConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe =
                  MakePipe(swizzleConfig, interpolatingConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(swizzleConfig, surfaceConfig);
            }
          }
        }
      }
    } else {  // (unpackOrMaskSwizzle and swapOrAlphaSwizzle are false)
      if (colorManagement) {
        if (downscale) {
          MOZ_ASSERT(!blendAnimation);
          if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(deinterlacingConfig, removeFrameRectConfig,
                              downscalingConfig, colorManagementConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(interpolatingConfig, removeFrameRectConfig,
                              downscalingConfig, colorManagementConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(removeFrameRectConfig, downscalingConfig,
                              colorManagementConfig, surfaceConfig);
            }
          } else {  // (removeFrameRect is false)
            if (deinterlace) {
              pipe = MakePipe(deinterlacingConfig, downscalingConfig,
                              colorManagementConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(interpolatingConfig, downscalingConfig,
                              colorManagementConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(downscalingConfig, colorManagementConfig,
                              surfaceConfig);
            }
          }
        } else {  // (downscale is false)
          if (blendAnimation) {
            if (deinterlace) {
              pipe = MakePipe(deinterlacingConfig, colorManagementConfig,
                              blendAnimationConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(interpolatingConfig, colorManagementConfig,
                              blendAnimationConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(colorManagementConfig, blendAnimationConfig,
                              surfaceConfig);
            }
          } else if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(deinterlacingConfig, colorManagementConfig,
                              removeFrameRectConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(interpolatingConfig, colorManagementConfig,
                              removeFrameRectConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(colorManagementConfig, removeFrameRectConfig,
                              surfaceConfig);
            }
          } else {  // (blendAnimation and removeFrameRect is false)
            if (deinterlace) {
              pipe = MakePipe(deinterlacingConfig, colorManagementConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(interpolatingConfig, colorManagementConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(colorManagementConfig, surfaceConfig);
            }
          }
        }
      } else {  // (colorManagement is false)
        if (downscale) {
          MOZ_ASSERT(!blendAnimation);
          if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(deinterlacingConfig, removeFrameRectConfig,
                              downscalingConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(interpolatingConfig, removeFrameRectConfig,
                              downscalingConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(removeFrameRectConfig, downscalingConfig,
                              surfaceConfig);
            }
          } else {  // (removeFrameRect is false)
            if (deinterlace) {
              pipe = MakePipe(deinterlacingConfig, downscalingConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(interpolatingConfig, downscalingConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(downscalingConfig, surfaceConfig);
            }
          }
        } else {  // (downscale is false)
          if (blendAnimation) {
            if (deinterlace) {
              pipe = MakePipe(deinterlacingConfig, blendAnimationConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(interpolatingConfig, blendAnimationConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(blendAnimationConfig, surfaceConfig);
            }
          } else if (removeFrameRect) {
            if (deinterlace) {
              pipe = MakePipe(deinterlacingConfig, removeFrameRectConfig,
                              surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(interpolatingConfig, removeFrameRectConfig,
                              surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(removeFrameRectConfig, surfaceConfig);
            }
          } else {  // (blendAnimation and removeFrameRect is false)
            if (deinterlace) {
              pipe = MakePipe(deinterlacingConfig, surfaceConfig);
            } else if (adam7Interpolate) {
              pipe = MakePipe(interpolatingConfig, surfaceConfig);
            } else {  // (deinterlace and adam7Interpolate are false)
              pipe = MakePipe(surfaceConfig);
            }
          }
        }
      }
    }

    return pipe;
  }

  /**
   * Creates and initializes a reorienting SurfacePipe.
   *
   * @param aDecoder The decoder whose current frame the SurfacePipe will write
   *                 to.
   * @param aInputSize The original size of the image.
   * @param aOutputSize The size the SurfacePipe should output. Must be the same
   *                    as @aInputSize or smaller. If smaller, the image will be
   *                    downscaled during decoding.
   * @param aFormat The surface format of the image; generally B8G8R8A8 or
   *                B8G8R8X8.
   * @param aOrientation The orientation of the image.
   *
   * @return A SurfacePipe if the parameters allowed one to be created
   *         successfully, or Nothing() if the SurfacePipe could not be
   *         initialized.
   */
  static Maybe<SurfacePipe> CreateReorientSurfacePipe(
      Decoder* aDecoder, const OrientedIntSize& aInputSize,
      const OrientedIntSize& aOutputSize, gfx::SurfaceFormat aFormat,
      qcms_transform* aTransform, const Orientation& aOrientation) {
    const bool downscale = aInputSize != aOutputSize;
    const bool colorManagement = aTransform != nullptr;

    // Construct configurations for the SurfaceFilters. Note that the order of
    // these filters is significant. We want to deinterlace or interpolate raw
    // input rows, before any other transformations, and we want to remove the
    // frame rect (which may involve adding blank rows or columns to the image)
    // before any downscaling, so that the new rows and columns are taken into
    // account.
    DownscalingConfig downscalingConfig{
        aOrientation.ToUnoriented(aInputSize).ToUnknownSize(), aFormat};
    ColorManagementConfig colorManagementConfig{aTransform};
    SurfaceConfig surfaceConfig{aDecoder, aOutputSize.ToUnknownSize(), aFormat,
                                /* mFlipVertically */ false,
                                /* mAnimParams */ Nothing()};
    ReorientSurfaceConfig reorientSurfaceConfig{aDecoder, aOutputSize, aFormat,
                                                aOrientation};

    Maybe<SurfacePipe> pipe;

    if (aOrientation.IsIdentity()) {
      if (colorManagement) {
        if (downscale) {
          pipe =
              MakePipe(downscalingConfig, colorManagementConfig, surfaceConfig);
        } else {  // (downscale is false)
          pipe = MakePipe(colorManagementConfig, surfaceConfig);
        }
      } else {  // (colorManagement is false)
        if (downscale) {
          pipe = MakePipe(downscalingConfig, surfaceConfig);
        } else {  // (downscale is false)
          pipe = MakePipe(surfaceConfig);
        }
      }
    } else {  // (orientation is not identity)
      if (colorManagement) {
        if (downscale) {
          pipe = MakePipe(downscalingConfig, colorManagementConfig,
                          reorientSurfaceConfig);
        } else {  // (downscale is false)
          pipe = MakePipe(colorManagementConfig, reorientSurfaceConfig);
        }
      } else {  // (colorManagement is false)
        if (downscale) {
          pipe = MakePipe(downscalingConfig, reorientSurfaceConfig);
        } else {  // (downscale is false)
          pipe = MakePipe(reorientSurfaceConfig);
        }
      }
    }

    return pipe;
  }

 private:
  template <typename... Configs>
  static Maybe<SurfacePipe> MakePipe(const Configs&... aConfigs) {
    auto pipe = MakeUnique<typename detail::FilterPipeline<Configs...>::Type>();
    nsresult rv = pipe->Configure(aConfigs...);
    if (NS_FAILED(rv)) {
      return Nothing();
    }

    return Some(SurfacePipe{std::move(pipe)});
  }

  virtual ~SurfacePipeFactory() = 0;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_SurfacePipeFactory_h
