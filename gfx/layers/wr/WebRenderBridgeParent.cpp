/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeParent.h"

#include "CompositableHost.h"
#include "gfxEnv.h"
#include "gfxPrefs.h"
#include "gfxEnv.h"
#include "GeckoProfiler.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/Range.h"
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/APZSampler.h"
#include "mozilla/layers/APZUpdater.h"
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
#include "mozilla/Telemetry.h"
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

void
record_telemetry_time(mozilla::wr::TelemetryProbe aProbe, uint64_t aTimeNs)
{
  uint32_t time_ms = (uint32_t)(aTimeNs / 1000000);
  switch (aProbe) {
    case mozilla::wr::TelemetryProbe::SceneBuildTime:
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::WR_SCENEBUILD_TIME, time_ms);
      break;
    case mozilla::wr::TelemetryProbe::SceneSwapTime:
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::WR_SCENESWAP_TIME, time_ms);
      break;
    case mozilla::wr::TelemetryProbe::RenderTime:
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::WR_RENDER_TIME, time_ms);
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }
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
                                             RefPtr<CompositorAnimationStorage>&& aAnimStorage,
                                             TimeDuration aVsyncRate)
  : mCompositorBridge(aCompositorBridge)
  , mPipelineId(aPipelineId)
  , mWidget(aWidget)
  , mApi(aApi)
  , mAsyncImageManager(aImageMgr)
  , mCompositorScheduler(aScheduler)
  , mAnimStorage(aAnimStorage)
  , mVsyncRate(aVsyncRate)
  , mChildLayerObserverEpoch(0)
  , mParentLayerObserverEpoch(0)
  , mWrEpoch{0}
  , mIdNamespace(aApi->GetNamespace())
  , mPaused(false)
  , mDestroyed(false)
  , mForceRendering(false)
  , mReceivedDisplayList(false)
{
  MOZ_ASSERT(mAsyncImageManager);
  MOZ_ASSERT(mAnimStorage);
  mAsyncImageManager->AddPipeline(mPipelineId);
  if (IsRootWebRenderBridgeParent()) {
    MOZ_ASSERT(!mCompositorScheduler);
    mCompositorScheduler = new CompositorVsyncScheduler(this, mWidget);
  }
}

WebRenderBridgeParent::WebRenderBridgeParent(const wr::PipelineId& aPipelineId)
  : mCompositorBridge(nullptr)
  , mPipelineId(aPipelineId)
  , mChildLayerObserverEpoch(0)
  , mParentLayerObserverEpoch(0)
  , mWrEpoch{0}
  , mIdNamespace{0}
  , mPaused(false)
  , mDestroyed(true)
  , mForceRendering(false)
  , mReceivedDisplayList(false)
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
                                       wr::TransactionBuilder& aUpdates)
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
      case OpUpdateResource::TOpAddExternalImageForTexture: {
        const auto& op = cmd.get_OpAddExternalImageForTexture();
        CompositableTextureHostRef texture;
        texture = TextureHost::AsTextureHost(op.textureParent());
        if (!AddExternalImageForTexture(op.externalImageId(), op.key(), texture, aUpdates)) {
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
                                        wr::TransactionBuilder& aResources)
{
  Range<wr::ImageKey> keys(&aKey, 1);
  // Check if key is obsoleted.
  if (keys[0].mNamespace != mIdNamespace) {
    return true;
  }

  RefPtr<DataSourceSurface> dSurf = SharedSurfacesParent::Acquire(aExtId);
  if (dSurf) {
    bool inserted = mSharedSurfaceIds.EnsureInserted(wr::AsUint64(aExtId));
    if (!inserted) {
      // We already have a mapping for this image, so decrement the ownership
      // counter just increased unnecessarily. This can happen when an image is
      // slow to decode and we need to invalidate it by updating its image key.
      SharedSurfacesParent::Release(aExtId);
    }

    if (!gfxEnv::EnableWebRenderRecording()) {
      wr::ImageDescriptor descriptor(dSurf->GetSize(), dSurf->Stride(),
                                     dSurf->GetFormat());
      aResources.AddExternalImage(aKey, descriptor, aExtId,
                                  wr::WrExternalImageBufferType::ExternalBuffer,
                                  0);
      return true;
    }
  } else {
    gfxCriticalNote << "DataSourceSurface of SharedSurfaces does not exist for extId:" << wr::AsUint64(aExtId);
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

bool
WebRenderBridgeParent::AddExternalImageForTexture(wr::ExternalImageId aExtId,
                                                  wr::ImageKey aKey,
                                                  TextureHost* aTexture,
                                                  wr::TransactionBuilder& aResources)
{
  Range<wr::ImageKey> keys(&aKey, 1);
  // Check if key is obsoleted.
  if (keys[0].mNamespace != mIdNamespace) {
    return true;
  }

  if(!aTexture) {
    gfxCriticalNote << "TextureHost does not exist for extId:" << wr::AsUint64(aExtId);
    return false;
  }

  if (!gfxEnv::EnableWebRenderRecording()) {
    WebRenderTextureHost* wrTexture = aTexture->AsWebRenderTextureHost();
    if (wrTexture) {
      wrTexture->PushResourceUpdates(aResources, TextureHost::ADD_IMAGE, keys,
                                     wrTexture->GetExternalImageKey());
      MOZ_ASSERT(!mTextureHosts.Get(wr::AsUint64(aKey), nullptr));
      mTextureHosts.Put(wr::AsUint64(aKey), CompositableTextureHostRef(aTexture));
      return true;
    }
  }
  RefPtr<DataSourceSurface> dSurf = aTexture->GetAsSurface();
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

  wr::TransactionBuilder txn;

  if (!UpdateResources(aResourceUpdates, aSmallShmems, aLargeShmems, txn)) {
    wr::IpcResourceUpdateQueue::ReleaseShmems(this, aSmallShmems);
    wr::IpcResourceUpdateQueue::ReleaseShmems(this, aLargeShmems);
    IPC_FAIL(this, "Invalid WebRender resource data shmem or address.");
  }

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

  // Once mWrEpoch has been rendered, we can delete these compositor animations
  mCompositorAnimationsToDelete.push(CompositorAnimationIdsForEpoch(mWrEpoch, std::move(aIds)));
  return IPC_OK();
}

void
WebRenderBridgeParent::RemoveEpochDataPriorTo(const wr::Epoch& aRenderedEpoch)
{
  while (!mCompositorAnimationsToDelete.empty()) {
    if (mCompositorAnimationsToDelete.front().mEpoch.mHandle > aRenderedEpoch.mHandle) {
      break;
    }
    for (uint64_t id : mCompositorAnimationsToDelete.front().mIds) {
      if (mActiveAnimations.erase(id) > 0) {
        mAnimStorage->ClearById(id);
      } else {
        NS_ERROR("Tried to delete invalid animation");
      }
    }
    mCompositorAnimationsToDelete.pop();
  }
}

bool
WebRenderBridgeParent::IsRootWebRenderBridgeParent() const
{
  return !!mWidget;
}

CompositorBridgeParent*
WebRenderBridgeParent::GetRootCompositorBridgeParent() const
{
  if (!mCompositorBridge) {
    return nullptr;
  }

  if (IsRootWebRenderBridgeParent()) {
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

RefPtr<WebRenderBridgeParent>
WebRenderBridgeParent::GetRootWebRenderBridgeParent() const
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return nullptr;
  }

  return cbp->GetWebRenderBridgeParent();
}

void
WebRenderBridgeParent::UpdateAPZFocusState(const FocusTarget& aFocus)
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return;
  }
  LayersId rootLayersId = cbp->RootLayerTreeId();
  if (RefPtr<APZUpdater> apz = cbp->GetAPZUpdater()) {
    apz->UpdateFocusState(rootLayersId, GetLayersId(), aFocus);
  }
}

void
WebRenderBridgeParent::UpdateAPZScrollData(const wr::Epoch& aEpoch,
                                           WebRenderScrollData&& aData)
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return;
  }
  LayersId rootLayersId = cbp->RootLayerTreeId();
  if (RefPtr<APZUpdater> apz = cbp->GetAPZUpdater()) {
    apz->UpdateScrollDataAndTreeState(rootLayersId, GetLayersId(), aEpoch, std::move(aData));
  }
}

void
WebRenderBridgeParent::UpdateAPZScrollOffsets(ScrollUpdatesMap&& aUpdates,
                                              uint32_t aPaintSequenceNumber)
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return;
  }
  LayersId rootLayersId = cbp->RootLayerTreeId();
  if (RefPtr<APZUpdater> apz = cbp->GetAPZUpdater()) {
    apz->UpdateScrollOffsets(rootLayersId, GetLayersId(), std::move(aUpdates), aPaintSequenceNumber);
  }
}

void
WebRenderBridgeParent::SetAPZSampleTime()
{
  CompositorBridgeParent* cbp = GetRootCompositorBridgeParent();
  if (!cbp) {
    return;
  }
  if (RefPtr<APZSampler> apz = cbp->GetAPZSampler()) {
    TimeStamp animationTime = cbp->GetTestingTimeStamp().valueOr(
        mCompositorScheduler->GetLastComposeTime());
    TimeDuration frameInterval = cbp->GetVsyncInterval();
    // As with the non-webrender codepath in AsyncCompositionManager, we want to
    // use the timestamp for the next vsync when advancing animations.
    if (frameInterval != TimeDuration::Forever()) {
      animationTime += frameInterval;
    }
    apz->SetSampleTime(animationTime);
  }
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetDisplayList(const gfx::IntSize& aSize,
                                          InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                          InfallibleTArray<OpDestroy>&& aToDestroy,
                                          const uint64_t& aFwdTransactionId,
                                          const TransactionId& aTransactionId,
                                          const wr::LayoutSize& aContentSize,
                                          ipc::ByteBuf&& dl,
                                          const wr::BuiltDisplayListDescriptor& dlDesc,
                                          const WebRenderScrollData& aScrollData,
                                          nsTArray<OpUpdateResource>&& aResourceUpdates,
                                          nsTArray<RefCountedShmem>&& aSmallShmems,
                                          nsTArray<ipc::Shmem>&& aLargeShmems,
                                          const wr::IdNamespace& aIdNamespace,
                                          const TimeStamp& aRefreshStartTime,
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

  // This ensures that destroy operations are always processed. It is not safe
  // to early-return from RecvDPEnd without doing so.
  AutoWebRenderBridgeParentAsyncMessageSender autoAsyncMessageSender(this, &aToDestroy);

  wr::Epoch wrEpoch = GetNextWrEpoch();

  mAsyncImageManager->SetCompositionTime(TimeStamp::Now());

  wr::TransactionBuilder txn;
  wr::AutoTransactionSender sender(mApi, &txn);

  ProcessWebRenderParentCommands(aCommands, txn);

  if (!UpdateResources(aResourceUpdates, aSmallShmems, aLargeShmems, txn)) {
    return IPC_FAIL(this, "Failed to deserialize resource updates");
  }

  mReceivedDisplayList = true;

  // aScrollData is moved into this function but that is not reflected by the
  // function signature due to the way the IPDL generator works. We remove the
  // const so that we can move this structure all the way to the desired
  // destination.
  // Also note that this needs to happen before the display list transaction is
  // sent to WebRender, so that the UpdateHitTestingTree call is guaranteed to
  // be in the updater queue at the time that the scene swap completes.
  UpdateAPZScrollData(wrEpoch, std::move(const_cast<WebRenderScrollData&>(aScrollData)));

  wr::Vec<uint8_t> dlData(std::move(dl));

  // If id namespaces do not match, it means the command is obsolete, probably
  // because the tab just moved to a new window.
  // In that case do not send the commands to webrender.
  if (mIdNamespace == aIdNamespace) {
    if (IsRootWebRenderBridgeParent()) {
      LayoutDeviceIntSize widgetSize = mWidget->GetClientSize();
      LayoutDeviceIntRect docRect(LayoutDeviceIntPoint(), widgetSize);
      txn.SetWindowParameters(widgetSize, docRect);
    }
    gfx::Color clearColor(0.f, 0.f, 0.f, 0.f);
    txn.SetDisplayList(clearColor, wrEpoch, LayerSize(aSize.width, aSize.height),
                       mPipelineId, aContentSize,
                       dlDesc, dlData);
    // The display list that we're sending to WR might contain references to
    // other pipelines that we just added to the async image manager in the
    // ProcessWebRenderParentCommands call above. If we send the display list
    // alone then WR will not yet have the content for those other pipelines
    // and so it will emit errors; the ApplyAsyncImages call below ensure that
    // we provide the pipeline content to WR as part of the same transaction.
    mAsyncImageManager->ApplyAsyncImages(txn);

    mApi->SendTransaction(txn);

    if (!gfxPrefs::WebRenderAsyncSceneBuild()) {
      // With async-scene-build enabled, we will trigger this after the scene
      // build is done, so we don't need to do it here.
      ScheduleGenerateFrame();
    }
  }

  HoldPendingTransactionId(wrEpoch, aTransactionId, aRefreshStartTime, aTxnStartTime, aFwdTime);

  if (mIdNamespace != aIdNamespace) {
    // Pretend we composited since someone is wating for this event,
    // though DisplayList was not pushed to webrender.
    TimeStamp now = TimeStamp::Now();
    mCompositorBridge->DidComposite(GetLayersId(), now, now);
  }

  if (ShouldParentObserveEpoch()) {
    mCompositorBridge->ObserveLayerUpdate(GetLayersId(), GetChildLayerObserverEpoch(), true);
  }

  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aSmallShmems);
  wr::IpcResourceUpdateQueue::ReleaseShmems(this, aLargeShmems);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvEmptyTransaction(const FocusTarget& aFocusTarget,
                                            const ScrollUpdatesMap& aUpdates,
                                            const uint32_t& aPaintSequenceNumber,
                                            InfallibleTArray<WebRenderParentCommand>&& aCommands,
                                            InfallibleTArray<OpDestroy>&& aToDestroy,
                                            const uint64_t& aFwdTransactionId,
                                            const TransactionId& aTransactionId,
                                            const wr::IdNamespace& aIdNamespace,
                                            const TimeStamp& aRefreshStartTime,
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

  // This ensures that destroy operations are always processed. It is not safe
  // to early-return without doing so.
  AutoWebRenderBridgeParentAsyncMessageSender autoAsyncMessageSender(this, &aToDestroy);

  bool scheduleComposite = false;

  UpdateAPZFocusState(aFocusTarget);
  if (!aUpdates.empty()) {
    // aUpdates is moved into this function but that is not reflected by the
    // function signature due to the way the IPDL generator works. We remove the
    // const so that we can move this structure all the way to the desired
    // destination.
    UpdateAPZScrollOffsets(std::move(const_cast<ScrollUpdatesMap&>(aUpdates)), aPaintSequenceNumber);
    scheduleComposite = true;
  }

  if (!aCommands.IsEmpty()) {
    mAsyncImageManager->SetCompositionTime(TimeStamp::Now());
    wr::TransactionBuilder txn;
    wr::Epoch wrEpoch = GetNextWrEpoch();
    txn.UpdateEpoch(mPipelineId, wrEpoch);
    ProcessWebRenderParentCommands(aCommands, txn);
    mApi->SendTransaction(txn);
    scheduleComposite = true;
  }

  bool sendDidComposite = true;
  if (scheduleComposite || !mPendingTransactionIds.empty()) {
    // If we are going to kick off a new composite as a result of this
    // transaction, or if there are already composite-triggering pending
    // transactions inflight, then set sendDidComposite to false because we will
    // send the DidComposite message after the composite occurs.
    // If there are no pending transactions and we're not going to do a
    // composite, then we leave sendDidComposite as true so we just send
    // the DidComposite notification now.
    sendDidComposite = false;
  }

  HoldPendingTransactionId(WrEpoch(), aTransactionId, aRefreshStartTime, aTxnStartTime, aFwdTime);

  if (scheduleComposite) {
    ScheduleGenerateFrame();
  } else if (sendDidComposite) {
    TimeStamp now = TimeStamp::Now();
    mCompositorBridge->DidComposite(GetLayersId(), now, now);
  }

  if (ShouldParentObserveEpoch()) {
    mCompositorBridge->ObserveLayerUpdate(GetLayersId(), GetChildLayerObserverEpoch(), true);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetFocusTarget(const FocusTarget& aFocusTarget)
{
  UpdateAPZFocusState(aFocusTarget);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvParentCommands(nsTArray<WebRenderParentCommand>&& aCommands)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  wr::TransactionBuilder txn;
  ProcessWebRenderParentCommands(aCommands, txn);
  mApi->SendTransaction(txn);
  return IPC_OK();
}

void
WebRenderBridgeParent::ProcessWebRenderParentCommands(const InfallibleTArray<WebRenderParentCommand>& aCommands,
                                                      wr::TransactionBuilder& aTxn)
{
  for (InfallibleTArray<WebRenderParentCommand>::index_type i = 0; i < aCommands.Length(); ++i) {
    const WebRenderParentCommand& cmd = aCommands[i];
    switch (cmd.type()) {
      case WebRenderParentCommand::TOpAddPipelineIdForCompositable: {
        const OpAddPipelineIdForCompositable& op = cmd.get_OpAddPipelineIdForCompositable();
        AddPipelineIdForCompositable(op.pipelineId(),
                                     op.handle(),
                                     op.isAsync());
        break;
      }
      case WebRenderParentCommand::TOpRemovePipelineIdForCompositable: {
        const OpRemovePipelineIdForCompositable& op = cmd.get_OpRemovePipelineIdForCompositable();
        RemovePipelineIdForCompositable(op.pipelineId(), aTxn);
        break;
      }
      case WebRenderParentCommand::TOpRemoveExternalImageId: {
        const OpRemoveExternalImageId& op = cmd.get_OpRemoveExternalImageId();
        RemoveExternalImageId(op.externalImageId());
        break;
      }
      case WebRenderParentCommand::TOpReleaseTextureOfImage: {
        const OpReleaseTextureOfImage& op = cmd.get_OpReleaseTextureOfImage();
        ReleaseTextureOfImage(op.key());
        break;
      }
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
        CompositorAnimations data(std::move(op.data()));
        if (data.animations().Length()) {
          mAnimStorage->SetAnimations(data.id(), data.animations());
          mActiveAnimations.insert(data.id());
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

void
WebRenderBridgeParent::FlushSceneBuilds()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  if (gfxPrefs::WebRenderAsyncSceneBuild()) {
    // If we are sending transactions through the scene builder thread, we need
    // to block until all the inflight transactions have been processed. This
    // flush message blocks until all previously sent scenes have been built
    // and received by the render backend thread.
    mApi->FlushSceneBuilder();
    // The post-swap hook for async-scene-building calls the
    // ScheduleRenderOnCompositorThread function from the scene builder thread,
    // which then triggers a call to ScheduleGenerateFrame() on the compositor
    // thread. But since *this* function is running on the compositor thread,
    // that scheduling will not happen until this call stack unwinds (or we
    // could spin a nested event loop, but that's more messy). Instead, we
    // simulate it ourselves by calling ScheduleGenerateFrame() directly.
    // In the case where async scene building is disabled, the
    // ScheduleGenerateFrame() call in RecvSetDisplayList() serves this purpose.
    // Note also that the post-swap hook will run and do another
    // ScheduleGenerateFrame() after we unwind here, so we will end up with an
    // extra render/composite that is probably avoidable, but in practice we
    // shouldn't be calling this function all that much in production so this
    // is probably fine. If it becomes an issue we can add more state tracking
    // machinery to optimize it away.
    ScheduleGenerateFrame();
  }
}

void
WebRenderBridgeParent::FlushFrameGeneration()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(IsRootWebRenderBridgeParent()); // This function is only useful on the root WRBP

  // This forces a new GenerateFrame transaction to be sent to the render
  // backend thread, if one is pending. This doesn't block on any other threads.
  mForceRendering = true;
  mCompositorScheduler->FlushPendingComposite();
  mForceRendering = false;
}

void
WebRenderBridgeParent::FlushFramePresentation()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  // This sends a message to the render backend thread to send a message
  // to the renderer thread, and waits for that message to be processed. So
  // this effectively blocks on the render backend and renderer threads,
  // following the same codepath that WebRender takes to render and composite
  // a frame.
  mApi->WaitFlushed();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvGetSnapshot(PTextureParent* aTexture)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(!mPaused);

  // This function should only get called in the root WRBP. If this function
  // gets called in a non-root WRBP, we will set mForceRendering in this WRBP
  // but it will have no effect because CompositeToTarget (which reads the
  // flag) only gets invoked in the root WRBP. So we assert that this is the
  // root WRBP (i.e. has a non-null mWidget) to catch violations of this rule.
  MOZ_ASSERT(IsRootWebRenderBridgeParent());

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

  TimeStamp start = TimeStamp::Now();
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

  FlushSceneBuilds();
  FlushFrameGeneration();
  mApi->Readback(start, size, buffer, buffer_size);

  return IPC_OK();
}

void
WebRenderBridgeParent::AddPipelineIdForCompositable(const wr::PipelineId& aPipelineId,
                                                    const CompositableHandle& aHandle,
                                                    const bool& aAsync)
{
  if (mDestroyed) {
    return;
  }

  MOZ_ASSERT(!mAsyncCompositables.Get(wr::AsUint64(aPipelineId)).get());

  RefPtr<CompositableHost> host;
  if (aAsync) {
    RefPtr<ImageBridgeParent> imageBridge = ImageBridgeParent::GetInstance(OtherPid());
    if (!imageBridge) {
       return;
    }
    host = imageBridge->FindCompositable(aHandle);
  } else {
    host = FindCompositable(aHandle);
  }
  if (!host) {
    return;
  }

  WebRenderImageHost* wrHost = host->AsWebRenderImageHost();
  MOZ_ASSERT(wrHost);
  if (!wrHost) {
    gfxCriticalNote << "Incompatible CompositableHost at WebRenderBridgeParent.";
  }

  if (!wrHost) {
    return;
  }

  wrHost->SetWrBridge(this);
  wrHost->EnableUseAsyncImagePipeline();
  mAsyncCompositables.Put(wr::AsUint64(aPipelineId), wrHost);
  mAsyncImageManager->AddAsyncImagePipeline(aPipelineId, wrHost);

  return;
}

void
WebRenderBridgeParent::RemovePipelineIdForCompositable(const wr::PipelineId& aPipelineId,
                                                       wr::TransactionBuilder& aTxn)
{
  if (mDestroyed) {
    return;
  }

  WebRenderImageHost* wrHost = mAsyncCompositables.Get(wr::AsUint64(aPipelineId)).get();
  if (!wrHost) {
    return;
  }

  wrHost->ClearWrBridge();
  mAsyncImageManager->RemoveAsyncImagePipeline(aPipelineId, aTxn);
  aTxn.RemovePipeline(aPipelineId);
  mAsyncCompositables.Remove(wr::AsUint64(aPipelineId));
  return;
}

void
WebRenderBridgeParent::RemoveExternalImageId(const ExternalImageId& aImageId)
{
  if (mDestroyed) {
    return;
  }

  uint64_t imageId = wr::AsUint64(aImageId);
  if (mSharedSurfaceIds.EnsureRemoved(imageId)) {
    mAsyncImageManager->HoldExternalImage(mPipelineId, mWrEpoch, aImageId);
  }
}

void
WebRenderBridgeParent::ReleaseTextureOfImage(const wr::ImageKey& aKey)
{
  if (mDestroyed) {
    return;
  }

  uint64_t id = wr::AsUint64(aKey);
  CompositableTextureHostRef texture;
  WebRenderTextureHost* wrTexture = nullptr;

  if (mTextureHosts.Get(id, &texture)) {
    wrTexture = texture->AsWebRenderTextureHost();
  }
  if (wrTexture) {
    mAsyncImageManager->HoldExternalImage(mPipelineId, mWrEpoch, wrTexture);
  }
  mTextureHosts.Remove(id);
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
  txn.ClearDisplayList(GetNextWrEpoch(), mPipelineId);
  mApi->SendTransaction(txn);
  // Schedule generate frame to clean up Pipeline
  ScheduleGenerateFrame();
  // Remove animations.
  for (const auto& id : mActiveAnimations) {
    mAnimStorage->ClearById(id);
  }
  mActiveAnimations.clear();
  std::queue<CompositorAnimationIdsForEpoch>().swap(mCompositorAnimationsToDelete); // clear queue
  return IPC_OK();
}

void
WebRenderBridgeParent::UpdateWebRender(CompositorVsyncScheduler* aScheduler,
                                       wr::WebRenderAPI* aApi,
                                       AsyncImagePipelineManager* aImageMgr,
                                       CompositorAnimationStorage* aAnimStorage,
                                       const TextureFactoryIdentifier& aTextureFactoryIdentifier)
{
  MOZ_ASSERT(!IsRootWebRenderBridgeParent());
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
  Unused << SendWrUpdated(mIdNamespace, aTextureFactoryIdentifier);
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
WebRenderBridgeParent::RecvScheduleComposite()
{
  if (mDestroyed) {
    return IPC_OK();
  }
  ScheduleGenerateFrame();
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvCapture()
{
  if (!mDestroyed) {
    mApi->Capture();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSyncWithCompositor()
{
  FlushSceneBuilds();
  if (RefPtr<WebRenderBridgeParent> root = GetRootWebRenderBridgeParent()) {
    root->FlushFrameGeneration();
  }
  FlushFramePresentation();
  // Finally, we force the AsyncImagePipelineManager to handle all the
  // pipeline updates produced in the last step, so that it frees any
  // unneeded textures. Then we can return from this sync IPC call knowing
  // that we've done everything we can to flush stuff on the compositor.
  mAsyncImageManager->ProcessPipelineUpdates();

  return IPC_OK();
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
  if (RefPtr<WebRenderBridgeParent> root = GetRootWebRenderBridgeParent()) {
    root->AdvanceAnimations();
  } else {
    AdvanceAnimations();
  }

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
  if (RefPtr<WebRenderBridgeParent> root = GetRootWebRenderBridgeParent()) {
    root->AdvanceAnimations();
  } else {
    AdvanceAnimations();
  }

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
  mCompositorBridge->SetTestAsyncScrollOffset(GetLayersId(), aScrollId, CSSPoint(aX, aY));
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetAsyncZoom(const FrameMetrics::ViewID& aScrollId,
                                        const float& aZoom)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  mCompositorBridge->SetTestAsyncZoom(GetLayersId(), aScrollId, LayerToParentLayerScale(aZoom));
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

bool
WebRenderBridgeParent::AdvanceAnimations()
{
  if (CompositorBridgeParent* cbp = GetRootCompositorBridgeParent()) {
    Maybe<TimeStamp> testingTimeStamp = cbp->GetTestingTimeStamp();
    if (testingTimeStamp) {
      // If we are on testing refresh mode, use the testing time stamp.  And
      // also we don't update mPreviousFrameTimeStamp since unlike normal
      // refresh mode, on the testing mode animations on the compositor are
      // synchronously composed, so we don't need to worry about the time gap
      // between the main thread and compositor thread.
      return AnimationHelper::SampleAnimations(mAnimStorage,
                                               *testingTimeStamp,
                                               *testingTimeStamp);
    }
  }

  TimeStamp lastComposeTime = mCompositorScheduler->GetLastComposeTime();
  const bool isAnimating =
    AnimationHelper::SampleAnimations(mAnimStorage,
                                      mPreviousFrameTimeStamp,
                                      lastComposeTime);

  // Reset the previous time stamp if we don't already have any running
  // animations to avoid using the time which is far behind for newly
  // started animations.
  mPreviousFrameTimeStamp = isAnimating ? lastComposeTime : TimeStamp();

  return isAnimating;
}

bool
WebRenderBridgeParent::SampleAnimations(nsTArray<wr::WrOpacityProperty>& aOpacityArray,
                                        nsTArray<wr::WrTransformProperty>& aTransformArray)
{
  const bool isAnimating = AdvanceAnimations();

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

  return isAnimating;
}

void
WebRenderBridgeParent::CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect)
{
  // This function should only get called in the root WRBP
  MOZ_ASSERT(IsRootWebRenderBridgeParent());

  // The two arguments are part of the CompositorVsyncSchedulerOwner API but in
  // this implementation they should never be non-null.
  MOZ_ASSERT(aTarget == nullptr);
  MOZ_ASSERT(aRect == nullptr);

  AUTO_PROFILER_TRACING("Paint", "CompositeToTraget");
  if (mPaused || !mReceivedDisplayList) {
    mPreviousFrameTimeStamp = TimeStamp();
    return;
  }

  if (!mForceRendering &&
      wr::RenderThread::Get()->TooManyPendingFrames(mApi->GetId())) {
    // Render thread is busy, try next time.
    mCompositorScheduler->ScheduleComposition();
    mPreviousFrameTimeStamp = TimeStamp();
    return;
  }

  TimeStamp start = TimeStamp::Now();
  mAsyncImageManager->SetCompositionTime(start);

  {
    // TODO: We can improve upon this by using two transactions: one for everything that
    // doesn't change the display list (in other words does not cause the scene to be
    // re-built), and one for the rest. This way, if an async pipeline needs to re-build
    // its display list, other async pipelines can still be rendered while the scene is
    // building. Those other async pipelines can go in the other transaction that
    // we create below.
    wr::TransactionBuilder txn;
    mAsyncImageManager->ApplyAsyncImages(txn);
    mApi->SendTransaction(txn);
  }

  if (!mAsyncImageManager->GetCompositeUntilTime().IsNull()) {
    // Trigger another CompositeToTarget() call because there might be another
    // frame that we want to generate after this one.
    // It will check if we actually want to generate the frame or not.
    mCompositorScheduler->ScheduleComposition();
  }

  if (!mAsyncImageManager->GetAndResetWillGenerateFrame() &&
      !mForceRendering) {
    // Could skip generating frame now.
    mPreviousFrameTimeStamp = TimeStamp();
    return;
  }

  // Ensure this GenerateFrame is handled on the render backend thread rather
  // than going through the scene builder thread. That way we continue generating
  // frames with the old scene even during slow scene builds.
  wr::TransactionBuilder txn(/* aUseSceneBuilderThread */ false);

  nsTArray<wr::WrOpacityProperty> opacityArray;
  nsTArray<wr::WrTransformProperty> transformArray;

  if (SampleAnimations(opacityArray, transformArray)) {
    ScheduleGenerateFrame();
  }
  // We do this even if the arrays are empty, because it will clear out any
  // previous properties store on the WR side, which is desirable.
  txn.UpdateDynamicProperties(opacityArray, transformArray);

  SetAPZSampleTime();

  wr::RenderThread::Get()->IncPendingFrameCount(mApi->GetId(), start);

#if defined(ENABLE_FRAME_LATENCY_LOG)
  auto startTime = TimeStamp::Now();
  mApi->SetFrameStartTime(startTime);
#endif

  txn.GenerateFrame();

  mApi->SendTransaction(txn);
}

void
WebRenderBridgeParent::HoldPendingTransactionId(const wr::Epoch& aWrEpoch,
                                                TransactionId aTransactionId,
                                                const TimeStamp& aRefreshStartTime,
                                                const TimeStamp& aTxnStartTime,
                                                const TimeStamp& aFwdTime)
{
  MOZ_ASSERT(aTransactionId > LastPendingTransactionId());
  mPendingTransactionIds.push(PendingTransactionId(aWrEpoch,
                                                   aTransactionId,
                                                   aRefreshStartTime,
                                                   aTxnStartTime,
                                                   aFwdTime));
}

TransactionId
WebRenderBridgeParent::LastPendingTransactionId()
{
  TransactionId id{0};
  if (!mPendingTransactionIds.empty()) {
    id = mPendingTransactionIds.back().mId;
  }
  return id;
}

TransactionId
WebRenderBridgeParent::FlushPendingTransactionIds()
{
  TransactionId id{0};
  if (!mPendingTransactionIds.empty()) {
    id = mPendingTransactionIds.back().mId;
    std::queue<PendingTransactionId>().swap(mPendingTransactionIds); // clear queue
  }
  return id;
}

TransactionId
WebRenderBridgeParent::FlushTransactionIdsForEpoch(const wr::Epoch& aEpoch, const TimeStamp& aEndTime)
{
  TransactionId id{0};
  while (!mPendingTransactionIds.empty()) {
    if (aEpoch.mHandle < mPendingTransactionIds.front().mEpoch.mHandle) {
      break;
    }

    if (!IsRootWebRenderBridgeParent() && !mVsyncRate.IsZero()) {
      double latencyMs = (aEndTime - mPendingTransactionIds.front().mTxnStartTime).ToMilliseconds();
      double latencyNorm = latencyMs / mVsyncRate.ToMilliseconds();
      int32_t fracLatencyNorm = lround(latencyNorm * 100.0);
      Telemetry::Accumulate(Telemetry::CONTENT_FRAME_TIME, fracLatencyNorm);
    }

#if defined(ENABLE_FRAME_LATENCY_LOG)
    if (mPendingTransactionIds.front().mRefreshStartTime) {
      int32_t latencyMs = lround((aEndTime - mPendingTransactionIds.front().mRefreshStartTime).ToMilliseconds());
      printf_stderr("From transaction start to end of generate frame latencyMs %d this %p\n", latencyMs, this);
    }
    if (mPendingTransactionIds.front().mFwdTime) {
      int32_t latencyMs = lround((aEndTime - mPendingTransactionIds.front().mFwdTime).ToMilliseconds());
      printf_stderr("From forwarding transaction to end of generate frame latencyMs %d this %p\n", latencyMs, this);
    }
#endif
    id = mPendingTransactionIds.front().mId;
    mPendingTransactionIds.pop();
  }
  return id;
}

LayersId
WebRenderBridgeParent::GetLayersId() const
{
  return wr::AsLayersId(mPipelineId);
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
WebRenderBridgeParent::FlushRendering()
{
  if (mDestroyed) {
    return;
  }

  // XXX: do we need to flush any scene building here?
  FlushFrameGeneration();
  FlushFramePresentation();
}

void
WebRenderBridgeParent::FlushRenderingAsync()
{
  if (mDestroyed) {
    return;
  }

  mCompositorScheduler->FlushPendingComposite();
}

void
WebRenderBridgeParent::Pause()
{
  MOZ_ASSERT(IsRootWebRenderBridgeParent());
#ifdef MOZ_WIDGET_ANDROID
  if (!IsRootWebRenderBridgeParent() || mDestroyed) {
    return;
  }
  mApi->Pause();
#endif
  mPaused = true;
}

bool
WebRenderBridgeParent::Resume()
{
  MOZ_ASSERT(IsRootWebRenderBridgeParent());
#ifdef MOZ_WIDGET_ANDROID
  if (!IsRootWebRenderBridgeParent() || mDestroyed) {
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

  wr::Epoch wrEpoch = GetNextWrEpoch();

  wr::TransactionBuilder txn;
  txn.ClearDisplayList(wrEpoch, mPipelineId);
  mReceivedDisplayList = false;

  // Schedule generate frame to clean up Pipeline
  ScheduleGenerateFrame();
  // WrFontKeys and WrImageKeys are deleted during WebRenderAPI destruction.
  for (auto iter = mTextureHosts.Iter(); !iter.Done(); iter.Next()) {
    WebRenderTextureHost* wrTexture = iter.Data()->AsWebRenderTextureHost();
    MOZ_ASSERT(wrTexture);
    if (wrTexture) {
      mAsyncImageManager->HoldExternalImage(mPipelineId, wrEpoch, wrTexture);
    }
  }
  mTextureHosts.Clear();
  for (auto iter = mAsyncCompositables.Iter(); !iter.Done(); iter.Next()) {
    wr::PipelineId pipelineId = wr::AsPipelineId(iter.Key());
    RefPtr<WebRenderImageHost> host = iter.Data();
    host->ClearWrBridge();
    mAsyncImageManager->RemoveAsyncImagePipeline(pipelineId, txn);
  }
  mAsyncCompositables.Clear();
  for (auto iter = mSharedSurfaceIds.Iter(); !iter.Done(); iter.Next()) {
    wr::ExternalImageId id = wr::ToExternalImageId(iter.Get()->GetKey());
    mAsyncImageManager->HoldExternalImage(mPipelineId, mWrEpoch, id);
  }
  mSharedSurfaceIds.Clear();

  mAsyncImageManager->RemovePipeline(mPipelineId, wrEpoch);
  txn.RemovePipeline(mPipelineId);

  mApi->SendTransaction(txn);

  for (const auto& id : mActiveAnimations) {
    mAnimStorage->ClearById(id);
  }
  mActiveAnimations.clear();
  std::queue<CompositorAnimationIdsForEpoch>().swap(mCompositorAnimationsToDelete); // clear queue

  if (IsRootWebRenderBridgeParent()) {
    mCompositorScheduler->Destroy();
  }

  // Before tearing down mApi we should make sure the above transaction has been
  // flushed back to the render backend thread. Otherwise the cleanup messages
  // that the WebRenderAPI destructor triggers can race ahead of the transaction
  // (because it goes directly to the RB thread, bypassing the scene builder
  // thread) and clear caches etc. that are still in use.
  FlushSceneBuilds();

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

wr::Epoch
WebRenderBridgeParent::GetNextWrEpoch()
{
  MOZ_RELEASE_ASSERT(mWrEpoch.mHandle != UINT32_MAX);
  mWrEpoch.mHandle++;
  return mWrEpoch;
}

void
WebRenderBridgeParent::ExtractImageCompositeNotifications(nsTArray<ImageCompositeNotificationInfo>* aNotifications)
{
  MOZ_ASSERT(IsRootWebRenderBridgeParent());
  if (mDestroyed) {
    return;
  }
  mAsyncImageManager->FlushImageNotifications(aNotifications);
}

} // namespace layers
} // namespace mozilla
