/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderAPI.h"

#include "DisplayItemClipChain.h"
#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/webrender/RendererOGL.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderCompositor.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/layers/SynchronousTask.h"

#define WRDL_LOG(...)
//#define WRDL_LOG(...) printf_stderr("WRDL(%p): " __VA_ARGS__)
//#define WRDL_LOG(...) if (XRE_IsContentProcess()) printf_stderr("WRDL(%p): " __VA_ARGS__)

namespace mozilla {
namespace wr {

using layers::Stringify;

class NewRenderer : public RendererEvent
{
public:
  NewRenderer(wr::DocumentHandle** aDocHandle, layers::CompositorBridgeParentBase* aBridge,
              uint32_t* aMaxTextureSize,
              bool* aUseANGLE,
              RefPtr<widget::CompositorWidget>&& aWidget,
              layers::SynchronousTask* aTask,
              LayoutDeviceIntSize aSize,
              layers::SyncHandle* aHandle)
    : mDocHandle(aDocHandle)
    , mMaxTextureSize(aMaxTextureSize)
    , mUseANGLE(aUseANGLE)
    , mBridge(aBridge)
    , mCompositorWidget(Move(aWidget))
    , mTask(aTask)
    , mSize(aSize)
    , mSyncHandle(aHandle)
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

    UniquePtr<RenderCompositor> compositor = RenderCompositor::Create(Move(mCompositorWidget));
    if (!compositor) {
      // RenderCompositor::Create puts a message into gfxCriticalNote if it is nullptr
      return;
    }

    *mUseANGLE = compositor->UseANGLE();

    wr::Renderer* wrRenderer = nullptr;
    if (!wr_window_new(aWindowId, mSize.width, mSize.height, compositor->gl(),
                       aRenderThread.ThreadPool().Raw(),
                       mDocHandle, &wrRenderer,
                       mMaxTextureSize)) {
      // wr_window_new puts a message into gfxCriticalNote if it returns false
      return;
    }
    MOZ_ASSERT(wrRenderer);

    RefPtr<RenderThread> thread = &aRenderThread;
    auto renderer = MakeUnique<RendererOGL>(Move(thread),
                                            Move(compositor),
                                            aWindowId,
                                            wrRenderer,
                                            mBridge);
    if (wrRenderer && renderer) {
      wr::WrExternalImageHandler handler = renderer->GetExternalImageHandler();
      wr_renderer_set_external_image_handler(wrRenderer, &handler);
      if (gfx::gfxVars::UseWebRenderProgramBinary()) {
        wr_renderer_update_program_cache(wrRenderer, aRenderThread.ProgramCache()->Raw());
      }
    }

    if (renderer) {
      layers::SyncObjectHost* syncObj = renderer->GetSyncObject();
      if (syncObj) {
        *mSyncHandle = syncObj->GetSyncHandle();
      }
    }

    aRenderThread.AddRenderer(aWindowId, Move(renderer));
  }

private:
  wr::DocumentHandle** mDocHandle;
  uint32_t* mMaxTextureSize;
  bool* mUseANGLE;
  layers::CompositorBridgeParentBase* mBridge;
  RefPtr<widget::CompositorWidget> mCompositorWidget;
  layers::SynchronousTask* mTask;
  LayoutDeviceIntSize mSize;
  layers::SyncHandle* mSyncHandle;
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


TransactionBuilder::TransactionBuilder()
{
  // We need the if statement to avoid miscompilation on windows, see
  // bug 1449982 comment 22.
  if (gfxPrefs::WebRenderAsyncSceneBuild()) {
    mTxn = wr_transaction_new(true);
    mResourceUpdates = wr_resource_updates_new();
  } else {
    mResourceUpdates = wr_resource_updates_new();
    mTxn = wr_transaction_new(false);
  }
}

TransactionBuilder::~TransactionBuilder()
{
  wr_transaction_delete(mTxn);
  wr_resource_updates_delete(mResourceUpdates);
}

void
TransactionBuilder::UpdateEpoch(PipelineId aPipelineId, Epoch aEpoch)
{
  wr_transaction_update_epoch(mTxn, aPipelineId, aEpoch);
}

void
TransactionBuilder::SetRootPipeline(PipelineId aPipelineId)
{
  wr_transaction_set_root_pipeline(mTxn, aPipelineId);
}

void
TransactionBuilder::RemovePipeline(PipelineId aPipelineId)
{
  wr_transaction_remove_pipeline(mTxn, aPipelineId);
}

void
TransactionBuilder::SetDisplayList(gfx::Color aBgColor,
                                   Epoch aEpoch,
                                   mozilla::LayerSize aViewportSize,
                                   wr::WrPipelineId pipeline_id,
                                   const wr::LayoutSize& content_size,
                                   wr::BuiltDisplayListDescriptor dl_descriptor,
                                   wr::Vec<uint8_t>& dl_data)
{
  wr_transaction_set_display_list(mTxn,
                                  aEpoch,
                                  ToColorF(aBgColor),
                                  aViewportSize.width, aViewportSize.height,
                                  pipeline_id,
                                  content_size,
                                  dl_descriptor,
                                  &dl_data.inner);
}

void
TransactionBuilder::ClearDisplayList(Epoch aEpoch, wr::WrPipelineId aPipelineId)
{
  wr_transaction_clear_display_list(mTxn, aEpoch, aPipelineId);
}

void
TransactionBuilder::GenerateFrame()
{
  wr_transaction_generate_frame(mTxn);
}

void
TransactionBuilder::UpdateDynamicProperties(const nsTArray<wr::WrOpacityProperty>& aOpacityArray,
                                     const nsTArray<wr::WrTransformProperty>& aTransformArray)
{
  wr_transaction_update_dynamic_properties(
      mTxn,
      aOpacityArray.IsEmpty() ?  nullptr : aOpacityArray.Elements(),
      aOpacityArray.Length(),
      aTransformArray.IsEmpty() ?  nullptr : aTransformArray.Elements(),
      aTransformArray.Length());
}

void
TransactionBuilder::AppendTransformProperties(const nsTArray<wr::WrTransformProperty>& aTransformArray)
{
  wr_transaction_append_transform_properties(
      mTxn,
      aTransformArray.IsEmpty() ? nullptr : aTransformArray.Elements(),
      aTransformArray.Length());
}

bool
TransactionBuilder::IsEmpty() const
{
  return wr_transaction_is_empty(mTxn);
}

void
TransactionBuilder::SetWindowParameters(const LayoutDeviceIntSize& aWindowSize,
                                        const LayoutDeviceIntRect& aDocumentRect)
{
  wr::DeviceUintSize wrWindowSize;
  wrWindowSize.width = aWindowSize.width;
  wrWindowSize.height = aWindowSize.height;
  wr::DeviceUintRect wrDocRect;
  wrDocRect.origin.x = aDocumentRect.x;
  wrDocRect.origin.y = aDocumentRect.y;
  wrDocRect.size.width = aDocumentRect.width;
  wrDocRect.size.height = aDocumentRect.height;
  wr_transaction_set_window_parameters(mTxn, &wrWindowSize, &wrDocRect);
}

void
TransactionBuilder::UpdateScrollPosition(const wr::WrPipelineId& aPipelineId,
                                         const layers::FrameMetrics::ViewID& aScrollId,
                                         const wr::LayoutPoint& aScrollPosition)
{
  wr_transaction_scroll_layer(mTxn, aPipelineId, aScrollId, aScrollPosition);
}

TransactionWrapper::TransactionWrapper(Transaction* aTxn)
  : mTxn(aTxn)
{
}

void
TransactionWrapper::AppendTransformProperties(const nsTArray<wr::WrTransformProperty>& aTransformArray)
{
  wr_transaction_append_transform_properties(
      mTxn,
      aTransformArray.IsEmpty() ? nullptr : aTransformArray.Elements(),
      aTransformArray.Length());
}

void
TransactionWrapper::UpdateScrollPosition(const wr::WrPipelineId& aPipelineId,
                                         const layers::FrameMetrics::ViewID& aScrollId,
                                         const wr::LayoutPoint& aScrollPosition)
{
  wr_transaction_scroll_layer(mTxn, aPipelineId, aScrollId, aScrollPosition);
}

/*static*/ void
WebRenderAPI::InitExternalLogHandler()
{
  // Redirect the webrender's log to gecko's log system.
  // The current log level is "error".
  mozilla::wr::wr_init_external_log_handler(wr::WrLogLevelFilter::Error);
}

/*static*/ void
WebRenderAPI::ShutdownExternalLogHandler()
{
  mozilla::wr::wr_shutdown_external_log_handler();
}

/*static*/ already_AddRefed<WebRenderAPI>
WebRenderAPI::Create(layers::CompositorBridgeParent* aBridge,
                     RefPtr<widget::CompositorWidget>&& aWidget,
                     const wr::WrWindowId& aWindowId,
                     LayoutDeviceIntSize aSize)
{
  MOZ_ASSERT(aBridge);
  MOZ_ASSERT(aWidget);
  static_assert(sizeof(size_t) == sizeof(uintptr_t),
      "The FFI bindings assume size_t is the same size as uintptr_t!");

  wr::DocumentHandle* docHandle = nullptr;
  uint32_t maxTextureSize = 0;
  bool useANGLE = false;
  layers::SyncHandle syncHandle = 0;

  // Dispatch a synchronous task because the DocumentHandle object needs to be created
  // on the render thread. If need be we could delay waiting on this task until
  // the next time we need to access the DocumentHandle object.
  layers::SynchronousTask task("Create Renderer");
  auto event = MakeUnique<NewRenderer>(&docHandle, aBridge, &maxTextureSize, &useANGLE,
                                       Move(aWidget), &task, aSize,
                                       &syncHandle);
  RenderThread::Get()->RunEvent(aWindowId, Move(event));

  task.Wait();

  if (!docHandle) {
    return nullptr;
  }

  return RefPtr<WebRenderAPI>(new WebRenderAPI(docHandle, aWindowId, maxTextureSize, useANGLE, syncHandle)).forget();
}

already_AddRefed<WebRenderAPI>
WebRenderAPI::Clone()
{
  wr::DocumentHandle* docHandle = nullptr;
  wr_api_clone(mDocHandle, &docHandle);

  RefPtr<WebRenderAPI> renderApi = new WebRenderAPI(docHandle, mId, mMaxTextureSize, mUseANGLE, mSyncHandle);
  renderApi->mRootApi = this; // Hold root api
  renderApi->mRootDocumentApi = this;
  return renderApi.forget();
}

already_AddRefed<WebRenderAPI>
WebRenderAPI::CreateDocument(LayoutDeviceIntSize aSize, int8_t aLayerIndex)
{
  wr::DeviceUintSize wrSize;
  wrSize.width = aSize.width;
  wrSize.height = aSize.height;
  wr::DocumentHandle* newDoc;

  wr_api_create_document(mDocHandle, &newDoc, wrSize, aLayerIndex);

  RefPtr<WebRenderAPI> api(new WebRenderAPI(newDoc, mId,
                                            mMaxTextureSize,
                                            mUseANGLE,
                                            mSyncHandle));
  api->mRootApi = this;
  return api.forget();
}

wr::WrIdNamespace
WebRenderAPI::GetNamespace() {
  return wr_api_get_namespace(mDocHandle);
}

extern void ClearBlobImageResources(WrIdNamespace aNamespace);

WebRenderAPI::~WebRenderAPI()
{
  if (!mRootDocumentApi) {
    wr_api_delete_document(mDocHandle);
  }

  if (!mRootApi) {
    RenderThread::Get()->SetDestroyed(GetId());

    layers::SynchronousTask task("Destroy WebRenderAPI");
    auto event = MakeUnique<RemoveRenderer>(&task);
    RunOnRenderThread(Move(event));
    task.Wait();

    wr_api_shut_down(mDocHandle);
  }

  // wr_api_get_namespace cannot be marked destructor-safe because it has a
  // return value, and we can't call it if MOZ_BUILD_WEBRENDER is not defined
  // because it's not destructor-safe. So let's just ifdef around it. This is
  // basically a hack to get around compile-time warnings, this code never runs
  // unless MOZ_BUILD_WEBRENDER is defined anyway.
#ifdef MOZ_BUILD_WEBRENDER
  wr::WrIdNamespace ns = GetNamespace();
#else
  wr::WrIdNamespace ns{0};
#endif

  // Clean up any resources the blob image renderer is holding onto that
  // can no longer be used once this WR API instance goes away.
  ClearBlobImageResources(ns);

  wr_api_delete(mDocHandle);
}

void
WebRenderAPI::SendTransaction(TransactionBuilder& aTxn)
{
  wr_transaction_update_resources(aTxn.Raw(), aTxn.RawUpdates());
  wr_api_send_transaction(mDocHandle, aTxn.Raw());
}

bool
WebRenderAPI::HitTest(const wr::WorldPoint& aPoint,
                      wr::WrPipelineId& aOutPipelineId,
                      layers::FrameMetrics::ViewID& aOutScrollId,
                      gfx::CompositorHitTestInfo& aOutHitInfo)
{
  static_assert(sizeof(gfx::CompositorHitTestInfo) == sizeof(uint16_t),
                "CompositorHitTestInfo should be u16-sized");
  return wr_api_hit_test(mDocHandle, aPoint,
          &aOutPipelineId, &aOutScrollId, (uint16_t*)&aOutHitInfo);
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
                aRenderThread.UpdateAndRender(aWindowId, /* aReadback */ true);
                wr_renderer_readback(aRenderThread.GetRenderer(aWindowId)->GetRenderer(),
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
WebRenderAPI::WakeSceneBuilder()
{
    wr_api_wake_scene_builder(mDocHandle);
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
WebRenderAPI::Capture()
{
  uint8_t bits = 3; //TODO: get from JavaScript
  const char* path = "wr-capture"; //TODO: get from JavaScript
  const char* border = "--------------------------\n";
  printf("%s Capturing WR state to: %s\n%s", border, path, border);
  wr_api_capture(mDocHandle, path, bits);
}


void
TransactionBuilder::Clear()
{
  wr_resource_updates_clear(mResourceUpdates);
}

void
TransactionBuilder::AddImage(ImageKey key, const ImageDescriptor& aDescriptor,
                             wr::Vec<uint8_t>& aBytes)
{
  wr_resource_updates_add_image(mResourceUpdates,
                                key,
                                &aDescriptor,
                                &aBytes.inner);
}

void
TransactionBuilder::AddBlobImage(ImageKey key, const ImageDescriptor& aDescriptor,
                                 wr::Vec<uint8_t>& aBytes)
{
  wr_resource_updates_add_blob_image(mResourceUpdates,
                                     key,
                                     &aDescriptor,
                                     &aBytes.inner);
}

void
TransactionBuilder::AddExternalImage(ImageKey key,
                                     const ImageDescriptor& aDescriptor,
                                     ExternalImageId aExtID,
                                     wr::WrExternalImageBufferType aBufferType,
                                     uint8_t aChannelIndex)
{
  wr_resource_updates_add_external_image(mResourceUpdates,
                                         key,
                                         &aDescriptor,
                                         aExtID,
                                         aBufferType,
                                         aChannelIndex);
}

void
TransactionBuilder::AddExternalImageBuffer(ImageKey aKey,
                                           const ImageDescriptor& aDescriptor,
                                           ExternalImageId aHandle)
{
  auto channelIndex = 0;
  AddExternalImage(aKey, aDescriptor, aHandle,
                   wr::WrExternalImageBufferType::ExternalBuffer,
                   channelIndex);
}

void
TransactionBuilder::UpdateImageBuffer(ImageKey aKey,
                                      const ImageDescriptor& aDescriptor,
                                      wr::Vec<uint8_t>& aBytes)
{
  wr_resource_updates_update_image(mResourceUpdates,
                                   aKey,
                                   &aDescriptor,
                                   &aBytes.inner);
}

void
TransactionBuilder::UpdateBlobImage(ImageKey aKey,
                                    const ImageDescriptor& aDescriptor,
                                    wr::Vec<uint8_t>& aBytes,
                                    const wr::DeviceUintRect& aDirtyRect)
{
  wr_resource_updates_update_blob_image(mResourceUpdates,
                                        aKey,
                                        &aDescriptor,
                                        &aBytes.inner,
                                        aDirtyRect);
}

void
TransactionBuilder::UpdateExternalImage(ImageKey aKey,
                                        const ImageDescriptor& aDescriptor,
                                        ExternalImageId aExtID,
                                        wr::WrExternalImageBufferType aBufferType,
                                        uint8_t aChannelIndex)
{
  wr_resource_updates_update_external_image(mResourceUpdates,
                                            aKey,
                                            &aDescriptor,
                                            aExtID,
                                            aBufferType,
                                            aChannelIndex);
}

void
TransactionBuilder::DeleteImage(ImageKey aKey)
{
  wr_resource_updates_delete_image(mResourceUpdates, aKey);
}

void
TransactionBuilder::AddRawFont(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes, uint32_t aIndex)
{
  wr_resource_updates_add_raw_font(mResourceUpdates, aKey, &aBytes.inner, aIndex);
}

void
TransactionBuilder::AddFontDescriptor(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes, uint32_t aIndex)
{
  wr_resource_updates_add_font_descriptor(mResourceUpdates, aKey, &aBytes.inner, aIndex);
}

void
TransactionBuilder::DeleteFont(wr::FontKey aKey)
{
  wr_resource_updates_delete_font(mResourceUpdates, aKey);
}

void
TransactionBuilder::AddFontInstance(wr::FontInstanceKey aKey,
                                    wr::FontKey aFontKey,
                                    float aGlyphSize,
                                    const wr::FontInstanceOptions* aOptions,
                                    const wr::FontInstancePlatformOptions* aPlatformOptions,
                                    wr::Vec<uint8_t>& aVariations)
{
  wr_resource_updates_add_font_instance(mResourceUpdates, aKey, aFontKey, aGlyphSize,
                                        aOptions, aPlatformOptions,
                                        &aVariations.inner);
}

void
TransactionBuilder::DeleteFontInstance(wr::FontInstanceKey aKey)
{
  wr_resource_updates_delete_font_instance(mResourceUpdates, aKey);
}

class FrameStartTime : public RendererEvent
{
public:
  explicit FrameStartTime(const TimeStamp& aTime)
    : mTime(aTime)
  {
    MOZ_COUNT_CTOR(FrameStartTime);
  }

  ~FrameStartTime()
  {
    MOZ_COUNT_DTOR(FrameStartTime);
  }

  virtual void Run(RenderThread& aRenderThread, WindowId aWindowId) override
  {
    auto renderer = aRenderThread.GetRenderer(aWindowId);
    if (renderer) {
      renderer->SetFrameStartTime(mTime);
    }
  }

private:
  TimeStamp mTime;
};

void
WebRenderAPI::SetFrameStartTime(const TimeStamp& aTime)
{
  auto event = MakeUnique<FrameStartTime>(aTime);
  RunOnRenderThread(Move(event));
}

void
WebRenderAPI::RunOnRenderThread(UniquePtr<RendererEvent> aEvent)
{
  auto event = reinterpret_cast<uintptr_t>(aEvent.release());
  wr_api_send_external_event(mDocHandle, event);
}

DisplayListBuilder::DisplayListBuilder(PipelineId aId,
                                       const wr::LayoutSize& aContentSize,
                                       size_t aCapacity)
{
  MOZ_COUNT_CTOR(DisplayListBuilder);
  mWrState = wr_state_new(aId, aContentSize, aCapacity);
}

DisplayListBuilder::~DisplayListBuilder()
{
  MOZ_COUNT_DTOR(DisplayListBuilder);
  wr_state_delete(mWrState);
}

void DisplayListBuilder::Save() { wr_dp_save(mWrState); }
void DisplayListBuilder::Restore() { wr_dp_restore(mWrState); }
void DisplayListBuilder::ClearSave() { wr_dp_clear_save(mWrState); }
void DisplayListBuilder::Dump() { wr_dump_display_list(mWrState); }

void
DisplayListBuilder::Finalize(wr::LayoutSize& aOutContentSize,
                             BuiltDisplayList& aOutDisplayList)
{
  wr_api_finalize_builder(mWrState,
                          &aOutContentSize,
                          &aOutDisplayList.dl_desc,
                          &aOutDisplayList.dl.inner);
}

void
DisplayListBuilder::PushStackingContext(const wr::LayoutRect& aBounds,
                                        const wr::WrClipId* aClipNodeId,
                                        const WrAnimationProperty* aAnimation,
                                        const float* aOpacity,
                                        const gfx::Matrix4x4* aTransform,
                                        wr::TransformStyle aTransformStyle,
                                        const gfx::Matrix4x4* aPerspective,
                                        const wr::MixBlendMode& aMixBlendMode,
                                        const nsTArray<wr::WrFilterOp>& aFilters,
                                        bool aIsBackfaceVisible)
{
  wr::LayoutTransform matrix;
  if (aTransform) {
    matrix = ToLayoutTransform(*aTransform);
  }
  const wr::LayoutTransform* maybeTransform = aTransform ? &matrix : nullptr;
  wr::LayoutTransform perspective;
  if (aPerspective) {
    perspective = ToLayoutTransform(*aPerspective);
  }
  const wr::LayoutTransform* maybePerspective = aPerspective ? &perspective : nullptr;
  const size_t* maybeClipNodeId = aClipNodeId ? &aClipNodeId->id : nullptr;
  WRDL_LOG("PushStackingContext b=%s t=%s\n", mWrState, Stringify(aBounds).c_str(),
      aTransform ? Stringify(*aTransform).c_str() : "none");
  wr_dp_push_stacking_context(mWrState, aBounds, maybeClipNodeId, aAnimation, aOpacity,
                              maybeTransform, aTransformStyle, maybePerspective,
                              aMixBlendMode, aFilters.Elements(), aFilters.Length(), aIsBackfaceVisible);
}

void
DisplayListBuilder::PopStackingContext()
{
  WRDL_LOG("PopStackingContext\n", mWrState);
  wr_dp_pop_stacking_context(mWrState);
}

wr::WrClipId
DisplayListBuilder::DefineClip(const Maybe<wr::WrScrollId>& aAncestorScrollId,
                               const Maybe<wr::WrClipId>& aAncestorClipId,
                               const wr::LayoutRect& aClipRect,
                               const nsTArray<wr::ComplexClipRegion>* aComplex,
                               const wr::WrImageMask* aMask)
{
  const size_t* ancestorScrollId = nullptr;
  if (aAncestorScrollId) {
    ancestorScrollId = &(aAncestorScrollId.ref().id);
  }
  const size_t* ancestorClipId = nullptr;
  if (aAncestorClipId) {
    ancestorClipId = &(aAncestorClipId.ref().id);
  }
  size_t clip_id = wr_dp_define_clip(mWrState,
      ancestorScrollId, ancestorClipId,
      aClipRect,
      aComplex ? aComplex->Elements() : nullptr,
      aComplex ? aComplex->Length() : 0,
      aMask);
  WRDL_LOG("DefineClip id=%zu as=%s ac=%s r=%s m=%p b=%s complex=%zu\n", mWrState,
      clip_id,
      aAncestorScrollId ? Stringify(aAncestorScrollId.ref().id).c_str() : "(nil)",
      aAncestorClipId ? Stringify(aAncestorClipId.ref().id).c_str() : "(nil)",
      Stringify(aClipRect).c_str(), aMask,
      aMask ? Stringify(aMask->rect).c_str() : "none",
      aComplex ? aComplex->Length() : 0);
  return wr::WrClipId { clip_id };
}

void
DisplayListBuilder::PushClip(const wr::WrClipId& aClipId,
                             const DisplayItemClipChain* aParent)
{
  wr_dp_push_clip(mWrState, aClipId.id);
  WRDL_LOG("PushClip id=%zu\n", mWrState, aClipId.id);
  if (!aParent) {
    mClipStack.push_back(wr::ScrollOrClipId(aClipId));
  } else {
    PushCacheOverride(aParent, aClipId);
  }
}

void
DisplayListBuilder::PopClip(const DisplayItemClipChain* aParent)
{
  WRDL_LOG("PopClip\n", mWrState);
  if (!aParent) {
    MOZ_ASSERT(mClipStack.back().is<wr::WrClipId>());
    mClipStack.pop_back();
  } else {
    PopCacheOverride(aParent);
  }
  wr_dp_pop_clip(mWrState);
}

void
DisplayListBuilder::PushCacheOverride(const DisplayItemClipChain* aParent,
                                      const wr::WrClipId& aClipId)
{
  // We need to walk the entire clip chain from aParent up and install aClipId
  // as an override for all of them. This is so that nested display items end up
  // with the correct parent clip regardless of which outside clip their clip
  // chains are attached to. Example:
  // nsDisplayStickyPosition with clipChain D -> C -> B -> A -> nullptr
  //   nsDisplayItem with clipChain F -> E -> B -> A -> nullptr
  // In this case the sticky clip that is generated by the sticky display item
  // is defined as a child of D, which is a child of C and so on. When we go
  // to define E for the nested display item, we want to make sure that it
  // is defined as a child of sticky clip, regardless of which of {D, C, B, A}
  // it has as a parent. {D, C, B, A} are what I refer to as the "outside clips"
  // because they are "outside" the sticky clip, and if we define E as a child
  // of any of those clips directly, then we end up losing the sticky clip from
  // the WR clip chain of the nested item. This is why we install cache
  // overrides for all of them so that when we walk the nested item's clip chain
  // from E to B we discover that really we want to use the sticky clip as E's
  // parent.
  for (const DisplayItemClipChain* i = aParent; i; i = i->mParent) {
    auto it = mCacheOverride.insert({ i, std::vector<wr::WrClipId>() });
    it.first->second.push_back(aClipId);
    WRDL_LOG("Pushing override %p -> %zu\n", mWrState, i, aClipId.id);
  }
}

void
DisplayListBuilder::PopCacheOverride(const DisplayItemClipChain* aParent)
{
  for (const DisplayItemClipChain* i = aParent; i; i = i->mParent) {
    auto it = mCacheOverride.find(i);
    MOZ_ASSERT(it != mCacheOverride.end());
    MOZ_ASSERT(!(it->second.empty()));
    WRDL_LOG("Popping override %p -> %zu\n", mWrState, i, it->second.back().id);
    it->second.pop_back();
    if (it->second.empty()) {
      mCacheOverride.erase(it);
    }
  }
}

Maybe<wr::WrClipId>
DisplayListBuilder::GetCacheOverride(const DisplayItemClipChain* aParent)
{
  auto it = mCacheOverride.find(aParent);
  return it == mCacheOverride.end() ? Nothing() : Some(it->second.back());
}

wr::WrStickyId
DisplayListBuilder::DefineStickyFrame(const wr::LayoutRect& aContentRect,
                                      const float* aTopMargin,
                                      const float* aRightMargin,
                                      const float* aBottomMargin,
                                      const float* aLeftMargin,
                                      const StickyOffsetBounds& aVerticalBounds,
                                      const StickyOffsetBounds& aHorizontalBounds,
                                      const wr::LayoutVector2D& aAppliedOffset)
{
  size_t id = wr_dp_define_sticky_frame(mWrState, aContentRect, aTopMargin,
      aRightMargin, aBottomMargin, aLeftMargin, aVerticalBounds, aHorizontalBounds,
      aAppliedOffset);
  WRDL_LOG("DefineSticky id=%zu c=%s t=%s r=%s b=%s l=%s v=%s h=%s a=%s\n",
      mWrState, id,
      Stringify(aContentRect).c_str(),
      aTopMargin ? Stringify(*aTopMargin).c_str() : "none",
      aRightMargin ? Stringify(*aRightMargin).c_str() : "none",
      aBottomMargin ? Stringify(*aBottomMargin).c_str() : "none",
      aLeftMargin ? Stringify(*aLeftMargin).c_str() : "none",
      Stringify(aVerticalBounds).c_str(),
      Stringify(aHorizontalBounds).c_str(),
      Stringify(aAppliedOffset).c_str());
  return wr::WrStickyId { id };
}

void
DisplayListBuilder::PushStickyFrame(const wr::WrStickyId& aStickyId,
                                    const DisplayItemClipChain* aParent)
{
  wr_dp_push_clip(mWrState, aStickyId.id);
  WRDL_LOG("PushSticky id=%zu\n", mWrState, aStickyId.id);
  // inside WR, a sticky id is just a regular clip id. so we can do some
  // handwaving here and turn the WrStickyId into a WrclipId and treat it
  // like any other out-of-band clip.
  wr::WrClipId stickyIdAsClipId;
  stickyIdAsClipId.id = aStickyId.id;
  PushCacheOverride(aParent, stickyIdAsClipId);
}

void
DisplayListBuilder::PopStickyFrame(const DisplayItemClipChain* aParent)
{
  WRDL_LOG("PopSticky\n", mWrState);
  PopCacheOverride(aParent);
  wr_dp_pop_clip(mWrState);
}

Maybe<wr::WrScrollId>
DisplayListBuilder::GetScrollIdForDefinedScrollLayer(layers::FrameMetrics::ViewID aViewId) const
{
  if (aViewId == layers::FrameMetrics::NULL_SCROLL_ID) {
    return Some(wr::WrScrollId::RootScrollNode());
  }

  auto it = mScrollIds.find(aViewId);
  if (it == mScrollIds.end()) {
    return Nothing();
  }

  return Some(it->second);
}

wr::WrScrollId
DisplayListBuilder::DefineScrollLayer(const layers::FrameMetrics::ViewID& aViewId,
                                      const Maybe<wr::WrScrollId>& aAncestorScrollId,
                                      const Maybe<wr::WrClipId>& aAncestorClipId,
                                      const wr::LayoutRect& aContentRect,
                                      const wr::LayoutRect& aClipRect)
{
  WRDL_LOG("DefineScrollLayer id=%" PRIu64 " as=%s ac=%s co=%s cl=%s\n", mWrState,
      aViewId,
      aAncestorScrollId ? Stringify(aAncestorScrollId.ref().id).c_str() : "(nil)",
      aAncestorClipId ? Stringify(aAncestorClipId.ref().id).c_str() : "(nil)",
      Stringify(aContentRect).c_str(), Stringify(aClipRect).c_str());

  auto it = mScrollIds.find(aViewId);
  if (it != mScrollIds.end()) {
    return it->second;
  }

  // We haven't defined aViewId before, so let's define it now.
  size_t numericScrollId = wr_dp_define_scroll_layer(
      mWrState,
      aViewId,
      aAncestorScrollId ? &(aAncestorScrollId->id) : nullptr,
      aAncestorClipId ? &(aAncestorClipId->id) : nullptr,
      aContentRect,
      aClipRect);
   auto wrScrollId = wr::WrScrollId { numericScrollId };
   mScrollIds[aViewId] = wrScrollId;
   return wrScrollId;
}

void
DisplayListBuilder::PushScrollLayer(const wr::WrScrollId& aScrollId)
{
  WRDL_LOG("PushScrollLayer id=%zu\n", mWrState, aScrollId.id);
  wr_dp_push_scroll_layer(mWrState, aScrollId.id);
  mClipStack.push_back(wr::ScrollOrClipId(aScrollId));
}

void
DisplayListBuilder::PopScrollLayer()
{
  MOZ_ASSERT(mClipStack.back().is<wr::WrScrollId>());
  WRDL_LOG("PopScrollLayer id=%zu\n", mWrState,
      mClipStack.back().as<wr::WrScrollId>().id);
  mClipStack.pop_back();
  wr_dp_pop_scroll_layer(mWrState);
}

void
DisplayListBuilder::PushClipAndScrollInfo(const wr::WrScrollId& aScrollId,
                                          const wr::WrClipId* aClipId)
{
  WRDL_LOG("PushClipAndScroll s=%zu c=%s\n", mWrState, aScrollId.id,
      aClipId ? Stringify(aClipId->id).c_str() : "none");
  wr_dp_push_clip_and_scroll_info(mWrState, aScrollId.id,
      aClipId ? &(aClipId->id) : nullptr);
  mClipStack.push_back(wr::ScrollOrClipId(aScrollId));
}

void
DisplayListBuilder::PopClipAndScrollInfo()
{
  MOZ_ASSERT(mClipStack.back().is<wr::WrScrollId>());
  WRDL_LOG("PopClipAndScroll\n", mWrState);
  mClipStack.pop_back();
  wr_dp_pop_clip_and_scroll_info(mWrState);
}

void
DisplayListBuilder::PushRect(const wr::LayoutRect& aBounds,
                             const wr::LayoutRect& aClip,
                             bool aIsBackfaceVisible,
                             const wr::ColorF& aColor)
{
  WRDL_LOG("PushRect b=%s cl=%s c=%s\n", mWrState,
      Stringify(aBounds).c_str(),
      Stringify(aClip).c_str(),
      Stringify(aColor).c_str());
  wr_dp_push_rect(mWrState, aBounds, aClip, aIsBackfaceVisible, aColor);
}

void
DisplayListBuilder::PushClearRect(const wr::LayoutRect& aBounds)
{
  WRDL_LOG("PushClearRect b=%s\n", mWrState,
      Stringify(aBounds).c_str());
  wr_dp_push_clear_rect(mWrState, aBounds);
}

void
DisplayListBuilder::PushLinearGradient(const wr::LayoutRect& aBounds,
                                       const wr::LayoutRect& aClip,
                                       bool aIsBackfaceVisible,
                                       const wr::LayoutPoint& aStartPoint,
                                       const wr::LayoutPoint& aEndPoint,
                                       const nsTArray<wr::GradientStop>& aStops,
                                       wr::ExtendMode aExtendMode,
                                       const wr::LayoutSize aTileSize,
                                       const wr::LayoutSize aTileSpacing)
{
  wr_dp_push_linear_gradient(mWrState,
                             aBounds, aClip, aIsBackfaceVisible,
                             aStartPoint, aEndPoint,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode,
                             aTileSize, aTileSpacing);
}

void
DisplayListBuilder::PushRadialGradient(const wr::LayoutRect& aBounds,
                                       const wr::LayoutRect& aClip,
                                       bool aIsBackfaceVisible,
                                       const wr::LayoutPoint& aCenter,
                                       const wr::LayoutSize& aRadius,
                                       const nsTArray<wr::GradientStop>& aStops,
                                       wr::ExtendMode aExtendMode,
                                       const wr::LayoutSize aTileSize,
                                       const wr::LayoutSize aTileSpacing)
{
  wr_dp_push_radial_gradient(mWrState,
                             aBounds, aClip, aIsBackfaceVisible,
                             aCenter, aRadius,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode,
                             aTileSize, aTileSpacing);
}

void
DisplayListBuilder::PushImage(const wr::LayoutRect& aBounds,
                              const wr::LayoutRect& aClip,
                              bool aIsBackfaceVisible,
                              wr::ImageRendering aFilter,
                              wr::ImageKey aImage,
                              bool aPremultipliedAlpha)
{
  wr::LayoutSize size;
  size.width = aBounds.size.width;
  size.height = aBounds.size.height;
  PushImage(aBounds, aClip, aIsBackfaceVisible, size, size, aFilter, aImage, aPremultipliedAlpha);
}

void
DisplayListBuilder::PushImage(const wr::LayoutRect& aBounds,
                              const wr::LayoutRect& aClip,
                              bool aIsBackfaceVisible,
                              const wr::LayoutSize& aStretchSize,
                              const wr::LayoutSize& aTileSpacing,
                              wr::ImageRendering aFilter,
                              wr::ImageKey aImage,
                              bool aPremultipliedAlpha)
{
  WRDL_LOG("PushImage b=%s cl=%s s=%s t=%s\n", mWrState,
      Stringify(aBounds).c_str(),
      Stringify(aClip).c_str(), Stringify(aStretchSize).c_str(),
      Stringify(aTileSpacing).c_str());
  wr_dp_push_image(mWrState, aBounds, aClip, aIsBackfaceVisible, aStretchSize, aTileSpacing, aFilter, aImage, aPremultipliedAlpha);
}

void
DisplayListBuilder::PushYCbCrPlanarImage(const wr::LayoutRect& aBounds,
                                         const wr::LayoutRect& aClip,
                                         bool aIsBackfaceVisible,
                                         wr::ImageKey aImageChannel0,
                                         wr::ImageKey aImageChannel1,
                                         wr::ImageKey aImageChannel2,
                                         wr::WrYuvColorSpace aColorSpace,
                                         wr::ImageRendering aRendering)
{
  wr_dp_push_yuv_planar_image(mWrState,
                              aBounds,
                              aClip,
                              aIsBackfaceVisible,
                              aImageChannel0,
                              aImageChannel1,
                              aImageChannel2,
                              aColorSpace,
                              aRendering);
}

void
DisplayListBuilder::PushNV12Image(const wr::LayoutRect& aBounds,
                                  const wr::LayoutRect& aClip,
                                  bool aIsBackfaceVisible,
                                  wr::ImageKey aImageChannel0,
                                  wr::ImageKey aImageChannel1,
                                  wr::WrYuvColorSpace aColorSpace,
                                  wr::ImageRendering aRendering)
{
  wr_dp_push_yuv_NV12_image(mWrState,
                            aBounds,
                            aClip,
                            aIsBackfaceVisible,
                            aImageChannel0,
                            aImageChannel1,
                            aColorSpace,
                            aRendering);
}

void
DisplayListBuilder::PushYCbCrInterleavedImage(const wr::LayoutRect& aBounds,
                                              const wr::LayoutRect& aClip,
                                              bool aIsBackfaceVisible,
                                              wr::ImageKey aImageChannel0,
                                              wr::WrYuvColorSpace aColorSpace,
                                              wr::ImageRendering aRendering)
{
  wr_dp_push_yuv_interleaved_image(mWrState,
                                   aBounds,
                                   aClip,
                                   aIsBackfaceVisible,
                                   aImageChannel0,
                                   aColorSpace,
                                   aRendering);
}

void
DisplayListBuilder::PushIFrame(const wr::LayoutRect& aBounds,
                               bool aIsBackfaceVisible,
                               PipelineId aPipeline)
{
  wr_dp_push_iframe(mWrState, aBounds, aIsBackfaceVisible, aPipeline);
}

void
DisplayListBuilder::PushBorder(const wr::LayoutRect& aBounds,
                               const wr::LayoutRect& aClip,
                               bool aIsBackfaceVisible,
                               const wr::BorderWidths& aWidths,
                               const Range<const wr::BorderSide>& aSides,
                               const wr::BorderRadius& aRadius)
{
  MOZ_ASSERT(aSides.length() == 4);
  if (aSides.length() != 4) {
    return;
  }
  wr_dp_push_border(mWrState, aBounds, aClip, aIsBackfaceVisible,
                    aWidths, aSides[0], aSides[1], aSides[2], aSides[3], aRadius);
}

void
DisplayListBuilder::PushBorderImage(const wr::LayoutRect& aBounds,
                                    const wr::LayoutRect& aClip,
                                    bool aIsBackfaceVisible,
                                    const wr::BorderWidths& aWidths,
                                    wr::ImageKey aImage,
                                    const wr::NinePatchDescriptor& aPatch,
                                    const wr::SideOffsets2D<float>& aOutset,
                                    const wr::RepeatMode& aRepeatHorizontal,
                                    const wr::RepeatMode& aRepeatVertical)
{
  wr_dp_push_border_image(mWrState, aBounds, aClip, aIsBackfaceVisible,
                          aWidths, aImage, aPatch, aOutset,
                          aRepeatHorizontal, aRepeatVertical);
}

void
DisplayListBuilder::PushBorderGradient(const wr::LayoutRect& aBounds,
                                       const wr::LayoutRect& aClip,
                                       bool aIsBackfaceVisible,
                                       const wr::BorderWidths& aWidths,
                                       const wr::LayoutPoint& aStartPoint,
                                       const wr::LayoutPoint& aEndPoint,
                                       const nsTArray<wr::GradientStop>& aStops,
                                       wr::ExtendMode aExtendMode,
                                       const wr::SideOffsets2D<float>& aOutset)
{
  wr_dp_push_border_gradient(mWrState, aBounds, aClip, aIsBackfaceVisible,
                             aWidths, aStartPoint, aEndPoint,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode, aOutset);
}

void
DisplayListBuilder::PushBorderRadialGradient(const wr::LayoutRect& aBounds,
                                             const wr::LayoutRect& aClip,
                                             bool aIsBackfaceVisible,
                                             const wr::BorderWidths& aWidths,
                                             const wr::LayoutPoint& aCenter,
                                             const wr::LayoutSize& aRadius,
                                             const nsTArray<wr::GradientStop>& aStops,
                                             wr::ExtendMode aExtendMode,
                                             const wr::SideOffsets2D<float>& aOutset)
{
  wr_dp_push_border_radial_gradient(
    mWrState, aBounds, aClip, aIsBackfaceVisible, aWidths, aCenter,
    aRadius, aStops.Elements(), aStops.Length(),
    aExtendMode, aOutset);
}

void
DisplayListBuilder::PushText(const wr::LayoutRect& aBounds,
                             const wr::LayoutRect& aClip,
                             bool aIsBackfaceVisible,
                             const wr::ColorF& aColor,
                             wr::FontInstanceKey aFontKey,
                             Range<const wr::GlyphInstance> aGlyphBuffer,
                             const wr::GlyphOptions* aGlyphOptions)
{
  wr_dp_push_text(mWrState, aBounds, aClip, aIsBackfaceVisible,
                  aColor,
                  aFontKey,
                  &aGlyphBuffer[0], aGlyphBuffer.length(),
                  aGlyphOptions);
}

void
DisplayListBuilder::PushLine(const wr::LayoutRect& aClip,
                             bool aIsBackfaceVisible,
                             const wr::Line& aLine)
{
 wr_dp_push_line(mWrState, &aClip, aIsBackfaceVisible,
                 &aLine.bounds, aLine.wavyLineThickness, aLine.orientation,
                 &aLine.color, aLine.style);
}

void
DisplayListBuilder::PushShadow(const wr::LayoutRect& aRect,
                               const wr::LayoutRect& aClip,
                               bool aIsBackfaceVisible,
                               const wr::Shadow& aShadow)
{
  wr_dp_push_shadow(mWrState, aRect, aClip, aIsBackfaceVisible, aShadow);
}

void
DisplayListBuilder::PopAllShadows()
{
  wr_dp_pop_all_shadows(mWrState);
}

void
DisplayListBuilder::PushBoxShadow(const wr::LayoutRect& aRect,
                                  const wr::LayoutRect& aClip,
                                  bool aIsBackfaceVisible,
                                  const wr::LayoutRect& aBoxBounds,
                                  const wr::LayoutVector2D& aOffset,
                                  const wr::ColorF& aColor,
                                  const float& aBlurRadius,
                                  const float& aSpreadRadius,
                                  const wr::BorderRadius& aBorderRadius,
                                  const wr::BoxShadowClipMode& aClipMode)
{
  wr_dp_push_box_shadow(mWrState, aRect, aClip, aIsBackfaceVisible,
                        aBoxBounds, aOffset, aColor,
                        aBlurRadius, aSpreadRadius, aBorderRadius,
                        aClipMode);
}

Maybe<wr::WrClipId>
DisplayListBuilder::TopmostClipId()
{
  for (auto it = mClipStack.crbegin(); it != mClipStack.crend(); it++) {
    if (it->is<wr::WrClipId>()) {
      return Some(it->as<wr::WrClipId>());
    }
  }
  return Nothing();
}

wr::WrScrollId
DisplayListBuilder::TopmostScrollId()
{
  for (auto it = mClipStack.crbegin(); it != mClipStack.crend(); it++) {
    if (it->is<wr::WrScrollId>()) {
      return it->as<wr::WrScrollId>();
    }
  }
  return wr::WrScrollId::RootScrollNode();
}

bool
DisplayListBuilder::TopmostIsClip()
{
  if (mClipStack.empty()) {
    return false;
  }
  return mClipStack.back().is<wr::WrClipId>();
}

void
DisplayListBuilder::SetHitTestInfo(const layers::FrameMetrics::ViewID& aScrollId,
                                   gfx::CompositorHitTestInfo aHitInfo)
{
  static_assert(sizeof(gfx::CompositorHitTestInfo) == sizeof(uint16_t),
                "CompositorHitTestInfo should be u16-sized");
  wr_set_item_tag(mWrState, aScrollId, static_cast<uint16_t>(aHitInfo));
}

void
DisplayListBuilder::ClearHitTestInfo()
{
  wr_clear_item_tag(mWrState);
}

} // namespace wr
} // namespace mozilla
