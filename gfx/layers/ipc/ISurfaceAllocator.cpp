/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ISurfaceAllocator.h"

#include "mozilla/layers/ImageBridgeParent.h" // for ImageBridgeParent
#include "mozilla/layers/TextureHost.h"       // for TextureHost

namespace mozilla {
namespace layers {

NS_IMPL_ISUPPORTS(GfxMemoryImageReporter, nsIMemoryReporter)

mozilla::Atomic<ptrdiff_t> GfxMemoryImageReporter::sAmount(0);

mozilla::ipc::SharedMemory::SharedMemoryType OptimalShmemType()
{
  return ipc::SharedMemory::SharedMemoryType::TYPE_BASIC;
}

void
HostIPCAllocator::SendFenceHandleIfPresent(PTextureParent* aTexture)
{
  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    return;
  }

  if (!(texture->GetFlags() & TextureFlags::RECYCLE) &&
     !texture->NeedsFenceHandle()) {
    return;
  }

  uint64_t textureId = TextureHost::GetTextureSerial(aTexture);

  // Send a ReleaseFence of CompositorOGL.
  FenceHandle fence = texture->GetCompositorReleaseFence();
  if (fence.IsValid()) {
    mPendingAsyncMessage.push_back(OpDeliverFence(textureId, fence));
  }

  // Send a ReleaseFence that is set to TextureHost by HwcComposer2D.
  fence = texture->GetAndResetReleaseFenceHandle();
  if (fence.IsValid()) {
    mPendingAsyncMessage.push_back(OpDeliverFence(textureId, fence));
  }
}

void
HostIPCAllocator::SendPendingAsyncMessages()
{
  if (mPendingAsyncMessage.empty()) {
    return;
  }

  // Some type of AsyncParentMessageData message could have
  // one file descriptor (e.g. OpDeliverFence).
  // A number of file descriptors per gecko ipc message have a limitation
  // on OS_POSIX (MACOSX or LINUX).
#if defined(OS_POSIX)
  static const uint32_t kMaxMessageNumber = FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE;
#else
  // default number that works everywhere else
  static const uint32_t kMaxMessageNumber = 250;
#endif

  InfallibleTArray<AsyncParentMessageData> messages;
  messages.SetCapacity(mPendingAsyncMessage.size());
  for (size_t i = 0; i < mPendingAsyncMessage.size(); i++) {
    messages.AppendElement(mPendingAsyncMessage[i]);
    // Limit maximum number of messages.
    if (messages.Length() >= kMaxMessageNumber) {
      SendAsyncMessage(messages);
      // Initialize Messages.
      messages.Clear();
    }
  }

  if (messages.Length() > 0) {
    SendAsyncMessage(messages);
  }
  mPendingAsyncMessage.clear();
}

void
CompositorBridgeParentIPCAllocator::NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId)
{
  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    return;
  }

  if (!(texture->GetFlags() & TextureFlags::RECYCLE) &&
     !texture->NeedsFenceHandle()) {
    return;
  }

  if (texture->GetFlags() & TextureFlags::RECYCLE) {
    SendFenceHandleIfPresent(aTexture);
    uint64_t textureId = TextureHost::GetTextureSerial(aTexture);
    mPendingAsyncMessage.push_back(
      OpNotifyNotUsed(textureId, aTransactionId));
    return;
  }

  // Gralloc requests to deliver fence to client side.
  // If client side does not use TextureFlags::RECYCLE flag,
  // The fence can not be delivered via LayerTransactionParent.
  // TextureClient might wait the fence delivery on main thread.

  MOZ_ASSERT(ImageBridgeParent::GetInstance(GetChildProcessId()));
  if (ImageBridgeParent::GetInstance(GetChildProcessId())) {
    // Send message back via PImageBridge.
    ImageBridgeParent::NotifyNotUsedToNonRecycle(
      GetChildProcessId(),
      aTexture,
      aTransactionId);
   } else {
     NS_ERROR("ImageBridgeParent should exist");
   }

   if (!IsAboutToSendAsyncMessages()) {
     SendPendingAsyncMessages();
   }
}

} // namespace layers
} // namespace mozilla
