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
#include "FrameMetrics.h"
#include "GLTypes.h"
#include "Units.h"

namespace mozilla {

namespace widget {
class CompositorWidget;
}

namespace layers {
class CompositorBridgeParentBase;
class WebRenderBridgeParent;
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
                                               RefPtr<widget::CompositorWidget>&& aWidget,
                                               LayoutDeviceIntSize aSize);

  wr::WindowId GetId() const { return mId; }

  void UpdateScrollPosition(const WrPipelineId& aPipelineId,
                            const layers::FrameMetrics::ViewID& aScrollId,
                            const WrPoint& aScrollPosition);

  void GenerateFrame();
  void GenerateFrame(const nsTArray<WrOpacityProperty>& aOpacityArray,
                     const nsTArray<WrTransformProperty>& aTransformArray);

  void SetWindowParameters(LayoutDeviceIntSize size);
  void SetRootDisplayList(gfx::Color aBgColor,
                          Epoch aEpoch,
                          LayerSize aViewportSize,
                          WrPipelineId pipeline_id,
                          const WrSize& content_size,
                          WrBuiltDisplayListDescriptor dl_descriptor,
                          uint8_t *dl_data,
                          size_t dl_size);

  void ClearRootDisplayList(Epoch aEpoch,
                            WrPipelineId pipeline_id);

  void SetRootPipeline(wr::PipelineId aPipeline);

  void AddImage(wr::ImageKey aKey,
                const ImageDescriptor& aDescriptor,
                Range<uint8_t> aBytes);

  void AddBlobImage(wr::ImageKey aKey,
                    const ImageDescriptor& aDescriptor,
                    Range<uint8_t> aBytes);

  void AddExternalImageBuffer(ImageKey key,
                              const ImageDescriptor& aDescriptor,
                              ExternalImageId aHandle);

  void AddExternalImage(ImageKey key,
                        const ImageDescriptor& aDescriptor,
                        ExternalImageId aExtID,
                        WrExternalImageBufferType aBufferType,
                        uint8_t aChannelIndex);

  void UpdateImageBuffer(wr::ImageKey aKey,
                         const ImageDescriptor& aDescriptor,
                         Range<uint8_t> aBytes);

  void DeleteImage(wr::ImageKey aKey);

  void AddRawFont(wr::FontKey aKey, Range<uint8_t> aBytes, uint32_t aIndex);

  void DeleteFont(wr::FontKey aKey);

  void SetProfilerEnabled(bool aEnabled);

  void RunOnRenderThread(UniquePtr<RendererEvent> aEvent);
  void Readback(gfx::IntSize aSize, uint8_t *aBuffer, uint32_t aBufferSize);

  void Pause();
  bool Resume();

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
  // Should be used only for shutdown handling
  void WaitFlushed();

  WrAPI* mWrApi;
  wr::WindowId mId;
  GLint mMaxTextureSize;
  bool mUseANGLE;

  friend class DisplayListBuilder;
  friend class layers::WebRenderBridgeParent;
};

/// This is a simple C++ wrapper around WrState defined in the rust bindings.
/// We may want to turn this into a direct wrapper on top of WebRenderFrameBuilder
/// instead, so the interface may change a bit.
class DisplayListBuilder {
public:
  explicit DisplayListBuilder(wr::PipelineId aId,
                              const WrSize& aContentSize);
  DisplayListBuilder(DisplayListBuilder&&) = default;

  ~DisplayListBuilder();

  void Begin(const LayerIntSize& aSize);

  void End();
  void Finalize(WrSize& aOutContentSize,
                wr::BuiltDisplayList& aOutDisplayList);

  void PushStackingContext(const WrRect& aBounds, // TODO: We should work with strongly typed rects
                           const uint64_t& aAnimationId,
                           const float* aOpacity,
                           const gfx::Matrix4x4* aTransform,
                           const WrMixBlendMode& aMixBlendMode);
  void PopStackingContext();

  void PushClip(const WrRect& aClipRect,
                const WrImageMask* aMask);
  void PopClip();

  void PushBuiltDisplayList(wr::BuiltDisplayList dl);

  void PushScrollLayer(const layers::FrameMetrics::ViewID& aScrollId,
                       const WrRect& aContentRect, // TODO: We should work with strongly typed rects
                       const WrRect& aClipRect);
  void PopScrollLayer();


  void PushRect(const WrRect& aBounds,
                const WrClipRegionToken aClip,
                const WrColor& aColor);

  void PushLinearGradient(const WrRect& aBounds,
                          const WrClipRegionToken aClip,
                          const WrPoint& aStartPoint,
                          const WrPoint& aEndPoint,
                          const nsTArray<WrGradientStop>& aStops,
                          wr::GradientExtendMode aExtendMode,
                          const WrSize aTileSize,
                          const WrSize aTileSpacing);

  void PushRadialGradient(const WrRect& aBounds,
                          const WrClipRegionToken aClip,
                          const WrPoint& aCenter,
                          const WrSize& aRadius,
                          const nsTArray<WrGradientStop>& aStops,
                          wr::GradientExtendMode aExtendMode,
                          const WrSize aTileSize,
                          const WrSize aTileSpacing);

  void PushImage(const WrRect& aBounds,
                 const WrClipRegionToken aClip,
                 wr::ImageRendering aFilter,
                 wr::ImageKey aImage);

  void PushImage(const WrRect& aBounds,
                 const WrClipRegionToken aClip,
                 const WrSize& aStretchSize,
                 const WrSize& aTileSpacing,
                 wr::ImageRendering aFilter,
                 wr::ImageKey aImage);

  void PushYCbCrPlanarImage(const WrRect& aBounds,
                            const WrClipRegionToken aClip,
                            wr::ImageKey aImageChannel0,
                            wr::ImageKey aImageChannel1,
                            wr::ImageKey aImageChannel2,
                            WrYuvColorSpace aColorSpace,
                            wr::ImageRendering aFilter);

  void PushNV12Image(const WrRect& aBounds,
                     const WrClipRegionToken aClip,
                     wr::ImageKey aImageChannel0,
                     wr::ImageKey aImageChannel1,
                     WrYuvColorSpace aColorSpace,
                     wr::ImageRendering aFilter);

  void PushYCbCrInterleavedImage(const WrRect& aBounds,
                                 const WrClipRegionToken aClip,
                                 wr::ImageKey aImageChannel0,
                                 WrYuvColorSpace aColorSpace,
                                 wr::ImageRendering aFilter);

  void PushIFrame(const WrRect& aBounds,
                  const WrClipRegionToken aClip,
                  wr::PipelineId aPipeline);

  void PushBorder(const WrRect& aBounds,
                  const WrClipRegionToken aClip,
                  const WrBorderWidths& aWidths,
                  const WrBorderSide& aTop,
                  const WrBorderSide& aRight,
                  const WrBorderSide& aBbottom,
                  const WrBorderSide& aLeft,
                  const WrBorderRadius& aRadius);

  void PushBorderImage(const WrRect& aBounds,
                       const WrClipRegionToken aClip,
                       const WrBorderWidths& aWidths,
                       wr::ImageKey aImage,
                       const WrNinePatchDescriptor& aPatch,
                       const WrSideOffsets2Df32& aOutset,
                       const WrRepeatMode& aRepeatHorizontal,
                       const WrRepeatMode& aRepeatVertical);

  void PushBorderGradient(const WrRect& aBounds,
                          const WrClipRegionToken aClip,
                          const WrBorderWidths& aWidths,
                          const WrPoint& aStartPoint,
                          const WrPoint& aEndPoint,
                          const nsTArray<WrGradientStop>& aStops,
                          wr::GradientExtendMode aExtendMode,
                          const WrSideOffsets2Df32& aOutset);

  void PushBorderRadialGradient(const WrRect& aBounds,
                                const WrClipRegionToken aClip,
                                const WrBorderWidths& aWidths,
                                const WrPoint& aCenter,
                                const WrSize& aRadius,
                                const nsTArray<WrGradientStop>& aStops,
                                wr::GradientExtendMode aExtendMode,
                                const WrSideOffsets2Df32& aOutset);

  void PushText(const WrRect& aBounds,
                const WrClipRegionToken aClip,
                const gfx::Color& aColor,
                wr::FontKey aFontKey,
                Range<const WrGlyphInstance> aGlyphBuffer,
                float aGlyphSize);

  void PushBoxShadow(const WrRect& aRect,
                     const WrClipRegionToken aClip,
                     const WrRect& aBoxBounds,
                     const WrPoint& aOffset,
                     const WrColor& aColor,
                     const float& aBlurRadius,
                     const float& aSpreadRadius,
                     const float& aBorderRadius,
                     const WrBoxShadowClipMode& aClipMode);

  WrClipRegionToken PushClipRegion(const WrRect& aMain,
                                   const WrImageMask* aMask = nullptr);
  WrClipRegionToken PushClipRegion(const WrRect& aMain,
                                   const nsTArray<WrComplexClipRegion>& aComplex,
                                   const WrImageMask* aMask = nullptr);

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
