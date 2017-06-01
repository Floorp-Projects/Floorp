/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderAPI.h"
#include "LayersLogging.h"
#include "mozilla/webrender/RendererOGL.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/layers/SynchronousTask.h"

#define WRDL_LOG(...)
//#define WRDL_LOG(...) printf_stderr("WRDL: " __VA_ARGS__)

namespace mozilla {
namespace wr {

using layers::Stringify;

class NewRenderer : public RendererEvent
{
public:
  NewRenderer(WrAPI** aApi, layers::CompositorBridgeParentBase* aBridge,
              GLint* aMaxTextureSize,
              bool* aUseANGLE,
              RefPtr<widget::CompositorWidget>&& aWidget,
              layers::SynchronousTask* aTask,
              bool aEnableProfiler,
              LayoutDeviceIntSize aSize)
    : mWrApi(aApi)
    , mMaxTextureSize(aMaxTextureSize)
    , mUseANGLE(aUseANGLE)
    , mBridge(aBridge)
    , mCompositorWidget(Move(aWidget))
    , mTask(aTask)
    , mEnableProfiler(aEnableProfiler)
    , mSize(aSize)
  {
    MOZ_COUNT_CTOR(NewRenderer);
  }

  ~NewRenderer()
  {
    MOZ_COUNT_DTOR(NewRenderer);
  }

  virtual void Run(RenderThread& aRenderThread, WindowId aWindowId) override
  {
    layers::AutoCompleteTask complete(mTask);

    RefPtr<gl::GLContext> gl;
    if (gfx::gfxVars::UseWebRenderANGLE()) {
      gl = gl::GLContextProviderEGL::CreateForCompositorWidget(mCompositorWidget, true);
      if (!gl || !gl->IsANGLE()) {
        gfxCriticalNote << "Failed ANGLE GL context creation for WebRender: " << gfx::hexa(gl.get());
        return;
      }
    }
    if (!gl) {
      gl = gl::GLContextProvider::CreateForCompositorWidget(mCompositorWidget, true);
    }
    if (!gl || !gl->MakeCurrent()) {
      gfxCriticalNote << "Failed GL context creation for WebRender: " << gfx::hexa(gl.get());
      return;
    }

    gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, mMaxTextureSize);
    *mUseANGLE = gl->IsANGLE();

    WrRenderer* wrRenderer = nullptr;
    if (!wr_window_new(aWindowId, mSize.width, mSize.height, gl.get(),
                       this->mEnableProfiler, mWrApi, &wrRenderer)) {
      // wr_window_new puts a message into gfxCriticalNote if it returns false
      return;
    }
    MOZ_ASSERT(wrRenderer);

    RefPtr<RenderThread> thread = &aRenderThread;
    auto renderer = MakeUnique<RendererOGL>(Move(thread),
                                            Move(gl),
                                            Move(mCompositorWidget),
                                            aWindowId,
                                            wrRenderer,
                                            mBridge);
    if (wrRenderer && renderer) {
      WrExternalImageHandler handler = renderer->GetExternalImageHandler();
      wr_renderer_set_external_image_handler(wrRenderer, &handler);
    }

    aRenderThread.AddRenderer(aWindowId, Move(renderer));
  }

private:
  WrAPI** mWrApi;
  GLint* mMaxTextureSize;
  bool* mUseANGLE;
  layers::CompositorBridgeParentBase* mBridge;
  RefPtr<widget::CompositorWidget> mCompositorWidget;
  layers::SynchronousTask* mTask;
  bool mEnableProfiler;
  LayoutDeviceIntSize mSize;
};

class RemoveRenderer : public RendererEvent
{
public:
  explicit RemoveRenderer(layers::SynchronousTask* aTask)
    : mTask(aTask)
  {
    MOZ_COUNT_CTOR(RemoveRenderer);
  }

  ~RemoveRenderer()
  {
    MOZ_COUNT_DTOR(RemoveRenderer);
  }

  virtual void Run(RenderThread& aRenderThread, WindowId aWindowId) override
  {
    aRenderThread.RemoveRenderer(aWindowId);
    layers::AutoCompleteTask complete(mTask);
  }

private:
  layers::SynchronousTask* mTask;
};


//static
already_AddRefed<WebRenderAPI>
WebRenderAPI::Create(bool aEnableProfiler,
                     layers::CompositorBridgeParentBase* aBridge,
                     RefPtr<widget::CompositorWidget>&& aWidget,
                     LayoutDeviceIntSize aSize)
{
  MOZ_ASSERT(aBridge);
  MOZ_ASSERT(aWidget);

  static uint64_t sNextId = 1;
  auto id = NewWindowId(sNextId++);

  WrAPI* wrApi = nullptr;
  GLint maxTextureSize = 0;
  bool useANGLE = false;

  // Dispatch a synchronous task because the WrApi object needs to be created
  // on the render thread. If need be we could delay waiting on this task until
  // the next time we need to access the WrApi object.
  layers::SynchronousTask task("Create Renderer");
  auto event = MakeUnique<NewRenderer>(&wrApi, aBridge, &maxTextureSize, &useANGLE,
                                       Move(aWidget), &task, aEnableProfiler, aSize);
  RenderThread::Get()->RunEvent(id, Move(event));

  task.Wait();

  if (!wrApi) {
    return nullptr;
  }

  return RefPtr<WebRenderAPI>(new WebRenderAPI(wrApi, id, maxTextureSize, useANGLE)).forget();
}

WrIdNamespace
WebRenderAPI::GetNamespace() {
  return wr_api_get_namespace(mWrApi);
}

WebRenderAPI::~WebRenderAPI()
{
  layers::SynchronousTask task("Destroy WebRenderAPI");
  auto event = MakeUnique<RemoveRenderer>(&task);
  RunOnRenderThread(Move(event));
  task.Wait();

  wr_api_delete(mWrApi);
}

void
WebRenderAPI::UpdateScrollPosition(const WrPipelineId& aPipelineId,
                                   const layers::FrameMetrics::ViewID& aScrollId,
                                   const WrPoint& aScrollPosition)
{
  wr_scroll_layer_with_id(mWrApi, aPipelineId, aScrollId, aScrollPosition);
}

void
WebRenderAPI::GenerateFrame()
{
  wr_api_generate_frame(mWrApi);
}

void
WebRenderAPI::GenerateFrame(const nsTArray<WrOpacityProperty>& aOpacityArray,
                            const nsTArray<WrTransformProperty>& aTransformArray)
{
  wr_api_generate_frame_with_properties(mWrApi,
                                        aOpacityArray.IsEmpty() ?
                                          nullptr : aOpacityArray.Elements(),
                                        aOpacityArray.Length(),
                                        aTransformArray.IsEmpty() ?
                                          nullptr : aTransformArray.Elements(),
                                        aTransformArray.Length());
}

void
WebRenderAPI::SetRootDisplayList(gfx::Color aBgColor,
                                 Epoch aEpoch,
                                 LayerSize aViewportSize,
                                 WrPipelineId pipeline_id,
                                 const WrSize& content_size,
                                 WrBuiltDisplayListDescriptor dl_descriptor,
                                 uint8_t *dl_data,
                                 size_t dl_size)
{
    wr_api_set_root_display_list(mWrApi,
                                 ToWrColor(aBgColor),
                                 aEpoch,
                                 aViewportSize.width, aViewportSize.height,
                                 pipeline_id,
                                 content_size,
                                 dl_descriptor,
                                 dl_data,
                                 dl_size);
}

void
WebRenderAPI::ClearRootDisplayList(Epoch aEpoch,
                                   WrPipelineId pipeline_id)
{
  wr_api_clear_root_display_list(mWrApi, aEpoch, pipeline_id);
}

void
WebRenderAPI::SetWindowParameters(LayoutDeviceIntSize size)
{
  wr_api_set_window_parameters(mWrApi, size.width, size.height);
}

void
WebRenderAPI::Readback(gfx::IntSize size,
                       uint8_t *buffer,
                       uint32_t buffer_size)
{
    class Readback : public RendererEvent
    {
        public:
            explicit Readback(layers::SynchronousTask* aTask,
                              gfx::IntSize aSize, uint8_t *aBuffer, uint32_t aBufferSize)
                : mTask(aTask)
                , mSize(aSize)
                , mBuffer(aBuffer)
                , mBufferSize(aBufferSize)
            {
                MOZ_COUNT_CTOR(Readback);
            }

            ~Readback()
            {
                MOZ_COUNT_DTOR(Readback);
            }

            virtual void Run(RenderThread& aRenderThread, WindowId aWindowId) override
            {
                aRenderThread.UpdateAndRender(aWindowId);
                wr_renderer_readback(aRenderThread.GetRenderer(aWindowId)->GetWrRenderer(),
                                     mSize.width, mSize.height, mBuffer, mBufferSize);
                layers::AutoCompleteTask complete(mTask);
            }

            layers::SynchronousTask* mTask;
            gfx::IntSize mSize;
            uint8_t *mBuffer;
            uint32_t mBufferSize;
    };

    layers::SynchronousTask task("Readback");
    auto event = MakeUnique<Readback>(&task, size, buffer, buffer_size);
    // This event will be passed from wr_backend thread to renderer thread. That
    // implies that all frame data have been processed when the renderer runs this
    // read-back event. Then, we could make sure this read-back event gets the
    // latest result.
    RunOnRenderThread(Move(event));

    task.Wait();
}

void
WebRenderAPI::Pause()
{
    class PauseEvent : public RendererEvent
    {
        public:
            explicit PauseEvent(layers::SynchronousTask* aTask)
                : mTask(aTask)
            {
                MOZ_COUNT_CTOR(PauseEvent);
            }

            ~PauseEvent()
            {
                MOZ_COUNT_DTOR(PauseEvent);
            }

            virtual void Run(RenderThread& aRenderThread, WindowId aWindowId) override
            {
                aRenderThread.Pause(aWindowId);
                layers::AutoCompleteTask complete(mTask);
            }

            layers::SynchronousTask* mTask;
    };

    layers::SynchronousTask task("Pause");
    auto event = MakeUnique<PauseEvent>(&task);
    // This event will be passed from wr_backend thread to renderer thread. That
    // implies that all frame data have been processed when the renderer runs this event.
    RunOnRenderThread(Move(event));

    task.Wait();
}

bool
WebRenderAPI::Resume()
{
    class ResumeEvent : public RendererEvent
    {
        public:
            explicit ResumeEvent(layers::SynchronousTask* aTask, bool* aResult)
                : mTask(aTask)
                , mResult(aResult)
            {
                MOZ_COUNT_CTOR(ResumeEvent);
            }

            ~ResumeEvent()
            {
                MOZ_COUNT_DTOR(ResumeEvent);
            }

            virtual void Run(RenderThread& aRenderThread, WindowId aWindowId) override
            {
                *mResult = aRenderThread.Resume(aWindowId);
                layers::AutoCompleteTask complete(mTask);
            }

            layers::SynchronousTask* mTask;
            bool* mResult;
    };

    bool result = false;
    layers::SynchronousTask task("Resume");
    auto event = MakeUnique<ResumeEvent>(&task, &result);
    // This event will be passed from wr_backend thread to renderer thread. That
    // implies that all frame data have been processed when the renderer runs this event.
    RunOnRenderThread(Move(event));

    task.Wait();
    return result;
}

void
WebRenderAPI::WaitFlushed()
{
    class WaitFlushedEvent : public RendererEvent
    {
        public:
            explicit WaitFlushedEvent(layers::SynchronousTask* aTask)
                : mTask(aTask)
            {
                MOZ_COUNT_CTOR(WaitFlushedEvent);
            }

            ~WaitFlushedEvent()
            {
                MOZ_COUNT_DTOR(WaitFlushedEvent);
            }

            virtual void Run(RenderThread& aRenderThread, WindowId aWindowId) override
            {
                layers::AutoCompleteTask complete(mTask);
            }

            layers::SynchronousTask* mTask;
    };

    layers::SynchronousTask task("WaitFlushed");
    auto event = MakeUnique<WaitFlushedEvent>(&task);
    // This event will be passed from wr_backend thread to renderer thread. That
    // implies that all frame data have been processed when the renderer runs this event.
    RunOnRenderThread(Move(event));

    task.Wait();
}

void
WebRenderAPI::SetRootPipeline(PipelineId aPipeline)
{
  wr_api_set_root_pipeline(mWrApi, aPipeline);
}

void
WebRenderAPI::AddImage(ImageKey key, const ImageDescriptor& aDescriptor,
                       Range<uint8_t> aBytes)
{
  wr_api_add_image(mWrApi,
                   key,
                   &aDescriptor,
                   RangeToByteSlice(aBytes));
}

void
WebRenderAPI::AddBlobImage(ImageKey key, const ImageDescriptor& aDescriptor,
                           Range<uint8_t> aBytes)
{
  wr_api_add_blob_image(mWrApi,
                        key,
                        &aDescriptor,
                        RangeToByteSlice(aBytes));
}

void
WebRenderAPI::AddExternalImage(ImageKey key,
                               const ImageDescriptor& aDescriptor,
                               ExternalImageId aExtID,
                               WrExternalImageBufferType aBufferType,
                               uint8_t aChannelIndex)
{
  wr_api_add_external_image(mWrApi,
                            key,
                            &aDescriptor,
                            aExtID,
                            aBufferType,
                            aChannelIndex);
}

void
WebRenderAPI::AddExternalImageBuffer(ImageKey key,
                                     const ImageDescriptor& aDescriptor,
                                     ExternalImageId aHandle)
{
  wr_api_add_external_image_buffer(mWrApi,
                                   key,
                                   &aDescriptor,
                                   aHandle);
}

void
WebRenderAPI::UpdateImageBuffer(ImageKey aKey,
                                const ImageDescriptor& aDescriptor,
                                Range<uint8_t> aBytes)
{
  wr_api_update_image(mWrApi,
                      aKey,
                      &aDescriptor,
                      RangeToByteSlice(aBytes));
}

void
WebRenderAPI::DeleteImage(ImageKey aKey)
{
  wr_api_delete_image(mWrApi, aKey);
}

void
WebRenderAPI::AddRawFont(wr::FontKey aKey, Range<uint8_t> aBytes, uint32_t aIndex)
{
  wr_api_add_raw_font(mWrApi, aKey, &aBytes[0], aBytes.length(), aIndex);
}

void
WebRenderAPI::DeleteFont(wr::FontKey aKey)
{
  wr_api_delete_font(mWrApi, aKey);
}

class EnableProfiler : public RendererEvent
{
public:
  explicit EnableProfiler(bool aEnabled)
    : mEnabled(aEnabled)
  {
    MOZ_COUNT_CTOR(EnableProfiler);
  }

  ~EnableProfiler()
  {
    MOZ_COUNT_DTOR(EnableProfiler);
  }

  virtual void Run(RenderThread& aRenderThread, WindowId aWindowId) override
  {
    auto renderer = aRenderThread.GetRenderer(aWindowId);
    if (renderer) {
      renderer->SetProfilerEnabled(mEnabled);
    }
  }

private:
  bool mEnabled;
};

void
WebRenderAPI::SetProfilerEnabled(bool aEnabled)
{
  auto event = MakeUnique<EnableProfiler>(aEnabled);
  RunOnRenderThread(Move(event));
}

void
WebRenderAPI::RunOnRenderThread(UniquePtr<RendererEvent> aEvent)
{
  auto event = reinterpret_cast<uintptr_t>(aEvent.release());
  wr_api_send_external_event(mWrApi, event);
}

DisplayListBuilder::DisplayListBuilder(PipelineId aId,
                                       const WrSize& aContentSize)
{
  MOZ_COUNT_CTOR(DisplayListBuilder);
  mWrState = wr_state_new(aId, aContentSize);
}

DisplayListBuilder::~DisplayListBuilder()
{
  MOZ_COUNT_DTOR(DisplayListBuilder);
  wr_state_delete(mWrState);
}

void
DisplayListBuilder::Begin(const LayerIntSize& aSize)
{
  wr_dp_begin(mWrState, aSize.width, aSize.height);
}

void
DisplayListBuilder::End()
{
  wr_dp_end(mWrState);
}

void
DisplayListBuilder::Finalize(WrSize& aOutContentSize,
                             BuiltDisplayList& aOutDisplayList)
{
  wr_api_finalize_builder(mWrState,
                          &aOutContentSize,
                          &aOutDisplayList.dl_desc,
                          &aOutDisplayList.dl.inner);
}

void
DisplayListBuilder::PushStackingContext(const WrRect& aBounds,
                                        const uint64_t& aAnimationId,
                                        const float* aOpacity,
                                        const gfx::Matrix4x4* aTransform,
                                        const WrMixBlendMode& aMixBlendMode)
{
  WrMatrix matrix;
  if (aTransform) {
    matrix = ToWrMatrix(*aTransform);
  }
  const WrMatrix* maybeTransform = aTransform ? &matrix : nullptr;
  WRDL_LOG("PushStackingContext b=%s t=%s\n", Stringify(aBounds).c_str(),
      aTransform ? Stringify(*aTransform).c_str() : "none");
  wr_dp_push_stacking_context(mWrState, aBounds, aAnimationId, aOpacity,
                              maybeTransform, aMixBlendMode);
}

void
DisplayListBuilder::PopStackingContext()
{
  WRDL_LOG("PopStackingContext\n");
  wr_dp_pop_stacking_context(mWrState);
}

void
DisplayListBuilder::PushClip(const WrRect& aClipRect,
                             const WrImageMask* aMask)
{
  WRDL_LOG("PushClip r=%s m=%p\n", Stringify(aClipRect).c_str(), aMask);
  wr_dp_push_clip(mWrState, aClipRect, aMask);
}

void
DisplayListBuilder::PopClip()
{
  WRDL_LOG("PopClip\n");
  wr_dp_pop_clip(mWrState);
}

void
DisplayListBuilder::PushBuiltDisplayList(BuiltDisplayList dl)
{
  wr_dp_push_built_display_list(mWrState,
                                dl.dl_desc,
                                dl.dl.Extract());
}

void
DisplayListBuilder::PushScrollLayer(const layers::FrameMetrics::ViewID& aScrollId,
                                    const WrRect& aContentRect,
                                    const WrRect& aClipRect)
{
  WRDL_LOG("PushScrollLayer id=%" PRIu64 " co=%s cl=%s\n",
      aScrollId, Stringify(aContentRect).c_str(), Stringify(aClipRect).c_str());
  wr_dp_push_scroll_layer(mWrState, aScrollId, aContentRect, aClipRect);
}

void
DisplayListBuilder::PopScrollLayer()
{
  WRDL_LOG("PopScrollLayer\n");
  wr_dp_pop_scroll_layer(mWrState);
}

void
DisplayListBuilder::PushRect(const WrRect& aBounds,
                             const WrClipRegionToken aClip,
                             const WrColor& aColor)
{
  WRDL_LOG("PushRect b=%s c=%s\n",
      Stringify(aBounds).c_str(),
      Stringify(aColor).c_str());
  wr_dp_push_rect(mWrState, aBounds, aClip, aColor);
}

void
DisplayListBuilder::PushLinearGradient(const WrRect& aBounds,
                                       const WrClipRegionToken aClip,
                                       const WrPoint& aStartPoint,
                                       const WrPoint& aEndPoint,
                                       const nsTArray<WrGradientStop>& aStops,
                                       wr::GradientExtendMode aExtendMode,
                                       const WrSize aTileSize,
                                       const WrSize aTileSpacing)
{
  wr_dp_push_linear_gradient(mWrState,
                             aBounds, aClip,
                             aStartPoint, aEndPoint,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode,
                             aTileSize, aTileSpacing);
}

void
DisplayListBuilder::PushRadialGradient(const WrRect& aBounds,
                                       const WrClipRegionToken aClip,
                                       const WrPoint& aCenter,
                                       const WrSize& aRadius,
                                       const nsTArray<WrGradientStop>& aStops,
                                       wr::GradientExtendMode aExtendMode,
                                       const WrSize aTileSize,
                                       const WrSize aTileSpacing)
{
  wr_dp_push_radial_gradient(mWrState,
                             aBounds, aClip,
                             aCenter, aRadius,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode,
                             aTileSize, aTileSpacing);
}

void
DisplayListBuilder::PushImage(const WrRect& aBounds,
                              const WrClipRegionToken aClip,
                              wr::ImageRendering aFilter,
                              wr::ImageKey aImage)
{
  WrSize size;
  size.width = aBounds.width;
  size.height = aBounds.height;
  PushImage(aBounds, aClip, size, size, aFilter, aImage);
}

void
DisplayListBuilder::PushImage(const WrRect& aBounds,
                              const WrClipRegionToken aClip,
                              const WrSize& aStretchSize,
                              const WrSize& aTileSpacing,
                              wr::ImageRendering aFilter,
                              wr::ImageKey aImage)
{
  WRDL_LOG("PushImage b=%s s=%s t=%s\n", Stringify(aBounds).c_str(),
      Stringify(aStretchSize).c_str(), Stringify(aTileSpacing).c_str());
  wr_dp_push_image(mWrState, aBounds, aClip, aStretchSize, aTileSpacing, aFilter, aImage);
}

void
DisplayListBuilder::PushYCbCrPlanarImage(const WrRect& aBounds,
                                         const WrClipRegionToken aClip,
                                         wr::ImageKey aImageChannel0,
                                         wr::ImageKey aImageChannel1,
                                         wr::ImageKey aImageChannel2,
                                         WrYuvColorSpace aColorSpace,
                                         wr::ImageRendering aRendering)
{
  wr_dp_push_yuv_planar_image(mWrState,
                              aBounds,
                              aClip,
                              aImageChannel0,
                              aImageChannel1,
                              aImageChannel2,
                              aColorSpace,
                              aRendering);
}

void
DisplayListBuilder::PushNV12Image(const WrRect& aBounds,
                                  const WrClipRegionToken aClip,
                                  wr::ImageKey aImageChannel0,
                                  wr::ImageKey aImageChannel1,
                                  WrYuvColorSpace aColorSpace,
                                  wr::ImageRendering aRendering)
{
  wr_dp_push_yuv_NV12_image(mWrState,
                            aBounds,
                            aClip,
                            aImageChannel0,
                            aImageChannel1,
                            aColorSpace,
                            aRendering);
}

void
DisplayListBuilder::PushYCbCrInterleavedImage(const WrRect& aBounds,
                                              const WrClipRegionToken aClip,
                                              wr::ImageKey aImageChannel0,
                                              WrYuvColorSpace aColorSpace,
                                              wr::ImageRendering aRendering)
{
  wr_dp_push_yuv_interleaved_image(mWrState,
                                   aBounds,
                                   aClip,
                                   aImageChannel0,
                                   aColorSpace,
                                   aRendering);
}

void
DisplayListBuilder::PushIFrame(const WrRect& aBounds,
                               const WrClipRegionToken aClip,
                               PipelineId aPipeline)
{
  wr_dp_push_iframe(mWrState, aBounds, aClip, aPipeline);
}

void
DisplayListBuilder::PushBorder(const WrRect& aBounds,
                               const WrClipRegionToken aClip,
                               const WrBorderWidths& aWidths,
                               const WrBorderSide& aTop,
                               const WrBorderSide& aRight,
                               const WrBorderSide& aBottom,
                               const WrBorderSide& aLeft,
                               const WrBorderRadius& aRadius)
{
  wr_dp_push_border(mWrState, aBounds, aClip,
                    aWidths, aTop, aRight, aBottom, aLeft, aRadius);
}

void
DisplayListBuilder::PushBorderImage(const WrRect& aBounds,
                                    const WrClipRegionToken aClip,
                                    const WrBorderWidths& aWidths,
                                    wr::ImageKey aImage,
                                    const WrNinePatchDescriptor& aPatch,
                                    const WrSideOffsets2Df32& aOutset,
                                    const WrRepeatMode& aRepeatHorizontal,
                                    const WrRepeatMode& aRepeatVertical)
{
  wr_dp_push_border_image(mWrState, aBounds, aClip,
                          aWidths, aImage, aPatch, aOutset,
                          aRepeatHorizontal, aRepeatVertical);
}

void
DisplayListBuilder::PushBorderGradient(const WrRect& aBounds,
                                       const WrClipRegionToken aClip,
                                       const WrBorderWidths& aWidths,
                                       const WrPoint& aStartPoint,
                                       const WrPoint& aEndPoint,
                                       const nsTArray<WrGradientStop>& aStops,
                                       wr::GradientExtendMode aExtendMode,
                                       const WrSideOffsets2Df32& aOutset)
{
  wr_dp_push_border_gradient(mWrState, aBounds, aClip,
                             aWidths, aStartPoint, aEndPoint,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode, aOutset);
}

void
DisplayListBuilder::PushBorderRadialGradient(const WrRect& aBounds,
                                             const WrClipRegionToken aClip,
                                             const WrBorderWidths& aWidths,
                                             const WrPoint& aCenter,
                                             const WrSize& aRadius,
                                             const nsTArray<WrGradientStop>& aStops,
                                             wr::GradientExtendMode aExtendMode,
                                             const WrSideOffsets2Df32& aOutset)
{
  wr_dp_push_border_radial_gradient(
    mWrState, aBounds, aClip, aWidths, aCenter,
    aRadius, aStops.Elements(), aStops.Length(),
    aExtendMode, aOutset);
}

void
DisplayListBuilder::PushText(const WrRect& aBounds,
                             const WrClipRegionToken aClip,
                             const gfx::Color& aColor,
                             wr::FontKey aFontKey,
                             Range<const WrGlyphInstance> aGlyphBuffer,
                             float aGlyphSize)
{
  wr_dp_push_text(mWrState, aBounds, aClip,
                  ToWrColor(aColor),
                  aFontKey,
                  &aGlyphBuffer[0], aGlyphBuffer.length(),
                  aGlyphSize);
}

void
DisplayListBuilder::PushBoxShadow(const WrRect& aRect,
                                  const WrClipRegionToken aClip,
                                  const WrRect& aBoxBounds,
                                  const WrPoint& aOffset,
                                  const WrColor& aColor,
                                  const float& aBlurRadius,
                                  const float& aSpreadRadius,
                                  const float& aBorderRadius,
                                  const WrBoxShadowClipMode& aClipMode)
{
  wr_dp_push_box_shadow(mWrState, aRect, aClip,
                        aBoxBounds, aOffset, aColor,
                        aBlurRadius, aSpreadRadius, aBorderRadius,
                        aClipMode);
}

WrClipRegionToken
DisplayListBuilder::PushClipRegion(const WrRect& aMain,
                                   const WrImageMask* aMask)
{
  WRDL_LOG("PushClipRegion r=%s m=%p\n", Stringify(aMain).c_str(), aMask);
  return wr_dp_push_clip_region(mWrState,
                                aMain,
                                nullptr, 0,
                                aMask);
}

WrClipRegionToken
DisplayListBuilder::PushClipRegion(const WrRect& aMain,
                                   const nsTArray<WrComplexClipRegion>& aComplex,
                                   const WrImageMask* aMask)
{
  WRDL_LOG("PushClipRegion r=%s cl=%d m=%p\n", Stringify(aMain).c_str(),
      (int)aComplex.Length(), aMask);
  return wr_dp_push_clip_region(mWrState,
                                aMain,
                                aComplex.Elements(), aComplex.Length(),
                                aMask);
}

} // namespace wr
} // namespace mozilla
