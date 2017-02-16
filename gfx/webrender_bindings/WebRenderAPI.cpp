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
              RefPtr<widget::CompositorWidget>&& aWidget,
              layers::SynchronousTask* aTask,
              bool aEnableProfiler)
    : mWrApi(aApi)
    , mMaxTextureSize(aMaxTextureSize)
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

    aRenderThread.AddRenderer(aWindowId, Move(renderer));
  }

private:
  WrAPI** mWrApi;
  GLint* mMaxTextureSize;
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

  // Dispatch a synchronous task because the WrApi object needs to be created
  // on the render thread. If need be we could delay waiting on this task until
  // the next time we need to access the WrApi object.
  layers::SynchronousTask task("Create Renderer");
  auto event = MakeUnique<NewRenderer>(&wrApi, aBridge, &maxTextureSize,
                                       Move(aWidget), &task, aEnableProfiler);
  RenderThread::Get()->RunEvent(id, Move(event));

  task.Wait();

  if (!wrApi) {
    return nullptr;
  }

  return RefPtr<WebRenderAPI>(new WebRenderAPI(wrApi, id, maxTextureSize)).forget();
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
WebRenderAPI::SetRootDisplayList(gfx::Color aBgColor,
                                 Epoch aEpoch,
                                 LayerSize aViewportSize,
                                 DisplayListBuilder& aBuilder)
{
  wr_api_set_root_display_list(mWrApi, aBuilder.mWrState,
                               aEpoch,
                               aViewportSize.width, aViewportSize.height);
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
    RunOnRenderThread(Move(event));

    task.Wait();
}

void
WebRenderAPI::SetRootPipeline(PipelineId aPipeline)
{
  wr_api_set_root_pipeline(mWrApi, aPipeline);
}

ImageKey
WebRenderAPI::AddImageBuffer(const ImageDescriptor& aDescritptor,
                             Range<uint8_t> aBytes)
{
  return ImageKey(wr_api_add_image(mWrApi,
                                   &aDescritptor,
                                   &aBytes[0], aBytes.length()));
}

ImageKey
WebRenderAPI::AddExternalImageHandle(gfx::IntSize aSize,
                                     gfx::SurfaceFormat aFormat,
                                     uint64_t aHandle)
{
  auto format = SurfaceFormatToWrImageFormat(aFormat).value();
  return ImageKey(wr_api_add_external_image_texture(mWrApi,
                                                    aSize.width, aSize.height, format,
                                                    aHandle));
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

wr::FontKey
WebRenderAPI::AddRawFont(Range<uint8_t> aBytes)
{
  return wr::FontKey(wr_api_add_raw_font(mWrApi, &aBytes[0], aBytes.length()));
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

DisplayListBuilder::DisplayListBuilder(const LayerIntSize& aSize, PipelineId aId)
{
  MOZ_COUNT_CTOR(DisplayListBuilder);
  mWrState = wr_state_new(aSize.width, aSize.height, aId);
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
DisplayListBuilder::End(WebRenderAPI& aApi, Epoch aEpoch)
{
  wr_dp_end(mWrState, aApi.mWrApi, aEpoch);
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

} // namespace wr
} // namespace mozilla
