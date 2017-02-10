/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeParent.h"

#include "CompositableHost.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/Range.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/WebRenderCompositableHolder.h"
#include "mozilla/layers/WebRenderCompositorOGL.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/CompositorWidget.h"

bool is_in_compositor_thread()
{
  return mozilla::layers::CompositorThreadHolder::IsInCompositorThread();
}

bool is_in_render_thread()
{
  return mozilla::wr::RenderThread::IsInRenderThread();
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
                                             const wr::PipelineId& aPipelineId,
                                             widget::CompositorWidget* aWidget,
                                             RefPtr<wr::WebRenderAPI>&& aApi,
                                             RefPtr<WebRenderCompositableHolder>&& aHolder)
  : mCompositorBridge(aCompositorBridge)
  , mPipelineId(aPipelineId)
  , mWidget(aWidget)
  , mBuilder(Nothing())
  , mApi(aApi)
  , mCompositableHolder(aHolder)
  , mChildLayerObserverEpoch(0)
  , mParentLayerObserverEpoch(0)
  , mPendingTransactionId(0)
  , mDestroyed(false)
{
  MOZ_ASSERT(mCompositableHolder);
  if (mWidget) {
    mCompositorScheduler = new CompositorVsyncScheduler(this, mWidget);
  }
}


mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvCreate(const gfx::IntSize& aSize)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  if (mBuilder.isSome()) {
    return IPC_OK();
  }
  MOZ_ASSERT(mApi);
  mBuilder.emplace(LayerIntSize(aSize.width, aSize.height), mPipelineId);

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
  MOZ_ASSERT(mBuilder.isSome());
  mDestroyed = true;
  ClearResources();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddImage(const gfx::IntSize& aSize,
                                    const uint32_t& aStride,
                                    const gfx::SurfaceFormat& aFormat,
                                    const ByteBuffer& aBuffer,
                                    wr::ImageKey* aOutImageKey)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mApi);
  *aOutImageKey = mApi->AddImageBuffer(aSize, aStride, aFormat,
                                       aBuffer.AsSlice());

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvUpdateImage(const wr::ImageKey& aImageKey,
                                       const gfx::IntSize& aSize,
                                       const gfx::SurfaceFormat& aFormat,
                                       const ByteBuffer& aBuffer)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mApi);
  mApi->UpdateImageBuffer(aImageKey, aSize, aFormat, aBuffer.AsSlice());

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDeleteImage(const wr::ImageKey& aImageKey)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mApi);
  mKeysToDelete.push_back(aImageKey);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPBegin(const gfx::IntSize& aSize,
                                   bool* aOutSuccess)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mBuilder.isSome());
  mBuilder.ref().Begin(LayerIntSize(aSize.width, aSize.height));
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

  ProcessWebrenderCommands(aCommands, wr::Epoch(aTransactionId));

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
WebRenderBridgeParent::ProcessWebrenderCommands(InfallibleTArray<WebRenderCommand>& aCommands, const wr::Epoch& aEpoch)
{
  MOZ_ASSERT(mBuilder.isSome());
  wr::DisplayListBuilder& builder = mBuilder.ref();
  // XXX remove it when external image key is used.
  std::vector<wr::ImageKey> keysToDelete;

  for (InfallibleTArray<WebRenderCommand>::index_type i = 0; i < aCommands.Length(); ++i) {
    const WebRenderCommand& cmd = aCommands[i];

    switch (cmd.type()) {
      case WebRenderCommand::TOpDPPushStackingContext: {
        const OpDPPushStackingContext& op = cmd.get_OpDPPushStackingContext();
        builder.PushStackingContext(op.bounds(), op.overflow(), op.mask().ptrOr(nullptr), op.opacity(), op.matrix(), op.mixBlendMode());
        break;
      }
      case WebRenderCommand::TOpDPPopStackingContext: {
        builder.PopStackingContext();
        break;
      }
      case WebRenderCommand::TOpDPPushRect: {
        const OpDPPushRect& op = cmd.get_OpDPPushRect();
        builder.PushRect(op.bounds(), op.clip(), op.color());
        break;
      }
      case WebRenderCommand::TOpDPPushBorder: {
        const OpDPPushBorder& op = cmd.get_OpDPPushBorder();
        builder.PushBorder(op.bounds(), op.clip(),
                           op.top(), op.right(), op.bottom(), op.left(),
                           op.radius());
        break;
      }
      case WebRenderCommand::TOpDPPushImage: {
        const OpDPPushImage& op = cmd.get_OpDPPushImage();
        builder.PushImage(op.bounds(), op.clip(),
                          op.mask().ptrOr(nullptr), op.filter(), wr::ImageKey(op.key()));
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

        nsIntRegion validBufferRegion = op.validBufferRegion().ToUnknownRegion();
        IntRect validRect = IntRect(IntPoint(0,0), dSurf->GetSize());
        if (!validBufferRegion.IsEmpty()) {
          IntPoint offset = validBufferRegion.GetBounds().TopLeft();
          validBufferRegion.MoveBy(-offset);
          validBufferRegion.AndWith(IntRect(IntPoint(0,0), dSurf->GetSize()));
          validRect = validBufferRegion.GetBounds().ToUnknownRect();

          // XXX Remove it when we can put subimage in WebRender.
          RefPtr<DrawTarget> target =
           gfx::Factory::CreateDrawTarget(gfx::BackendType::SKIA, validRect.Size(), SurfaceFormat::B8G8R8A8);
          for (auto iter = validBufferRegion.RectIter(); !iter.Done(); iter.Next()) {
            IntRect regionRect = iter.Get();
            Rect srcRect(regionRect.x + offset.x, regionRect.y + offset.y, regionRect.width, regionRect.height);
            Rect dstRect(regionRect.x, regionRect.y, regionRect.width, regionRect.height);
            target->DrawSurface(dSurf, dstRect, srcRect);
          }
          RefPtr<SourceSurface> surf = target->Snapshot();
          dSurf = surf->GetDataSurface();
        }

        DataSourceSurface::MappedSurface map;
        if (!dSurf->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
          break;
        }

        wr::ImageKey key;
        auto slice = Range<uint8_t>(map.mData, validRect.height * map.mStride);
        key = mApi->AddImageBuffer(validRect.Size(), map.mStride, SurfaceFormat::B8G8R8A8, slice);

        builder.PushImage(op.bounds(), op.clip(), op.mask().ptrOr(nullptr), op.filter(), key);
        keysToDelete.push_back(key);
        dSurf->Unmap();
        break;
      }
      case WebRenderCommand::TOpDPPushIframe: {
        const OpDPPushIframe& op = cmd.get_OpDPPushIframe();
        builder.PushIFrame(op.bounds(), op.clip(), op.pipelineId());
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
        const nsTArray<WrGlyphArray>& glyph_array = op.glyph_array();

        for (size_t i = 0; i < glyph_array.Length(); i++) {
          const nsTArray<WrGlyphInstance>& glyphs = glyph_array[i].glyphs;

          // TODO: We are leaking the key
          wr::FontKey fontKey;
          auto slice = Range<uint8_t>(op.font_buffer().mData, op.font_buffer_length());
          fontKey = mApi->AddRawFont(slice);
          builder.PushText(op.bounds(),
                           op.clip(),
                           glyph_array[i].color,
                           fontKey,
                           Range<const WrGlyphInstance>(glyphs.Elements(), glyphs.Length()),
                           op.glyph_size());
        }

        break;
      }
      default:
        NS_RUNTIMEABORT("not reached");
    }
  }
  builder.End(*mApi, aEpoch);

  ScheduleComposition();
  DeleteOldImages();

  // XXX remove it when external image key is used.
  if (!keysToDelete.empty()) {
    mKeysToDelete.swap(keysToDelete);
  }

  if (ShouldParentObserveEpoch()) {
    mCompositorBridge->ObserveLayerUpdate(mPipelineId.mHandle, GetChildLayerObserverEpoch(), true);
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

  MOZ_ASSERT(mBuilder.isSome());
  mApi->Readback(size, buffer, buffer_size);

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddExternalImageId(const uint64_t& aImageId,
                                              const CompositableHandle& aHandle)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  MOZ_ASSERT(!mExternalImageIds.Get(aImageId).get());

  ImageBridgeParent* imageBridge = ImageBridgeParent::GetInstance(OtherPid());
  if (!imageBridge) {
     return IPC_FAIL_NO_REASON(this);
  }
  RefPtr<CompositableHost> host = imageBridge->FindCompositable(aHandle);
  if (!host) {
    NS_ERROR("CompositableHost not found in the map!");
    return IPC_FAIL_NO_REASON(this);
  }
  if (host->GetType() != CompositableType::IMAGE &&
      host->GetType() != CompositableType::CONTENT_SINGLE &&
      host->GetType() != CompositableType::CONTENT_DOUBLE) {
    NS_ERROR("Incompatible CompositableHost");
    return IPC_OK();
  }

  mCompositableHolder->AddExternalImageId(aImageId, host);
  mExternalImageIds.Put(aImageId, host);

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddExternalImageIdForCompositable(const uint64_t& aImageId,
                                                             const CompositableHandle& aHandle)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(!mExternalImageIds.Get(aImageId).get());

  RefPtr<CompositableHost> host = FindCompositable(aHandle);
  if (host->GetType() != CompositableType::IMAGE &&
      host->GetType() != CompositableType::CONTENT_SINGLE &&
      host->GetType() != CompositableType::CONTENT_DOUBLE) {
    NS_ERROR("Incompatible CompositableHost");
    return IPC_OK();
  }

  mCompositableHolder->AddExternalImageId(aImageId, host);
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
  mCompositableHolder->RemoveExternalImageId(aImageId);

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
  mCompositorBridge->ObserveLayerUpdate(mPipelineId.mHandle, GetChildLayerObserverEpoch(), false);
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
  // TODO(bug 1328602) With the RenderThread, calling SetRootStackingContext
  // should trigger the composition on the render thread.
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
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
  for (wr::ImageKey key : mKeysToDelete) {
    mApi->DeleteImage(key);
  }
  mKeysToDelete.clear();
}

void
WebRenderBridgeParent::ScheduleComposition()
{
  MOZ_ASSERT(mApi);
  // TODO(bug 1328602) should probably send a message to the render
  // thread and force rendering, although in most cases where this is
  // called, rendering should be triggered automatically already (maybe
  // not in the ImageBridge case).
}

void
WebRenderBridgeParent::ClearResources()
{
  DeleteOldImages();
  if (mCompositableHolder) {
    for (auto iter = mExternalImageIds.Iter(); !iter.Done(); iter.Next()) {
      uint64_t externalImageId = iter.Key();
      mCompositableHolder->RemoveExternalImageId(externalImageId);
    }
  }
  mExternalImageIds.Clear();

  if (mBuilder.isSome()) {
    mBuilder.reset();
  }
  if (mCompositorScheduler) {
    mCompositorScheduler->Destroy();
    mCompositorScheduler = nullptr;
  }

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
  if (!AddCompositable(aHandle, aInfo)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvReleaseCompositable(const CompositableHandle& aHandle)
{
  ReleaseCompositable(aHandle);
  return IPC_OK();
}

void
WebRenderBridgeParent::SetWebRenderProfilerEnabled(bool aEnabled)
{
  if (mWidget) {
    // Only set the flag to "root" WebRenderBridgeParent.
    mApi->SetProfilerEnabled(aEnabled);
  }
}

TextureFactoryIdentifier
WebRenderBridgeParent::GetTextureFactoryIdentifier()
{
  MOZ_ASSERT(mApi);

  return TextureFactoryIdentifier(LayersBackend::LAYERS_WR,
                                  XRE_GetProcessType(),
                                  mApi->GetMaxTextureSize());
}

} // namespace layers
} // namespace mozilla
