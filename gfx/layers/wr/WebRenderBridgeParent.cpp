/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeParent.h"

#include "CompositableHost.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/RenderThread.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/WebRenderCompositorOGL.h"
#include "mozilla/widget/CompositorWidget.h"

bool is_in_compositor_thread()
{
  return mozilla::layers::CompositorThreadHolder::IsInCompositorThread();
}

bool is_in_render_thread()
{
  return mozilla::layers::RenderThread::IsInRenderThread();
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
                                             const uint64_t& aPipelineId,
                                             widget::CompositorWidget* aWidget,
                                             gl::GLContext* aGlContext,
                                             wrwindowstate* aWrWindowState,
                                             layers::Compositor* aCompositor)
  : mCompositorBridge(aCompositorBridge)
  , mPipelineId(aPipelineId)
  , mWidget(aWidget)
  , mWRState(nullptr)
  , mGLContext(aGlContext)
  , mWRWindowState(aWrWindowState)
  , mCompositor(aCompositor)
  , mChildLayerObserverEpoch(0)
  , mParentLayerObserverEpoch(0)
  , mPendingTransactionId(0)
  , mDestroyed(false)
{
  MOZ_ASSERT(mGLContext);
  MOZ_ASSERT(mCompositor);

  if (!mWRWindowState) {
    // mWRWindowState should only be null for the root WRBP of a layers tree,
    // i.e. the one created by the CompositorBridgeParent as opposed to the
    // CrossProcessCompositorBridgeParent
    MOZ_ASSERT(mWidget);
    mWRWindowState = wr_init_window(mPipelineId,
                                    aGlContext,
                                    gfxPrefs::WebRenderProfilerEnabled());
  }
  if (mWidget) {
    mCompositorScheduler = new CompositorVsyncScheduler(this, mWidget);
  }
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvCreate(const uint32_t& aWidth,
                                  const uint32_t& aHeight)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  if (mWRState) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRWindowState);
  mGLContext->MakeCurrent();
  mWRState = wr_create(mWRWindowState, aWidth, aHeight, mPipelineId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvShutdown()
{
  if (mDestroyed) {
    return IPC_OK();
  }
  Destroy();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

void
WebRenderBridgeParent::Destroy()
{
  if (mDestroyed) {
    return;
  }
  MOZ_ASSERT(mWRState);
  mDestroyed = true;
  ClearResources();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddImage(const uint32_t& aWidth,
                                    const uint32_t& aHeight,
                                    const uint32_t& aStride,
                                    const WRImageFormat& aFormat,
                                    const ByteBuffer& aBuffer,
                                    WRImageKey* aOutImageKey)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRWindowState);
  *aOutImageKey = wr_add_image(mWRWindowState, aWidth, aHeight, aStride, aFormat,
                               aBuffer.mData, aBuffer.mLength);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvUpdateImage(const WRImageKey& aImageKey,
                                       const uint32_t& aWidth,
                                       const uint32_t& aHeight,
                                       const WRImageFormat& aFormat,
                                       const ByteBuffer& aBuffer)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRWindowState);
  wr_update_image(mWRWindowState, aImageKey, aWidth, aHeight, aFormat,
                  aBuffer.mData, aBuffer.mLength);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDeleteImage(const WRImageKey& aImageKey)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRWindowState);
  mKeysToDelete.push_back(aImageKey);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPBegin(const uint32_t& aWidth,
                                   const uint32_t& aHeight,
                                   bool* aOutSuccess)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRState);
  wr_dp_begin(mWRWindowState, mWRState, aWidth, aHeight);
  *aOutSuccess = true;
  return IPC_OK();
}

void
WebRenderBridgeParent::HandleDPEnd(InfallibleTArray<WebRenderCommand>&& aCommands,
                                 InfallibleTArray<OpDestroy>&& aToDestroy,
                                 const uint64_t& aFwdTransactionId,
                                 const uint64_t& aTransactionId)
{
  UpdateFwdTransactionId(aFwdTransactionId);

  if (mDestroyed) {
    for (const auto& op : aToDestroy) {
      DestroyActor(op);
    }
    return;
  }
  // This ensures that destroy operations are always processed. It is not safe
  // to early-return from RecvDPEnd without doing so.
  AutoWebRenderBridgeParentAsyncMessageSender autoAsyncMessageSender(this, &aToDestroy);

  ProcessWebrenderCommands(aCommands);

  // The transaction ID might get reset to 1 if the page gets reloaded, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1145295#c41
  // Otherwise, it should be continually increasing.
  MOZ_ASSERT(aTransactionId == 1 || aTransactionId > mPendingTransactionId);
  mPendingTransactionId = aTransactionId;
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPEnd(InfallibleTArray<WebRenderCommand>&& aCommands,
                                 InfallibleTArray<OpDestroy>&& aToDestroy,
                                 const uint64_t& aFwdTransactionId,
                                 const uint64_t& aTransactionId)
{
  HandleDPEnd(Move(aCommands), Move(aToDestroy), aFwdTransactionId, aTransactionId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPSyncEnd(InfallibleTArray<WebRenderCommand>&& aCommands,
                                     InfallibleTArray<OpDestroy>&& aToDestroy,
                                     const uint64_t& aFwdTransactionId,
                                     const uint64_t& aTransactionId)
{
  HandleDPEnd(Move(aCommands), Move(aToDestroy), aFwdTransactionId, aTransactionId);
  return IPC_OK();
}

void
WebRenderBridgeParent::ProcessWebrenderCommands(InfallibleTArray<WebRenderCommand>& aCommands)
{
  MOZ_ASSERT(mWRState);
  // XXX remove it when external image key is used.
  std::vector<WRImageKey> keysToDelete;

  for (InfallibleTArray<WebRenderCommand>::index_type i = 0; i < aCommands.Length(); ++i) {
    const WebRenderCommand& cmd = aCommands[i];

    switch (cmd.type()) {
      case WebRenderCommand::TOpDPPushStackingContext: {
        const OpDPPushStackingContext& op = cmd.get_OpDPPushStackingContext();
        wr_dp_push_stacking_context(mWRState, op.bounds(), op.overflow(), op.mask().ptrOr(nullptr), &(op.matrix().components[0]));
        break;
      }
      case WebRenderCommand::TOpDPPopStackingContext: {
        wr_dp_pop_stacking_context(mWRState);
        break;
      }
      case WebRenderCommand::TOpDPPushRect: {
        const OpDPPushRect& op = cmd.get_OpDPPushRect();
        wr_dp_push_rect(mWRState, op.bounds(), op.clip(), op.r(), op.g(), op.b(), op.a());
        break;
      }
      case WebRenderCommand::TOpDPPushBorder: {
        const OpDPPushBorder& op = cmd.get_OpDPPushBorder();
        wr_dp_push_border(mWRState, op.bounds(),
                          op.clip(), op.top(), op.right(), op.bottom(), op.left(),
                          op.top_left_radius(), op.top_right_radius(),
                          op.bottom_left_radius(), op.bottom_right_radius());
        break;
      }
      case WebRenderCommand::TOpDPPushImage: {
        const OpDPPushImage& op = cmd.get_OpDPPushImage();
        wr_dp_push_image(mWRState, op.bounds(), op.clip(), op.mask().ptrOr(nullptr), op.key());
        break;
      }
      case WebRenderCommand::TOpDPPushExternalImageId: {
        const OpDPPushExternalImageId& op = cmd.get_OpDPPushExternalImageId();
        MOZ_ASSERT(mExternalImageIds.Get(op.externalImageId()).get());

        RefPtr<CompositableHost> host = mExternalImageIds.Get(op.externalImageId());
        if (!host) {
          break;
        }
        RefPtr<DataSourceSurface> dSurf = host->GetAsSurface();
        if (!dSurf) {
          break;
        }
        DataSourceSurface::MappedSurface map;
        if (!dSurf->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
          break;
        }
        gfx::IntSize size = dSurf->GetSize();
        WRImageKey key = wr_add_image(mWRWindowState, size.width, size.height, map.mStride, RGBA8, map.mData, size.height * map.mStride);
        wr_dp_push_image(mWRState, op.bounds(), op.clip(), op.mask().ptrOr(nullptr), key);
        keysToDelete.push_back(key);
        dSurf->Unmap();
        break;
      }
      case WebRenderCommand::TOpDPPushIframe: {
        const OpDPPushIframe& op = cmd.get_OpDPPushIframe();
        wr_dp_push_iframe(mWRWindowState, mWRState, op.bounds(), op.clip(), op.layersid());
        break;
      }
      case WebRenderCommand::TCompositableOperation: {
        EditReplyVector replyv;
        if (!ReceiveCompositableUpdate(cmd.get_CompositableOperation(),
                                       replyv)) {
          NS_ERROR("ReceiveCompositableUpdate failed");
        }
        break;
      }
      case WebRenderCommand::TOpDPPushText: {
        const OpDPPushText& op = cmd.get_OpDPPushText();
        const nsTArray<WRGlyphArray>& glyph_array = op.glyph_array();

        for (size_t i = 0; i < glyph_array.Length(); i++) {
          const nsTArray<WRGlyphInstance>& glyphs = glyph_array[i].glyphs;

          wr_dp_push_text(mWRWindowState,
                          mWRState,
                          op.bounds(),
                          op.clip(),
                          glyph_array[i].color,
                          glyphs.Elements(),
                          glyphs.Length(),
                          op.glyph_size(),
                          op.font_buffer().mData,
                          op.font_buffer_length());
        }

        break;
      }
      default:
        NS_RUNTIMEABORT("not reached");
    }
  }
  wr_dp_end(mWRWindowState, mWRState);
  ScheduleComposition();
  DeleteOldImages();

  // XXX remove it when external image key is used.
  if (!keysToDelete.empty()) {
    mKeysToDelete.swap(keysToDelete);
  }

  if (ShouldParentObserveEpoch()) {
    mCompositorBridge->ObserveLayerUpdate(mPipelineId, GetChildLayerObserverEpoch(), true);
  }
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPGetSnapshot(PTextureParent* aTexture)
{
  if (mDestroyed) {
    return IPC_OK();
  }

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

  MOZ_ASSERT(mWRState);
  mGLContext->MakeCurrent();
  wr_readback_into_buffer(mWRWindowState, size.width, size.height, buffer, buffer_size);

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddExternalImageId(const uint64_t& aImageId,
                                              const uint64_t& aAsyncContainerId)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(!mExternalImageIds.Get(aImageId).get());

  ImageBridgeParent* imageBridge = ImageBridgeParent::GetInstance(OtherPid());
  if (!imageBridge) {
     return IPC_FAIL_NO_REASON(this);
  }
  CompositableHost* host = imageBridge->FindCompositable(aAsyncContainerId);
  if (!host) {
    NS_ERROR("CompositableHost not found in the map!");
    return IPC_FAIL_NO_REASON(this);
  }
  if (host->GetType() != CompositableType::IMAGE) {
    NS_ERROR("Incompatible CompositableHost");
    return IPC_OK();
  }

  host->SetCompositor(mCompositor);
  mCompositor->AsWebRenderCompositorOGL()->AddExternalImageId(aImageId, host);
  mExternalImageIds.Put(aImageId, host);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddExternalImageIdForCompositable(const uint64_t& aImageId,
                                                             PCompositableParent* aCompositable)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(!mExternalImageIds.Get(aImageId).get());

  CompositableHost* host = CompositableHost::FromIPDLActor(aCompositable);
  if (host->GetType() != CompositableType::IMAGE) {
    NS_ERROR("Incompatible CompositableHost");
    return IPC_OK();
  }

  host->SetCompositor(mCompositor);
  mCompositor->AsWebRenderCompositorOGL()->AddExternalImageId(aImageId, host);
  mExternalImageIds.Put(aImageId, host);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvRemoveExternalImageId(const uint64_t& aImageId)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mExternalImageIds.Get(aImageId).get());
  mExternalImageIds.Remove(aImageId);
  mCompositor->AsWebRenderCompositorOGL()->RemoveExternalImageId(aImageId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetLayerObserverEpoch(const uint64_t& aLayerObserverEpoch)
{
  mChildLayerObserverEpoch = aLayerObserverEpoch;
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvClearCachedResources()
{
  mCompositorBridge->ObserveLayerUpdate(mPipelineId, GetChildLayerObserverEpoch(), false);
  return IPC_OK();
}

void
WebRenderBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  Destroy();
}

void
WebRenderBridgeParent::CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect)
{
  if (aTarget) {
    // XXX Add compositing to DrawTarget
    return;
  }
  if (!mWidget) {
    return;
  }

  TimeStamp start = TimeStamp::Now();

  mCompositor->SetCompositionTime(TimeStamp::Now());
  mCompositor->AsWebRenderCompositorOGL()->UpdateExternalImages();

  MOZ_ASSERT(mWRState);
  mozilla::widget::WidgetRenderingContext widgetContext;
#if defined(XP_MACOSX)
  widgetContext.mGL = mGLContext;
#elif defined(MOZ_WIDGET_ANDROID)
  widgetContext.mCompositor = mCompositor;
#endif
  if (!mWidget->PreRender(&widgetContext)) {
    return;
  }
  // XXX set clear color if MOZ_WIDGET_ANDROID is defined.
  mWidget->DrawWindowUnderlay(&widgetContext, LayoutDeviceIntRect());
  mGLContext->MakeCurrent();
  wr_composite_window(mWRWindowState);
  mGLContext->SwapBuffers();
  mWidget->DrawWindowOverlay(&widgetContext, LayoutDeviceIntRect());
  mWidget->PostRender(&widgetContext);

  TimeStamp end = TimeStamp::Now();
  mCompositorBridge->NotifyDidComposite(mPendingTransactionId, start, end);
  mPendingTransactionId = 0;

  // Calls for TextureHosts recycling
  mCompositor->EndFrame();
  mCompositor->FlushPendingNotifyNotUsed();
}

void
WebRenderBridgeParent::DidComposite(uint64_t aTransactionId, TimeStamp aStart, TimeStamp aEnd)
{
  mCompositorBridge->NotifyDidComposite(aTransactionId, aStart, aEnd);
}

WebRenderBridgeParent::~WebRenderBridgeParent()
{
}

void
WebRenderBridgeParent::DeleteOldImages()
{
  for (WRImageKey key : mKeysToDelete) {
    wr_delete_image(mWRWindowState, key);
  }
  mKeysToDelete.clear();
}

void
WebRenderBridgeParent::ScheduleComposition()
{
  mCompositor->AsWebRenderCompositorOGL()->ScheduleComposition();
}

void
WebRenderBridgeParent::ClearResources()
{
  DeleteOldImages();
  for (auto iter = mExternalImageIds.Iter(); !iter.Done(); iter.Next()) {
    uint64_t externalImageId = iter.Key();
    mCompositor->AsWebRenderCompositorOGL()->RemoveExternalImageId(externalImageId);
  }
  mExternalImageIds.Clear();

  if (mWRState) {
    wr_destroy(mWRWindowState, mWRState);
    mWRState = nullptr;
  }
  if (mCompositorScheduler) {
    mCompositorScheduler->Destroy();
    mCompositorScheduler = nullptr;
  }
  mGLContext = nullptr;
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

PCompositableParent*
WebRenderBridgeParent::AllocPCompositableParent(const TextureInfo& aInfo)
{
  PCompositableParent* actor = CompositableHost::CreateIPDLActor(this, aInfo);
  CompositableHost* compositable = CompositableHost::FromIPDLActor(actor);
  MOZ_ASSERT(compositable);
  compositable->SetCompositor(mCompositor);
  return actor;
}

bool
WebRenderBridgeParent::DeallocPCompositableParent(PCompositableParent* aActor)
{
  return CompositableHost::DestroyIPDLActor(aActor);
}

void
WebRenderBridgeParent::SetWebRenderProfilerEnabled(bool aEnabled)
{
  if (mWidget) {
    // Only set the flag to "root" WebRenderBridgeParent.
    wr_profiler_set_enabled(mWRWindowState, aEnabled);
  }
}

} // namespace layers
} // namespace mozilla
