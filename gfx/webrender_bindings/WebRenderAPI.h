/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_WEBRENDERAPI_H
#define MOZILLA_LAYERS_WEBRENDERAPI_H

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Range.h"
#include "mozilla/gfx/webrender.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "Units.h"

namespace mozilla {

namespace widget {
class CompositorWidget;
}

namespace layers {

class DisplayListBuilder;
class CompositorBridgeParentBase;
class RendererOGL;
class NewRenderer;

class WebRenderAPI
{
NS_INLINE_DECL_REFCOUNTING(WebRenderAPI);
public:
  /// Compositor thread only.
  static already_AddRefed<WebRenderAPI> Create(bool aEnableProfiler,
                                               CompositorBridgeParentBase* aBridge,
                                               RefPtr<widget::CompositorWidget>&& aWidget);

  gfx::WindowId GetId() const { return mId; }

  void SetRootDisplayList(gfx::Color aBgColor,
                          gfx::Epoch aEpoch,
                          LayerSize aViewportSize,
                          DisplayListBuilder& aBuilder);

  void SetRootPipeline(gfx::PipelineId aPipeline);

  gfx::ImageKey AddImageBuffer(gfx::IntSize aSize,
                               uint32_t aStride,
                               gfx::SurfaceFormat aFormat,
                               Range<uint8_t> aBytes);

  gfx::ImageKey AddExternalImageHandle(gfx::IntSize aSize,
                                       gfx::SurfaceFormat aFormat,
                                       uint64_t aHandle);

  void UpdateImageBuffer(gfx::ImageKey aKey,
                         gfx::IntSize aSize,
                         gfx::SurfaceFormat aFormat,
                         Range<uint8_t> aBytes);

  void DeleteImage(gfx::ImageKey aKey);

  gfx::FontKey AddRawFont(Range<uint8_t> aBytes);

  void DeleteFont(gfx::FontKey aKey);

  void SetProfilerEnabled(bool aEnabled);

protected:
  WebRenderAPI(WrAPI* aRawApi, gfx::WindowId aId)
  : mWRApi(aRawApi)
  , mId(aId)
  {}

  ~WebRenderAPI();

  WrAPI* mWRApi;
  gfx::WindowId mId;

  friend class NewRenderer;
  friend class DisplayListBuilder;
};

/// This is a simple C++ wrapper around WRState defined in the rust bindings.
/// We may want to turn this into a direct wrapper on top of WebRenderFrameBuilder
/// instead, so the interface may change a bit.
class DisplayListBuilder {
public:
  DisplayListBuilder(const LayerIntSize& aSize, gfx::PipelineId aId);
  DisplayListBuilder(DisplayListBuilder&&) = default;

  ~DisplayListBuilder();

  void Begin(const LayerIntSize& aSize);

  void End(WebRenderAPI& aApi, gfx::Epoch aEpoch);

  void PushStackingContext(const WRRect& aBounds, // TODO: We should work with strongly typed rects
                           const WRRect& aOverflow,
                           const WRImageMask* aMask, // TODO: needs a wrapper.
                           const gfx::Matrix4x4& aTransform);

  void PopStackingContext();

  void PushRect(const WRRect& aBounds,
                const WRRect& aClip,
                const gfx::Color& aColor);

  void PushImage(const WRRect& aBounds,
                 const WRRect& aClip,
                 const WRImageMask* aMask,
                 const WRTextureFilter aFilter,
                 WRImageKey aImage);

  void PushIFrame(const WRRect& aBounds,
                  const WRRect& aClip,
                  gfx::PipelineId aPipeline);

  void PushBorder(const WRRect& bounds,
                  const WRRect& clip,
                  const WRBorderSide& top,
                  const WRBorderSide& right,
                  const WRBorderSide& bottom,
                  const WRBorderSide& left,
                  const WRLayoutSize& top_left_radius,
                  const WRLayoutSize& top_right_radius,
                  const WRLayoutSize& bottom_left_radius,
                  const WRLayoutSize& bottom_right_radius);

  void PushText(const WRRect& aBounds,
                const WRRect& aClip,
                const gfx::Color& aColor,
                gfx::FontKey aFontKey,
                Range<const WRGlyphInstance> aGlyphBuffer,
                float aGlyphSize);

  // Try to avoid using this when possible.
  WRState* Raw() { return mWRState; }
protected:
  WRState* mWRState;

  friend class WebRenderAPI;
};

} // namespace
} // namespace

#endif