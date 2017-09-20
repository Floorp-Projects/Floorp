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
//#define WRDL_LOG(...) printf_stderr("WRDL(%p): " __VA_ARGS__)

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

    *mUseANGLE = gl->IsANGLE();

    wr::Renderer* wrRenderer = nullptr;
    if (!wr_window_new(aWindowId, mSize.width, mSize.height, gl.get(),
                       aRenderThread.ThreadPool().Raw(),
                       mDocHandle, &wrRenderer,
                       mMaxTextureSize)) {
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
      wr::WrExternalImageHandler handler = renderer->GetExternalImageHandler();
      wr_renderer_set_external_image_handler(wrRenderer, &handler);
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


//static
already_AddRefed<WebRenderAPI>
WebRenderAPI::Create(layers::CompositorBridgeParentBase* aBridge,
                     RefPtr<widget::CompositorWidget>&& aWidget,
                     LayoutDeviceIntSize aSize)
{
  MOZ_ASSERT(aBridge);
  MOZ_ASSERT(aWidget);

  static uint64_t sNextId = 1;
  auto id = NewWindowId(sNextId++);

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
  RenderThread::Get()->RunEvent(id, Move(event));

  task.Wait();

  if (!docHandle) {
    return nullptr;
  }

  return RefPtr<WebRenderAPI>(new WebRenderAPI(docHandle, id, maxTextureSize, useANGLE, syncHandle)).forget();
}

already_AddRefed<WebRenderAPI>
WebRenderAPI::Clone()
{
  wr::DocumentHandle* docHandle = nullptr;
  wr_api_clone(mDocHandle, &docHandle);

  RefPtr<WebRenderAPI> renderApi = new WebRenderAPI(docHandle, mId, mMaxTextureSize, mUseANGLE, mSyncHandle);
  renderApi->mRootApi = this; // Hold root api
  return renderApi.forget();
}

wr::WrIdNamespace
WebRenderAPI::GetNamespace() {
  return wr_api_get_namespace(mDocHandle);
}

WebRenderAPI::~WebRenderAPI()
{
  if (!mRootApi) {
    layers::SynchronousTask task("Destroy WebRenderAPI");
    auto event = MakeUnique<RemoveRenderer>(&task);
    RunOnRenderThread(Move(event));
    task.Wait();
  }

  wr_api_delete(mDocHandle);
}

void
WebRenderAPI::UpdateScrollPosition(const wr::WrPipelineId& aPipelineId,
                                   const layers::FrameMetrics::ViewID& aScrollId,
                                   const wr::LayoutPoint& aScrollPosition)
{
  wr_scroll_layer_with_id(mDocHandle, aPipelineId, aScrollId, aScrollPosition);
}

void
WebRenderAPI::GenerateFrame()
{
  wr_api_generate_frame(mDocHandle);
}

void
WebRenderAPI::GenerateFrame(const nsTArray<wr::WrOpacityProperty>& aOpacityArray,
                            const nsTArray<wr::WrTransformProperty>& aTransformArray)
{
  wr_api_generate_frame_with_properties(mDocHandle,
                                        aOpacityArray.IsEmpty() ?
                                          nullptr : aOpacityArray.Elements(),
                                        aOpacityArray.Length(),
                                        aTransformArray.IsEmpty() ?
                                          nullptr : aTransformArray.Elements(),
                                        aTransformArray.Length());
}

void
WebRenderAPI::SetDisplayList(gfx::Color aBgColor,
                             Epoch aEpoch,
                             mozilla::LayerSize aViewportSize,
                             wr::WrPipelineId pipeline_id,
                             const LayoutSize& content_size,
                             wr::BuiltDisplayListDescriptor dl_descriptor,
                             uint8_t *dl_data,
                             size_t dl_size,
                             ResourceUpdateQueue& aResources)
{
  wr_api_set_display_list(mDocHandle,
                          ToColorF(aBgColor),
                          aEpoch,
                          aViewportSize.width, aViewportSize.height,
                          pipeline_id,
                          content_size,
                          dl_descriptor,
                          dl_data,
                          dl_size,
                          aResources.Raw());
}

void
WebRenderAPI::ClearDisplayList(Epoch aEpoch, wr::WrPipelineId pipeline_id)
{
  wr_api_clear_display_list(mDocHandle, aEpoch, pipeline_id);
}

void
WebRenderAPI::SetWindowParameters(LayoutDeviceIntSize size)
{
  wr_api_set_window_parameters(mDocHandle, size.width, size.height);
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
  wr_api_set_root_pipeline(mDocHandle, aPipeline);
}

void
WebRenderAPI::UpdateResources(ResourceUpdateQueue& aUpdates)
{
  wr_api_update_resources(mDocHandle, aUpdates.Raw());
}

ResourceUpdateQueue::ResourceUpdateQueue()
{
  mUpdates = wr_resource_updates_new();
}

ResourceUpdateQueue::ResourceUpdateQueue(ResourceUpdateQueue&& aFrom)
{
  mUpdates = aFrom.mUpdates;
  aFrom.mUpdates = nullptr;
}

ResourceUpdateQueue&
ResourceUpdateQueue::operator=(ResourceUpdateQueue&& aFrom)
{
  mUpdates = aFrom.mUpdates;
  aFrom.mUpdates = nullptr;
  return *this;
}

ResourceUpdateQueue::~ResourceUpdateQueue()
{
  if (mUpdates) {
    wr_resource_updates_delete(mUpdates);
  }
}

void
ResourceUpdateQueue::Clear()
{
  wr_resource_updates_clear(mUpdates);
}

void
ResourceUpdateQueue::AddImage(ImageKey key, const ImageDescriptor& aDescriptor,
                              wr::Vec_u8& aBytes)
{
  wr_resource_updates_add_image(mUpdates,
                                key,
                                &aDescriptor,
                                &aBytes.inner);
}

void
ResourceUpdateQueue::AddBlobImage(ImageKey key, const ImageDescriptor& aDescriptor,
                                  wr::Vec_u8& aBytes)
{
  wr_resource_updates_add_blob_image(mUpdates,
                                     key,
                                     &aDescriptor,
                                     &aBytes.inner);
}

void
ResourceUpdateQueue::AddExternalImage(ImageKey key,
                                      const ImageDescriptor& aDescriptor,
                                      ExternalImageId aExtID,
                                      wr::WrExternalImageBufferType aBufferType,
                                      uint8_t aChannelIndex)
{
  wr_resource_updates_add_external_image(mUpdates,
                                         key,
                                         &aDescriptor,
                                         aExtID,
                                         aBufferType,
                                         aChannelIndex);
}

void
ResourceUpdateQueue::AddExternalImageBuffer(ImageKey aKey,
                                            const ImageDescriptor& aDescriptor,
                                            ExternalImageId aHandle)
{
  auto channelIndex = 0;
  AddExternalImage(aKey, aDescriptor, aHandle,
                   wr::WrExternalImageBufferType::ExternalBuffer,
                   channelIndex);
}

void
ResourceUpdateQueue::UpdateImageBuffer(ImageKey aKey,
                                       const ImageDescriptor& aDescriptor,
                                       wr::Vec_u8& aBytes)
{
  wr_resource_updates_update_image(mUpdates,
                                   aKey,
                                   &aDescriptor,
                                   &aBytes.inner);
}

void
ResourceUpdateQueue::UpdateBlobImage(ImageKey aKey,
                                     const ImageDescriptor& aDescriptor,
                                     wr::Vec_u8& aBytes)
{
  wr_resource_updates_update_blob_image(mUpdates,
                                        aKey,
                                        &aDescriptor,
                                        &aBytes.inner);
}

void
ResourceUpdateQueue::UpdateExternalImage(ImageKey aKey,
                                         const ImageDescriptor& aDescriptor,
                                         ExternalImageId aExtID,
                                         wr::WrExternalImageBufferType aBufferType,
                                         uint8_t aChannelIndex)
{
  wr_resource_updates_update_external_image(mUpdates,
                                            aKey,
                                            &aDescriptor,
                                            aExtID,
                                            aBufferType,
                                            aChannelIndex);
}

void
ResourceUpdateQueue::DeleteImage(ImageKey aKey)
{
  wr_resource_updates_delete_image(mUpdates, aKey);
}

void
ResourceUpdateQueue::AddRawFont(wr::FontKey aKey, wr::Vec_u8& aBytes, uint32_t aIndex)
{
  wr_resource_updates_add_raw_font(mUpdates, aKey, &aBytes.inner, aIndex);
}

void
ResourceUpdateQueue::DeleteFont(wr::FontKey aKey)
{
  wr_resource_updates_delete_font(mUpdates, aKey);
}

void
ResourceUpdateQueue::AddFontInstance(wr::FontInstanceKey aKey,
                                     wr::FontKey aFontKey,
                                     float aGlyphSize,
                                     const wr::FontInstanceOptions* aOptions,
                                     const wr::FontInstancePlatformOptions* aPlatformOptions)
{
  wr_resource_updates_add_font_instance(mUpdates, aKey, aFontKey, aGlyphSize,
                                        aOptions, aPlatformOptions);
}

void
ResourceUpdateQueue::DeleteFontInstance(wr::FontInstanceKey aKey)
{
  wr_resource_updates_delete_font_instance(mUpdates, aKey);
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
                                       const wr::LayoutSize& aContentSize)
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
                                        const uint64_t& aAnimationId,
                                        const float* aOpacity,
                                        const gfx::Matrix4x4* aTransform,
                                        wr::TransformStyle aTransformStyle,
                                        const gfx::Matrix4x4* aPerspective,
                                        const wr::MixBlendMode& aMixBlendMode,
                                        const nsTArray<wr::WrFilterOp>& aFilters)
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
  WRDL_LOG("PushStackingContext b=%s t=%s\n", mWrState, Stringify(aBounds).c_str(),
      aTransform ? Stringify(*aTransform).c_str() : "none");
  wr_dp_push_stacking_context(mWrState, aBounds, aAnimationId, aOpacity,
                              maybeTransform, aTransformStyle, maybePerspective,
                              aMixBlendMode, aFilters.Elements(), aFilters.Length());
}

void
DisplayListBuilder::PopStackingContext()
{
  WRDL_LOG("PopStackingContext\n", mWrState);
  wr_dp_pop_stacking_context(mWrState);
}

wr::WrClipId
DisplayListBuilder::DefineClip(const wr::LayoutRect& aClipRect,
                               const nsTArray<wr::WrComplexClipRegion>* aComplex,
                               const wr::WrImageMask* aMask)
{
  uint64_t clip_id = wr_dp_define_clip(mWrState, aClipRect,
      aComplex ? aComplex->Elements() : nullptr,
      aComplex ? aComplex->Length() : 0,
      aMask);
  WRDL_LOG("DefineClip id=%" PRIu64 " r=%s m=%p b=%s complex=%zu\n", mWrState,
      clip_id, Stringify(aClipRect).c_str(), aMask,
      aMask ? Stringify(aMask->rect).c_str() : "none",
      aComplex ? aComplex->Length() : 0);
  return wr::WrClipId { clip_id };
}

void
DisplayListBuilder::PushClip(const wr::WrClipId& aClipId, bool aRecordInStack)
{
  wr_dp_push_clip(mWrState, aClipId.id);
  WRDL_LOG("PushClip id=%" PRIu64 "\n", mWrState, aClipId.id);
  if (aRecordInStack) {
    mClipIdStack.push_back(aClipId);
  }
}

void
DisplayListBuilder::PopClip(bool aRecordInStack)
{
  WRDL_LOG("PopClip id=%" PRIu64 "\n", mWrState, mClipIdStack.back().id);
  if (aRecordInStack) {
    mClipIdStack.pop_back();
  }
  wr_dp_pop_clip(mWrState);
}

void
DisplayListBuilder::PushBuiltDisplayList(BuiltDisplayList &dl)
{
  WRDL_LOG("PushBuiltDisplayList\n", mWrState);
  wr_dp_push_built_display_list(mWrState,
                                dl.dl_desc,
                                &dl.dl.inner);
}

bool
DisplayListBuilder::IsScrollLayerDefined(layers::FrameMetrics::ViewID aScrollId) const
{
  return mScrollParents.find(aScrollId) != mScrollParents.end();
}

void
DisplayListBuilder::DefineScrollLayer(const layers::FrameMetrics::ViewID& aScrollId,
                                      const wr::LayoutRect& aContentRect,
                                      const wr::LayoutRect& aClipRect)
{
  WRDL_LOG("DefineScrollLayer id=%" PRIu64 " co=%s cl=%s\n", mWrState,
      aScrollId, Stringify(aContentRect).c_str(), Stringify(aClipRect).c_str());

  Maybe<layers::FrameMetrics::ViewID> parent =
      mScrollIdStack.empty() ? Nothing() : Some(mScrollIdStack.back());
  auto it = mScrollParents.insert({aScrollId, parent});
  if (it.second) {
    // An insertion took place, which means we haven't defined aScrollId before.
    // So let's define it now.
    wr_dp_define_scroll_layer(mWrState, aScrollId, aContentRect, aClipRect);
  } else {
    // aScrollId was already a key in mScrollParents so check that the parent
    // value is the same.
    MOZ_ASSERT(it.first->second == parent);
  }
}

void
DisplayListBuilder::PushScrollLayer(const layers::FrameMetrics::ViewID& aScrollId)
{
  WRDL_LOG("PushScrollLayer id=%" PRIu64 "\n", mWrState, aScrollId);
  wr_dp_push_scroll_layer(mWrState, aScrollId);
  mScrollIdStack.push_back(aScrollId);
}

void
DisplayListBuilder::PopScrollLayer()
{
  WRDL_LOG("PopScrollLayer id=%" PRIu64 "\n", mWrState, mScrollIdStack.back());
  mScrollIdStack.pop_back();
  wr_dp_pop_scroll_layer(mWrState);
}

void
DisplayListBuilder::PushClipAndScrollInfo(const layers::FrameMetrics::ViewID& aScrollId,
                                          const wr::WrClipId* aClipId)
{
  WRDL_LOG("PushClipAndScroll s=%" PRIu64 " c=%s\n", mWrState, aScrollId,
      aClipId ? Stringify(aClipId->id).c_str() : "none");
  wr_dp_push_clip_and_scroll_info(mWrState, aScrollId,
      aClipId ? &(aClipId->id) : nullptr);
  mScrollIdStack.push_back(aScrollId);
}

void
DisplayListBuilder::PopClipAndScrollInfo()
{
  WRDL_LOG("PopClipAndScroll\n", mWrState);
  mScrollIdStack.pop_back();
  wr_dp_pop_clip_and_scroll_info(mWrState);
}

void
DisplayListBuilder::PushRect(const wr::LayoutRect& aBounds,
                             const wr::LayoutRect& aClip,
                             const wr::ColorF& aColor)
{
  WRDL_LOG("PushRect b=%s cl=%s c=%s\n", mWrState,
      Stringify(aBounds).c_str(),
      Stringify(aClip).c_str(),
      Stringify(aColor).c_str());
  wr_dp_push_rect(mWrState, aBounds, aClip, aColor);
}

void
DisplayListBuilder::PushLinearGradient(const wr::LayoutRect& aBounds,
                                       const wr::LayoutRect& aClip,
                                       const wr::LayoutPoint& aStartPoint,
                                       const wr::LayoutPoint& aEndPoint,
                                       const nsTArray<wr::GradientStop>& aStops,
                                       wr::ExtendMode aExtendMode,
                                       const wr::LayoutSize aTileSize,
                                       const wr::LayoutSize aTileSpacing)
{
  wr_dp_push_linear_gradient(mWrState,
                             aBounds, aClip,
                             aStartPoint, aEndPoint,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode,
                             aTileSize, aTileSpacing);
}

void
DisplayListBuilder::PushRadialGradient(const wr::LayoutRect& aBounds,
                                       const wr::LayoutRect& aClip,
                                       const wr::LayoutPoint& aCenter,
                                       const wr::LayoutSize& aRadius,
                                       const nsTArray<wr::GradientStop>& aStops,
                                       wr::ExtendMode aExtendMode,
                                       const wr::LayoutSize aTileSize,
                                       const wr::LayoutSize aTileSpacing)
{
  wr_dp_push_radial_gradient(mWrState,
                             aBounds, aClip,
                             aCenter, aRadius,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode,
                             aTileSize, aTileSpacing);
}

void
DisplayListBuilder::PushImage(const wr::LayoutRect& aBounds,
                              const wr::LayoutRect& aClip,
                              wr::ImageRendering aFilter,
                              wr::ImageKey aImage)
{
  wr::LayoutSize size;
  size.width = aBounds.size.width;
  size.height = aBounds.size.height;
  PushImage(aBounds, aClip, size, size, aFilter, aImage);
}

void
DisplayListBuilder::PushImage(const wr::LayoutRect& aBounds,
                              const wr::LayoutRect& aClip,
                              const wr::LayoutSize& aStretchSize,
                              const wr::LayoutSize& aTileSpacing,
                              wr::ImageRendering aFilter,
                              wr::ImageKey aImage)
{
  WRDL_LOG("PushImage b=%s cl=%s s=%s t=%s\n", mWrState,
      Stringify(aBounds).c_str(),
      Stringify(aClip).c_str(), Stringify(aStretchSize).c_str(),
      Stringify(aTileSpacing).c_str());
  wr_dp_push_image(mWrState, aBounds, aClip, aStretchSize, aTileSpacing, aFilter, aImage);
}

void
DisplayListBuilder::PushYCbCrPlanarImage(const wr::LayoutRect& aBounds,
                                         const wr::LayoutRect& aClip,
                                         wr::ImageKey aImageChannel0,
                                         wr::ImageKey aImageChannel1,
                                         wr::ImageKey aImageChannel2,
                                         wr::WrYuvColorSpace aColorSpace,
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
DisplayListBuilder::PushNV12Image(const wr::LayoutRect& aBounds,
                                  const wr::LayoutRect& aClip,
                                  wr::ImageKey aImageChannel0,
                                  wr::ImageKey aImageChannel1,
                                  wr::WrYuvColorSpace aColorSpace,
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
DisplayListBuilder::PushYCbCrInterleavedImage(const wr::LayoutRect& aBounds,
                                              const wr::LayoutRect& aClip,
                                              wr::ImageKey aImageChannel0,
                                              wr::WrYuvColorSpace aColorSpace,
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
DisplayListBuilder::PushIFrame(const wr::LayoutRect& aBounds,
                               PipelineId aPipeline)
{
  wr_dp_push_iframe(mWrState, aBounds, aPipeline);
}

void
DisplayListBuilder::PushBorder(const wr::LayoutRect& aBounds,
                               const wr::LayoutRect& aClip,
                               const wr::BorderWidths& aWidths,
                               const Range<const wr::BorderSide>& aSides,
                               const wr::BorderRadius& aRadius)
{
  MOZ_ASSERT(aSides.length() == 4);
  if (aSides.length() != 4) {
    return;
  }
  wr_dp_push_border(mWrState, aBounds, aClip,
                    aWidths, aSides[0], aSides[1], aSides[2], aSides[3], aRadius);
}

void
DisplayListBuilder::PushBorderImage(const wr::LayoutRect& aBounds,
                                    const wr::LayoutRect& aClip,
                                    const wr::BorderWidths& aWidths,
                                    wr::ImageKey aImage,
                                    const wr::NinePatchDescriptor& aPatch,
                                    const wr::SideOffsets2D_f32& aOutset,
                                    const wr::RepeatMode& aRepeatHorizontal,
                                    const wr::RepeatMode& aRepeatVertical)
{
  wr_dp_push_border_image(mWrState, aBounds, aClip,
                          aWidths, aImage, aPatch, aOutset,
                          aRepeatHorizontal, aRepeatVertical);
}

void
DisplayListBuilder::PushBorderGradient(const wr::LayoutRect& aBounds,
                                       const wr::LayoutRect& aClip,
                                       const wr::BorderWidths& aWidths,
                                       const wr::LayoutPoint& aStartPoint,
                                       const wr::LayoutPoint& aEndPoint,
                                       const nsTArray<wr::GradientStop>& aStops,
                                       wr::ExtendMode aExtendMode,
                                       const wr::SideOffsets2D_f32& aOutset)
{
  wr_dp_push_border_gradient(mWrState, aBounds, aClip,
                             aWidths, aStartPoint, aEndPoint,
                             aStops.Elements(), aStops.Length(),
                             aExtendMode, aOutset);
}

void
DisplayListBuilder::PushBorderRadialGradient(const wr::LayoutRect& aBounds,
                                             const wr::LayoutRect& aClip,
                                             const wr::BorderWidths& aWidths,
                                             const wr::LayoutPoint& aCenter,
                                             const wr::LayoutSize& aRadius,
                                             const nsTArray<wr::GradientStop>& aStops,
                                             wr::ExtendMode aExtendMode,
                                             const wr::SideOffsets2D_f32& aOutset)
{
  wr_dp_push_border_radial_gradient(
    mWrState, aBounds, aClip, aWidths, aCenter,
    aRadius, aStops.Elements(), aStops.Length(),
    aExtendMode, aOutset);
}

void
DisplayListBuilder::PushText(const wr::LayoutRect& aBounds,
                             const wr::LayoutRect& aClip,
                             const gfx::Color& aColor,
                             wr::FontInstanceKey aFontKey,
                             Range<const wr::GlyphInstance> aGlyphBuffer,
                             const wr::GlyphOptions* aGlyphOptions)
{
  wr_dp_push_text(mWrState, aBounds, aClip,
                  ToColorF(aColor),
                  aFontKey,
                  &aGlyphBuffer[0], aGlyphBuffer.length(),
                  aGlyphOptions);
}

void
DisplayListBuilder::PushLine(const wr::LayoutRect& aClip,
                             const wr::Line& aLine)
{
 wr_dp_push_line(mWrState, aClip, aLine.baseline, aLine.start, aLine.end,
                 aLine.orientation, aLine.width, aLine.color, aLine.style);

/* TODO(Gankro): remove this
  LayoutRect rect;
  if (aLine.orientation == wr::LineOrientation::Horizontal) {
    rect.origin.x = aLine.start;
    rect.origin.y = aLine.baseline;
    rect.size.width = aLine.end - aLine.start;
    rect.size.height = aLine.width;
  } else {
    rect.origin.x = aLine.baseline;
    rect.origin.y = aLine.start;
    rect.size.width = aLine.width;
    rect.size.height = aLine.end - aLine.start;
  }

  PushRect(rect, aClip, aLine.color);
*/
}

void
DisplayListBuilder::PushTextShadow(const wr::LayoutRect& aRect,
                                   const wr::LayoutRect& aClip,
                                   const wr::TextShadow& aShadow)
{
  wr_dp_push_text_shadow(mWrState, aRect, aClip, aShadow);
}

void
DisplayListBuilder::PopTextShadow()
{
  wr_dp_pop_text_shadow(mWrState);
}

void
DisplayListBuilder::PushBoxShadow(const wr::LayoutRect& aRect,
                                  const wr::LayoutRect& aClip,
                                  const wr::LayoutRect& aBoxBounds,
                                  const wr::LayoutVector2D& aOffset,
                                  const wr::ColorF& aColor,
                                  const float& aBlurRadius,
                                  const float& aSpreadRadius,
                                  const float& aBorderRadius,
                                  const wr::BoxShadowClipMode& aClipMode)
{
  wr_dp_push_box_shadow(mWrState, aRect, aClip,
                        aBoxBounds, aOffset, aColor,
                        aBlurRadius, aSpreadRadius, aBorderRadius,
                        aClipMode);
}

Maybe<wr::WrClipId>
DisplayListBuilder::TopmostClipId()
{
  if (mClipIdStack.empty()) {
    return Nothing();
  }
  return Some(mClipIdStack.back());
}

layers::FrameMetrics::ViewID
DisplayListBuilder::TopmostScrollId()
{
  if (mScrollIdStack.empty()) {
    return layers::FrameMetrics::NULL_SCROLL_ID;
  }
  return mScrollIdStack.back();
}

Maybe<layers::FrameMetrics::ViewID>
DisplayListBuilder::ParentScrollIdFor(layers::FrameMetrics::ViewID aScrollId)
{
  auto it = mScrollParents.find(aScrollId);
  return (it == mScrollParents.end() ? Nothing() : it->second);
}

} // namespace wr
} // namespace mozilla
