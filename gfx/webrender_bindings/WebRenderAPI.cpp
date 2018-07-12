/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderAPI.h"

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
  NewRenderer(wr::DocumentHandle** aDocHandle,
              layers::CompositorBridgeParent* aBridge,
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
    , mCompositorWidget(std::move(aWidget))
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

    UniquePtr<RenderCompositor> compositor = RenderCompositor::Create(std::move(mCompositorWidget));
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
    auto renderer = MakeUnique<RendererOGL>(std::move(thread),
                                            std::move(compositor),
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

    aRenderThread.AddRenderer(aWindowId, std::move(renderer));
  }

private:
  wr::DocumentHandle** mDocHandle;
  uint32_t* mMaxTextureSize;
  bool* mUseANGLE;
  layers::CompositorBridgeParent* mBridge;
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


TransactionBuilder::TransactionBuilder(bool aUseSceneBuilderThread)
  : mUseSceneBuilderThread(aUseSceneBuilderThread)
{
  mTxn = wr_transaction_new(mUseSceneBuilderThread);
}

TransactionBuilder::~TransactionBuilder()
{
  wr_transaction_delete(mTxn);
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
                                       std::move(aWidget), &task, aSize,
                                       &syncHandle);
  RenderThread::Get()->RunEvent(aWindowId, std::move(event));

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

WebRenderAPI::~WebRenderAPI()
{
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

void
WebRenderAPI::SendTransaction(TransactionBuilder& aTxn)
{
  wr_api_send_transaction(mDocHandle, aTxn.Raw(), aTxn.UseSceneBuilderThread());
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
WebRenderAPI::Readback(const TimeStamp& aStartTime,
                       gfx::IntSize size,
                       uint8_t *buffer,
                       uint32_t buffer_size)
{
    class Readback : public RendererEvent
    {
        public:
            explicit Readback(layers::SynchronousTask* aTask,
                              TimeStamp aStartTime,
                              gfx::IntSize aSize, uint8_t *aBuffer, uint32_t aBufferSize)
                : mTask(aTask)
                , mStartTime(aStartTime)
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
                aRenderThread.UpdateAndRender(aWindowId, mStartTime, /* aReadback */ true);
                wr_renderer_readback(aRenderThread.GetRenderer(aWindowId)->GetRenderer(),
                                     mSize.width, mSize.height, mBuffer, mBufferSize);
                layers::AutoCompleteTask complete(mTask);
            }

            layers::SynchronousTask* mTask;
            TimeStamp mStartTime;
            gfx::IntSize mSize;
            uint8_t *mBuffer;
            uint32_t mBufferSize;
    };

    layers::SynchronousTask task("Readback");
    auto event = MakeUnique<Readback>(&task, aStartTime, size, buffer, buffer_size);
    // This event will be passed from wr_backend thread to renderer thread. That
    // implies that all frame data have been processed when the renderer runs this
    // read-back event. Then, we could make sure this read-back event gets the
    // latest result.
    RunOnRenderThread(std::move(event));

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
    RunOnRenderThread(std::move(event));

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
    RunOnRenderThread(std::move(event));

    task.Wait();
    return result;
}

void
WebRenderAPI::WakeSceneBuilder()
{
    wr_api_wake_scene_builder(mDocHandle);
}

void
WebRenderAPI::FlushSceneBuilder()
{
    wr_api_flush_scene_builder(mDocHandle);
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
    RunOnRenderThread(std::move(event));

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
  wr_resource_updates_clear(mTxn);
}

void
TransactionBuilder::AddImage(ImageKey key, const ImageDescriptor& aDescriptor,
                             wr::Vec<uint8_t>& aBytes)
{
  wr_resource_updates_add_image(mTxn,
                                key,
                                &aDescriptor,
                                &aBytes.inner);
}

void
TransactionBuilder::AddBlobImage(ImageKey key, const ImageDescriptor& aDescriptor,
                                 wr::Vec<uint8_t>& aBytes)
{
  wr_resource_updates_add_blob_image(mTxn,
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
  wr_resource_updates_add_external_image(mTxn,
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
  wr_resource_updates_update_image(mTxn,
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
  wr_resource_updates_update_blob_image(mTxn,
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
  wr_resource_updates_update_external_image(mTxn,
                                            aKey,
                                            &aDescriptor,
                                            aExtID,
                                            aBufferType,
                                            aChannelIndex);
}

void
TransactionBuilder::UpdateExternalImageWithDirtyRect(ImageKey aKey,
                                                     const ImageDescriptor& aDescriptor,
                                                     ExternalImageId aExtID,
                                                     wr::WrExternalImageBufferType aBufferType,
                                                     const wr::DeviceUintRect& aDirtyRect,
                                                     uint8_t aChannelIndex)
{
  wr_resource_updates_update_external_image_with_dirty_rect(mTxn,
                                                            aKey,
                                                            &aDescriptor,
                                                            aExtID,
                                                            aBufferType,
                                                            aChannelIndex,
                                                            aDirtyRect);
}

void
TransactionBuilder::DeleteImage(ImageKey aKey)
{
  wr_resource_updates_delete_image(mTxn, aKey);
}

void
TransactionBuilder::AddRawFont(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes, uint32_t aIndex)
{
  wr_resource_updates_add_raw_font(mTxn, aKey, &aBytes.inner, aIndex);
}

void
TransactionBuilder::AddFontDescriptor(wr::FontKey aKey, wr::Vec<uint8_t>& aBytes, uint32_t aIndex)
{
  wr_resource_updates_add_font_descriptor(mTxn, aKey, &aBytes.inner, aIndex);
}

void
TransactionBuilder::DeleteFont(wr::FontKey aKey)
{
  wr_resource_updates_delete_font(mTxn, aKey);
}

void
TransactionBuilder::AddFontInstance(wr::FontInstanceKey aKey,
                                    wr::FontKey aFontKey,
                                    float aGlyphSize,
                                    const wr::FontInstanceOptions* aOptions,
                                    const wr::FontInstancePlatformOptions* aPlatformOptions,
                                    wr::Vec<uint8_t>& aVariations)
{
  wr_resource_updates_add_font_instance(mTxn, aKey, aFontKey, aGlyphSize,
                                        aOptions, aPlatformOptions,
                                        &aVariations.inner);
}

void
TransactionBuilder::DeleteFontInstance(wr::FontInstanceKey aKey)
{
  wr_resource_updates_delete_font_instance(mTxn, aKey);
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
  RunOnRenderThread(std::move(event));
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
  : mActiveFixedPosTracker(nullptr)
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

Maybe<wr::WrClipId>
DisplayListBuilder::PushStackingContext(const wr::LayoutRect& aBounds,
                                        const wr::WrClipId* aClipNodeId,
                                        const WrAnimationProperty* aAnimation,
                                        const float* aOpacity,
                                        const gfx::Matrix4x4* aTransform,
                                        wr::TransformStyle aTransformStyle,
                                        const gfx::Matrix4x4* aPerspective,
                                        const wr::MixBlendMode& aMixBlendMode,
                                        const nsTArray<wr::WrFilterOp>& aFilters,
                                        bool aIsBackfaceVisible,
                                        const wr::GlyphRasterSpace& aRasterSpace)
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

  bool outIsReferenceFrame = false;
  uintptr_t outReferenceFrameId = 0;
  wr_dp_push_stacking_context(mWrState, aBounds, maybeClipNodeId, aAnimation,
                              aOpacity, maybeTransform, aTransformStyle,
                              maybePerspective, aMixBlendMode,
                              aFilters.Elements(), aFilters.Length(),
                              aIsBackfaceVisible, aRasterSpace,
                              &outIsReferenceFrame, &outReferenceFrameId);
  return outIsReferenceFrame ? Some(wr::WrClipId { outReferenceFrameId }) : Nothing();
}

void
DisplayListBuilder::PopStackingContext(bool aIsReferenceFrame)
{
  WRDL_LOG("PopStackingContext\n", mWrState);
  wr_dp_pop_stacking_context(mWrState, aIsReferenceFrame);
}

wr::WrClipChainId
DisplayListBuilder::DefineClipChain(const Maybe<wr::WrClipChainId>& aParent,
                                    const nsTArray<wr::WrClipId>& aClips)
{
  nsTArray<size_t> clipIds;
  for (wr::WrClipId id : aClips) {
    clipIds.AppendElement(id.id);
  }
  uint64_t clipchainId = wr_dp_define_clipchain(mWrState,
      aParent ? &(aParent->id) : nullptr,
      clipIds.Elements(), clipIds.Length());
  WRDL_LOG("DefineClipChain id=%" PRIu64 " p=%s clips=%zu\n", mWrState,
      clipchainId,
      aParent ? Stringify(aParent->id).c_str() : "(nil)",
      clipIds.Length());
  return wr::WrClipChainId{ clipchainId };
}

wr::WrClipId
DisplayListBuilder::DefineClip(const Maybe<wr::WrClipId>& aParentId,
                               const wr::LayoutRect& aClipRect,
                               const nsTArray<wr::ComplexClipRegion>* aComplex,
                               const wr::WrImageMask* aMask)
{
  size_t clip_id = wr_dp_define_clip(mWrState,
      aParentId ? &(aParentId->id) : nullptr,
      aClipRect,
      aComplex ? aComplex->Elements() : nullptr,
      aComplex ? aComplex->Length() : 0,
      aMask);
  WRDL_LOG("DefineClip id=%zu p=%s r=%s m=%p b=%s complex=%zu\n", mWrState,
      clip_id, aParentId ? Stringify(aParentId->id).c_str() : "(nil)",
      Stringify(aClipRect).c_str(), aMask,
      aMask ? Stringify(aMask->rect).c_str() : "none",
      aComplex ? aComplex->Length() : 0);
  return wr::WrClipId { clip_id };
}

void
DisplayListBuilder::PushClip(const wr::WrClipId& aClipId)
{
  WRDL_LOG("PushClip id=%zu\n", mWrState, aClipId.id);
  wr_dp_push_clip(mWrState, aClipId.id);
}

void
DisplayListBuilder::PopClip()
{
  WRDL_LOG("PopClip\n", mWrState);
  wr_dp_pop_clip(mWrState);
}

wr::WrClipId
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
  return wr::WrClipId { id };
}

Maybe<wr::WrClipId>
DisplayListBuilder::GetScrollIdForDefinedScrollLayer(layers::FrameMetrics::ViewID aViewId) const
{
  if (aViewId == layers::FrameMetrics::NULL_SCROLL_ID) {
    return Some(wr::WrClipId::RootScrollNode());
  }

  auto it = mScrollIds.find(aViewId);
  if (it == mScrollIds.end()) {
    return Nothing();
  }

  return Some(it->second);
}

wr::WrClipId
DisplayListBuilder::DefineScrollLayer(const layers::FrameMetrics::ViewID& aViewId,
                                      const Maybe<wr::WrClipId>& aParentId,
                                      const wr::LayoutRect& aContentRect,
                                      const wr::LayoutRect& aClipRect)
{
  auto it = mScrollIds.find(aViewId);
  if (it != mScrollIds.end()) {
    return it->second;
  }

  // We haven't defined aViewId before, so let's define it now.
  size_t numericScrollId = wr_dp_define_scroll_layer(
      mWrState,
      aViewId,
      aParentId ? &(aParentId->id) : nullptr,
      aContentRect,
      aClipRect);

  WRDL_LOG("DefineScrollLayer id=%" PRIu64 "/%zu p=%s co=%s cl=%s\n", mWrState,
      aViewId, numericScrollId,
      aParentId ? Stringify(aParentId->id).c_str() : "(nil)",
      Stringify(aContentRect).c_str(), Stringify(aClipRect).c_str());

   auto clipId = wr::WrClipId { numericScrollId };
   mScrollIds[aViewId] = clipId;
   return clipId;
}

void
DisplayListBuilder::PushClipAndScrollInfo(const wr::WrClipId& aScrollId,
                                          const wr::WrClipChainId* aClipChainId)
{
  WRDL_LOG("PushClipAndScroll s=%zu c=%s\n", mWrState, aScrollId.id,
      aClipChainId ? Stringify(aClipChainId->id).c_str() : "none");
  wr_dp_push_clip_and_scroll_info(mWrState, aScrollId.id,
      aClipChainId ? &(aClipChainId->id) : nullptr);
}

void
DisplayListBuilder::PopClipAndScrollInfo()
{
  WRDL_LOG("PopClipAndScroll\n", mWrState);
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
                               PipelineId aPipeline,
                               bool aIgnoreMissingPipeline)
{
  wr_dp_push_iframe(mWrState, aBounds, aIsBackfaceVisible, aPipeline, aIgnoreMissingPipeline);
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
                                    const uint32_t aWidth,
                                    const uint32_t aHeight,
                                    const wr::SideOffsets2D<uint32_t>& aSlice,
                                    const wr::SideOffsets2D<float>& aOutset,
                                    const wr::RepeatMode& aRepeatHorizontal,
                                    const wr::RepeatMode& aRepeatVertical)
{
  wr_dp_push_border_image(mWrState, aBounds, aClip, aIsBackfaceVisible,
                          aWidths, aImage, aWidth, aHeight, aSlice, aOutset,
                          aRepeatHorizontal, aRepeatVertical);
}

void
DisplayListBuilder::PushBorderGradient(const wr::LayoutRect& aBounds,
                                       const wr::LayoutRect& aClip,
                                       bool aIsBackfaceVisible,
                                       const wr::BorderWidths& aWidths,
                                       const uint32_t aWidth,
                                       const uint32_t aHeight,
                                       const wr::SideOffsets2D<uint32_t>& aSlice,
                                       const wr::LayoutPoint& aStartPoint,
                                       const wr::LayoutPoint& aEndPoint,
                                       const nsTArray<wr::GradientStop>& aStops,
                                       wr::ExtendMode aExtendMode,
                                       const wr::SideOffsets2D<float>& aOutset)
{
  wr_dp_push_border_gradient(mWrState, aBounds, aClip, aIsBackfaceVisible,
                             aWidths, aWidth, aHeight, aSlice, aStartPoint, aEndPoint,
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

Maybe<layers::FrameMetrics::ViewID>
DisplayListBuilder::GetContainingFixedPosScrollTarget(const ActiveScrolledRoot* aAsr)
{
  return mActiveFixedPosTracker
      ? mActiveFixedPosTracker->GetScrollTargetForASR(aAsr)
      : Nothing();
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

DisplayListBuilder::FixedPosScrollTargetTracker::FixedPosScrollTargetTracker(
    DisplayListBuilder& aBuilder,
    const ActiveScrolledRoot* aAsr,
    layers::FrameMetrics::ViewID aScrollId)
  : mParentTracker(aBuilder.mActiveFixedPosTracker)
  , mBuilder(aBuilder)
  , mAsr(aAsr)
  , mScrollId(aScrollId)
{
  aBuilder.mActiveFixedPosTracker = this;
}

DisplayListBuilder::FixedPosScrollTargetTracker::~FixedPosScrollTargetTracker()
{
  mBuilder.mActiveFixedPosTracker = mParentTracker;
}

Maybe<layers::FrameMetrics::ViewID>
DisplayListBuilder::FixedPosScrollTargetTracker::GetScrollTargetForASR(const ActiveScrolledRoot* aAsr)
{
  return aAsr == mAsr ? Some(mScrollId) : Nothing();
}

} // namespace wr
} // namespace mozilla
