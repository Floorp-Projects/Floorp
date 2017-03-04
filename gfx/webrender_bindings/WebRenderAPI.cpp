/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderAPI.h"
#include "mozilla/webrender/RendererOGL.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/layers/SynchronousTask.h"

namespace mozilla {
namespace wr {

class NewRenderer : public RendererEvent
{
public:
  NewRenderer(WrAPI** aApi, layers::CompositorBridgeParentBase* aBridge,
              GLint* aMaxTextureSize,
              bool* aUseANGLE,
              RefPtr<widget::CompositorWidget>&& aWidget,
              layers::SynchronousTask* aTask,
              bool aEnableProfiler)
    : mWrApi(aApi)
    , mMaxTextureSize(aMaxTextureSize)
    , mUseANGLE(aUseANGLE)
    , mBridge(aBridge)
    , mCompositorWidget(Move(aWidget))
    , mTask(aTask)
    , mEnableProfiler(aEnableProfiler)
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

    RefPtr<gl::GLContext> gl = gl::GLContextProvider::CreateForCompositorWidget(mCompositorWidget, true);
    if (!gl || !gl->MakeCurrent()) {
      return;
    }

    gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, mMaxTextureSize);
    *mUseANGLE = gl->IsANGLE();

    WrRenderer* wrRenderer = nullptr;
    if (!wr_window_new(aWindowId, gl.get(), this->mEnableProfiler, mWrApi, &wrRenderer)) {
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
                     RefPtr<widget::CompositorWidget>&& aWidget)
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
                                       Move(aWidget), &task, aEnableProfiler);
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
WebRenderAPI::GenerateFrame()
{
  wr_api_generate_frame(mWrApi);
}

void
WebRenderAPI::SetRootDisplayList(gfx::Color aBgColor,
                                 Epoch aEpoch,
                                 LayerSize aViewportSize,
				 WrPipelineId pipeline_id,
				 WrBuiltDisplayListDescriptor dl_descriptor,
				 uint8_t *dl_data,
				 size_t dl_size,
				 WrAuxiliaryListsDescriptor aux_descriptor,
				 uint8_t *aux_data,
				 size_t aux_size)
{
    wr_api_set_root_display_list(mWrApi,
				 aEpoch,
				 aViewportSize.width, aViewportSize.height,
                                 pipeline_id,
                                 dl_descriptor,
                                 dl_data,
                                 dl_size,
                                 aux_descriptor,
                                 aux_data,
                                 aux_size);
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
                wr_renderer_readback(mSize.width, mSize.height, mBuffer, mBufferSize);
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
WebRenderAPI::SetRootPipeline(PipelineId aPipeline)
{
  wr_api_set_root_pipeline(mWrApi, aPipeline);
}

void
WebRenderAPI::AddImage(ImageKey key, const ImageDescriptor& aDescritptor,
                       Range<uint8_t> aBytes)
{
  wr_api_add_image(mWrApi,
                   key,
                   &aDescritptor,
                   &aBytes[0], aBytes.length());
}

void
WebRenderAPI::AddExternalImageHandle(ImageKey key,
                                     gfx::IntSize aSize,
                                     gfx::SurfaceFormat aFormat,
                                     uint64_t aHandle)
{
  auto format = SurfaceFormatToWrImageFormat(aFormat).value();
  wr_api_add_external_image_handle(mWrApi,
                                   key,
                                   aSize.width, aSize.height, format,
                                   aHandle);
}

void
WebRenderAPI::AddExternalImageBuffer(ImageKey key,
                                     gfx::IntSize aSize,
                                     gfx::SurfaceFormat aFormat,
                                     uint64_t aHandle)
{
  auto format = SurfaceFormatToWrImageFormat(aFormat).value();
  wr_api_add_external_image_buffer(mWrApi,
                                   key,
                                   aSize.width, aSize.height, format,
                                   aHandle);
}

void
WebRenderAPI::UpdateImageBuffer(ImageKey aKey,
                                const ImageDescriptor& aDescritptor,
                                Range<uint8_t> aBytes)
{
  wr_api_update_image(mWrApi,
                      aKey,
                      &aDescritptor,
                      &aBytes[0], aBytes.length());
}

void
WebRenderAPI::DeleteImage(ImageKey aKey)
{
  wr_api_delete_image(mWrApi, aKey);
}

void
WebRenderAPI::AddRawFont(wr::FontKey key, Range<uint8_t> aBytes)
{
  wr_api_add_raw_font(mWrApi, key, &aBytes[0], aBytes.length());
}

void
WebRenderAPI::DeleteFont(wr::FontKey aKey)
{
  printf("XXX - WebRender does not seem to implement deleting a font! Leaking it...\n");
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

DisplayListBuilder::DisplayListBuilder(PipelineId aId)
{
  MOZ_COUNT_CTOR(DisplayListBuilder);
  mWrState = wr_state_new(aId);
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
DisplayListBuilder::Finalize(WrBuiltDisplayListDescriptor& dl_descriptor,
                             wr::VecU8& dl_data,
                             WrAuxiliaryListsDescriptor& aux_descriptor,
                             wr::VecU8& aux_data)
{
    wr_api_finalize_builder(mWrState,
                            dl_descriptor,
                            dl_data.inner,
                            aux_descriptor,
                            aux_data.inner);
}

void
DisplayListBuilder::PushStackingContext(const WrRect& aBounds,
                                        const WrRect& aOverflow,
                                        const WrImageMask* aMask,
                                        const float aOpacity,
                                        const gfx::Matrix4x4& aTransform,
                                        const WrMixBlendMode& aMixBlendMode)
{
  wr_dp_push_stacking_context(mWrState, aBounds, aOverflow, aMask, aOpacity,
                              &aTransform.components[0], aMixBlendMode);
}

void
DisplayListBuilder::PopStackingContext()
{
  wr_dp_pop_stacking_context(mWrState);
}

void
DisplayListBuilder::PushScrollLayer(const WrRect& aBounds,
                                    const WrRect& aOverflow,
                                    const WrImageMask* aMask)
{
  wr_dp_push_scroll_layer(mWrState, aBounds, aOverflow, aMask);
}

void
DisplayListBuilder::PopScrollLayer()
{
  wr_dp_pop_scroll_layer(mWrState);
}

void
DisplayListBuilder::PushRect(const WrRect& aBounds,
                             const WrRect& aClip,
                             const WrColor& aColor)
{
  wr_dp_push_rect(mWrState, aBounds, aClip, aColor);
}

void
DisplayListBuilder::PushLinearGradient(const WrRect& aBounds,
                                       const WrRect& aClip,
                                       const WrPoint& aStartPoint,
                                       const WrPoint& aEndPoint,
                                       const nsTArray<WrGradientStop>& aStops,
                                       wr::GradientExtendMode aExtendMode)
{
  wr_dp_push_linear_gradient(mWrState,
                             aBounds, aClip,
                             aStartPoint, aEndPoint,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode);
}

void
DisplayListBuilder::PushRadialGradient(const WrRect& aBounds,
                                       const WrRect& aClip,
                                       const WrPoint& aStartCenter,
                                       const WrPoint& aEndCenter,
                                       float aStartRadius,
                                       float aEndRadius,
                                       const nsTArray<WrGradientStop>& aStops,
                                       wr::GradientExtendMode aExtendMode)
{
  wr_dp_push_radial_gradient(mWrState,
                             aBounds, aClip,
                             aStartCenter, aEndCenter,
                             aStartRadius, aEndRadius,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode);
}

void
DisplayListBuilder::PushImage(const WrRect& aBounds,
                              const WrRect& aClip,
                              const WrImageMask* aMask,
                              wr::ImageRendering aFilter,
                              wr::ImageKey aImage)
{
  wr_dp_push_image(mWrState, aBounds, aClip, aMask, aFilter, aImage);
}

void
DisplayListBuilder::PushIFrame(const WrRect& aBounds,
                               const WrRect& aClip,
                               PipelineId aPipeline)
{
  wr_dp_push_iframe(mWrState, aBounds, aClip, aPipeline);
}

void
DisplayListBuilder::PushBorder(const WrRect& aBounds,
                               const WrRect& aClip,
                               const WrBorderSide& aTop,
                               const WrBorderSide& aRight,
                               const WrBorderSide& aBottom,
                               const WrBorderSide& aLeft,
                               const WrBorderRadius& aRadius)
{
  wr_dp_push_border(mWrState, aBounds, aClip,
                    aTop, aRight, aBottom, aLeft,
                    aRadius);
}

void
DisplayListBuilder::PushText(const WrRect& aBounds,
                             const WrRect& aClip,
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
                                  const WrRect& aClip,
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

} // namespace wr
} // namespace mozilla
