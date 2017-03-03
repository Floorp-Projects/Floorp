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

  void GenerateFrame();

  void SetRootDisplayList(gfx::Color aBgColor,
                          Epoch aEpoch,
                          LayerSize aViewportSize,
                          WrPipelineId pipeline_id,
                          WrBuiltDisplayListDescriptor dl_descriptor,
                          uint8_t *dl_data,
                          size_t dl_size,
                          WrAuxiliaryListsDescriptor aux_descriptor,
                          uint8_t *aux_data,
                          size_t aux_size);

  void SetRootPipeline(wr::PipelineId aPipeline);

  void AddImage(wr::ImageKey aKey,
                const ImageDescriptor& aDescriptor,
                Range<uint8_t> aBytes);

  void AddExternalImageHandle(ImageKey key,
                              gfx::IntSize aSize,
                              gfx::SurfaceFormat aFormat,
                              uint64_t aHandle);

  void AddExternalImageBuffer(ImageKey key,
                              gfx::IntSize aSize,
                              gfx::SurfaceFormat aFormat,
                              uint64_t aHandle);

  void UpdateImageBuffer(wr::ImageKey aKey,
                         const ImageDescriptor& aDescriptor,
                         Range<uint8_t> aBytes);

  void DeleteImage(wr::ImageKey aKey);

  void AddRawFont(wr::FontKey aKey, Range<uint8_t> aBytes);

  void DeleteFont(wr::FontKey aKey);

  void SetProfilerEnabled(bool aEnabled);

  void RunOnRenderThread(UniquePtr<RendererEvent> aEvent);
  void Readback(gfx::IntSize aSize, uint8_t *aBuffer, uint32_t aBufferSize);

  WrIdNamespace GetNamespace();
  GLint GetMaxTextureSize() const { return mMaxTextureSize; }
  bool GetUseANGLE() const { return mUseANGLE; }

protected:
  WebRenderAPI(WrAPI* aRawApi, wr::WindowId aId, GLint aMaxTextureSize, bool aUseANGLE)
    : mWrApi(aRawApi)
    , mId(aId)
    , mMaxTextureSize(aMaxTextureSize)
    , mUseANGLE(aUseANGLE)
  {}

  ~WebRenderAPI();

  WrAPI* mWrApi;
  wr::WindowId mId;
  GLint mMaxTextureSize;
  bool mUseANGLE;

  friend class DisplayListBuilder;
};

/// This is a simple C++ wrapper around WrState defined in the rust bindings.
/// We may want to turn this into a direct wrapper on top of WebRenderFrameBuilder
/// instead, so the interface may change a bit.
class DisplayListBuilder {
public:
  explicit DisplayListBuilder(wr::PipelineId aId);
  DisplayListBuilder(DisplayListBuilder&&) = default;

  ~DisplayListBuilder();

  void Begin(const LayerIntSize& aSize);

  void End();
  void Finalize(WrBuiltDisplayListDescriptor& dl_descriptor,
                wr::VecU8& dl_data,
                WrAuxiliaryListsDescriptor& aux_descriptor,
                wr::VecU8& aux_data);

  void PushStackingContext(const WrRect& aBounds, // TODO: We should work with strongly typed rects
                           const WrRect& aOverflow,
                           const WrImageMask* aMask, // TODO: needs a wrapper.
                           const float aOpacity,
                           const gfx::Matrix4x4& aTransform,
                           const WrMixBlendMode& aMixBlendMode);

  void PopStackingContext();

  void PushScrollLayer(const WrRect& aBounds, // TODO: We should work with strongly typed rects
                       const WrRect& aOverflow,
                       const WrImageMask* aMask); // TODO: needs a wrapper.

  void PopScrollLayer();


  void PushRect(const WrRect& aBounds,
                const WrRect& aClip,
                const WrColor& aColor);

  void PushLinearGradient(const WrRect& aBounds,
                          const WrRect& aClip,
                          const WrPoint& aStartPoint,
                          const WrPoint& aEndPoint,
                          const nsTArray<WrGradientStop>& aStops,
                          wr::GradientExtendMode aExtendMode);

  void PushRadialGradient(const WrRect& aBounds,
                          const WrRect& aClip,
                          const WrPoint& aStartCenter,
                          const WrPoint& aEndCenter,
                          float aStartRadius,
                          float aEndRadius,
                          const nsTArray<WrGradientStop>& aStops,
                          wr::GradientExtendMode aExtendMode);

  void PushImage(const WrRect& aBounds,
                 const WrRect& aClip,
                 const WrImageMask* aMask,
                 wr::ImageRendering aFilter,
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

  void PushBoxShadow(const WrRect& aRect,
                     const WrRect& aClip,
                     const WrRect& aBoxBounds,
                     const WrPoint& aOffset,
                     const WrColor& aColor,
                     const float& aBlurRadius,
                     const float& aSpreadRadius,
                     const float& aBorderRadius,
                     const WrBoxShadowClipMode& aClipMode);

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
