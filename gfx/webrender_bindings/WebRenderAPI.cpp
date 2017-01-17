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

inline Maybe<WRImageFormat>
SurfaceFormatToWRImageFormat(gfx::SurfaceFormat aFormat) {
  // TODO: fix the formats (RGB/BGR permutations, etc.)
  switch (aFormat) {
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::A8R8G8B8:
      return Some(WRImageFormat::RGBA8);
    case gfx::SurfaceFormat::B8G8R8X8:
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::X8R8G8B8:
    case gfx::SurfaceFormat::R8G8B8:
    case gfx::SurfaceFormat::B8G8R8:
      return Some(WRImageFormat::RGB8);
    case gfx::SurfaceFormat::A8:
      return Some(WRImageFormat::A8);
    case gfx::SurfaceFormat::UNKNOWN:
      return Some(WRImageFormat::Invalid);
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
  : mWRApi(aApi)
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
    wr_window_new(aWindowId.mHandle, this->mEnableProfiler, mWRApi, &wrRenderer);
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

  WrAPI** mWRApi;
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

#ifdef MOZ_ENABLE_WEBRENDER
  // Need to wrap this in an ifdef otherwise VC++ emits a warning (treated as error)
  // in the non-webrender targets.
  // We should be able to remove this #ifdef if/when we remove the
  // MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE annotations in webrender.h
  wr_api_delete(mWRApi);
#endif

  layers::SynchronousTask task("Destroy WebRenderAPI");
  auto event = MakeUnique<RemoveRenderer>(&task);
  // TODO use the WebRender API instead of scheduling on this message loop directly.
  // this needs PR #688
  RenderThread::Get()->RunEvent(mId, Move(event));
  task.Wait();
}

void
WebRenderAPI::SetRootDisplayList(gfx::Color aBgColor,
                                 Epoch aEpoch,
                                 LayerSize aViewportSize,
                                 DisplayListBuilder& aBuilder)
{
  wr_api_set_root_display_list(mWRApi, aBuilder.mWRState,
                               aEpoch.mHandle,
                               aViewportSize.width, aViewportSize.height);
}

void
WebRenderAPI::SetRootPipeline(PipelineId aPipeline)
{
  wr_api_set_root_pipeline(mWRApi, aPipeline.mHandle);
}

ImageKey
WebRenderAPI::AddImageBuffer(gfx::IntSize aSize,
                             uint32_t aStride,
                             gfx::SurfaceFormat aFormat,
                             Range<uint8_t> aBytes)
{
  auto format = SurfaceFormatToWRImageFormat(aFormat).value();
  return ImageKey(wr_api_add_image(mWRApi,
                                       aSize.width, aSize.height,
                                       aStride, format,
                                       &aBytes[0], aBytes.length()));
}

ImageKey
WebRenderAPI::AddExternalImageHandle(gfx::IntSize aSize,
                                     gfx::SurfaceFormat aFormat,
                                     uint64_t aHandle)
{
  auto format = SurfaceFormatToWRImageFormat(aFormat).value();
  return ImageKey(wr_api_add_external_image_texture(mWRApi,
                                                         aSize.width, aSize.height, format,
                                                         aHandle));
}

void
WebRenderAPI::UpdateImageBuffer(ImageKey aKey,
                                gfx::IntSize aSize,
                                gfx::SurfaceFormat aFormat,
                                Range<uint8_t> aBytes)
{
  auto format = SurfaceFormatToWRImageFormat(aFormat).value();
  wr_api_update_image(mWRApi,
                      aKey.mHandle,
                      aSize.width, aSize.height, format,
                      &aBytes[0], aBytes.length());
}

void
WebRenderAPI::DeleteImage(ImageKey aKey)
{
  wr_api_delete_image(mWRApi, aKey.mHandle);
}

wr::FontKey
WebRenderAPI::AddRawFont(Range<uint8_t> aBytes)
{
  return wr::FontKey(wr_api_add_raw_font(mWRApi, &aBytes[0], aBytes.length()));
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
  RenderThread::Get()->RunEvent(mId, Move(event));
}

DisplayListBuilder::DisplayListBuilder(const LayerIntSize& aSize, PipelineId aId)
{
  MOZ_COUNT_CTOR(DisplayListBuilder);
  mWRState = wr_state_new(aSize.width, aSize.height, aId.mHandle);
}

DisplayListBuilder::~DisplayListBuilder()
{
  MOZ_COUNT_DTOR(DisplayListBuilder);
#ifdef MOZ_ENABLE_WEBRENDER
  wr_state_delete(mWRState);
#endif
}

void
DisplayListBuilder::Begin(const LayerIntSize& aSize)
{
  wr_dp_begin(mWRState, aSize.width, aSize.height);
}

void
DisplayListBuilder::End(WebRenderAPI& aApi, Epoch aEpoch)
{
  wr_dp_end(mWRState, aApi.mWRApi, aEpoch.mHandle);
}

void
DisplayListBuilder::PushStackingContext(const WrRect& aBounds,
                                        const WrRect& aOverflow,
                                        const WRImageMask* aMask,
                                        const gfx::Matrix4x4& aTransform)
{
  wr_dp_push_stacking_context(mWRState, aBounds, aOverflow, aMask,
                              &aTransform.components[0]);
}

void
DisplayListBuilder::PopStackingContext()
{
  wr_dp_pop_stacking_context(mWRState);
}

void
DisplayListBuilder::PushRect(const WrRect& aBounds,
                             const WrRect& aClip,
                             const gfx::Color& aColor)
{
  wr_dp_push_rect(mWRState, aBounds, aClip,
                  aColor.r, aColor.g, aColor.b, aColor.a);
}

void
DisplayListBuilder::PushImage(const WrRect& aBounds,
                              const WrRect& aClip,
                              const WRImageMask* aMask,
                              const WRTextureFilter aFilter,
                              WRImageKey aImage)
{
  wr_dp_push_image(mWRState, aBounds, aClip, aMask, aFilter, aImage);
}

void
DisplayListBuilder::PushIFrame(const WrRect& aBounds,
                               const WrRect& aClip,
                               PipelineId aPipeline)
{
  wr_dp_push_iframe(mWRState, aBounds, aClip, aPipeline.mHandle);
}

void
DisplayListBuilder::PushBorder(const WrRect& aBounds,
                               const WrRect& aClip,
                               const WRBorderSide& aTop,
                               const WRBorderSide& aRight,
                               const WRBorderSide& aBottom,
                               const WRBorderSide& aLeft,
                               const WrLayoutSize& aTopLeftRadius,
                               const WrLayoutSize& aTopRightRadius,
                               const WrLayoutSize& aBottomLeftRadius,
                               const WrLayoutSize& aBottomRightRadius)
{
  wr_dp_push_border(mWRState, aBounds, aClip,
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
  wr_dp_push_text(mWRState, aBounds, aClip,
                  ToWRColor(aColor),
                  aFontKey.mHandle,
                  &aGlyphBuffer[0], aGlyphBuffer.length(),
                  aGlyphSize);
}

} // namespace
} // namespace
