/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderAPI.h"

#include "mozilla/StaticPrefs_gfx.h"
#include "LayersLogging.h"
#include "mozilla/ipc/ByteBuf.h"
#include "mozilla/webrender/RendererOGL.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderCompositor.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/layers/SynchronousTask.h"
#include "TextDrawTarget.h"
#include "malloc_decls.h"

// clang-format off
#define WRDL_LOG(...)
//#define WRDL_LOG(...) printf_stderr("WRDL(%p): " __VA_ARGS__)
//#define WRDL_LOG(...) if (XRE_IsContentProcess()) printf_stderr("WRDL(%p): " __VA_ARGS__)
// clang-format on

namespace mozilla {
namespace wr {

using layers::Stringify;

MOZ_DEFINE_MALLOC_SIZE_OF(WebRenderMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(WebRenderMallocEnclosingSizeOf)

class NewRenderer : public RendererEvent {
 public:
  NewRenderer(wr::DocumentHandle** aDocHandle,
              layers::CompositorBridgeParent* aBridge, int32_t* aMaxTextureSize,
              bool* aUseANGLE, bool* aUseDComp, bool* aUseTripleBuffering,
              RefPtr<widget::CompositorWidget>&& aWidget,
              layers::SynchronousTask* aTask, LayoutDeviceIntSize aSize,
              layers::SyncHandle* aHandle)
      : mDocHandle(aDocHandle),
        mMaxTextureSize(aMaxTextureSize),
        mUseANGLE(aUseANGLE),
        mUseDComp(aUseDComp),
        mUseTripleBuffering(aUseTripleBuffering),
        mBridge(aBridge),
        mCompositorWidget(std::move(aWidget)),
        mTask(aTask),
        mSize(aSize),
        mSyncHandle(aHandle) {
    MOZ_COUNT_CTOR(NewRenderer);
  }

  MOZ_COUNTED_DTOR(NewRenderer)

  void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
    layers::AutoCompleteTask complete(mTask);

    UniquePtr<RenderCompositor> compositor =
        RenderCompositor::Create(std::move(mCompositorWidget));
    if (!compositor) {
      // RenderCompositor::Create puts a message into gfxCriticalNote if it is
      // nullptr
      return;
    }

    *mUseANGLE = compositor->UseANGLE();
    *mUseDComp = compositor->UseDComp();
    *mUseTripleBuffering = compositor->UseTripleBuffering();

    void* swCtx = nullptr;
    if (gfx::gfxVars::UseSoftwareWebRender()) {
      swCtx = wr_swgl_create_context();
    }

    // Only allow the panic on GL error functionality in nightly builds,
    // since it (deliberately) crashes the GPU process if any GL call
    // returns an error code.
    bool panic_on_gl_error = false;
#ifdef NIGHTLY_BUILD
    panic_on_gl_error =
        StaticPrefs::gfx_webrender_panic_on_gl_error_AtStartup();
#endif

    bool allow_texture_swizzling = gfx::gfxVars::UseGLSwizzle();
    bool isMainWindow = true;  // TODO!
    bool supportLowPriorityTransactions = isMainWindow;
    bool supportLowPriorityThreadpool =
        supportLowPriorityTransactions &&
        StaticPrefs::gfx_webrender_enable_low_priority_pool();
    bool supportPictureCaching = isMainWindow;
    wr::Renderer* wrRenderer = nullptr;
    if (!wr_window_new(
            aWindowId, mSize.width, mSize.height,
            supportLowPriorityTransactions, supportLowPriorityThreadpool,
            allow_texture_swizzling,
            StaticPrefs::gfx_webrender_picture_caching() &&
                supportPictureCaching,
#ifdef NIGHTLY_BUILD
            StaticPrefs::gfx_webrender_start_debug_server(),
#else
            false,
#endif
            swCtx, compositor->gl(), compositor->SurfaceOriginIsTopLeft(),
            aRenderThread.GetProgramCache()
                ? aRenderThread.GetProgramCache()->Raw()
                : nullptr,
            aRenderThread.GetShaders()
                ? aRenderThread.GetShaders()->RawShaders()
                : nullptr,
            aRenderThread.ThreadPool().Raw(),
            aRenderThread.ThreadPoolLP().Raw(), &WebRenderMallocSizeOf,
            &WebRenderMallocEnclosingSizeOf, (uint32_t)wr::RenderRoot::Default,
            compositor->ShouldUseNativeCompositor() ? compositor.get()
                                                    : nullptr,
            compositor->GetMaxUpdateRects(),
            compositor->GetMaxPartialPresentRects(), mDocHandle, &wrRenderer,
            mMaxTextureSize,
            StaticPrefs::gfx_webrender_enable_gpu_markers_AtStartup(),
            panic_on_gl_error)) {
      // wr_window_new puts a message into gfxCriticalNote if it returns false
      if (swCtx) {
        wr_swgl_destroy_context(swCtx);
      }
      return;
    }
    MOZ_ASSERT(wrRenderer);

    RefPtr<RenderThread> thread = &aRenderThread;
    auto renderer =
        MakeUnique<RendererOGL>(std::move(thread), std::move(compositor),
                                aWindowId, wrRenderer, mBridge, swCtx);
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
  int32_t* mMaxTextureSize;
  bool* mUseANGLE;
  bool* mUseDComp;
  bool* mUseTripleBuffering;
  layers::CompositorBridgeParent* mBridge;
  RefPtr<widget::CompositorWidget> mCompositorWidget;
  layers::SynchronousTask* mTask;
  LayoutDeviceIntSize mSize;
  layers::SyncHandle* mSyncHandle;
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

TransactionBuilder::TransactionBuilder(bool aUseSceneBuilderThread)
    : mUseSceneBuilderThread(aUseSceneBuilderThread) {
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
    const wr::LayoutSize& content_size,
    wr::BuiltDisplayListDescriptor dl_descriptor, wr::Vec<uint8_t>& dl_data) {
  wr_transaction_set_display_list(mTxn, aEpoch, ToColorF(aBgColor),
                                  aViewportSize, pipeline_id, content_size,
                                  dl_descriptor, &dl_data.inner);
}

void TransactionBuilder::ClearDisplayList(Epoch aEpoch,
                                          wr::WrPipelineId aPipelineId) {
  wr_transaction_clear_display_list(mTxn, aEpoch, aPipelineId);
}

void TransactionBuilder::GenerateFrame() {
  wr_transaction_generate_frame(mTxn);
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
    LayoutDeviceIntSize aSize) {
  MOZ_ASSERT(aBridge);
  MOZ_ASSERT(aWidget);
  static_assert(
      sizeof(size_t) == sizeof(uintptr_t),
      "The FFI bindings assume size_t is the same size as uintptr_t!");

  wr::DocumentHandle* docHandle = nullptr;
  int32_t maxTextureSize = 0;
  bool useANGLE = false;
  bool useDComp = false;
  bool useTripleBuffering = false;
  layers::SyncHandle syncHandle = 0;

  // Dispatch a synchronous task because the DocumentHandle object needs to be
  // created on the render thread. If need be we could delay waiting on this
  // task until the next time we need to access the DocumentHandle object.
  layers::SynchronousTask task("Create Renderer");
  auto event = MakeUnique<NewRenderer>(
      &docHandle, aBridge, &maxTextureSize, &useANGLE, &useDComp,
      &useTripleBuffering, std::move(aWidget), &task, aSize, &syncHandle);
  RenderThread::Get()->RunEvent(aWindowId, std::move(event));

  task.Wait();

  if (!docHandle) {
    return nullptr;
  }

  return RefPtr<WebRenderAPI>(
             new WebRenderAPI(docHandle, aWindowId, maxTextureSize, useANGLE,
                              useDComp, useTripleBuffering, syncHandle,
                              wr::RenderRoot::Default))
      .forget();
}

already_AddRefed<WebRenderAPI> WebRenderAPI::Clone() {
  wr::DocumentHandle* docHandle = nullptr;
  wr_api_clone(mDocHandle, &docHandle);

  RefPtr<WebRenderAPI> renderApi =
      new WebRenderAPI(docHandle, mId, mMaxTextureSize, mUseANGLE, mUseDComp,
                       mUseTripleBuffering, mSyncHandle, mRenderRoot);
  renderApi->mRootApi = this;  // Hold root api
  renderApi->mRootDocumentApi = this;

  return renderApi.forget();
}

already_AddRefed<WebRenderAPI> WebRenderAPI::CreateDocument(
    LayoutDeviceIntSize aSize, int8_t aLayerIndex, wr::RenderRoot aRenderRoot) {
  wr::DeviceIntSize wrSize;
  wrSize.width = aSize.width;
  wrSize.height = aSize.height;
  wr::DocumentHandle* newDoc;

  wr_api_create_document(mDocHandle, &newDoc, wrSize, aLayerIndex,
                         (uint32_t)aRenderRoot);

  RefPtr<WebRenderAPI> api(
      new WebRenderAPI(newDoc, mId, mMaxTextureSize, mUseANGLE, mUseDComp,
                       mUseTripleBuffering, mSyncHandle, aRenderRoot));
  api->mRootApi = this;
  return api.forget();
}

wr::WrIdNamespace WebRenderAPI::GetNamespace() {
  return wr_api_get_namespace(mDocHandle);
}

WebRenderAPI::WebRenderAPI(wr::DocumentHandle* aHandle, wr::WindowId aId,
                           uint32_t aMaxTextureSize, bool aUseANGLE,
                           bool aUseDComp, bool aUseTripleBuffering,
                           layers::SyncHandle aSyncHandle,
                           wr::RenderRoot aRenderRoot)
    : mDocHandle(aHandle),
      mId(aId),
      mMaxTextureSize(aMaxTextureSize),
      mUseANGLE(aUseANGLE),
      mUseDComp(aUseDComp),
      mUseTripleBuffering(aUseTripleBuffering),
      mCaptureSequence(false),
      mSyncHandle(aSyncHandle),
      mRenderRoot(aRenderRoot) {}

WebRenderAPI::~WebRenderAPI() {
  if (!mRootDocumentApi) {
    wr_api_delete_document(mDocHandle);
  }

  if (!mRootApi) {
    RenderThread::Get()->SetDestroyed(GetId());

    layers::SynchronousTask task("Destroy WebRenderAPI");
    auto event = MakeUnique<RemoveRenderer>(&task);
    RunOnRenderThread(std::move(event));
    task.Wait();

    wr_api_shut_down(mDocHandle);
  }

  wr_api_delete(mDocHandle);
}

void WebRenderAPI::UpdateDebugFlags(uint32_t aFlags) {
  wr_api_set_debug_flags(mDocHandle, wr::DebugFlags{aFlags});
}

void WebRenderAPI::SendTransaction(TransactionBuilder& aTxn) {
  wr_api_send_transaction(mDocHandle, aTxn.Raw(), aTxn.UseSceneBuilderThread());
}

/* static */
void WebRenderAPI::SendTransactions(
    const RenderRootArray<RefPtr<WebRenderAPI>>& aApis,
    RenderRootArray<TransactionBuilder*>& aTxns) {
  if (!aApis[RenderRoot::Default]) {
    return;
  }

  AutoTArray<DocumentHandle*, kRenderRootCount> documentHandles;
  AutoTArray<Transaction*, kRenderRootCount> txns;
  Maybe<bool> useSceneBuilderThread;
  for (auto& api : aApis) {
    if (!api) {
      continue;
    }
    auto& txn = aTxns[api->GetRenderRoot()];
    if (txn) {
      documentHandles.AppendElement(api->mDocHandle);
      txns.AppendElement(txn->Raw());
      if (useSceneBuilderThread.isSome()) {
        MOZ_ASSERT(txn->UseSceneBuilderThread() == *useSceneBuilderThread);
      } else {
        useSceneBuilderThread.emplace(txn->UseSceneBuilderThread());
      }
    }
  }
  if (!txns.IsEmpty()) {
    wr_api_send_transactions(documentHandles.Elements(), txns.Elements(),
                             txns.Length(), *useSceneBuilderThread);
  }
}

enum SideBitsPacked {
  eSideBitsPackedTop = 0x1000,
  eSideBitsPackedRight = 0x2000,
  eSideBitsPackedBottom = 0x4000,
  eSideBitsPackedLeft = 0x8000
};

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
                            const Range<uint8_t>& buffer) {
  class Readback : public RendererEvent {
   public:
    explicit Readback(layers::SynchronousTask* aTask, TimeStamp aStartTime,
                      gfx::IntSize aSize, const gfx::SurfaceFormat& aFormat,
                      const Range<uint8_t>& aBuffer)
        : mTask(aTask),
          mStartTime(aStartTime),
          mSize(aSize),
          mFormat(aFormat),
          mBuffer(aBuffer) {
      MOZ_COUNT_CTOR(Readback);
    }

    MOZ_COUNTED_DTOR_OVERRIDE(Readback)

    void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
      aRenderThread.UpdateAndRender(aWindowId, VsyncId(), mStartTime,
                                    /* aRender */ true, Some(mSize),
                                    wr::SurfaceFormatToImageFormat(mFormat),
                                    Some(mBuffer));
      layers::AutoCompleteTask complete(mTask);
    }

    layers::SynchronousTask* mTask;
    TimeStamp mStartTime;
    gfx::IntSize mSize;
    gfx::SurfaceFormat mFormat;
    const Range<uint8_t>& mBuffer;
  };

  // Disable debug flags during readback. See bug 1436020.
  UpdateDebugFlags(0);

  layers::SynchronousTask task("Readback");
  auto event = MakeUnique<Readback>(&task, aStartTime, size, aFormat, buffer);
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
  wr_api_accumulate_memory_report(mDocHandle, aReport);
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

void WebRenderAPI::ToggleCaptureSequence() {
  mCaptureSequence = !mCaptureSequence;
  if (mCaptureSequence) {
    uint8_t bits = 9;                          // TODO: get from JavaScript
    const char* path = "wr-capture-sequence";  // TODO: get from JavaScript
    wr_api_start_capture_sequence(mDocHandle, path, bits);
  } else {
    wr_api_stop_capture_sequence(mDocHandle);
  }
}

void WebRenderAPI::SetTransactionLogging(bool aValue) {
  wr_api_set_transaction_logging(mDocHandle, aValue);
}

void WebRenderAPI::SetCompositionRecorder(
    UniquePtr<layers::WebRenderCompositionRecorder> aRecorder) {
  class SetCompositionRecorderEvent final : public RendererEvent {
   public:
    explicit SetCompositionRecorderEvent(
        UniquePtr<layers::WebRenderCompositionRecorder> aRecorder)
        : mRecorder(std::move(aRecorder)) {
      MOZ_COUNT_CTOR(SetCompositionRecorderEvent);
    }

    ~SetCompositionRecorderEvent() {
      MOZ_COUNT_DTOR(SetCompositionRecorderEvent);
    }

    void Run(RenderThread& aRenderThread, WindowId aWindowId) override {
      MOZ_ASSERT(mRecorder);

      aRenderThread.SetCompositionRecorderForWindow(aWindowId,
                                                    std::move(mRecorder));
    }

   private:
    UniquePtr<layers::WebRenderCompositionRecorder> mRecorder;
  };

  auto event = MakeUnique<SetCompositionRecorderEvent>(std::move(aRecorder));
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
    bool aAllowSacrificingSubpixelAA) {
  wr_transaction_set_quality_settings(mTxn, aAllowSacrificingSubpixelAA);
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
                                       const wr::LayoutSize& aContentSize,
                                       size_t aCapacity,
                                       layers::DisplayItemCache* aCache,
                                       RenderRoot aRenderRoot)
    : mCurrentSpaceAndClipChain(wr::RootScrollNodeWithChain()),
      mActiveFixedPosTracker(nullptr),
      mPipelineId(aId),
      mContentSize(aContentSize),
      mRenderRoot(aRenderRoot),
      mDisplayItemCache(aCache) {
  MOZ_COUNT_CTOR(DisplayListBuilder);
  mWrState = wr_state_new(aId, aContentSize, aCapacity);

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

void DisplayListBuilder::Finalize(wr::LayoutSize& aOutContentSize,
                                  BuiltDisplayList& aOutDisplayList) {
  wr_api_finalize_builder(mWrState, &aOutContentSize, &aOutDisplayList.dl_desc,
                          &aOutDisplayList.dl.inner);
}

void DisplayListBuilder::Finalize(
    layers::RenderRootDisplayListData& aOutTransaction) {
  MOZ_ASSERT(mRenderRoot == wr::RenderRoot::Default);

  if (mDisplayItemCache && mDisplayItemCache->IsEnabled()) {
    wr_dp_set_cache_size(mWrState, mDisplayItemCache->CurrentSize());
  }

  wr::VecU8 dl;
  wr_api_finalize_builder(mWrState, &aOutTransaction.mContentSize,
                          &aOutTransaction.mDLDesc, &dl.inner);
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
           Stringify(aBounds).c_str(),
           transform ? Stringify(*transform).c_str() : "none");

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
    const nsTArray<wr::ComplexClipRegion>* aComplex,
    const wr::ImageMask* aMask) {
  CancelGroup();

  WrClipId clipId;
  if (aParent) {
    clipId = wr_dp_define_clip_with_parent_clip(
        mWrState, aParent.ptr(), aClipRect,
        aComplex ? aComplex->Elements() : nullptr,
        aComplex ? aComplex->Length() : 0, aMask);
  } else {
    clipId = wr_dp_define_clip_with_parent_clip_chain(
        mWrState, &mCurrentSpaceAndClipChain, aClipRect,
        aComplex ? aComplex->Elements() : nullptr,
        aComplex ? aComplex->Length() : 0, aMask);
  }

  WRDL_LOG("DefineClip id=%zu p=%s r=%s m=%p b=%s complex=%zu\n", mWrState,
           clipId.id, aParent ? Stringify(aParent->clip.id).c_str() : "(nil)",
           Stringify(aClipRect).c_str(), aMask,
           aMask ? Stringify(aMask->rect).c_str() : "none",
           aComplex ? aComplex->Length() : 0);

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
           mWrState, spatialId.id, Stringify(aContentRect).c_str(),
           aTopMargin ? Stringify(*aTopMargin).c_str() : "none",
           aRightMargin ? Stringify(*aRightMargin).c_str() : "none",
           aBottomMargin ? Stringify(*aBottomMargin).c_str() : "none",
           aLeftMargin ? Stringify(*aLeftMargin).c_str() : "none",
           Stringify(aVerticalBounds).c_str(),
           Stringify(aHorizontalBounds).c_str(),
           Stringify(aAppliedOffset).c_str());

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
           aParent ? Stringify(aParent->space.id).c_str() : "(nil)",
           Stringify(aContentRect).c_str(), Stringify(aClipRect).c_str());

  mScrollIds[aViewId] = spaceAndClip;
  return spaceAndClip;
}

void DisplayListBuilder::PushRect(const wr::LayoutRect& aBounds,
                                  const wr::LayoutRect& aClip,
                                  bool aIsBackfaceVisible,
                                  const wr::ColorF& aColor) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushRect b=%s cl=%s c=%s\n", mWrState, Stringify(aBounds).c_str(),
           Stringify(clip).c_str(), Stringify(aColor).c_str());
  wr_dp_push_rect(mWrState, aBounds, clip, aIsBackfaceVisible,
                  &mCurrentSpaceAndClipChain, aColor);
}

void DisplayListBuilder::PushRoundedRect(const wr::LayoutRect& aBounds,
                                         const wr::LayoutRect& aClip,
                                         bool aIsBackfaceVisible,
                                         const wr::ColorF& aColor) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushRoundedRect b=%s cl=%s c=%s\n", mWrState,
           Stringify(aBounds).c_str(), Stringify(clip).c_str(),
           Stringify(aColor).c_str());

  AutoTArray<wr::ComplexClipRegion, 1> clips;
  clips.AppendElement(wr::SimpleRadii(aBounds, aBounds.size.width / 2));
  // TODO: use `mCurrentSpaceAndClipChain.clip_chain` as a parent?
  auto clipId = DefineClip(Nothing(), aBounds, &clips, nullptr);
  auto spaceAndClip = WrSpaceAndClip{mCurrentSpaceAndClipChain.space, clipId};

  wr_dp_push_rect_with_parent_clip(mWrState, aBounds, clip, aIsBackfaceVisible,
                                   &spaceAndClip, aColor);
}

void DisplayListBuilder::PushHitTest(const wr::LayoutRect& aBounds,
                                     const wr::LayoutRect& aClip,
                                     bool aIsBackfaceVisible) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushHitTest b=%s cl=%s\n", mWrState, Stringify(aBounds).c_str(),
           Stringify(clip).c_str());
  wr_dp_push_hit_test(mWrState, aBounds, clip, aIsBackfaceVisible,
                      &mCurrentSpaceAndClipChain);
}

void DisplayListBuilder::PushRectWithAnimation(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::ColorF& aColor,
    const WrAnimationProperty* aAnimation) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushRectWithAnimation b=%s cl=%s c=%s\n", mWrState,
           Stringify(aBounds).c_str(), Stringify(clip).c_str(),
           Stringify(aColor).c_str());

  wr_dp_push_rect_with_animation(mWrState, aBounds, clip, aIsBackfaceVisible,
                                 &mCurrentSpaceAndClipChain, aColor,
                                 aAnimation);
}

void DisplayListBuilder::PushClearRect(const wr::LayoutRect& aBounds) {
  wr::LayoutRect clip = MergeClipLeaf(aBounds);
  WRDL_LOG("PushClearRect b=%s c=%s\n", mWrState, Stringify(aBounds).c_str(),
           Stringify(clip).c_str());
  wr_dp_push_clear_rect(mWrState, aBounds, clip, &mCurrentSpaceAndClipChain);
}

void DisplayListBuilder::PushClearRectWithComplexRegion(
    const wr::LayoutRect& aBounds, const wr::ComplexClipRegion& aRegion) {
  wr::LayoutRect clip = MergeClipLeaf(aBounds);
  WRDL_LOG("PushClearRectWithComplexRegion b=%s c=%s\n", mWrState,
           Stringify(aBounds).c_str(), Stringify(clip).c_str());

  AutoTArray<wr::ComplexClipRegion, 1> clips;
  auto clipId = DefineClip(Nothing(), aBounds, &clips, nullptr);
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
           Stringify(aBounds).c_str(), Stringify(clip).c_str());

  AutoTArray<wr::ComplexClipRegion, 1> clips;
  clips.AppendElement(aRegion);
  auto clipId = DefineClip(Nothing(), aBounds, &clips, nullptr);
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
    bool aPreferCompositorSurface) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushImage b=%s cl=%s\n", mWrState, Stringify(aBounds).c_str(),
           Stringify(clip).c_str());
  wr_dp_push_image(mWrState, aBounds, clip, aIsBackfaceVisible,
                   &mCurrentSpaceAndClipChain, aFilter, aImage,
                   aPremultipliedAlpha, aColor, aPreferCompositorSurface);
}

void DisplayListBuilder::PushRepeatingImage(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, const wr::LayoutSize& aStretchSize,
    const wr::LayoutSize& aTileSpacing, wr::ImageRendering aFilter,
    wr::ImageKey aImage, bool aPremultipliedAlpha, const wr::ColorF& aColor) {
  wr::LayoutRect clip = MergeClipLeaf(aClip);
  WRDL_LOG("PushImage b=%s cl=%s s=%s t=%s\n", mWrState,
           Stringify(aBounds).c_str(), Stringify(clip).c_str(),
           Stringify(aStretchSize).c_str(), Stringify(aTileSpacing).c_str());
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
    bool aPreferCompositorSurface) {
  wr_dp_push_yuv_planar_image(mWrState, aBounds, MergeClipLeaf(aClip),
                              aIsBackfaceVisible, &mCurrentSpaceAndClipChain,
                              aImageChannel0, aImageChannel1, aImageChannel2,
                              aColorDepth, aColorSpace, aColorRange, aRendering,
                              aPreferCompositorSurface);
}

void DisplayListBuilder::PushNV12Image(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
    wr::ImageKey aImageChannel1, wr::WrColorDepth aColorDepth,
    wr::WrYuvColorSpace aColorSpace, wr::WrColorRange aColorRange,
    wr::ImageRendering aRendering, bool aPreferCompositorSurface) {
  wr_dp_push_yuv_NV12_image(
      mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
      &mCurrentSpaceAndClipChain, aImageChannel0, aImageChannel1, aColorDepth,
      aColorSpace, aColorRange, aRendering, aPreferCompositorSurface);
}

void DisplayListBuilder::PushYCbCrInterleavedImage(
    const wr::LayoutRect& aBounds, const wr::LayoutRect& aClip,
    bool aIsBackfaceVisible, wr::ImageKey aImageChannel0,
    wr::WrColorDepth aColorDepth, wr::WrYuvColorSpace aColorSpace,
    wr::WrColorRange aColorRange, wr::ImageRendering aRendering,
    bool aPreferCompositorSurface) {
  wr_dp_push_yuv_interleaved_image(
      mWrState, aBounds, MergeClipLeaf(aClip), aIsBackfaceVisible,
      &mCurrentSpaceAndClipChain, aImageChannel0, aColorDepth, aColorSpace,
      aColorRange, aRendering, aPreferCompositorSurface);
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

    auto clipId = DefineClip(Nothing(), *mClipChainLeaf);
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

void DisplayListBuilder::CancelGroup() {
  if (!mDisplayItemCache || !mCurrentCacheSlot) {
    return;
  }

  wr_dp_cancel_item_group(mWrState);
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

uint16_t SideBitsToHitInfoBits(SideBits aSideBits) {
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

void DisplayListBuilder::SetHitTestInfo(
    const layers::ScrollableLayerGuid::ViewID& aScrollId,
    gfx::CompositorHitTestInfo aHitInfo, SideBits aSideBits) {
  static_assert(gfx::DoesCompositorHitTestInfoFitIntoBits<12>(),
                "CompositorHitTestFlags MAX value has to be less than number "
                "of bits in uint16_t minus 4 for SideBitsPacked");

  uint16_t hitInfoBits = static_cast<uint16_t>(aHitInfo.serialize()) |
                         SideBitsToHitInfoBits(aSideBits);

  wr_set_item_tag(mWrState, aScrollId, hitInfoBits);
}

void DisplayListBuilder::ClearHitTestInfo() { wr_clear_item_tag(mWrState); }

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
