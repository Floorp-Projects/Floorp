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
 * Flags for SurfacePipeFactory, used in conjuction with the factory functions
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
      Decoder* aDecoder, const nsIntSize& aInputSize,
      const nsIntSize& aOutputSize, const nsIntRect& aFrameRect,
      gfx::SurfaceFormat aFormat, const Maybe<AnimationParams>& aAnimParams,
      SurfacePipeFlags aFlags) {
    const bool deinterlace = bool(aFlags & SurfacePipeFlags::DEINTERLACE);
    const bool flipVertically =
        bool(aFlags & SurfacePipeFlags::FLIP_VERTICALLY);
    const bool progressiveDisplay =
        bool(aFlags & SurfacePipeFlags::PROGRESSIVE_DISPLAY);
    const bool downscale = aInputSize != aOutputSize;
    const bool removeFrameRect = !aFrameRect.IsEqualEdges(
        nsIntRect(0, 0, aInputSize.width, aInputSize.height));
    const bool blendAnimation = aAnimParams.isSome();

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
    RemoveFrameRectConfig removeFrameRectConfig{aFrameRect};
    BlendAnimationConfig blendAnimationConfig{aDecoder};
    DownscalingConfig downscalingConfig{aInputSize, aFormat};
    SurfaceConfig surfaceConfig{aDecoder, aOutputSize, aFormat, flipVertically,
                                aAnimParams};

    Maybe<SurfacePipe> pipe;

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
          pipe =
              MakePipe(removeFrameRectConfig, downscalingConfig, surfaceConfig);
        }
      } else {  // (removeFrameRect is false)
        if (deinterlace) {
          pipe =
              MakePipe(deinterlacingConfig, downscalingConfig, surfaceConfig);
        } else if (adam7Interpolate) {
          pipe =
              MakePipe(interpolatingConfig, downscalingConfig, surfaceConfig);
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
