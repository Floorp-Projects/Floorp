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

inline Maybe<WrImageFormat>
SurfaceFormatToWrImageFormat(gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case gfx::SurfaceFormat::B8G8R8X8:
      // TODO: WebRender will have a BGRA + opaque flag for this but does not
      // have it yet (cf. issue #732).
    case gfx::SurfaceFormat::B8G8R8A8:
      return Some(WrImageFormat::RGBA8);
    case gfx::SurfaceFormat::B8G8R8:
      return Some(WrImageFormat::RGB8);
    case gfx::SurfaceFormat::A8:
      return Some(WrImageFormat::A8);
    case gfx::SurfaceFormat::UNKNOWN:
      return Some(WrImageFormat::Invalid);
    default:
      return Nothing();
  }
}

class NewRenderer : public RendererEvent
{
public:
  NewRenderer(WrAPI** aApi, layers::CompositorBridgeParentBase* aBridge,
              RefPtr<widget::CompositorWidget>&& aWidget,
              layers::SynchronousTask* aTask,
              bool aEnableProfiler)
  : mWrApi(aApi)
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

    wr_gl_init(&*gl);

    WrRenderer* wrRenderer = nullptr;
    wr_window_new(aWindowId.mHandle, this->mEnableProfiler, mWrApi, &wrRenderer);
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

  WrAPI** mWrApi;
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
  WindowId id(sNextId++);

  WrAPI* wrApi = nullptr;

  // Dispatch a synchronous task because the WrApi object needs to be created
  // on the render thread. If need be we could delay waiting on this task until
  // the next time we need to access the WrApi object.
  layers::SynchronousTask task("Create Renderer");
  auto event = MakeUnique<NewRenderer>(&wrApi, aBridge, Move(aWidget), &task, aEnableProfiler);
  RenderThread::Get()->RunEvent(id, Move(event));

  task.Wait();

  if (!wrApi) {
    return nullptr;
  }

  return RefPtr<WebRenderAPI>(new WebRenderAPI(wrApi, id)).forget();
}

WebRenderAPI::~WebRenderAPI()
{
  layers::SynchronousTask task("Destroy WebRenderAPI");
  auto event = MakeUnique<RemoveRenderer>(&task);
  RunOnRenderThread(Move(event));
  task.Wait();

#ifdef MOZ_ENABLE_WEBRENDER
  // Need to wrap this in an ifdef otherwise VC++ emits a warning (treated as error)
  // in the non-webrender targets.
  // We should be able to remove this #ifdef if/when we remove the
  // MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE annotations in webrender.h
  wr_api_delete(mWrApi);
#endif
}

void
WebRenderAPI::SetRootDisplayList(gfx::Color aBgColor,
                                 Epoch aEpoch,
                                 LayerSize aViewportSize,
                                 DisplayListBuilder& aBuilder)
{
  wr_api_set_root_display_list(mWrApi, aBuilder.mWrState,
                               aEpoch.mHandle,
                               aViewportSize.width, aViewportSize.height);
}

void
WebRenderAPI::SetRootPipeline(PipelineId aPipeline)
{
  wr_api_set_root_pipeline(mWrApi, aPipeline.mHandle);
}

ImageKey
WebRenderAPI::AddImageBuffer(gfx::IntSize aSize,
                             uint32_t aStride,
                             gfx::SurfaceFormat aFormat,
                             Range<uint8_t> aBytes)
{
  auto format = SurfaceFormatToWrImageFormat(aFormat).value();
  return ImageKey(wr_api_add_image(mWrApi,
                                   aSize.width, aSize.height,
                                   aStride, format,
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
                                gfx::IntSize aSize,
                                gfx::SurfaceFormat aFormat,
                                Range<uint8_t> aBytes)
{
  auto format = SurfaceFormatToWrImageFormat(aFormat).value();
  wr_api_update_image(mWrApi,
                      aKey.mHandle,
                      aSize.width, aSize.height, format,
                      &aBytes[0], aBytes.length());
}

void
WebRenderAPI::DeleteImage(ImageKey aKey)
{
  wr_api_delete_image(mWrApi, aKey.mHandle);
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
  explicit EnableProfiler(bool aEnabled) : mEnabled(aEnabled) { MOZ_COUNT_CTOR(EnableProfiler); }
  ~EnableProfiler()  { MOZ_COUNT_DTOR(EnableProfiler); }

  virtual void Run(RenderThread& aRenderThread, WindowId aWindowId) override
  {
    auto renderer = aRenderThread.GetRenderer(aWindowId);
    if (renderer) {
      renderer->SetProfilerEnabled(mEnabled);
    }
  }

  bool mEnabled;
};

void
WebRenderAPI::SetProfilerEnabled(bool aEnabled)
{
  auto event = MakeUnique<EnableProfiler>(aEnabled);
  RunOnRenderThread(Move(event));
}

void
WebRenderAPI::RunOnRenderThread(UniquePtr<RendererEvent>&& aEvent)
{
  auto event = reinterpret_cast<uintptr_t>(aEvent.release());
  wr_api_send_external_event(mWrApi, event);
}

DisplayListBuilder::DisplayListBuilder(const LayerIntSize& aSize, PipelineId aId)
{
  MOZ_COUNT_CTOR(DisplayListBuilder);
  mWrState = wr_state_new(aSize.width, aSize.height, aId.mHandle);
}

DisplayListBuilder::~DisplayListBuilder()
{
  MOZ_COUNT_DTOR(DisplayListBuilder);
#ifdef MOZ_ENABLE_WEBRENDER
  wr_state_delete(mWrState);
#endif
}

void
DisplayListBuilder::Begin(const LayerIntSize& aSize)
{
  wr_dp_begin(mWrState, aSize.width, aSize.height);
}

void
DisplayListBuilder::End(WebRenderAPI& aApi, Epoch aEpoch)
{
  wr_dp_end(mWrState, aApi.mWrApi, aEpoch.mHandle);
}

void
DisplayListBuilder::PushStackingContext(const WrRect& aBounds,
                                        const WrRect& aOverflow,
                                        const WrImageMask* aMask,
                                        const gfx::Matrix4x4& aTransform)
{
  wr_dp_push_stacking_context(mWrState, aBounds, aOverflow, aMask,
                              &aTransform.components[0]);
}

void
DisplayListBuilder::PopStackingContext()
{
  wr_dp_pop_stacking_context(mWrState);
}

void
DisplayListBuilder::PushRect(const WrRect& aBounds,
                             const WrRect& aClip,
                             const gfx::Color& aColor)
{
  wr_dp_push_rect(mWrState, aBounds, aClip,
                  aColor.r, aColor.g, aColor.b, aColor.a);
}

void
DisplayListBuilder::PushImage(const WrRect& aBounds,
                              const WrRect& aClip,
                              const WrImageMask* aMask,
                              const WrTextureFilter aFilter,
                              wr::ImageKey aImage)
{
  wr_dp_push_image(mWrState, aBounds, aClip, aMask, aFilter, aImage.mHandle);
}

void
DisplayListBuilder::PushIFrame(const WrRect& aBounds,
                               const WrRect& aClip,
                               PipelineId aPipeline)
{
  wr_dp_push_iframe(mWrState, aBounds, aClip, aPipeline.mHandle);
}

void
DisplayListBuilder::PushBorder(const WrRect& aBounds,
                               const WrRect& aClip,
                               const WrBorderSide& aTop,
                               const WrBorderSide& aRight,
                               const WrBorderSide& aBottom,
                               const WrBorderSide& aLeft,
                               const WrLayoutSize& aTopLeftRadius,
                               const WrLayoutSize& aTopRightRadius,
                               const WrLayoutSize& aBottomLeftRadius,
                               const WrLayoutSize& aBottomRightRadius)
{
  wr_dp_push_border(mWrState, aBounds, aClip,
                    aTop, aRight, aBottom, aLeft,
                    aTopLeftRadius, aTopRightRadius,
                    aBottomLeftRadius, aBottomRightRadius);
}

void
DisplayListBuilder::PushText(const WrRect& aBounds,
                             const WrRect& aClip,
                             const gfx::Color& aColor,
                             wr::FontKey aFontKey,
                             Range<const WRGlyphInstance> aGlyphBuffer,
                             float aGlyphSize)
{
  wr_dp_push_text(mWrState, aBounds, aClip,
                  ToWrColor(aColor),
                  aFontKey.mHandle,
                  &aGlyphBuffer[0], aGlyphBuffer.length(),
                  aGlyphSize);
}

} // namespace
} // namespace
