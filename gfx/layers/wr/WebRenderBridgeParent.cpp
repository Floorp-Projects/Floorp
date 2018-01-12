/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeParent.h"

#include "apz/src/AsyncPanZoomController.h"
#include "CompositableHost.h"
#include "gfxEnv.h"
#include "gfxPrefs.h"
#include "gfxEnv.h"
#include "GeckoProfiler.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/Range.h"
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/SharedSurfacesParent.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/CompositorWidget.h"

bool is_in_main_thread()
{
  return NS_IsMainThread();
}

bool is_in_compositor_thread()
{
  return mozilla::layers::CompositorThreadHolder::IsInCompositorThread();
}

bool is_in_render_thread()
{
  return mozilla::wr::RenderThread::IsInRenderThread();
}

bool is_glcontext_egl(void* glcontext_ptr)
{
  MOZ_ASSERT(glcontext_ptr);

  mozilla::gl::GLContext* glcontext = reinterpret_cast<mozilla::gl::GLContext*>(glcontext_ptr);
  if (!glcontext) {
    return false;
  }
  return glcontext->GetContextType() == mozilla::gl::GLContextType::EGL;
}

bool is_glcontext_angle(void* glcontext_ptr)
{
  MOZ_ASSERT(glcontext_ptr);

  mozilla::gl::GLContext* glcontext = reinterpret_cast<mozilla::gl::GLContext*>(glcontext_ptr);
  if (!glcontext) {
    return false;
  }
  return glcontext->IsANGLE();
}

bool gfx_use_wrench()
{
  return gfxEnv::EnableWebRenderRecording();
}

const char* gfx_wr_resource_path_override()
{
  const char* resourcePath = PR_GetEnv("WR_RESOURCE_PATH");
  if (!resourcePath || resourcePath[0] == '\0') {
    return nullptr;
  }
  return resourcePath;
}

void gfx_critical_note(const char* msg)
{
  gfxCriticalNote << msg;
}

void gfx_critical_error(const char* msg)
{
  gfxCriticalError() << msg;
}

void gecko_printf_stderr_output(const char* msg)
{
  printf_stderr("%s\n", msg);
}

void* get_proc_address_from_glcontext(void* glcontext_ptr, const char* procname)
{
  MOZ_ASSERT(glcontext_ptr);

  mozilla::gl::GLContext* glcontext = reinterpret_cast<mozilla::gl::GLContext*>(glcontext_ptr);
  if (!glcontext) {
    return nullptr;
  }
  PRFuncPtr p = glcontext->LookupSymbol(procname);
  return reinterpret_cast<void*>(p);
}

void
gecko_profiler_register_thread(const char* name)
{
  PROFILER_REGISTER_THREAD(name);
}

void
gecko_profiler_unregister_thread()
{
  PROFILER_UNREGISTER_THREAD();
}

namespace mozilla {

namespace layers {

using namespace mozilla::gfx;

class MOZ_STACK_CLASS AutoWebRenderBridgeParentAsyncMessageSender
{
public:
  explicit AutoWebRenderBridgeParentAsyncMessageSender(WebRenderBridgeParent* aWebRenderBridgeParent,
                                                       InfallibleTArray<OpDestroy>* aDestroyActors = nullptr)
    : mWebRenderBridgeParent(aWebRenderBridgeParent)
    , mActorsToDestroy(aDestroyActors)
  {
    mWebRenderBridgeParent->SetAboutToSendAsyncMessages();
  }

  ~AutoWebRenderBridgeParentAsyncMessageSender()
  {
    mWebRenderBridgeParent->SendPendingAsyncMessages();
    if (mActorsToDestroy) {
      // Destroy the actors after sending the async messages because the latter may contain
      // references to some actors.
      for (const auto& op : *mActorsToDestroy) {
        mWebRenderBridgeParent->DestroyActor(op);
      }
    }
  }
private:
  WebRenderBridgeParent* mWebRenderBridgeParent;
  InfallibleTArray<OpDestroy>* mActorsToDestroy;
};

WebRenderBridgeParent::WebRenderBridgeParent(CompositorBridgeParentBase* aCompositorBridge,
                                             const wr::PipelineId& aPipelineId,
                                             widget::CompositorWidget* aWidget,
                                             CompositorVsyncScheduler* aScheduler,
                                             RefPtr<wr::WebRenderAPI>&& aApi,
                                             RefPtr<AsyncImagePipelineManager>&& aImageMgr,
                                             RefPtr<CompositorAnimationStorage>&& aAnimStorage)
  : mCompositorBridge(aCompositorBridge)
  , mPipelineId(aPipelineId)
  , mWidget(aWidget)
  , mApi(aApi)
  , mAsyncImageManager(aImageMgr)
  , mCompositorScheduler(aScheduler)
  , mAnimStorage(aAnimStorage)
  , mChildLayerObserverEpoch(0)
  , mParentLayerObserverEpoch(0)
  , mWrEpoch(0)
  , mIdNamespace(aApi->GetNamespace())
  , mPaused(false)
  , mDestroyed(false)
  , mForceRendering(false)
{
  MOZ_ASSERT(mAsyncImageManager);
  MOZ_ASSERT(mAnimStorage);
  mAsyncImageManager->AddPipeline(mPipelineId);
  if (mWidget) {
    MOZ_ASSERT(!mCompositorScheduler);
    mCompositorScheduler = new CompositorVsyncScheduler(this, mWidget);
  }
}

WebRenderBridgeParent::WebRenderBridgeParent(const wr::PipelineId& aPipelineId)
  : mCompositorBridge(nullptr)
  , mPipelineId(aPipelineId)
  , mChildLayerObserverEpoch(0)
  , mParentLayerObserverEpoch(0)
  , mWrEpoch(0)
  , mIdNamespace{0}
  , mPaused(false)
  , mDestroyed(true)
  , mForceRendering(false)
{
}

/* static */ WebRenderBridgeParent*
WebRenderBridgeParent::CreateDestroyed(const wr::PipelineId& aPipelineId)
{
  return new WebRenderBridgeParent(aPipelineId);
}

WebRenderBridgeParent::~WebRenderBridgeParent()
{
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvCreate(const gfx::IntSize& aSize)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  MOZ_ASSERT(mApi);

#ifdef MOZ_WIDGET_ANDROID
  // XXX temporary hack.
  // XXX Remove it when APZ is supported.
  // XXX Broken by Dynamic Toolbar v3. See: Bug 1335895
//  RefPtr<UiCompositorControllerParent> uiController = UiCompositorControllerParent::GetFromRootLayerTreeId(/* Root Layer Tree ID */);
//  if (uiController) {
//    uiController->ToolbarAnimatorMessageFromCompositor(/*FIRST_PAINT*/ 5);
//  }
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvShutdown()
{
  return HandleShutdown();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvShutdownSync()
{
  return HandleShutdown();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::HandleShutdown()
{
  Destroy();
  IProtocol* mgr = Manager();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

void
WebRenderBridgeParent::Destroy()
{
  if (mDestroyed) {
    return;
  }
  mDestroyed = true;
  ClearResources();
}

bool
WebRenderBridgeParent::UpdateResources(const nsTArray<OpUpdateResource>& aResourceUpdates,
                                       const nsTArray<RefCountedShmem>& aSmallShmems,
                                       const nsTArray<ipc::Shmem>& aLargeShmems,
                                       wr::ResourceUpdateQueue& aUpdates)
{
  wr::ShmSegmentsReader reader(aSmallShmems, aLargeShmems);

  for (const auto& cmd : aResourceUpdates) {
    switch (cmd.type()) {
      case OpUpdateResource::TOpAddImage: {
        const auto& op = cmd.get_OpAddImage();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.AddImage(op.key(), op.descriptor(), bytes);
        break;
      }
      case OpUpdateResource::TOpUpdateImage: {
        const auto& op = cmd.get_OpUpdateImage();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.UpdateImageBuffer(op.key(), op.descriptor(), bytes);
        break;
      }
      case OpUpdateResource::TOpAddBlobImage: {
        const auto& op = cmd.get_OpAddBlobImage();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.AddBlobImage(op.key(), op.descriptor(), bytes);
        break;
      }
      case OpUpdateResource::TOpUpdateBlobImage: {
        const auto& op = cmd.get_OpUpdateBlobImage();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.UpdateBlobImage(op.key(), op.descriptor(), bytes, wr::ToDeviceUintRect(op.dirtyRect()));
        break;
      }
      case OpUpdateResource::TOpAddExternalImage: {
        const auto& op = cmd.get_OpAddExternalImage();
        if (!AddExternalImage(op.externalImageId(), op.key(), aUpdates)) {
          return false;
        }
        break;
      }
      case OpUpdateResource::TOpAddRawFont: {
        const auto& op = cmd.get_OpAddRawFont();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.AddRawFont(op.key(), bytes, op.fontIndex());
        break;
      }
      case OpUpdateResource::TOpAddFontDescriptor: {
        const auto& op = cmd.get_OpAddFontDescriptor();
        wr::Vec<uint8_t> bytes;
        if (!reader.Read(op.bytes(), bytes)) {
          return false;
        }
        aUpdates.AddFontDescriptor(op.key(), bytes, op.fontIndex());
        break;
      }
      case OpUpdateResource::TOpAddFontInstance: {
        const auto& op = cmd.get_OpAddFontInstance();
        wr::Vec<uint8_t> variations;
        if (!reader.Read(op.variations(), variations)) {
            return false;
        }
        aUpdates.AddFontInstance(op.instanceKey(), op.fontKey(),
                                 op.glyphSize(),
                                 op.options().ptrOr(nullptr),
                                 op.platformOptions().ptrOr(nullptr),
                                 variations);
        break;
      }
      case OpUpdateResource::TOpDeleteImage: {
        const auto& op = cmd.get_OpDeleteImage();
        aUpdates.DeleteImage(op.key());
        break;
      }
      case OpUpdateResource::TOpDeleteFont: {
        const auto& op = cmd.get_OpDeleteFont();
        aUpdates.DeleteFont(op.key());
        break;
      }
      case OpUpdateResource::TOpDeleteFontInstance: {
        const auto& op = cmd.get_OpDeleteFontInstance();
        aUpdates.DeleteFontInstance(op.key());
        break;
      }
      case OpUpdateResource::T__None: break;
    }
  }

  return true;
}

bool
WebRenderBridgeParent::AddExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey,
                                        wr::ResourceUpdateQueue& aResources)
{
  Range<wr::ImageKey> keys(&aKey, 1);
  // Check if key is obsoleted.
  if (keys[0].mNamespace != mIdNamespace) {
    return true;
  }

  RefPtr<DataSourceSurface> dSurf = SharedSurfacesParent::Get(aExtId);
  if (dSurf) {
    if (!gfxEnv::EnableWebRenderRecording()) {
      wr::ImageDescriptor descriptor(dSurf->GetSize(), dSurf->Stride(),
                                     dSurf->GetFormat());
      aResources.AddExternalImage(aKey, descriptor, aExtId,
                                  wr::WrExternalImageBufferType::ExternalBuffer,
                                  0);
      return true;
    }
  } else {
    MOZ_ASSERT(mExternalImageIds.Get(wr::AsUint64(aExtId)).get());

    RefPtr<WebRenderImageHost> host = mExternalImageIds.Get(wr::AsUint64(aExtId));
    if (!host) {
      gfxCriticalNote << "CompositableHost does not exist for extId:" << wr::AsUint64(aExtId);
      return false;
    }
    if (!gfxEnv::EnableWebRenderRecording()) {
      TextureHost* texture = host->GetAsTextureHostForComposite();
      if (!texture) {
        gfxCriticalNote << "TextureHost does not exist for extId:" << wr::AsUint64(aExtId);
        return false;
      }
      WebRenderTextureHost* wrTexture = texture->AsWebRenderTextureHost();
      if (wrTexture) {
        wrTexture->PushResourceUpdates(aResources, TextureHost::ADD_IMAGE, keys,
                                       wrTexture->GetExternalImageKey());
        return true;
      }
    }
    dSurf = host->GetAsSurface();
  }

  if (!dSurf) {
    gfxCriticalNote << "TextureHost does not return DataSourceSurface for extId:" << wr::AsUint64(aExtId);
    return false;
  }

  DataSourceSurface::MappedSurface map;
  if (!dSurf->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
    gfxCriticalNote << "DataSourceSurface failed to map for Image for extId:" << wr::AsUint64(aExtId);
    return false;
  }

  IntSize size = dSurf->GetSize();
  wr::ImageDescriptor descriptor(size, map.mStride, dSurf->GetFormat());
  wr::Vec<uint8_t> data;
  data.PushBytes(Range<uint8_t>(map.mData, size.height * map.mStride));
  aResources.AddImage(keys[0], descriptor, data);
  dSurf->Unmap();

  return true;
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvUpdateResources(nsTArray<OpUpdateResource>&& aResourceUpdates,
                                           nsTArray<RefCountedShmem>&& aSmallShmems,
                                           nsTArray<ipc::Shmem>&& aLargeShmems)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  wr::ResourceUpdateQueue updates;

  if (!UpdateResources(aResourceUpdates, aSmallShmems, aLargeShmems, updates)) {
    wr::IpcResourceUpdateQueue::ReleaseShmems(this, aSmallShmems);
    wr::IpcResourceUpdateQueue::ReleaseShmems(this, aLargeShmems);
    IPC_FAIL(this, "Invalid WebRender resource data shmem or address.");
  }

  wr::TransactionBuilder txn;
  txn.UpdateResources(updates);
  mApi->SendTransaction(txn);

  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aSmallShmems);
  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aLargeShmems);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDeleteCompositorAnimations(InfallibleTArray<uint64_t>&& aIds)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  for (uint32_t i = 0; i < aIds.Length(); i++) {
    if (mActiveAnimations.erase(aIds[i]) > 0) {
      mAnimStorage->ClearById(aIds[i]);
    } else {
      NS_ERROR("Tried to delete invalid animation");
    }
  }

  return IPC_OK();
}

CompositorBridgeParent*
WebRenderBridgeParent::GetRootCompositorBridgeParent() const
{
  if (!mCompositorBridge) {
    return nullptr;
  }

  if (mWidget) {
    // This WebRenderBridgeParent is attached to the root
    // CompositorBridgeParent.
    return static_cast<CompositorBridgeParent*>(mCompositorBridge);
  }

  // Otherwise, this WebRenderBridgeParent is attached to a
  // CrossProcessCompositorBridgeParent so we have an extra level of
  // indirection to unravel.
  CompositorBridgeParent::LayerTreeState* lts =
      CompositorBridgeParent::GetIndirectShadowTree(GetLayersId());
  if (!lts) {
    return nullptr;
  }
  return lts->mParent;
}

void
WebRenderBridgeParent::UpdateAPZ(bool aUpdateHitTestingTree)
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return;
  }
  uint64_t rootLayersId = cbp->RootLayerTreeId();
  RefPtr<WebRenderBridgeParent> rootWrbp = cbp->GetWebRenderBridgeParent();
  if (!rootWrbp) {
    return;
  }
  if (RefPtr<APZCTreeManager> apzc = cbp->GetAPZCTreeManager()) {
    apzc->UpdateFocusState(rootLayersId, GetLayersId(),
                           mScrollData.GetFocusTarget());
    if (aUpdateHitTestingTree) {
      apzc->UpdateHitTestingTree(rootLayersId, rootWrbp->GetScrollData(),
          mScrollData.IsFirstPaint(), GetLayersId(),
          mScrollData.GetPaintSequenceNumber());
    }
  }
}

bool
WebRenderBridgeParent::PushAPZStateToWR(nsTArray<wr::WrTransformProperty>& aTransformArray)
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return false;
  }
  if (RefPtr<APZCTreeManager> apzc = cbp->GetAPZCTreeManager()) {
    TimeStamp animationTime = cbp->GetTestingTimeStamp().valueOr(
        mCompositorScheduler->GetLastComposeTime());
    TimeDuration frameInterval = cbp->GetVsyncInterval();
    // As with the non-webrender codepath in AsyncCompositionManager, we want to
    // use the timestamp for the next vsync when advancing animations.
    if (frameInterval != TimeDuration::Forever()) {
      animationTime += frameInterval;
    }
    return apzc->PushStateToWR(mApi, animationTime, aTransformArray);
  }
  return false;
}

const WebRenderScrollData&
WebRenderBridgeParent::GetScrollData() const
{
  MOZ_ASSERT(mozilla::layers::CompositorThreadHolder::IsInCompositorThread());
  return mScrollData;
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetDisplayList(const gfx::IntSize& aSize,
                                          InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                          InfallibleTArray<OpDestroy>&& aToDestroy,
                                          const uint64_t& aFwdTransactionId,
                                          const uint64_t& aTransactionId,
                                          const wr::LayoutSize& aContentSize,
                                          ipc::ByteBuf&& dl,
                                          const wr::BuiltDisplayListDescriptor& dlDesc,
                                          const WebRenderScrollData& aScrollData,
                                          nsTArray<OpUpdateResource>&& aResourceUpdates,
                                          nsTArray<RefCountedShmem>&& aSmallShmems,
                                          nsTArray<ipc::Shmem>&& aLargeShmems,
                                          const wr::IdNamespace& aIdNamespace,
                                          const TimeStamp& aTxnStartTime,
                                          const TimeStamp& aFwdTime)
{
  if (mDestroyed) {
    for (const auto& op : aToDestroy) {
      DestroyActor(op);
    }
    return IPC_OK();
  }

  AUTO_PROFILER_TRACING("Paint", "SetDisplayList");
  UpdateFwdTransactionId(aFwdTransactionId);
  AutoClearReadLocks clearLocks(mReadLocks);

  // This ensures that destroy operations are always processed. It is not safe
  // to early-return from RecvDPEnd without doing so.
  AutoWebRenderBridgeParentAsyncMessageSender autoAsyncMessageSender(this, &aToDestroy);

  uint32_t wrEpoch = GetNextWrEpoch();

  mAsyncImageManager->SetCompositionTime(TimeStamp::Now());

  ProcessWebRenderParentCommands(aCommands);

  wr::ResourceUpdateQueue resources;
  if (!UpdateResources(aResourceUpdates, aSmallShmems, aLargeShmems, resources)) {
    return IPC_FAIL(this, "Failed to deserialize resource updates");
  }

  wr::TransactionBuilder txn;
  txn.UpdateResources(resources);

  wr::Vec<uint8_t> dlData(Move(dl));

  // If id namespaces do not match, it means the command is obsolete, probably
  // because the tab just moved to a new window.
  // In that case do not send the commands to webrender.
  if (mIdNamespace == aIdNamespace) {
    if (mWidget) {
      LayoutDeviceIntSize size = mWidget->GetClientSize();
      txn.SetWindowParameters(size);
    }
    gfx::Color clearColor(0.f, 0.f, 0.f, 0.f);
    txn.SetDisplayList(clearColor, wr::NewEpoch(wrEpoch), LayerSize(aSize.width, aSize.height),
                       mPipelineId, aContentSize,
                       dlDesc, dlData);

    mApi->SendTransaction(txn);

    ScheduleGenerateFrame();

    if (ShouldParentObserveEpoch()) {
      mCompositorBridge->ObserveLayerUpdate(GetLayersId(), GetChildLayerObserverEpoch(), true);
    }
  }

  HoldPendingTransactionId(wrEpoch, aTransactionId, aTxnStartTime, aFwdTime);

  mScrollData = aScrollData;
  UpdateAPZ(true);

  if (mIdNamespace != aIdNamespace) {
    // Pretend we composited since someone is wating for this event,
    // though DisplayList was not pushed to webrender.
    TimeStamp now = TimeStamp::Now();
    mCompositorBridge->DidComposite(wr::AsUint64(mPipelineId), now, now);
  }

  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aSmallShmems);
  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aLargeShmems);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvEmptyTransaction(const FocusTarget& aFocusTarget,
                                            InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                            InfallibleTArray<OpDestroy>&& aToDestroy,
                                            const uint64_t& aFwdTransactionId,
                                            const uint64_t& aTransactionId,
                                            const wr::IdNamespace& aIdNamespace,
                                            const TimeStamp& aTxnStartTime,
                                            const TimeStamp& aFwdTime)
{
  if (mDestroyed) {
    for (const auto& op : aToDestroy) {
      DestroyActor(op);
    }
    return IPC_OK();
  }

  AUTO_PROFILER_TRACING("Paint", "EmptyTransaction");
  UpdateFwdTransactionId(aFwdTransactionId);
  AutoClearReadLocks clearLocks(mReadLocks);

  // This ensures that destroy operations are always processed. It is not safe
  // to early-return without doing so.
  AutoWebRenderBridgeParentAsyncMessageSender autoAsyncMessageSender(this, &aToDestroy);

  if (!aCommands.IsEmpty()) {
    mAsyncImageManager->SetCompositionTime(TimeStamp::Now());
    ProcessWebRenderParentCommands(aCommands);
    ScheduleGenerateFrame();
  }

  mScrollData.SetFocusTarget(aFocusTarget);
  UpdateAPZ(false);

  if (!aCommands.IsEmpty()) {
    wr::TransactionBuilder txn;
    uint32_t wrEpoch = GetNextWrEpoch();
    txn.UpdateEpoch(mPipelineId, wr::NewEpoch(wrEpoch));
    mApi->SendTransaction(txn);

    HoldPendingTransactionId(wrEpoch, aTransactionId, aTxnStartTime, aFwdTime);
  } else {
    bool sendDidComposite = false;
    if (mPendingTransactionIds.empty()) {
      sendDidComposite = true;
    }
    HoldPendingTransactionId(mWrEpoch, aTransactionId, aTxnStartTime, aFwdTime);
    // If WebRenderBridgeParent does not have pending DidComposites,
    // send DidComposite now.
    if (sendDidComposite) {
      TimeStamp now = TimeStamp::Now();
      mCompositorBridge->DidComposite(wr::AsUint64(mPipelineId), now, now);
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetFocusTarget(const FocusTarget& aFocusTarget)
{
  mScrollData.SetFocusTarget(aFocusTarget);
  UpdateAPZ(false);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvParentCommands(nsTArray<WebRenderParentCommand>&& aCommands)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  ProcessWebRenderParentCommands(aCommands);
  return IPC_OK();
}

void
WebRenderBridgeParent::ProcessWebRenderParentCommands(const InfallibleTArray<WebRenderParentCommand>& aCommands)
{
  for (InfallibleTArray<WebRenderParentCommand>::index_type i = 0; i < aCommands.Length(); ++i) {
    const WebRenderParentCommand& cmd = aCommands[i];
    switch (cmd.type()) {
      case WebRenderParentCommand::TOpUpdateAsyncImagePipeline: {
        const OpUpdateAsyncImagePipeline& op = cmd.get_OpUpdateAsyncImagePipeline();
        mAsyncImageManager->UpdateAsyncImagePipeline(op.pipelineId(),
                                                      op.scBounds(),
                                                      op.scTransform(),
                                                      op.scaleToSize(),
                                                      op.filter(),
                                                      op.mixBlendMode());
        break;
      }
      case WebRenderParentCommand::TCompositableOperation: {
        if (!ReceiveCompositableUpdate(cmd.get_CompositableOperation())) {
          NS_ERROR("ReceiveCompositableUpdate failed");
        }
        break;
      }
      case WebRenderParentCommand::TOpAddCompositorAnimations: {
        const OpAddCompositorAnimations& op = cmd.get_OpAddCompositorAnimations();
        CompositorAnimations data(Move(op.data()));
        if (data.animations().Length()) {
          mAnimStorage->SetAnimations(data.id(), data.animations());
          mActiveAnimations.insert(data.id());
          // Store the default opacity
          if (op.opacity().type() == OptionalOpacity::Tfloat) {
            mAnimStorage->SetAnimatedValue(data.id(), op.opacity().get_float());
          }
          // Store the default transform
          if (op.transform().type() == OptionalTransform::TMatrix4x4) {
            Matrix4x4 transform(Move(op.transform().get_Matrix4x4()));
            mAnimStorage->SetAnimatedValue(data.id(), Move(transform));
          }
        }
        break;
      }
      default: {
        // other commands are handle on the child
        break;
      }
    }
  }
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvGetSnapshot(PTextureParent* aTexture)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(!mPaused);

  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    // We kill the content process rather than have it continue with an invalid
    // snapshot, that may be too harsh and we could decide to return some sort
    // of error to the child process and let it deal with it...
    return IPC_FAIL_NO_REASON(this);
  }

  // XXX Add other TextureHost supports.
  // Only BufferTextureHost is supported now.
  BufferTextureHost* bufferTexture = texture->AsBufferTextureHost();
  if (!bufferTexture) {
    // We kill the content process rather than have it continue with an invalid
    // snapshot, that may be too harsh and we could decide to return some sort
    // of error to the child process and let it deal with it...
    return IPC_FAIL_NO_REASON(this);
  }

  MOZ_ASSERT(bufferTexture->GetBufferDescriptor().type() == BufferDescriptor::TRGBDescriptor);
  DebugOnly<uint32_t> stride = ImageDataSerializer::GetRGBStride(bufferTexture->GetBufferDescriptor().get_RGBDescriptor());
  uint8_t* buffer = bufferTexture->GetBuffer();
  IntSize size = bufferTexture->GetSize();

  // We only support B8G8R8A8 for now.
  MOZ_ASSERT(buffer);
  MOZ_ASSERT(bufferTexture->GetFormat() == SurfaceFormat::B8G8R8A8);
  uint32_t buffer_size = size.width * size.height * 4;

  // Assert the stride of the buffer is what webrender expects
  MOZ_ASSERT((uint32_t)(size.width * 4) == stride);

  mForceRendering = true;

  if (mCompositorScheduler->NeedsComposite()) {
    mCompositorScheduler->CancelCurrentCompositeTask();
    mCompositorScheduler->ForceComposeToTarget(nullptr, nullptr);
  }

  mApi->Readback(size, buffer, buffer_size);

  mForceRendering = false;

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddPipelineIdForCompositable(const wr::PipelineId& aPipelineId,
                                                        const CompositableHandle& aHandle,
                                                        const bool& aAsync)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  MOZ_ASSERT(!mAsyncCompositables.Get(wr::AsUint64(aPipelineId)).get());

  RefPtr<CompositableHost> host;
  if (aAsync) {
    RefPtr<ImageBridgeParent> imageBridge = ImageBridgeParent::GetInstance(OtherPid());
    if (!imageBridge) {
       return IPC_FAIL_NO_REASON(this);
    }
    host = imageBridge->FindCompositable(aHandle);
  } else {
    host = FindCompositable(aHandle);
  }
  if (!host) {
    return IPC_FAIL_NO_REASON(this);
  }

  WebRenderImageHost* wrHost = host->AsWebRenderImageHost();
  MOZ_ASSERT(wrHost);
  if (!wrHost) {
    gfxCriticalNote << "Incompatible CompositableHost at WebRenderBridgeParent.";
  }

  if (!wrHost) {
    return IPC_OK();
  }

  wrHost->SetWrBridge(this);
  wrHost->EnableUseAsyncImagePipeline();
  mAsyncCompositables.Put(wr::AsUint64(aPipelineId), wrHost);
  mAsyncImageManager->AddAsyncImagePipeline(aPipelineId, wrHost);

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvRemovePipelineIdForCompositable(const wr::PipelineId& aPipelineId)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  WebRenderImageHost* wrHost = mAsyncCompositables.Get(wr::AsUint64(aPipelineId)).get();
  if (!wrHost) {
    return IPC_OK();
  }

  wr::TransactionBuilder txn;

  wrHost->ClearWrBridge();
  mAsyncImageManager->RemoveAsyncImagePipeline(aPipelineId, txn);
  txn.RemovePipeline(aPipelineId);
  mApi->SendTransaction(txn);
  mAsyncCompositables.Remove(wr::AsUint64(aPipelineId));
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddExternalImageIdForCompositable(const ExternalImageId& aImageId,
                                                             const CompositableHandle& aHandle)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(!mExternalImageIds.Get(wr::AsUint64(aImageId)).get());

  RefPtr<CompositableHost> host = FindCompositable(aHandle);
  WebRenderImageHost* wrHost = host->AsWebRenderImageHost();

  MOZ_ASSERT(wrHost);
  if (!wrHost) {
    gfxCriticalNote << "Incompatible CompositableHost for external image at WebRenderBridgeParent.";
  }

  if (!wrHost) {
    return IPC_OK();
  }

  wrHost->SetWrBridge(this);
  mExternalImageIds.Put(wr::AsUint64(aImageId), wrHost);

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvRemoveExternalImageId(const ExternalImageId& aImageId)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  WebRenderImageHost* wrHost = mExternalImageIds.Get(wr::AsUint64(aImageId)).get();
  if (!wrHost) {
    return IPC_OK();
  }

  wrHost->ClearWrBridge();
  mExternalImageIds.Remove(wr::AsUint64(aImageId));

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetLayerObserverEpoch(const uint64_t& aLayerObserverEpoch)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  mChildLayerObserverEpoch = aLayerObserverEpoch;
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvClearCachedResources()
{
  if (mDestroyed) {
    return IPC_OK();
  }
  mCompositorBridge->ObserveLayerUpdate(GetLayersId(), GetChildLayerObserverEpoch(), false);

  // Clear resources
  wr::TransactionBuilder txn;
  txn.ClearDisplayList(wr::NewEpoch(GetNextWrEpoch()), mPipelineId);
  mApi->SendTransaction(txn);
  // Schedule generate frame to clean up Pipeline
  ScheduleGenerateFrame();
  // Remove animations.
  for (std::unordered_set<uint64_t>::iterator iter = mActiveAnimations.begin(); iter != mActiveAnimations.end(); iter++) {
    mAnimStorage->ClearById(*iter);
  }
  mActiveAnimations.clear();
  return IPC_OK();
}

void
WebRenderBridgeParent::UpdateWebRender(CompositorVsyncScheduler* aScheduler,
                                       wr::WebRenderAPI* aApi,
                                       AsyncImagePipelineManager* aImageMgr,
                                       CompositorAnimationStorage* aAnimStorage)
{
  MOZ_ASSERT(!mWidget);
  MOZ_ASSERT(aScheduler);
  MOZ_ASSERT(aApi);
  MOZ_ASSERT(aImageMgr);
  MOZ_ASSERT(aAnimStorage);

  if (mDestroyed) {
    return;
  }

  // Update id name space to identify obsoleted keys.
  // Since usage of invalid keys could cause crash in webrender.
  mIdNamespace = aApi->GetNamespace();
  // XXX Remove it when webrender supports sharing/moving Keys between different webrender instances.
  // XXX It requests client to update/reallocate webrender related resources,
  // but parent side does not wait end of the update.
  // The code could become simpler if we could serialise old keys deallocation and new keys allocation.
  // But we do not do it, it is because client side deallocate old layers/webrender keys
  // after new layers/webrender keys allocation.
  // Without client side's layout refactoring, we could not finish all old layers/webrender keys removals
  // before new layer/webrender keys allocation. In future, we could address the problem.
  Unused << SendWrUpdated(mIdNamespace);
  CompositorBridgeParentBase* cBridge = mCompositorBridge;
  // XXX Stop to clear resources if webreder supports resources sharing between different webrender instances.
  ClearResources();
  mCompositorBridge = cBridge;
  mCompositorScheduler = aScheduler;
  mApi = aApi;
  mAsyncImageManager = aImageMgr;
  mAnimStorage = aAnimStorage;

  Unused << GetNextWrEpoch(); // Update webrender epoch
  // Register pipeline to updated AsyncImageManager.
  mAsyncImageManager->AddPipeline(mPipelineId);
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvForceComposite()
{
  if (mDestroyed) {
    return IPC_OK();
  }
  ScheduleGenerateFrame();
  return IPC_OK();
}

already_AddRefed<AsyncPanZoomController>
WebRenderBridgeParent::GetTargetAPZC(const FrameMetrics::ViewID& aScrollId)
{
  RefPtr<AsyncPanZoomController> apzc;
  if (CompositorBridgeParent* cbp = GetRootCompositorBridgeParent()) {
    if (RefPtr<APZCTreeManager> apzctm = cbp->GetAPZCTreeManager()) {
      apzc = apzctm->GetTargetAPZC(GetLayersId(), aScrollId);
    }
  }
  return apzc.forget();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetConfirmedTargetAPZC(const uint64_t& aBlockId,
                                                  nsTArray<ScrollableLayerGuid>&& aTargets)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  mCompositorBridge->SetConfirmedTargetAPZC(GetLayersId(), aBlockId, aTargets);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetTestSampleTime(const TimeStamp& aTime)
{
  if (!mCompositorBridge->SetTestSampleTime(GetLayersId(), aTime)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvLeaveTestMode()
{
  mCompositorBridge->LeaveTestMode(GetLayersId());
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvGetAnimationOpacity(const uint64_t& aCompositorAnimationsId,
                                               float* aOpacity,
                                               bool* aHasAnimationOpacity)
{
  if (mDestroyed) {
    return IPC_FAIL_NO_REASON(this);
  }

  MOZ_ASSERT(mAnimStorage);
  AdvanceAnimations();

  Maybe<float> opacity = mAnimStorage->GetAnimationOpacity(aCompositorAnimationsId);
  if (opacity) {
    *aOpacity = *opacity;
    *aHasAnimationOpacity = true;
  } else {
    *aHasAnimationOpacity = false;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvGetAnimationTransform(const uint64_t& aCompositorAnimationsId,
                                                 MaybeTransform* aTransform)
{
  if (mDestroyed) {
    return IPC_FAIL_NO_REASON(this);
  }

  MOZ_ASSERT(mAnimStorage);
  AdvanceAnimations();

  Maybe<Matrix4x4> transform = mAnimStorage->GetAnimationTransform(aCompositorAnimationsId);
  if (transform) {
    *aTransform = *transform;
  } else {
    *aTransform = mozilla::void_t();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetAsyncScrollOffset(const FrameMetrics::ViewID& aScrollId,
                                                const float& aX,
                                                const float& aY)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aScrollId);
  if (!apzc) {
    return IPC_FAIL_NO_REASON(this);
  }
  apzc->SetTestAsyncScrollOffset(CSSPoint(aX, aY));
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetAsyncZoom(const FrameMetrics::ViewID& aScrollId,
                                        const float& aZoom)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  RefPtr<AsyncPanZoomController> apzc = GetTargetAPZC(aScrollId);
  if (!apzc) {
    return IPC_FAIL_NO_REASON(this);
  }
  apzc->SetTestAsyncZoom(LayerToParentLayerScale(aZoom));
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvFlushApzRepaints()
{
  if (mDestroyed) {
    return IPC_OK();
  }
  mCompositorBridge->FlushApzRepaints(GetLayersId());
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvGetAPZTestData(APZTestData* aOutData)
{
  mCompositorBridge->GetAPZTestData(GetLayersId(), aOutData);
  return IPC_OK();
}

void
WebRenderBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  Destroy();
}

void
WebRenderBridgeParent::AdvanceAnimations()
{
  TimeStamp animTime = mCompositorScheduler->GetLastComposeTime();
  if (CompositorBridgeParent* cbp = GetRootCompositorBridgeParent()) {
    animTime = cbp->GetTestingTimeStamp().valueOr(animTime);
  }

  AnimationHelper::SampleAnimations(mAnimStorage,
                                    !mPreviousFrameTimeStamp.IsNull() ?
                                    mPreviousFrameTimeStamp : animTime);

  // Reset the previous time stamp if we don't already have any running
  // animations to avoid using the time which is far behind for newly
  // started animations.
  mPreviousFrameTimeStamp =
    mAnimStorage->AnimatedValueCount() ? animTime : TimeStamp();
}

void
WebRenderBridgeParent::SampleAnimations(nsTArray<wr::WrOpacityProperty>& aOpacityArray,
                                        nsTArray<wr::WrTransformProperty>& aTransformArray)
{
  AdvanceAnimations();

  // return the animated data if has
  if (mAnimStorage->AnimatedValueCount()) {
    for(auto iter = mAnimStorage->ConstAnimatedValueTableIter();
        !iter.Done(); iter.Next()) {
      AnimatedValue * value = iter.UserData();
      if (value->mType == AnimatedValue::TRANSFORM) {
        aTransformArray.AppendElement(
          wr::ToWrTransformProperty(iter.Key(), value->mTransform.mTransformInDevSpace));
      } else if (value->mType == AnimatedValue::OPACITY) {
        aOpacityArray.AppendElement(
          wr::ToWrOpacityProperty(iter.Key(), value->mOpacity));
      }
    }
  }
}

void
WebRenderBridgeParent::CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect)
{
  AUTO_PROFILER_TRACING("Paint", "CompositeToTraget");
  if (mPaused) {
    return;
  }

  if (!mForceRendering &&
      wr::RenderThread::Get()->TooManyPendingFrames(mApi->GetId())) {
    // Render thread is busy, try next time.
    mCompositorScheduler->ScheduleComposition();
    return;
  }

  mAsyncImageManager->SetCompositionTime(TimeStamp::Now());
  mAsyncImageManager->ApplyAsyncImages();

  if (!mAsyncImageManager->GetCompositeUntilTime().IsNull()) {
    // Trigger another CompositeToTarget() call because there might be another
    // frame that we want to generate after this one.
    // It will check if we actually want to generate the frame or not.
    mCompositorScheduler->ScheduleComposition();
  }

  if (!mAsyncImageManager->GetAndResetWillGenerateFrame() &&
      !mForceRendering) {
    // Could skip generating frame now.
    return;
  }

  nsTArray<wr::WrOpacityProperty> opacityArray;
  nsTArray<wr::WrTransformProperty> transformArray;

  SampleAnimations(opacityArray, transformArray);
  if (!transformArray.IsEmpty() || !opacityArray.IsEmpty()) {
    ScheduleGenerateFrame();
  }

  if (PushAPZStateToWR(transformArray)) {
    ScheduleGenerateFrame();
  }

  wr::RenderThread::Get()->IncPendingFrameCount(mApi->GetId());

#if defined(ENABLE_FRAME_LATENCY_LOG)
  auto startTime = TimeStamp::Now();
  mApi->SetFrameStartTime(startTime);
#endif

  wr::TransactionBuilder txn;

  if (!transformArray.IsEmpty() || !opacityArray.IsEmpty()) {
    txn.UpdateDynamicProperties(opacityArray, transformArray);
  }

  txn.GenerateFrame();

  mApi->SendTransaction(txn);
}

void
WebRenderBridgeParent::HoldPendingTransactionId(uint32_t aWrEpoch,
                                                uint64_t aTransactionId,
                                                const TimeStamp& aTxnStartTime,
                                                const TimeStamp& aFwdTime)
{
  // The transaction ID might get reset to 1 if the page gets reloaded, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1145295#c41
  // Otherwise, it should be continually increasing.
  MOZ_ASSERT(aTransactionId == 1 || aTransactionId > LastPendingTransactionId());
  // Handle TransactionIdAllocator(RefreshDriver) change.
  if (aTransactionId == 1) {
    FlushPendingTransactionIds();
  }
  mPendingTransactionIds.push(PendingTransactionId(wr::NewEpoch(aWrEpoch), aTransactionId, aTxnStartTime, aFwdTime));
}

uint64_t
WebRenderBridgeParent::LastPendingTransactionId()
{
  uint64_t id = 0;
  if (!mPendingTransactionIds.empty()) {
    id = mPendingTransactionIds.back().mId;
  }
  return id;
}

uint64_t
WebRenderBridgeParent::FlushPendingTransactionIds()
{
  uint64_t id = 0;
  while (!mPendingTransactionIds.empty()) {
    id = mPendingTransactionIds.front().mId;
    mPendingTransactionIds.pop();
  }
  return id;
}

uint64_t
WebRenderBridgeParent::FlushTransactionIdsForEpoch(const wr::Epoch& aEpoch, const TimeStamp& aEndTime)
{
  uint64_t id = 0;
  while (!mPendingTransactionIds.empty()) {
    int64_t diff =
      static_cast<int64_t>(aEpoch.mHandle) - static_cast<int64_t>(mPendingTransactionIds.front().mEpoch.mHandle);
    if (diff < 0) {
      break;
    }
#if defined(ENABLE_FRAME_LATENCY_LOG)
    if (mPendingTransactionIds.front().mTxnStartTime) {
      uint32_t latencyMs = round((aEndTime - mPendingTransactionIds.front().mTxnStartTime).ToMilliseconds());
      printf_stderr("From transaction start to end of generate frame latencyMs %d this %p\n", latencyMs, this);
    }
    if (mPendingTransactionIds.front().mFwdTime) {
      uint32_t latencyMs = round((aEndTime - mPendingTransactionIds.front().mFwdTime).ToMilliseconds());
      printf_stderr("From forwarding transaction to end of generate frame latencyMs %d this %p\n", latencyMs, this);
    }
#endif
    id = mPendingTransactionIds.front().mId;
    mPendingTransactionIds.pop();
  }
  return id;
}

uint64_t
WebRenderBridgeParent::GetLayersId() const
{
  return wr::AsUint64(mPipelineId);
}

void
WebRenderBridgeParent::ScheduleGenerateFrame()
{
  if (mCompositorScheduler) {
    mAsyncImageManager->SetWillGenerateFrame();
    mCompositorScheduler->ScheduleComposition();
  }
}

void
WebRenderBridgeParent::FlushRendering(bool aIsSync)
{
  if (mDestroyed) {
    return;
  }

  if (!mCompositorScheduler->NeedsComposite()) {
    return;
  }

  mForceRendering = true;
  mCompositorScheduler->CancelCurrentCompositeTask();
  mCompositorScheduler->ForceComposeToTarget(nullptr, nullptr);
  if (aIsSync) {
    mApi->WaitFlushed();
  }
  mForceRendering = false;
}

void
WebRenderBridgeParent::Pause()
{
  MOZ_ASSERT(mWidget);
#ifdef MOZ_WIDGET_ANDROID
  if (!mWidget || mDestroyed) {
    return;
  }
  mApi->Pause();
#endif
  mPaused = true;
}

bool
WebRenderBridgeParent::Resume()
{
  MOZ_ASSERT(mWidget);
#ifdef MOZ_WIDGET_ANDROID
  if (!mWidget || mDestroyed) {
    return false;
  }

  if (!mApi->Resume()) {
    return false;
  }
#endif
  mPaused = false;
  return true;
}

void
WebRenderBridgeParent::ClearResources()
{
  if (!mApi) {
    return;
  }

  uint32_t wrEpoch = GetNextWrEpoch();

  wr::TransactionBuilder txn;
  txn.ClearDisplayList(wr::NewEpoch(wrEpoch), mPipelineId);

  // Schedule generate frame to clean up Pipeline
  ScheduleGenerateFrame();
  // WrFontKeys and WrImageKeys are deleted during WebRenderAPI destruction.
  for (auto iter = mExternalImageIds.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->ClearWrBridge();
  }
  mExternalImageIds.Clear();
  for (auto iter = mAsyncCompositables.Iter(); !iter.Done(); iter.Next()) {
    wr::PipelineId pipelineId = wr::AsPipelineId(iter.Key());
    RefPtr<WebRenderImageHost> host = iter.Data();
    host->ClearWrBridge();
    mAsyncImageManager->RemoveAsyncImagePipeline(pipelineId, txn);
  }
  mAsyncCompositables.Clear();

  mAsyncImageManager->RemovePipeline(mPipelineId, wr::NewEpoch(wrEpoch));
  txn.RemovePipeline(mPipelineId);

  mApi->SendTransaction(txn);

  for (std::unordered_set<uint64_t>::iterator iter = mActiveAnimations.begin(); iter != mActiveAnimations.end(); iter++) {
    mAnimStorage->ClearById(*iter);
  }
  mActiveAnimations.clear();

  if (mWidget) {
    mCompositorScheduler->Destroy();
  }
  mAnimStorage = nullptr;
  mCompositorScheduler = nullptr;
  mAsyncImageManager = nullptr;
  mApi = nullptr;
  mCompositorBridge = nullptr;
}

bool
WebRenderBridgeParent::ShouldParentObserveEpoch()
{
  if (mParentLayerObserverEpoch == mChildLayerObserverEpoch) {
    return false;
  }

  mParentLayerObserverEpoch = mChildLayerObserverEpoch;
  return true;
}

void
WebRenderBridgeParent::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

void
WebRenderBridgeParent::SendPendingAsyncMessages()
{
  MOZ_ASSERT(mCompositorBridge);
  mCompositorBridge->SendPendingAsyncMessages();
}

void
WebRenderBridgeParent::SetAboutToSendAsyncMessages()
{
  MOZ_ASSERT(mCompositorBridge);
  mCompositorBridge->SetAboutToSendAsyncMessages();
}

void
WebRenderBridgeParent::NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

base::ProcessId
WebRenderBridgeParent::GetChildProcessId()
{
  return OtherPid();
}

bool
WebRenderBridgeParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvNewCompositable(const CompositableHandle& aHandle,
                                           const TextureInfo& aInfo)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  if (!AddCompositable(aHandle, aInfo, /* aUseWebRender */ true)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvReleaseCompositable(const CompositableHandle& aHandle)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  ReleaseCompositable(aHandle);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvInitReadLocks(ReadLockArray&& aReadLocks)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  if (!AddReadLocks(Move(aReadLocks))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

TextureFactoryIdentifier
WebRenderBridgeParent::GetTextureFactoryIdentifier()
{
  MOZ_ASSERT(mApi);

  return TextureFactoryIdentifier(LayersBackend::LAYERS_WR,
                                  XRE_GetProcessType(),
                                  mApi->GetMaxTextureSize(),
                                  mApi->GetUseANGLE(),
                                  false,
                                  false,
                                  false,
                                  mApi->GetSyncHandle());
}

uint32_t
WebRenderBridgeParent::GetNextWrEpoch()
{
  MOZ_RELEASE_ASSERT(mWrEpoch != UINT32_MAX);
  return ++mWrEpoch;
}

void
WebRenderBridgeParent::ExtractImageCompositeNotifications(nsTArray<ImageCompositeNotificationInfo>* aNotifications)
{
  MOZ_ASSERT(mWidget);
  if (mDestroyed) {
    return;
  }
  mAsyncImageManager->FlushImageNotifications(aNotifications);
}

} // namespace layers
} // namespace mozilla
