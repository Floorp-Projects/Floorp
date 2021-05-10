/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderAPI.h"

#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/ipc/ByteBuf.h"
#include "mozilla/webrender/RendererOGL.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/ToString.h"
#include "mozilla/webrender/RenderCompositor.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/layers/SynchronousTask.h"
#include "TextDrawTarget.h"
#include "malloc_decls.h"
#include "GLContext.h"

// clang-format off
#define WRDL_LOG(...)
//#define WRDL_LOG(...) printf_stderr("WRDL(%p): " __VA_ARGS__)
//#define WRDL_LOG(...) if (XRE_IsContentProcess()) printf_stderr("WRDL(%p): " __VA_ARGS__)
// clang-format on

namespace mozilla {
using namespace layers;

namespace wr {

MOZ_DEFINE_MALLOC_SIZE_OF(WebRenderMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(WebRenderMallocEnclosingSizeOf)

enum SideBitsPacked {
  eSideBitsPackedTop = 0x1000,
  eSideBitsPackedRight = 0x2000,
  eSideBitsPackedBottom = 0x4000,
  eSideBitsPackedLeft = 0x8000
};

static uint16_t SideBitsToHitInfoBits(SideBits aSideBits) {
  uint16_t ret = 0;
  if (aSideBits & SideBits::eTop) {
    ret |= eSideBitsPackedTop;
  }
  if (aSideBits & SideBits::eRight) {
    ret |= eSideBitsPackedRight;
  }
  if (aSideBits & SideBits::eBottom) {
    ret |= eSideBitsPackedBottom;
  }
  if (aSideBits & SideBits::eLeft) {
    ret |= eSideBitsPackedLeft;
  }
  return ret;
}

class NewRenderer : public RendererEvent {
 public:
  NewRenderer(wr::DocumentHandle** aDocHandle,
              layers::CompositorBridgeParent* aBridge,
              WebRenderBackend* aBackend, WebRenderCompositor* aCompositor,
              int32_t* aMaxTextureSize, bool* aUseANGLE, bool* aUseDComp,
              bool* aUseTripleBuffering, bool* aSupportsExternalBufferTextures,
              RefPtr<widget::CompositorWidget>&& aWidget,
              layers::SynchronousTask* aTask, LayoutDeviceIntSize aSize,
              layers::WindowKind aWindowKind, layers::SyncHandle* aHandle,
              nsACString* aError)
      : mDocHandle(aDocHandle),
        mBackend(aBackend),
        mCompositor(aCompositor),
        mMaxTextureSize(aMaxTextureSize),
        mUseANGLE(aUseANGLE),
        mUseDComp(aUseDComp),
        mUseTripleBuffering(aUseTripleBuffering),
        mSupportsExternalBufferTextures(aSupportsExternalBufferTextures),
        mBridge(aBridge),
        mCompositorWidget(std::move(aWidget)),
        mTask(aTask),
        mSize(aSize),
        mWindowKind(aWindowKind),
        mSyncHandle(aHandle),
        mError(aError) {
    MOZ_COUNT_CTOR(NewRenderer);
  }

  MOZ_COUNTED_DTOR(NewRenderer)

  void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
    layers::AutoCompleteTask complete(mTask);

    UniquePtr<RenderCompositor> compositor =
        RenderCompositor::Create(std::move(mCompositorWidget), *mError);
    if (!compositor) {
      if (!mError->IsEmpty()) {
        gfxCriticalNote << mError->BeginReading();
      }
      return;
    }

    *mBackend = compositor->BackendType();
    *mCompositor = compositor->CompositorType();
    *mUseANGLE = compositor->UseANGLE();
    *mUseDComp = compositor->UseDComp();
    *mUseTripleBuffering = compositor->UseTripleBuffering();
    *mSupportsExternalBufferTextures =
        compositor->SupportsExternalBufferTextures();

    // Only allow the panic on GL error functionality in nightly builds,
    // since it (deliberately) crashes the GPU process if any GL call
    // returns an error code.
    bool panic_on_gl_error = false;
#ifdef NIGHTLY_BUILD
    panic_on_gl_error =
        StaticPrefs::gfx_webrender_panic_on_gl_error_AtStartup();
#endif

    bool isMainWindow = true;  // TODO!
    bool supportLowPriorityTransactions = isMainWindow;
    bool supportLowPriorityThreadpool =
        supportLowPriorityTransactions &&
        StaticPrefs::gfx_webrender_enable_low_priority_pool();
    wr::Renderer* wrRenderer = nullptr;
    char* errorMessage = nullptr;
    int picTileWidth = StaticPrefs::gfx_webrender_picture_tile_width();
    int picTileHeight = StaticPrefs::gfx_webrender_picture_tile_height();
    auto* swgl = compositor->swgl();
    auto* gl = (compositor->gl() && !swgl) ? compositor->gl() : nullptr;
    auto* progCache = (aRenderThread.GetProgramCache() && !swgl)
                          ? aRenderThread.GetProgramCache()->Raw()
                          : nullptr;
    auto* shaders = (aRenderThread.GetShaders() && !swgl)
                        ? aRenderThread.GetShaders()->RawShaders()
                        : nullptr;

    // Check That if we are not using SWGL, we have at least a GL or GLES 3.0
    // context.
    if (gl && !swgl) {
      bool versionCheck =
          gl->IsAtLeast(gl::ContextProfile::OpenGLCore, 300) ||
          gl->IsAtLeast(gl::ContextProfile::OpenGLCompatibility, 300) ||
          gl->IsAtLeast(gl::ContextProfile::OpenGLES, 300);

      if (!versionCheck) {
        gfxCriticalNote << "GL context version (" << gl->Version()
                        << ") insufficent for hardware WebRender";

        mError->AssignASCII("GL context version insufficient");
        return;
      }
    }

    if (!wr_window_new(
            aWindowId, mSize.width, mSize.height,
            mWindowKind == WindowKind::MAIN, supportLowPriorityTransactions,
            supportLowPriorityThreadpool, gfx::gfxVars::UseGLSwizzle(),
            gfx::gfxVars::UseWebRenderScissoredCacheClears(), swgl, gl,
            compositor->SurfaceOriginIsTopLeft(), progCache, shaders,
            aRenderThread.ThreadPool().Raw(),
            aRenderThread.ThreadPoolLP().Raw(), &WebRenderMallocSizeOf,
            &WebRenderMallocEnclosingSizeOf, 0, compositor.get(),
            compositor->ShouldUseNativeCompositor(),
            compositor->GetMaxUpdateRects(), compositor->UsePartialPresent(),
            compositor->GetMaxPartialPresentRects(),
            compositor->ShouldDrawPreviousPartialPresentRegions(), mDocHandle,
            &wrRenderer, mMaxTextureSize, &errorMessage,
            StaticPrefs::gfx_webrender_enable_gpu_markers_AtStartup(),
            panic_on_gl_error, picTileWidth, picTileHeight)) {
      // wr_window_new puts a message into gfxCriticalNote if it returns false
      MOZ_ASSERT(errorMessage);
      mError->AssignASCII(errorMessage);
      wr_api_free_error_msg(errorMessage);
      return;
    }
    MOZ_ASSERT(wrRenderer);

    RefPtr<RenderThread> thread = &aRenderThread;
    auto renderer =
        MakeUnique<RendererOGL>(std::move(thread), std::move(compositor),
                                aWindowId, wrRenderer, mBridge);
    if (wrRenderer && renderer) {
      wr::WrExternalImageHandler handler = renderer->GetExternalImageHandler();
      wr_renderer_set_external_image_handler(wrRenderer, &handler);
    }

    if (renderer) {
      layers::SyncObjectHost* syncObj = renderer->GetSyncObject();
      if (syncObj) {
        *mSyncHandle = syncObj->GetSyncHandle();
      }
    }

    aRenderThread.AddRenderer(aWindowId, std::move(renderer));
  }

 private:
  wr::DocumentHandle** mDocHandle;
  WebRenderBackend* mBackend;
  WebRenderCompositor* mCompositor;
  int32_t* mMaxTextureSize;
  bool* mUseANGLE;
  bool* mUseDComp;
  bool* mUseTripleBuffering;
  bool* mSupportsExternalBufferTextures;
  layers::CompositorBridgeParent* mBridge;
  RefPtr<widget::CompositorWidget> mCompositorWidget;
  layers::SynchronousTask* mTask;
  LayoutDeviceIntSize mSize;
  layers::WindowKind mWindowKind;
  layers::SyncHandle* mSyncHandle;
  nsACString* mError;
};

class RemoveRenderer : public RendererEvent {
 public:
  explicit RemoveRenderer(layers::SynchronousTask* aTask) : mTask(aTask) {
    MOZ_COUNT_CTOR(RemoveRenderer);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(RemoveRenderer)

  void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
    aRenderThread.RemoveRenderer(aWindowId);
    layers::AutoCompleteTask complete(mTask);
  }

 private:
  layers::SynchronousTask* mTask;
};

TransactionBuilder::TransactionBuilder(WebRenderAPI* aApi,
                                       bool aUseSceneBuilderThread)
    : mUseSceneBuilderThread(aUseSceneBuilderThread),
      mApiBackend(aApi->GetBackendType()) {
  mTxn = wr_transaction_new(mUseSceneBuilderThread);
}

TransactionBuilder::~TransactionBuilder() { wr_transaction_delete(mTxn); }

void TransactionBuilder::SetLowPriority(bool aIsLowPriority) {
  wr_transaction_set_low_priority(mTxn, aIsLowPriority);
}

void TransactionBuilder::UpdateEpoch(PipelineId aPipelineId, Epoch aEpoch) {
  wr_transaction_update_epoch(mTxn, aPipelineId, aEpoch);
}

void TransactionBuilder::SetRootPipeline(PipelineId aPipelineId) {
  wr_transaction_set_root_pipeline(mTxn, aPipelineId);
}

void TransactionBuilder::RemovePipeline(PipelineId aPipelineId) {
  wr_transaction_remove_pipeline(mTxn, aPipelineId);
}

void TransactionBuilder::SetDisplayList(
    const gfx::DeviceColor& aBgColor, Epoch aEpoch,
    const wr::LayoutSize& aViewportSize, wr::WrPipelineId pipeline_id,
    wr::BuiltDisplayListDescriptor dl_descriptor, wr::Vec<uint8_t>& dl_data) {
  wr_transaction_set_display_list(mTxn, aEpoch, ToColorF(aBgColor),
                                  aViewportSize, pipeline_id, dl_descriptor,
                                  &dl_data.inner);
}

void TransactionBuilder::ClearDisplayList(Epoch aEpoch,
                                          wr::WrPipelineId aPipelineId) {
  wr_transaction_clear_display_list(mTxn, aEpoch, aPipelineId);
}

void TransactionBuilder::GenerateFrame(const VsyncId& aVsyncId) {
  wr_transaction_generate_frame(mTxn, aVsyncId.mId);
}

void TransactionBuilder::InvalidateRenderedFrame() {
  wr_transaction_invalidate_rendered_frame(mTxn);
}

void TransactionBuilder::UpdateDynamicProperties(
    const nsTArray<wr::WrOpacityProperty>& aOpacityArray,
    const nsTArray<wr::WrTransformProperty>& aTransformArray,
    const nsTArray<wr::WrColorProperty>& aColorArray) {
  wr_transaction_update_dynamic_properties(
      mTxn, aOpacityArray.IsEmpty() ? nullptr : aOpacityArray.Elements(),
      aOpacityArray.Length(),
      aTransformArray.IsEmpty() ? nullptr : aTransformArray.Elements(),
      aTransformArray.Length(),
      aColorArray.IsEmpty() ? nullptr : aColorArray.Elements(),
      aColorArray.Length());
}

bool TransactionBuilder::IsEmpty() const {
  return wr_transaction_is_empty(mTxn);
}

bool TransactionBuilder::IsResourceUpdatesEmpty() const {
  return wr_transaction_resource_updates_is_empty(mTxn);
}

bool TransactionBuilder::IsRenderedFrameInvalidated() const {
  return wr_transaction_is_rendered_frame_invalidated(mTxn);
}

void TransactionBuilder::SetDocumentView(
    const LayoutDeviceIntRect& aDocumentRect) {
  wr::DeviceIntRect wrDocRect;
  wrDocRect.origin.x = aDocumentRect.x;
  wrDocRect.origin.y = aDocumentRect.y;
  wrDocRect.size.width = aDocumentRect.width;
  wrDocRect.size.height = aDocumentRect.height;
  wr_transaction_set_document_view(mTxn, &wrDocRect);
}

void TransactionBuilder::UpdateScrollPosition(
    const wr::WrPipelineId& aPipelineId,
    const layers::ScrollableLayerGuid::ViewID& aScrollId,
    const wr::LayoutPoint& aScrollPosition) {
  wr_transaction_scroll_layer(mTxn, aPipelineId, aScrollId, aScrollPosition);
}

TransactionWrapper::TransactionWrapper(Transaction* aTxn) : mTxn(aTxn) {}

void TransactionWrapper::UpdateDynamicProperties(
    const nsTArray<wr::WrOpacityProperty>& aOpacityArray,
    const nsTArray<wr::WrTransformProperty>& aTransformArray,
    const nsTArray<wr::WrColorProperty>& aColorArray) {
  wr_transaction_update_dynamic_properties(
      mTxn, aOpacityArray.IsEmpty() ? nullptr : aOpacityArray.Elements(),
      aOpacityArray.Length(),
      aTransformArray.IsEmpty() ? nullptr : aTransformArray.Elements(),
      aTransformArray.Length(),
      aColorArray.IsEmpty() ? nullptr : aColorArray.Elements(),
      aColorArray.Length());
}

void TransactionWrapper::AppendTransformProperties(
    const nsTArray<wr::WrTransformProperty>& aTransformArray) {
  wr_transaction_append_transform_properties(
      mTxn, aTransformArray.IsEmpty() ? nullptr : aTransformArray.Elements(),
      aTransformArray.Length());
}

void TransactionWrapper::UpdateScrollPosition(
    const wr::WrPipelineId& aPipelineId,
    const layers::ScrollableLayerGuid::ViewID& aScrollId,
    const wr::LayoutPoint& aScrollPosition) {
  wr_transaction_scroll_layer(mTxn, aPipelineId, aScrollId, aScrollPosition);
}

void TransactionWrapper::UpdatePinchZoom(float aZoom) {
  wr_transaction_pinch_zoom(mTxn, aZoom);
}

void TransactionWrapper::UpdateIsTransformAsyncZooming(uint64_t aAnimationId,
                                                       bool aIsZooming) {
  wr_transaction_set_is_transform_async_zooming(mTxn, aAnimationId, aIsZooming);
}

/*static*/
already_AddRefed<WebRenderAPI> WebRenderAPI::Create(
    layers::CompositorBridgeParent* aBridge,
    RefPtr<widget::CompositorWidget>&& aWidget, const wr::WrWindowId& aWindowId,
    LayoutDeviceIntSize aSize, layers::WindowKind aWindowKind,
    nsACString& aError) {
  MOZ_ASSERT(aBridge);
  MOZ_ASSERT(aWidget);
  static_assert(
      sizeof(size_t) == sizeof(uintptr_t),
      "The FFI bindings assume size_t is the same size as uintptr_t!");

  wr::DocumentHandle* docHandle = nullptr;
  WebRenderBackend backend = WebRenderBackend::HARDWARE;
  WebRenderCompositor compositor = WebRenderCompositor::DRAW;
  int32_t maxTextureSize = 0;
  bool useANGLE = false;
  bool useDComp = false;
  bool useTripleBuffering = false;
  bool supportsExternalBufferTextures = false;
  layers::SyncHandle syncHandle = 0;

  // Dispatch a synchronous task because the DocumentHandle object needs to be
  // created on the render thread. If need be we could delay waiting on this
  // task until the next time we need to access the DocumentHandle object.
  layers::SynchronousTask task("Create Renderer");
  auto event = MakeUnique<NewRenderer>(
      &docHandle, aBridge, &backend, &compositor, &maxTextureSize, &useANGLE,
      &useDComp, &useTripleBuffering, &supportsExternalBufferTextures,
      std::move(aWidget), &task, aSize, aWindowKind, &syncHandle, &aError);
  RenderThread::Get()->RunEvent(aWindowId, std::move(event));

  task.Wait();

  if (!docHandle) {
    return nullptr;
  }

  return RefPtr<WebRenderAPI>(
             new WebRenderAPI(docHandle, aWindowId, backend, compositor,
                              maxTextureSize, useANGLE, useDComp,
                              useTripleBuffering,
                              supportsExternalBufferTextures, syncHandle))
      .forget();
}

already_AddRefed<WebRenderAPI> WebRenderAPI::Clone() {
  wr::DocumentHandle* docHandle = nullptr;
  wr_api_clone(mDocHandle, &docHandle);

  RefPtr<WebRenderAPI> renderApi =
      new WebRenderAPI(docHandle, mId, mBackend, mCompositor, mMaxTextureSize,
                       mUseANGLE, mUseDComp, mUseTripleBuffering,
                       mSupportsExternalBufferTextures, mSyncHandle);
  renderApi->mRootApi = this;  // Hold root api
  renderApi->mRootDocumentApi = this;

  return renderApi.forget();
}

wr::WrIdNamespace WebRenderAPI::GetNamespace() {
  return wr_api_get_namespace(mDocHandle);
}

WebRenderAPI::WebRenderAPI(wr::DocumentHandle* aHandle, wr::WindowId aId,
                           WebRenderBackend aBackend,
                           WebRenderCompositor aCompositor,
                           uint32_t aMaxTextureSize, bool aUseANGLE,
                           bool aUseDComp, bool aUseTripleBuffering,
                           bool aSupportsExternalBufferTextures,
                           layers::SyncHandle aSyncHandle)
    : mDocHandle(aHandle),
      mId(aId),
      mBackend(aBackend),
      mCompositor(aCompositor),
      mMaxTextureSize(aMaxTextureSize),
      mUseANGLE(aUseANGLE),
      mUseDComp(aUseDComp),
      mUseTripleBuffering(aUseTripleBuffering),
      mSupportsExternalBufferTextures(aSupportsExternalBufferTextures),
      mCaptureSequence(false),
      mSyncHandle(aSyncHandle),
      mRendererDestroyed(false) {}

WebRenderAPI::~WebRenderAPI() {
  if (!mRootDocumentApi) {
    wr_api_delete_document(mDocHandle);
  }

  if (!mRootApi) {
    MOZ_RELEASE_ASSERT(mRendererDestroyed);
    wr_api_shut_down(mDocHandle);
  }

  wr_api_delete(mDocHandle);
}

void WebRenderAPI::DestroyRenderer() {
  MOZ_RELEASE_ASSERT(!mRootApi);

  RenderThread::Get()->SetDestroyed(GetId());
  // Call wr_api_stop_render_backend() before RemoveRenderer.
  wr_api_stop_render_backend(mDocHandle);

  layers::SynchronousTask task("Destroy WebRenderAPI");
  auto event = MakeUnique<RemoveRenderer>(&task);
  RunOnRenderThread(std::move(event));
  task.Wait();

  mRendererDestroyed = true;
}

void WebRenderAPI::UpdateDebugFlags(uint32_t aFlags) {
  wr_api_set_debug_flags(mDocHandle, wr::DebugFlags{aFlags});
}

void WebRenderAPI::SendTransaction(TransactionBuilder& aTxn) {
  wr_api_send_transaction(mDocHandle, aTxn.Raw(), aTxn.UseSceneBuilderThread());
}

SideBits ExtractSideBitsFromHitInfoBits(uint16_t& aHitInfoBits) {
  SideBits sideBits = SideBits::eNone;
  if (aHitInfoBits & eSideBitsPackedTop) {
    sideBits |= SideBits::eTop;
  }
  if (aHitInfoBits & eSideBitsPackedRight) {
    sideBits |= SideBits::eRight;
  }
  if (aHitInfoBits & eSideBitsPackedBottom) {
    sideBits |= SideBits::eBottom;
  }
  if (aHitInfoBits & eSideBitsPackedLeft) {
    sideBits |= SideBits::eLeft;
  }

  aHitInfoBits &= 0x0fff;
  return sideBits;
}

std::vector<WrHitResult> WebRenderAPI::HitTest(const wr::WorldPoint& aPoint) {
  static_assert(gfx::DoesCompositorHitTestInfoFitIntoBits<12>(),
                "CompositorHitTestFlags MAX value has to be less than number "
                "of bits in uint16_t minus 4 for SideBitsPacked");

  nsTArray<wr::HitResult> wrResults;
  wr_api_hit_test(mDocHandle, aPoint, &wrResults);

  std::vector<WrHitResult> geckoResults;
  for (wr::HitResult wrResult : wrResults) {
    WrHitResult geckoResult;
    geckoResult.mLayersId = wr::AsLayersId(wrResult.pipeline_id);
    geckoResult.mScrollId =
        static_cast<layers::ScrollableLayerGuid::ViewID>(wrResult.scroll_id);
    geckoResult.mSideBits = ExtractSideBitsFromHitInfoBits(wrResult.hit_info);
    geckoResult.mHitInfo.deserialize(wrResult.hit_info);
    geckoResults.push_back(geckoResult);
  }
  return geckoResults;
}

void WebRenderAPI::Readback(const TimeStamp& aStartTime, gfx::IntSize size,
                            const gfx::SurfaceFormat& aFormat,
                            const Range<uint8_t>& buffer, bool* aNeedsYFlip) {
  class Readback : public RendererEvent {
   public:
    explicit Readback(layers::SynchronousTask* aTask, TimeStamp aStartTime,
                      gfx::IntSize aSize, const gfx::SurfaceFormat& aFormat,
                      const Range<uint8_t>& aBuffer, bool* aNeedsYFlip)
        : mTask(aTask),
          mStartTime(aStartTime),
          mSize(aSize),
          mFormat(aFormat),
          mBuffer(aBuffer),
          mNeedsYFlip(aNeedsYFlip) {
      MOZ_COUNT_CTOR(Readback);
    }

    MOZ_COUNTED_DTOR_OVERRIDE(Readback)

    void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
      aRenderThread.UpdateAndRender(aWindowId, VsyncId(), mStartTime,
                                    /* aRender */ true, Some(mSize),
                                    wr::SurfaceFormatToImageFormat(mFormat),
                                    Some(mBuffer), mNeedsYFlip);
      layers::AutoCompleteTask complete(mTask);
    }

    layers::SynchronousTask* mTask;
    TimeStamp mStartTime;
    gfx::IntSize mSize;
    gfx::SurfaceFormat mFormat;
    const Range<uint8_t>& mBuffer;
    bool* mNeedsYFlip;
  };

  // Disable debug flags during readback. See bug 1436020.
  UpdateDebugFlags(0);

  layers::SynchronousTask task("Readback");
  auto event = MakeUnique<Readback>(&task, aStartTime, size, aFormat, buffer,
                                    aNeedsYFlip);
  // This event will be passed from wr_backend thread to renderer thread. That
  // implies that all frame data have been processed when the renderer runs this
  // read-back event. Then, we could make sure this read-back event gets the
  // latest result.
  RunOnRenderThread(std::move(event));

  task.Wait();

  UpdateDebugFlags(gfx::gfxVars::WebRenderDebugFlags());
}

void WebRenderAPI::ClearAllCaches() { wr_api_clear_all_caches(mDocHandle); }

void WebRenderAPI::EnableNativeCompositor(bool aEnable) {
  wr_api_enable_native_compositor(mDocHandle, aEnable);
}

void WebRenderAPI::EnableMultithreading(bool aEnable) {
  wr_api_enable_multithreading(mDocHandle, aEnable);
}

void WebRenderAPI::SetBatchingLookback(uint32_t aCount) {
  wr_api_set_batching_lookback(mDocHandle, aCount);
}

void WebRenderAPI::SetClearColor(const gfx::DeviceColor& aColor) {
  RenderThread::Get()->SetClearColor(mId, ToColorF(aColor));
}

void WebRenderAPI::SetProfilerUI(const nsCString& aUIString) {
  RenderThread::Get()->SetProfilerUI(mId, aUIString);
}

void WebRenderAPI::Pause() {
  class PauseEvent : public RendererEvent {
   public:
    explicit PauseEvent(layers::SynchronousTask* aTask) : mTask(aTask) {
      MOZ_COUNT_CTOR(PauseEvent);
    }

    MOZ_COUNTED_DTOR_OVERRIDE(PauseEvent)

    void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
      aRenderThread.Pause(aWindowId);
      layers::AutoCompleteTask complete(mTask);
    }

    layers::SynchronousTask* mTask;
  };

  layers::SynchronousTask task("Pause");
  auto event = MakeUnique<PauseEvent>(&task);
  // This event will be passed from wr_backend thread to renderer thread. That
  // implies that all frame data have been processed when the renderer runs this
  // event.
  RunOnRenderThread(std::move(event));

  task.Wait();
}

bool WebRenderAPI::Resume() {
  class ResumeEvent : public RendererEvent {
   public:
    explicit ResumeEvent(layers::SynchronousTask* aTask, bool* aResult)
        : mTask(aTask), mResult(aResult) {
      MOZ_COUNT_CTOR(ResumeEvent);
    }

    MOZ_COUNTED_DTOR_OVERRIDE(ResumeEvent)

    void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
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
  // implies that all frame data have been processed when the renderer runs this
  // event.
  RunOnRenderThread(std::move(event));

  task.Wait();
  return result;
}

void WebRenderAPI::NotifyMemoryPressure() {
  wr_api_notify_memory_pressure(mDocHandle);
}

void WebRenderAPI::AccumulateMemoryReport(MemoryReport* aReport) {
  wr_api_accumulate_memory_report(mDocHandle, aReport, &WebRenderMallocSizeOf,
                                  &WebRenderMallocEnclosingSizeOf);
}

void WebRenderAPI::WakeSceneBuilder() { wr_api_wake_scene_builder(mDocHandle); }

void WebRenderAPI::FlushSceneBuilder() {
  wr_api_flush_scene_builder(mDocHandle);
}

void WebRenderAPI::WaitFlushed() {
  class WaitFlushedEvent : public RendererEvent {
   public:
    explicit WaitFlushedEvent(layers::SynchronousTask* aTask) : mTask(aTask) {
      MOZ_COUNT_CTOR(WaitFlushedEvent);
    }

    MOZ_COUNTED_DTOR_OVERRIDE(WaitFlushedEvent)

    void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
      layers::AutoCompleteTask complete(mTask);
    }

    layers::SynchronousTask* mTask;
  };

  layers::SynchronousTask task("WaitFlushed");
  auto event = MakeUnique<WaitFlushedEvent>(&task);
  // This event will be passed from wr_backend thread to renderer thread. That
  // implies that all frame data have been processed when the renderer runs this
  // event.
  RunOnRenderThread(std::move(event));

  task.Wait();
}

void WebRenderAPI::Capture() {
  // see CaptureBits
  // SCENE | FRAME | TILE_CACHE
  uint8_t bits = 15;                // TODO: get from JavaScript
  const char* path = "wr-capture";  // TODO: get from JavaScript
  wr_api_capture(mDocHandle, path, bits);
}

void WebRenderAPI::StartCaptureSequence(const nsCString& aPath,
                                        uint32_t aFlags) {
  if (mCaptureSequence) {
    wr_api_stop_capture_sequence(mDocHandle);
  }

  wr_api_start_capture_sequence(mDocHandle, PromiseFlatCString(aPath).get(),
                                aFlags);

  mCaptureSequence = true;
}

void WebRenderAPI::StopCaptureSequence() {
  if (mCaptureSequence) {
    wr_api_stop_capture_sequence(mDocHandle);
  }

  mCaptureSequence = false;
}

void WebRenderAPI::BeginRecording(const TimeStamp& aRecordingStart,
                                  wr::PipelineId aRootPipelineId) {
  class BeginRecordingEvent final : public RendererEvent {
   public:
    explicit BeginRecordingEvent(const TimeStamp& aRecordingStart,
                                 wr::PipelineId aRootPipelineId)
        : mRecordingStart(aRecordingStart), mRootPipelineId(aRootPipelineId) {
      MOZ_COUNT_CTOR(BeginRecordingEvent);
    }

    ~BeginRecordingEvent() { MOZ_COUNT_DTOR(BeginRecordingEvent); }

    void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
      aRenderThread.BeginRecordingForWindow(aWindowId, mRecordingStart,
                                            mRootPipelineId);
    }

   private:
    TimeStamp mRecordingStart;
    wr::PipelineId mRootPipelineId;
  };

  auto event =
      MakeUnique<BeginRecordingEvent>(aRecordingStart, aRootPipelineId);
  RunOnRenderThread(std::move(event));
}

RefPtr<WebRenderAPI::WriteCollectedFramesPromise>
WebRenderAPI::WriteCollectedFrames() {
  class WriteCollectedFramesEvent final : public RendererEvent {
   public:
    explicit WriteCollectedFramesEvent() {
      MOZ_COUNT_CTOR(WriteCollectedFramesEvent);
    }

    MOZ_COUNTED_DTOR(WriteCollectedFramesEvent)

    void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
      aRenderThread.WriteCollectedFramesForWindow(aWindowId);
      mPromise.Resolve(true, __func__);
    }

    RefPtr<WebRenderAPI::WriteCollectedFramesPromise> GetPromise() {
      return mPromise.Ensure(__func__);
    }

   private:
    MozPromiseHolder<WebRenderAPI::WriteCollectedFramesPromise> mPromise;
  };

  auto event = MakeUnique<WriteCollectedFramesEvent>();
  auto promise = event->GetPromise();

  RunOnRenderThread(std::move(event));
  return promise;
}

RefPtr<WebRenderAPI::GetCollectedFramesPromise>
WebRenderAPI::GetCollectedFrames() {
  class GetCollectedFramesEvent final : public RendererEvent {
   public:
    explicit GetCollectedFramesEvent() {
      MOZ_COUNT_CTOR(GetCollectedFramesEvent);
    }

    MOZ_COUNTED_DTOR(GetCollectedFramesEvent);

    void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
      Maybe<layers::CollectedFrames> frames =
          aRenderThread.GetCollectedFramesForWindow(aWindowId);

      if (frames) {
        mPromise.Resolve(std::move(*frames), __func__);
      } else {
        mPromise.Reject(NS_ERROR_UNEXPECTED, __func__);
      }
    }

    RefPtr<WebRenderAPI::GetCollectedFramesPromise> GetPromise() {
      return mPromise.Ensure(__func__);
    }

   private:
    MozPromiseHolder<WebRenderAPI::GetCollectedFramesPromise> mPromise;
  };

  auto event = MakeUnique<GetCollectedFramesEvent>();
  auto promise = event->GetPromise();

  RunOnRenderThread(std::move(event));
  return promise;
}

void TransactionBuilder::Clear() { wr_resource_updates_clear(mTxn); }

void TransactionBuilder::Notify(wr::Checkpoint aWhen,
                                UniquePtr<NotificationHandler> aEvent) {
  wr_transaction_notify(mTxn, aWhen,
                        reinterpret_cast<uintptr_t>(aEvent.release()));
}

void TransactionBuilder::AddImage(ImageKey key,
                                  const ImageDescriptor& aDescriptor,
                                  wr::Vec<uint8_t>& aBytes) {
  wr_resource_updates_add_image(mTxn, key, &aDescriptor, &aBytes.inner);
}

void TransactionBuilder::AddBlobImage(BlobImageKey key,
                                      const ImageDescriptor& aDescriptor,
                                      wr::Vec<uint8_t>& aBytes,
                                      const wr::DeviceIntRect& aVisibleRect) {
  wr_resource_updates_add_blob_image(mTxn, key, &aDescriptor, &aBytes.inner,
                                     aVisibleRect);
}

void TransactionBuilder::AddExternalImage(ImageKey key,
                                          const ImageDescriptor& aDescriptor,
                                          ExternalImageId aExtID,
                                          wr::ExternalImageType aImageType,
                                          uint8_t aChannelIndex) {
  wr_resource_updates_add_external_image(mTxn, key, &aDescriptor, aExtID,
                                         &aImageType, aChannelIndex);
}

void TransactionBuilder::AddExternalImageBuffer(
    ImageKey aKey, const ImageDescriptor& aDescriptor,
    ExternalImageId aHandle) {
  auto channelIndex = 0;
  AddExternalImage(aKey, aDescriptor, aHandle, wr::ExternalImageType::Buffer(),
                   channelIndex);
}

void TransactionBuilder::UpdateImageBuffer(ImageKey aKey,
                                           const ImageDescriptor& aDescriptor,
                                           wr::Vec<uint8_t>& aBytes) {
  wr_resource_updates_update_image(mTxn, aKey, &aDescriptor, &aBytes.inner);
}

void TransactionBuilder::UpdateBlobImage(BlobImageKey aKey,
                                         const ImageDescriptor& aDescriptor,
                                         wr::Vec<uint8_t>& aBytes,
                                         const wr::DeviceIntRect& aVisibleRect,
                                         const wr::LayoutIntRect& aDirtyRect) {
  wr_resource_updates_update_blob_image(mTxn, aKey, &aDescriptor, &aBytes.inner,
                                        aVisibleRect, aDirtyRect);
}

void TransactionBuilder::UpdateExternalImage(ImageKey aKey,
                                             const ImageDescriptor& aDescriptor,
                                             ExternalImageId aExtID,
                                             wr::ExternalImageType aImageType,
                                             uint8_t aChannelIndex) {
  wr_resource_updates_update_external_image(mTxn, aKey, &aDescriptor, aExtID,
                                            &aImageType, aChannelIndex);
}

void TransactionBuilder::UpdateExternalImageWithDirtyRect(
    ImageKey aKey, const ImageDescriptor& aDescriptor, ExternalImageId aExtID,
    wr::ExternalImageType aImageType, const wr::DeviceIntRect& aDirtyRect,
    uint8_t aChannelIndex) {
  wr_resource_updates_update_external_image_with_dirty_rect(
      mTxn, aKey, &aDescriptor, aExtID, &aImageType, aChannelIndex, aDirtyRect);
}

void TransactionBuilder::SetBlobImageVisibleArea(
    BlobImageKey aKey, const wr::DeviceIntRect& aArea) {
  wr_resource_updates_set_blob_image_visible_area(mTxn, aKey, &aArea);
}

void TransactionBuilder::DeleteImage(ImageKey aKey) {
  wr_resource_updates_delete_image(mTxn, aKey);
}

void TransactionBuilder::DeleteBlobImage(BlobImageKey aKey) {
  wr_resource_updates_delete_blob_image(mTxn, aKey);
}

void TransactionBuilder::AddRawFont(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes,
                                    uint32_t aIndex) {
  wr_resource_updates_add_raw_font(mTxn, aKey, &aBytes.inner, aIndex);
}

void TransactionBuilder::AddFontDescriptor(wr::FontKey aKey,
                                           wr::Vec<uint8_t>& aBytes,
                                           uint32_t aIndex) {
  wr_resource_updates_add_font_descriptor(mTxn, aKey, &aBytes.inner, aIndex);
}

void TransactionBuilder::DeleteFont(wr::FontKey aKey) {
  wr_resource_updates_delete_font(mTxn, aKey);
}

void TransactionBuilder::AddFontInstance(
    wr::FontInstanceKey aKey, wr::FontKey aFontKey, float aGlyphSize,
    const wr::FontInstanceOptions* aOptions,
    const wr::FontInstancePlatformOptions* aPlatformOptions,
    wr::Vec<uint8_t>& aVariations) {
  wr_resource_updates_add_font_instance(mTxn, aKey, aFontKey, aGlyphSize,
                                        aOptions, aPlatformOptions,
                                        &aVariations.inner);
}

void TransactionBuilder::DeleteFontInstance(wr::FontInstanceKey aKey) {
  wr_resource_updates_delete_font_instance(mTxn, aKey);
}

void TransactionBuilder::UpdateQualitySettings(
    bool aForceSubpixelAAWherePossible) {
  wr_transaction_set_quality_settings(mTxn, aForceSubpixelAAWherePossible);
}

class FrameStartTime : public RendererEvent {
 public:
  explicit FrameStartTime(const TimeStamp& aTime) : mTime(aTime) {
    MOZ_COUNT_CTOR(FrameStartTime);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(FrameStartTime)

  void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
    auto renderer = aRenderThread.GetRenderer(aWindowId);
    if (renderer) {
      renderer->SetFrameStartTime(mTime);
    }
  }

 private:
  TimeStamp mTime;
};

void WebRenderAPI::SetFrameStartTime(const TimeStamp& aTime) {
  auto event = MakeUnique<FrameStartTime>(aTime);
  RunOnRenderThread(std::move(event));
}

void WebRenderAPI::RunOnRenderThread(UniquePtr<RendererEvent> aEvent) {
  auto event = reinterpret_cast<uintptr_t>(aEvent.release());
  wr_api_send_external_event(mDocHandle, event);
}

DisplayListBuilder::DisplayListBuilder(PipelineId aId,
                                       WebRenderBackend aBackend,
                                       size_t aCapacity,
                                       layers::DisplayItemCache* aCache)
    : mCurrentSpaceAndClipChain(wr::RootScrollNodeWithChain()),
      mActiveFixedPosTracker(nullptr),
      mPipelineId(aId),
      mBackend(aBackend),
      mDisplayItemCache(aCache) {
  MOZ_COUNT_CTOR(DisplayListBuilder);
  mWrState = wr_state_new(aId, aCapacity);

  if (mDisplayItemCache && mDisplayItemCache->IsEnabled()) {
    mDisplayItemCache->SetPipelineId(aId);
  }
}

DisplayListBuilder::~DisplayListBuilder() {
  MOZ_COUNT_DTOR(DisplayListBuilder);
  wr_state_delete(mWrState);
}

void DisplayListBuilder::Save() { wr_dp_save(mWrState); }
void DisplayListBuilder::Restore() { wr_dp_restore(mWrState); }
void DisplayListBuilder::ClearSave() { wr_dp_clear_save(mWrState); }

usize DisplayListBuilder::Dump(usize aIndent, const Maybe<usize>& aStart,
                               const Maybe<usize>& aEnd) {
  return wr_dump_display_list(mWrState, aIndent, aStart.ptrOr(nullptr),
                              aEnd.ptrOr(nullptr));
}

void DisplayListBuilder::DumpSerializedDisplayList() {
  wr_dump_serialized_display_list(mWrState);
}

void DisplayListBuilder::Finalize(BuiltDisplayList& aOutDisplayList) {
  wr_api_finalize_builder(mWrState, &aOutDisplayList.dl_desc,
                          &aOutDisplayList.dl.inner);
}

void DisplayListBuilder::Finalize(layers::DisplayListData& aOutTransaction) {
  if (mDisplayItemCache && mDisplayItemCache->IsEnabled()) {
    wr_dp_set_cache_size(mWrState, mDisplayItemCache->CurrentSize());
  }

  wr::VecU8 dl;
  wr_api_finalize_builder(mWrState, &aOutTransaction.mDLDesc, &dl.inner);
  aOutTransaction.mDL.emplace(dl.inner.data, dl.inner.length,
                              dl.inner.capacity);
  aOutTransaction.mRemotePipelineIds = std::move(mRemotePipelineIds);
  dl.inner.capacity = 0;
  dl.inner.data = nullptr;
}

Maybe<wr::WrSpatialId> DisplayListBuilder::PushStackingContext(
    const wr::StackingContextParams& aParams, const wr::LayoutRect& aBounds,
    const wr::RasterSpace& aRasterSpace) {
  MOZ_ASSERT(mClipChainLeaf.isNothing(),
             "Non-empty leaf from clip chain given, but not used with SC!");

  wr::LayoutTransform matrix;
  const gfx::Matrix4x4* transform = aParams.mTransformPtr;
  if (transform) {
    matrix = ToLayoutTransform(*transform);
  }
  const wr::LayoutTransform* maybeTransform = transform ? &matrix : nullptr;
  WRDL_LOG("PushStackingContext b=%s t=%s\n", mWrState,
           ToString(aBounds).c_str(),
           transform ? ToString(*transform).c_str() : "none");

  auto spatialId = wr_dp_push_stacking_context(
      mWrState, aBounds, mCurrentSpaceAndClipChain.space, &aParams,
      maybeTransform, aParams.mFilters.Elements(), aParams.mFilters.Length(),
      aParams.mFilterDatas.Elements(), aParams.mFilterDatas.Length(),
      aRasterSpace);

  return spatialId.id != 0 ? Some(spatialId) : Nothing();
}

void DisplayListBuilder::PopStackingContext(bool aIsReferenceFrame) {
  WRDL_LOG("PopStackingContext\n", mWrState);
  wr_dp_pop_stacking_context(mWrState, aIsReferenceFrame);
}

wr::WrClipChainId DisplayListBuilder::DefineClipChain(
    const nsTArray<wr::WrClipId>& aClips, bool aParentWithCurrentChain) {
  CancelGroup();

  const uint64_t* parent = nullptr;
  if (aParentWithCurrentChain &&
      mCurrentSpaceAndClipChain.clip_chain != wr::ROOT_CLIP_CHAIN) {
    parent = &mCurrentSpaceAndClipChain.clip_chain;
  }
  uint64_t clipchainId = wr_dp_define_clipchain(
      mWrState, parent, aClips.Elements(), aClips.Length());
  WRDL_LOG("DefineClipChain id=%" PRIu64 " clips=%zu\n", mWrState, clipchainId,
           aClips.Length());
  return wr::WrClipChainId{clipchainId};
}

wr::WrClipId DisplayListBuilder::DefineClip(
    const Maybe<wr::WrSpaceAndClip>& aParent, const wr::LayoutRect& aClipRect,
    const nsTArray<wr::ComplexClipRegion>* aComplex) {
  CancelGroup();

  WrClipId clipId;
  if (aParent) {
    clipId = wr_dp_define_clip_with_parent_clip(
        mWrState, aParent.ptr(), aClipRect,
        aComplex ? aComplex->Elements() : nullptr,
        aComplex ? aComplex->Length() : 0);
  } else {
    clipId = wr_dp_define_clip_with_parent_clip_chain(
        mWrState, &mCurrentSpaceAndClipChain, aClipRect,
        aComplex ? aComplex->Elements() : nullptr,
        aComplex ? aComplex->Length() : 0);
  }

  WRDL_LOG("DefineClip id=%zu p=%s r=%s complex=%zu\n", mWrState, clipId.id,
           aParent ? ToString(aParent->clip.id).c_str() : "(nil)",
           ToString(aClipRect).c_str(), aComplex ? aComplex->Length() : 0);

  return clipId;
}

wr::WrClipId DisplayListBuilder::DefineImageMaskClip(
    const wr::ImageMask& aMask, const nsTArray<wr::LayoutPoint>& aPoints,
    wr::FillRule aFillRule) {
  CancelGroup();

  WrClipId clipId = wr_dp_define_image_mask_clip_with_parent_clip_chain(
      mWrState, &mCurrentSpaceAndClipChain, aMask, aPoints.Elements(),
      aPoints.Length(), aFillRule);

  return clipId;
}

wr::WrClipId DisplayListBuilder::DefineRoundedRectClip(
    const wr::ComplexClipRegion& aComplex) {
  CancelGroup();

  WrClipId clipId = wr_dp_define_rounded_rect_clip_with_parent_clip_chain(
      mWrState, &mCurrentSpaceAndClipChain, aComplex);

  return clipId;
}

wr::WrClipId DisplayListBuilder::DefineRectClip(wr::LayoutRect aClipRect) {
  CancelGroup();

  WrClipId clipId = wr_dp_define_rect_clip_with_parent_clip_chain(
      mWrState, &mCurrentSpaceAndClipChain, aClipRect);

  return clipId;
}

wr::WrSpatialId DisplayListBuilder::DefineStickyFrame(
    const wr::LayoutRect& aContentRect, const float* aTopMargin,
    const float* aRightMargin, const float* aBottomMargin,
    const float* aLeftMargin, const StickyOffsetBounds& aVerticalBounds,
    const StickyOffsetBounds& aHorizontalBounds,
    const wr::LayoutVector2D& aAppliedOffset) {
  auto spatialId = wr_dp_define_sticky_frame(
      mWrState, mCurrentSpaceAndClipChain.space, aContentRect, aTopMargin,
      aRightMargin, aBottomMargin, aLeftMargin, aVerticalBounds,
      aHorizontalBounds, aAppliedOffset);

  WRDL_LOG("DefineSticky id=%zu c=%s t=%s r=%s b=%s l=%s v=%s h=%s a=%s\n",
           mWrState, spatialId.id, ToString(aContentRect).c_str(),
           aTopMargin ? ToString(*aTopMargin).c_str() : "none",
           aRightMargin ? ToString(*aRightMargin).c_str() : "none",
           aBottomMargin ? ToString(*aBottomMargin).c_str() : "none",
           aLeftMargin ? ToString(*aLeftMargin).c_str() : "none",
           ToString(aVerticalBounds).c_str(),
           ToString(aHorizontalBounds).c_str(),
           ToString(aAppliedOffset).c_str());

  return spatialId;
}

Maybe<wr::WrSpaceAndClip> DisplayListBuilder::GetScrollIdForDefinedScrollLayer(
    layers::ScrollableLayerGuid::ViewID aViewId) const {
  if (aViewId == layers::ScrollableLayerGuid::NULL_SCROLL_ID) {
    return Some(wr::RootScrollNode());
  }

  auto it = mScrollIds.find(aViewId);
  if (it == mScrollIds.end()) {
    return Nothing();
  }

  return Some(it->second);
}

wr::WrSpaceAndClip DisplayListBuilder::DefineScrollLayer(
    const layers::ScrollableLayerGuid::ViewID& aViewId,
    const Maybe<wr::WrSpaceAndClip>& aParent,
    const wr::LayoutRect& aContentRect, const wr::LayoutRect& aClipRect,
    const wr::LayoutPoint& aScrollOffset) {
  auto it = mScrollIds.find(aViewId);
  if (it != mScrollIds.end()) {
    return it->second;
  }

  // We haven't defined aViewId before, so let's define it now.
  wr::WrSpaceAndClip defaultParent = wr::RootScrollNode();
  // Note: we are currently ignoring the clipId on the stack here
  defaultParent.space = mCurrentSpaceAndClipChain.space;

  auto spaceAndClip = wr_dp_define_scroll_layer(
      mWrState, aViewId, aParent ? aParent.ptr() : &defaultParent, aContentRect,
      aClipRect, aScrollOffset);

  WRDL_LOG("DefineScrollLayer id=%" PRIu64 "/%zu p=%s co=%s cl=%s\n", mWrState,
           aViewId, spaceAndClip.space.id,
           aParent ? ToString(aParent->space.id).c_str() : "(nil)",
           ToString(aContentRect).c_str(), ToString(aClipRect).c_str());

  mScrollIds[aViewId] = spaceAndClip;
  return spaceAndClip;
}

void DisplayListBuilder::PushRect(const wr::LayoutRect& aBounds,
                                  const wr::LayoutRect& aClip,
                                  bool aIsBackfaceVisible,
                                  const wr::ColorF& aColor) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushRect b=%s cl=%s c=%s\n", mWrState, ToString(aBounds).c_str(),
           ToString(clip).c_str(), ToString(aColor).c_str());
  wr_dp_push_rect(mWrState, aBounds, clip, aIsBackfaceVisible,
                  &mCurrentSpaceAndClipChain, aColor);
}

void DisplayListBuilder::PushRoundedRect(const wr::LayoutRect& aBounds,
                                         const wr::LayoutRect& aClip,
                                         bool aIsBackfaceVisible,
                                         const wr::ColorF& aColor) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushRoundedRect b=%s cl=%s c=%s\n", mWrState,
           ToString(aBounds).c_str(), ToString(clip).c_str(),
           ToString(aColor).c_str());

  // Draw the rounded rectangle as a border with rounded corners. We could also
  // draw this as a rectangle clipped to a rounded rectangle, but:
  // - clips are not cached; borders are
  // - a simple border like this will be drawn as an image
  // - Processing lots of clips is not WebRender's strong point.
  //
  // Made the borders thicker than one half the width/height, to avoid
  // little white dots at the center at some magnifications.
  wr::BorderSide side = {aColor, wr::BorderStyle::Solid};
  float h = aBounds.size.width * 0.6f;
  float v = aBounds.size.height * 0.6f;
  wr::LayoutSideOffsets widths = {v, h, v, h};
  wr::BorderRadius radii = {{h, v}, {h, v}, {h, v}, {h, v}};

  // Anti-aliased borders are required for rounded borders.
  wr_dp_push_border(mWrState, aBounds, clip, aIsBackfaceVisible,
                    &mCurrentSpaceAndClipChain, wr::AntialiasBorder::Yes,
                    widths, side, side, side, side, radii);
}

void DisplayListBuilder::PushHitTest(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible,
    const layers::ScrollableLayerGuid::ViewID& aScrollId,
    gfx::CompositorHitTestInfo aHitInfo, SideBits aSideBits) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushHitTest b=%s cl=%s\n", mWrState, ToString(aBounds).c_str(),
           ToString(clip).c_str());

  static_assert(gfx::DoesCompositorHitTestInfoFitIntoBits<12>(),
                "CompositorHitTestFlags MAX value has to be less than number "
                "of bits in uint16_t minus 4 for SideBitsPacked");

  uint16_t hitInfoBits = static_cast<uint16_t>(aHitInfo.serialize()) |
                         SideBitsToHitInfoBits(aSideBits);

  wr_dp_push_hit_test(mWrState, aBounds, clip, aIsBackfaceVisible,
                      &mCurrentSpaceAndClipChain, aScrollId, hitInfoBits);
}

void DisplayListBuilder::PushRectWithAnimation(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::ColorF& aColor,
    const WrAnimationProperty* aAnimation) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushRectWithAnimation b=%s cl=%s c=%s\n", mWrState,
           ToString(aBounds).c_str(), ToString(clip).c_str(),
           ToString(aColor).c_str());

  wr_dp_push_rect_with_animation(mWrState, aBounds, clip, aIsBackfaceVisible,
                                 &mCurrentSpaceAndClipChain, aColor,
                                 aAnimation);
}

void DisplayListBuilder::PushClearRect(const wr::LayoutRect& aBounds) {
  wr::LayoutRect clip = MergeClipLeaf(aBounds);
  WRDL_LOG("PushClearRect b=%s c=%s\n", mWrState, ToString(aBounds).c_str(),
           ToString(clip).c_str());
  wr_dp_push_clear_rect(mWrState, aBounds, clip, &mCurrentSpaceAndClipChain);
}

void DisplayListBuilder::PushClearRectWithComplexRegion(
    const wr::LayoutRect& aBounds, const wr::ComplexClipRegion& aRegion) {
  wr::LayoutRect clip = MergeClipLeaf(aBounds);
  WRDL_LOG("PushClearRectWithComplexRegion b=%s c=%s\n", mWrState,
           ToString(aBounds).c_str(), ToString(clip).c_str());

  // TODO(gw): This doesn't pass the complex region through to WR, as clear
  //           rects with complex clips are currently broken. This is the
  //           only place they are used, and they are used only for a single
  //           case (close buttons on Win7 machines). We might be able to
  //           get away with not supporting this at all in WR, using the
  //           non-clipped clear rect is an improvement for now, at least.
  //           See https://bugzilla.mozilla.org/show_bug.cgi?id=1636683 for
  //           more information.
  AutoTArray<wr::ComplexClipRegion, 1> clips;
  auto clipId = DefineClip(Nothing(), aBounds, &clips);
  auto spaceAndClip = WrSpaceAndClip{mCurrentSpaceAndClipChain.space, clipId};

  wr_dp_push_clear_rect_with_parent_clip(mWrState, aBounds, clip,
                                         &spaceAndClip);
}

void DisplayListBuilder::PushBackdropFilter(
    const wr::LayoutRect& aBounds, const wr::ComplexClipRegion& aRegion,
    const nsTArray<wr::FilterOp>& aFilters,
    const nsTArray<wr::WrFilterData>& aFilterDatas, bool aIsBackfaceVisible) {
  wr::LayoutRect clip = MergeClipLeaf(aBounds);
  WRDL_LOG("PushBackdropFilter b=%s c=%s\n", mWrState,
           ToString(aBounds).c_str(), ToString(clip).c_str());

  AutoTArray<wr::ComplexClipRegion, 1> clips;
  clips.AppendElement(aRegion);
  auto clipId = DefineClip(Nothing(), aBounds, &clips);
  auto spaceAndClip = WrSpaceAndClip{mCurrentSpaceAndClipChain.space, clipId};

  wr_dp_push_backdrop_filter_with_parent_clip(
      mWrState, aBounds, clip, aIsBackfaceVisible, &spaceAndClip,
      aFilters.Elements(), aFilters.Length(), aFilterDatas.Elements(),
      aFilterDatas.Length());
}

void DisplayListBuilder::PushLinearGradient(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::LayoutPoint& aStartPoint,
    const wr::LayoutPoint& aEndPoint, const nsTArray<wr::GradientStop>& aStops,
    wr::ExtendMode aExtendMode, const wr::LayoutSize aTileSize,
    const wr::LayoutSize aTileSpacing) {
  wr_dp_push_linear_gradient(
      mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
      &mCurrentSpaceAndClipChain, aStartPoint, aEndPoint, aStops.Elements(),
      aStops.Length(), aExtendMode, aTileSize, aTileSpacing);
}

void DisplayListBuilder::PushRadialGradient(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::LayoutPoint& aCenter,
    const wr::LayoutSize& aRadius, const nsTArray<wr::GradientStop>& aStops,
    wr::ExtendMode aExtendMode, const wr::LayoutSize aTileSize,
    const wr::LayoutSize aTileSpacing) {
  wr_dp_push_radial_gradient(
      mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
      &mCurrentSpaceAndClipChain, aCenter, aRadius, aStops.Elements(),
      aStops.Length(), aExtendMode, aTileSize, aTileSpacing);
}

void DisplayListBuilder::PushConicGradient(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::LayoutPoint& aCenter, const float aAngle,
    const nsTArray<wr::GradientStop>& aStops, wr::ExtendMode aExtendMode,
    const wr::LayoutSize aTileSize, const wr::LayoutSize aTileSpacing) {
  wr_dp_push_conic_gradient(mWrState, aBounds, MergeClipLeaf(aClip),
                            aIsBackfaceVisible, &mCurrentSpaceAndClipChain,
                            aCenter, aAngle, aStops.Elements(), aStops.Length(),
                            aExtendMode, aTileSize, aTileSpacing);
}

void DisplayListBuilder::PushImage(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, wr::ImageRendering aFilter, wr::ImageKey aImage,
    bool aPremultipliedAlpha, const wr::ColorF& aColor,
    bool aPreferCompositorSurface, bool aSupportsExternalCompositing) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushImage b=%s cl=%s\n", mWrState, ToString(aBounds).c_str(),
           ToString(clip).c_str());
  wr_dp_push_image(mWrState, aBounds, clip, aIsBackfaceVisible,
                   &mCurrentSpaceAndClipChain, aFilter, aImage,
                   aPremultipliedAlpha, aColor, aPreferCompositorSurface,
                   aSupportsExternalCompositing);
}

void DisplayListBuilder::PushRepeatingImage(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::LayoutSize& aStretchSize,
    const wr::LayoutSize& aTileSpacing, wr::ImageRendering aFilter,
    wr::ImageKey aImage, bool aPremultipliedAlpha, const wr::ColorF& aColor) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushImage b=%s cl=%s s=%s t=%s\n", mWrState,
           ToString(aBounds).c_str(), ToString(clip).c_str(),
           ToString(aStretchSize).c_str(), ToString(aTileSpacing).c_str());
  wr_dp_push_repeating_image(
      mWrState, aBounds, clip, aIsBackfaceVisible, &mCurrentSpaceAndClipChain,
      aStretchSize, aTileSpacing, aFilter, aImage, aPremultipliedAlpha, aColor);
}

void DisplayListBuilder::PushYCbCrPlanarImage(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
    wr::ImageKey aImageChannel1, wr::ImageKey aImageChannel2,
    wr::WrColorDepth aColorDepth, wr::WrYuvColorSpace aColorSpace,
    wr::WrColorRange aColorRange, wr::ImageRendering aRendering,
    bool aPreferCompositorSurface, bool aSupportsExternalCompositing) {
  wr_dp_push_yuv_planar_image(
      mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
      &mCurrentSpaceAndClipChain, aImageChannel0, aImageChannel1,
      aImageChannel2, aColorDepth, aColorSpace, aColorRange, aRendering,
      aPreferCompositorSurface, aSupportsExternalCompositing);
}

void DisplayListBuilder::PushNV12Image(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
    wr::ImageKey aImageChannel1, wr::WrColorDepth aColorDepth,
    wr::WrYuvColorSpace aColorSpace, wr::WrColorRange aColorRange,
    wr::ImageRendering aRendering, bool aPreferCompositorSurface,
    bool aSupportsExternalCompositing) {
  wr_dp_push_yuv_NV12_image(
      mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
      &mCurrentSpaceAndClipChain, aImageChannel0, aImageChannel1, aColorDepth,
      aColorSpace, aColorRange, aRendering, aPreferCompositorSurface,
      aSupportsExternalCompositing);
}

void DisplayListBuilder::PushYCbCrInterleavedImage(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
    wr::WrColorDepth aColorDepth, wr::WrYuvColorSpace aColorSpace,
    wr::WrColorRange aColorRange, wr::ImageRendering aRendering,
    bool aPreferCompositorSurface, bool aSupportsExternalCompositing) {
  wr_dp_push_yuv_interleaved_image(
      mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
      &mCurrentSpaceAndClipChain, aImageChannel0, aColorDepth, aColorSpace,
      aColorRange, aRendering, aPreferCompositorSurface,
      aSupportsExternalCompositing);
}

void DisplayListBuilder::PushIFrame(const wr::LayoutRect& aBounds,
                                    bool aIsBackfaceVisible,
                                    PipelineId aPipeline,
                                    bool aIgnoreMissingPipeline) {
  mRemotePipelineIds.AppendElement(aPipeline);
  wr_dp_push_iframe(mWrState, aBounds, MergeClipLeaf(aBounds),
                    aIsBackfaceVisible, &mCurrentSpaceAndClipChain, aPipeline,
                    aIgnoreMissingPipeline);
}

void DisplayListBuilder::PushBorder(const wr::LayoutRect& aBounds,
                                    const wr::LayoutRect& aClip,
                                    bool aIsBackfaceVisible,
                                    const wr::LayoutSideOffsets& aWidths,
                                    const Range<const wr::BorderSide>& aSides,
                                    const wr::BorderRadius& aRadius,
                                    wr::AntialiasBorder aAntialias) {
  MOZ_ASSERT(aSides.length() == 4);
  if (aSides.length() != 4) {
    return;
  }
  wr_dp_push_border(mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
                    &mCurrentSpaceAndClipChain, aAntialias, aWidths, aSides[0],
                    aSides[1], aSides[2], aSides[3], aRadius);
}

void DisplayListBuilder::PushBorderImage(const wr::LayoutRect& aBounds,
                                         const wr::LayoutRect& aClip,
                                         bool aIsBackfaceVisible,
                                         const wr::WrBorderImage& aParams) {
  wr_dp_push_border_image(mWrState, aBounds, MergeClipLeaf(aClip),
                          aIsBackfaceVisible, &mCurrentSpaceAndClipChain,
                          &aParams);
}

void DisplayListBuilder::PushBorderGradient(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::LayoutSideOffsets& aWidths,
    const int32_t aWidth, const int32_t aHeight, bool aFill,
    const wr::DeviceIntSideOffsets& aSlice, const wr::LayoutPoint& aStartPoint,
    const wr::LayoutPoint& aEndPoint, const nsTArray<wr::GradientStop>& aStops,
    wr::ExtendMode aExtendMode, const wr::LayoutSideOffsets& aOutset) {
  wr_dp_push_border_gradient(mWrState, aBounds, MergeClipLeaf(aClip),
                             aIsBackfaceVisible, &mCurrentSpaceAndClipChain,
                             aWidths, aWidth, aHeight, aFill, aSlice,
                             aStartPoint, aEndPoint, aStops.Elements(),
                             aStops.Length(), aExtendMode, aOutset);
}

void DisplayListBuilder::PushBorderRadialGradient(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::LayoutSideOffsets& aWidths, bool aFill,
    const wr::LayoutPoint& aCenter, const wr::LayoutSize& aRadius,
    const nsTArray<wr::GradientStop>& aStops, wr::ExtendMode aExtendMode,
    const wr::LayoutSideOffsets& aOutset) {
  wr_dp_push_border_radial_gradient(
      mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
      &mCurrentSpaceAndClipChain, aWidths, aFill, aCenter, aRadius,
      aStops.Elements(), aStops.Length(), aExtendMode, aOutset);
}

void DisplayListBuilder::PushBorderConicGradient(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::LayoutSideOffsets& aWidths, bool aFill,
    const wr::LayoutPoint& aCenter, const float aAngle,
    const nsTArray<wr::GradientStop>& aStops, wr::ExtendMode aExtendMode,
    const wr::LayoutSideOffsets& aOutset) {
  wr_dp_push_border_conic_gradient(
      mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
      &mCurrentSpaceAndClipChain, aWidths, aFill, aCenter, aAngle,
      aStops.Elements(), aStops.Length(), aExtendMode, aOutset);
}

void DisplayListBuilder::PushText(const wr::LayoutRect& aBounds,
                                  const wr::LayoutRect& aClip,
                                  bool aIsBackfaceVisible,
                                  const wr::ColorF& aColor,
                                  wr::FontInstanceKey aFontKey,
                                  Range<const wr::GlyphInstance> aGlyphBuffer,
                                  const wr::GlyphOptions* aGlyphOptions) {
  wr_dp_push_text(mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
                  &mCurrentSpaceAndClipChain, aColor, aFontKey,
                  &aGlyphBuffer[0], aGlyphBuffer.length(), aGlyphOptions);
}

void DisplayListBuilder::PushLine(const wr::LayoutRect& aClip,
                                  bool aIsBackfaceVisible,
                                  const wr::Line& aLine) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  wr_dp_push_line(mWrState, &clip, aIsBackfaceVisible,
                  &mCurrentSpaceAndClipChain, &aLine.bounds,
                  aLine.wavyLineThickness, aLine.orientation, &aLine.color,
                  aLine.style);
}

void DisplayListBuilder::PushShadow(const wr::LayoutRect& aRect,
                                    const wr::LayoutRect& aClip,
                                    bool aIsBackfaceVisible,
                                    const wr::Shadow& aShadow,
                                    bool aShouldInflate) {
  // Local clip_rects are translated inside of shadows, as they are assumed to
  // be part of the element drawing itself, and not a parent frame clipping it.
  // As such, it is not sound to apply the MergeClipLeaf optimization inside of
  // shadows. So we disable the optimization when we encounter a shadow.
  // Shadows don't span frames, so we don't have to worry about MergeClipLeaf
  // being re-enabled mid-shadow. The optimization is restored in PopAllShadows.
  SuspendClipLeafMerging();
  wr_dp_push_shadow(mWrState, aRect, aClip, aIsBackfaceVisible,
                    &mCurrentSpaceAndClipChain, aShadow, aShouldInflate);
}

void DisplayListBuilder::PopAllShadows() {
  wr_dp_pop_all_shadows(mWrState);
  ResumeClipLeafMerging();
}

void DisplayListBuilder::SuspendClipLeafMerging() {
  if (mClipChainLeaf) {
    // No one should reinitialize mClipChainLeaf while we're suspended
    MOZ_ASSERT(!mSuspendedClipChainLeaf);

    mSuspendedClipChainLeaf = mClipChainLeaf;
    mSuspendedSpaceAndClipChain = Some(mCurrentSpaceAndClipChain);

    auto clipId = DefineRectClip(*mClipChainLeaf);
    auto clipChainId = DefineClipChain({clipId}, true);

    mCurrentSpaceAndClipChain.clip_chain = clipChainId.id;
    mClipChainLeaf = Nothing();
  }
}

void DisplayListBuilder::ResumeClipLeafMerging() {
  if (mSuspendedClipChainLeaf) {
    mCurrentSpaceAndClipChain = *mSuspendedSpaceAndClipChain;
    mClipChainLeaf = mSuspendedClipChainLeaf;

    mSuspendedClipChainLeaf = Nothing();
    mSuspendedSpaceAndClipChain = Nothing();
  }
}

void DisplayListBuilder::PushBoxShadow(
    const wr::LayoutRect& aRect, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::LayoutRect& aBoxBounds,
    const wr::LayoutVector2D& aOffset, const wr::ColorF& aColor,
    const float& aBlurRadius, const float& aSpreadRadius,
    const wr::BorderRadius& aBorderRadius,
    const wr::BoxShadowClipMode& aClipMode) {
  wr_dp_push_box_shadow(mWrState, aRect, MergeClipLeaf(aClip),
                        aIsBackfaceVisible, &mCurrentSpaceAndClipChain,
                        aBoxBounds, aOffset, aColor, aBlurRadius, aSpreadRadius,
                        aBorderRadius, aClipMode);
}

void DisplayListBuilder::StartGroup(nsPaintedDisplayItem* aItem) {
  if (!mDisplayItemCache || mDisplayItemCache->IsFull()) {
    return;
  }

  MOZ_ASSERT(!mCurrentCacheSlot);
  mCurrentCacheSlot = mDisplayItemCache->AssignSlot(aItem);

  if (mCurrentCacheSlot) {
    wr_dp_start_item_group(mWrState);
  }
}

void DisplayListBuilder::CancelGroup(const bool aDiscard) {
  if (!mDisplayItemCache || !mCurrentCacheSlot) {
    return;
  }

  wr_dp_cancel_item_group(mWrState, aDiscard);
  mCurrentCacheSlot = Nothing();
}

void DisplayListBuilder::FinishGroup() {
  if (!mDisplayItemCache || !mCurrentCacheSlot) {
    return;
  }

  MOZ_ASSERT(mCurrentCacheSlot);

  if (wr_dp_finish_item_group(mWrState, mCurrentCacheSlot.ref())) {
    mDisplayItemCache->MarkSlotOccupied(mCurrentCacheSlot.ref(),
                                        CurrentSpaceAndClipChain());
    mDisplayItemCache->Stats().AddCached();
  }

  mCurrentCacheSlot = Nothing();
}

bool DisplayListBuilder::ReuseItem(nsPaintedDisplayItem* aItem) {
  if (!mDisplayItemCache) {
    return false;
  }

  mDisplayItemCache->Stats().AddTotal();

  if (mDisplayItemCache->IsEmpty()) {
    return false;
  }

  Maybe<uint16_t> slot =
      mDisplayItemCache->CanReuseItem(aItem, CurrentSpaceAndClipChain());

  if (slot) {
    mDisplayItemCache->Stats().AddReused();
    wr_dp_push_reuse_items(mWrState, slot.ref());
    return true;
  }

  return false;
}

Maybe<layers::ScrollableLayerGuid::ViewID>
DisplayListBuilder::GetContainingFixedPosScrollTarget(
    const ActiveScrolledRoot* aAsr) {
  return mActiveFixedPosTracker
             ? mActiveFixedPosTracker->GetScrollTargetForASR(aAsr)
             : Nothing();
}

Maybe<SideBits> DisplayListBuilder::GetContainingFixedPosSideBits(
    const ActiveScrolledRoot* aAsr) {
  return mActiveFixedPosTracker
             ? mActiveFixedPosTracker->GetSideBitsForASR(aAsr)
             : Nothing();
}

DisplayListBuilder::FixedPosScrollTargetTracker::FixedPosScrollTargetTracker(
    DisplayListBuilder& aBuilder, const ActiveScrolledRoot* aAsr,
    layers::ScrollableLayerGuid::ViewID aScrollId, SideBits aSideBits)
    : mParentTracker(aBuilder.mActiveFixedPosTracker),
      mBuilder(aBuilder),
      mAsr(aAsr),
      mScrollId(aScrollId),
      mSideBits(aSideBits) {
  aBuilder.mActiveFixedPosTracker = this;
}

DisplayListBuilder::FixedPosScrollTargetTracker::
    ~FixedPosScrollTargetTracker() {
  mBuilder.mActiveFixedPosTracker = mParentTracker;
}

Maybe<layers::ScrollableLayerGuid::ViewID>
DisplayListBuilder::FixedPosScrollTargetTracker::GetScrollTargetForASR(
    const ActiveScrolledRoot* aAsr) {
  return aAsr == mAsr ? Some(mScrollId) : Nothing();
}

Maybe<SideBits>
DisplayListBuilder::FixedPosScrollTargetTracker::GetSideBitsForASR(
    const ActiveScrolledRoot* aAsr) {
  return aAsr == mAsr ? Some(mSideBits) : Nothing();
}

already_AddRefed<gfxContext> DisplayListBuilder::GetTextContext(
    wr::IpcResourceUpdateQueue& aResources,
    const layers::StackingContextHelper& aSc,
    layers::RenderRootStateManager* aManager, nsDisplayItem* aItem,
    nsRect& aBounds, const gfx::Point& aDeviceOffset) {
  if (!mCachedTextDT) {
    mCachedTextDT = new layout::TextDrawTarget(*this, aResources, aSc, aManager,
                                               aItem, aBounds);
    mCachedContext = gfxContext::CreateOrNull(mCachedTextDT, aDeviceOffset);
  } else {
    mCachedTextDT->Reinitialize(aResources, aSc, aManager, aItem, aBounds);
    mCachedContext->SetDeviceOffset(aDeviceOffset);
    mCachedContext->SetMatrix(gfx::Matrix());
  }

  RefPtr<gfxContext> tmp = mCachedContext;
  return tmp.forget();
}

}  // namespace wr
}  // namespace mozilla

extern "C" {

void wr_transaction_notification_notified(uintptr_t aHandler,
                                          mozilla::wr::Checkpoint aWhen) {
  auto handler = reinterpret_cast<mozilla::wr::NotificationHandler*>(aHandler);
  handler->Notify(aWhen);
  // TODO: it would be better to get a callback when the object is destroyed on
  // the rust side and delete then.
  delete handler;
}

void wr_register_thread_local_arena() {
#ifdef MOZ_MEMORY
  jemalloc_thread_local_arena(true);
#endif
}

}  // extern C
