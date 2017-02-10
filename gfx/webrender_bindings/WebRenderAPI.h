/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_WEBRENDERAPI_H
#define MOZILLA_LAYERS_WEBRENDERAPI_H

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Range.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "Units.h"

namespace mozilla {

namespace widget {
class CompositorWidget;
}

namespace layers {
class CompositorBridgeParentBase;
}

namespace wr {

class DisplayListBuilder;
class RendererOGL;
class RendererEvent;

class WebRenderAPI
{
  NS_INLINE_DECL_REFCOUNTING(WebRenderAPI);

public:
  /// This can be called on the compositor thread only.
  static already_AddRefed<WebRenderAPI> Create(bool aEnableProfiler,
                                               layers::CompositorBridgeParentBase* aBridge,
                                               RefPtr<widget::CompositorWidget>&& aWidget);

  wr::WindowId GetId() const { return mId; }

  void SetRootDisplayList(gfx::Color aBgColor,
                          wr::Epoch aEpoch,
                          LayerSize aViewportSize,
                          DisplayListBuilder& aBuilder);

  void SetRootPipeline(wr::PipelineId aPipeline);

  wr::ImageKey AddImageBuffer(gfx::IntSize aSize,
                              uint32_t aStride,
                              gfx::SurfaceFormat aFormat,
                              Range<uint8_t> aBytes);

  wr::ImageKey AddExternalImageHandle(gfx::IntSize aSize,
                                      gfx::SurfaceFormat aFormat,
                                      uint64_t aHandle);

  void UpdateImageBuffer(wr::ImageKey aKey,
                         gfx::IntSize aSize,
                         gfx::SurfaceFormat aFormat,
                         Range<uint8_t> aBytes);

  void DeleteImage(wr::ImageKey aKey);

  wr::FontKey AddRawFont(Range<uint8_t> aBytes);

  void DeleteFont(wr::FontKey aKey);

  void SetProfilerEnabled(bool aEnabled);

  void RunOnRenderThread(UniquePtr<RendererEvent> aEvent);
  void Readback(gfx::IntSize aSize, uint8_t *aBuffer, uint32_t aBufferSize);

  GLint GetMaxTextureSize() const { return mMaxTextureSize; }

protected:
  WebRenderAPI(WrAPI* aRawApi, wr::WindowId aId, GLint aMaxTextureSize)
    : mWrApi(aRawApi)
    , mId(aId)
    , mMaxTextureSize(aMaxTextureSize)
  {}

  ~WebRenderAPI();

  WrAPI* mWrApi;
  wr::WindowId mId;
  GLint mMaxTextureSize;

  friend class DisplayListBuilder;
};

/// This is a simple C++ wrapper around WrState defined in the rust bindings.
/// We may want to turn this into a direct wrapper on top of WebRenderFrameBuilder
/// instead, so the interface may change a bit.
class DisplayListBuilder {
public:
  DisplayListBuilder(const LayerIntSize& aSize, wr::PipelineId aId);
  DisplayListBuilder(DisplayListBuilder&&) = default;

  ~DisplayListBuilder();

  void Begin(const LayerIntSize& aSize);

  void End(WebRenderAPI& aApi, wr::Epoch aEpoch);

  void PushStackingContext(const WrRect& aBounds, // TODO: We should work with strongly typed rects
                           const WrRect& aOverflow,
                           const WrImageMask* aMask, // TODO: needs a wrapper.
                           const float aOpacity,
                           const gfx::Matrix4x4& aTransform,
                           const WrMixBlendMode& aMixBlendMode);

  void PopStackingContext();

  void PushRect(const WrRect& aBounds,
                const WrRect& aClip,
                const WrColor& aColor);

  void PushImage(const WrRect& aBounds,
                 const WrRect& aClip,
                 const WrImageMask* aMask,
                 const WrTextureFilter aFilter,
                 wr::ImageKey aImage);

  void PushIFrame(const WrRect& aBounds,
                  const WrRect& aClip,
                  wr::PipelineId aPipeline);

  void PushBorder(const WrRect& aBounds,
                  const WrRect& aClip,
                  const WrBorderSide& aTop,
                  const WrBorderSide& aRight,
                  const WrBorderSide& aBbottom,
                  const WrBorderSide& aLeft,
                  const WrBorderRadius& aRadius);

  void PushText(const WrRect& aBounds,
                const WrRect& aClip,
                const gfx::Color& aColor,
                wr::FontKey aFontKey,
                Range<const WrGlyphInstance> aGlyphBuffer,
                float aGlyphSize);

  // Try to avoid using this when possible.
  WrState* Raw() { return mWrState; }
protected:
  WrState* mWrState;

  friend class WebRenderAPI;
};

Maybe<WrImageFormat>
SurfaceFormatToWrImageFormat(gfx::SurfaceFormat aFormat);

} // namespace wr
} // namespace mozilla

#endif
