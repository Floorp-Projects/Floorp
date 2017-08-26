/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundChildImpl.h"

#include "ActorsChild.h" // IndexedDB
#include "BroadcastChannelChild.h"
#include "ServiceWorkerManagerChild.h"
#include "FileDescriptorSetChild.h"
#ifdef MOZ_WEBRTC
#include "CamerasChild.h"
#endif
#include "mozilla/media/MediaChild.h"
#include "mozilla/Assertions.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/dom/PFileSystemRequestChild.h"
#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIndexedDBUtilsChild.h"
#include "mozilla/dom/ipc/IPCBlobInputStreamChild.h"
#include "mozilla/dom/ipc/PendingIPCBlobChild.h"
#include "mozilla/dom/quota/PQuotaChild.h"
#include "mozilla/dom/StorageIPC.h"
#include "mozilla/dom/GamepadEventChannelChild.h"
#include "mozilla/dom/GamepadTestChannelChild.h"
#include "mozilla/dom/LocalStorage.h"
#include "mozilla/dom/MessagePortChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/PBackgroundTestChild.h"
#include "mozilla/ipc/PChildToParentStreamChild.h"
#include "mozilla/ipc/PParentToChildStreamChild.h"
#include "mozilla/layout/VsyncChild.h"
#include "mozilla/net/HttpBackgroundChannelChild.h"
#include "mozilla/net/PUDPSocketChild.h"
#include "mozilla/dom/network/UDPSocketChild.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "nsID.h"
#include "nsTraceRefcnt.h"

namespace {

class TestChild final : public mozilla::ipc::PBackgroundTestChild
{
  friend class mozilla::ipc::BackgroundChildImpl;

  nsCString mTestArg;

  explicit TestChild(const nsCString& aTestArg)
    : mTestArg(aTestArg)
  {
    MOZ_COUNT_CTOR(TestChild);
  }

protected:
  ~TestChild() override
  {
    MOZ_COUNT_DTOR(TestChild);
  }

public:
  mozilla::ipc::IPCResult
  Recv__delete__(const nsCString& aTestArg) override;
};

} // namespace

namespace mozilla {
namespace ipc {

using mozilla::dom::UDPSocketChild;
using mozilla::net::PUDPSocketChild;

using mozilla::dom::asmjscache::PAsmJSCacheEntryChild;
using mozilla::dom::cache::PCacheChild;
using mozilla::dom::cache::PCacheStorageChild;
using mozilla::dom::cache::PCacheStreamControlChild;
using mozilla::dom::LocalStorage;
using mozilla::dom::StorageDBChild;

using mozilla::dom::WebAuthnTransactionChild;

// -----------------------------------------------------------------------------
// BackgroundChildImpl::ThreadLocal
// -----------------------------------------------------------------------------

BackgroundChildImpl::
ThreadLocal::ThreadLocal()
  : mCurrentFileHandle(nullptr)
{
  // May happen on any thread!
  MOZ_COUNT_CTOR(mozilla::ipc::BackgroundChildImpl::ThreadLocal);
}

BackgroundChildImpl::
ThreadLocal::~ThreadLocal()
{
  // May happen on any thread!
  MOZ_COUNT_DTOR(mozilla::ipc::BackgroundChildImpl::ThreadLocal);
}

// -----------------------------------------------------------------------------
// BackgroundChildImpl
// -----------------------------------------------------------------------------

BackgroundChildImpl::BackgroundChildImpl()
{
  // May happen on any thread!
  MOZ_COUNT_CTOR(mozilla::ipc::BackgroundChildImpl);
}

BackgroundChildImpl::~BackgroundChildImpl()
{
  // May happen on any thread!
  MOZ_COUNT_DTOR(mozilla::ipc::BackgroundChildImpl);
}

void
BackgroundChildImpl::ProcessingError(Result aCode, const char* aReason)
{
  // May happen on any thread!

  nsAutoCString abortMessage;

  switch (aCode) {

#define HANDLE_CASE(_result)                                                   \
    case _result:                                                              \
      abortMessage.AssignLiteral(#_result);                                    \
      break

    HANDLE_CASE(MsgDropped);
    HANDLE_CASE(MsgNotKnown);
    HANDLE_CASE(MsgNotAllowed);
    HANDLE_CASE(MsgPayloadError);
    HANDLE_CASE(MsgProcessingError);
    HANDLE_CASE(MsgRouteError);
    HANDLE_CASE(MsgValueError);

#undef HANDLE_CASE

    default:
      MOZ_CRASH("Unknown error code!");
  }

  MOZ_CRASH_UNSAFE_PRINTF("%s: %s", abortMessage.get(), aReason);
}

void
BackgroundChildImpl::ActorDestroy(ActorDestroyReason aWhy)
{
  // May happen on any thread!
}

PBackgroundTestChild*
BackgroundChildImpl::AllocPBackgroundTestChild(const nsCString& aTestArg)
{
  return new TestChild(aTestArg);
}

bool
BackgroundChildImpl::DeallocPBackgroundTestChild(PBackgroundTestChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<TestChild*>(aActor);
  return true;
}

BackgroundChildImpl::PBackgroundIDBFactoryChild*
BackgroundChildImpl::AllocPBackgroundIDBFactoryChild(
                                                const LoggingInfo& aLoggingInfo)
{
  MOZ_CRASH("PBackgroundIDBFactoryChild actors should be manually "
            "constructed!");
}

bool
BackgroundChildImpl::DeallocPBackgroundIDBFactoryChild(
                                             PBackgroundIDBFactoryChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

BackgroundChildImpl::PBackgroundIndexedDBUtilsChild*
BackgroundChildImpl::AllocPBackgroundIndexedDBUtilsChild()
{
  MOZ_CRASH("PBackgroundIndexedDBUtilsChild actors should be manually "
            "constructed!");
}

bool
BackgroundChildImpl::DeallocPBackgroundIndexedDBUtilsChild(
                                         PBackgroundIndexedDBUtilsChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

BackgroundChildImpl::PBackgroundStorageChild*
BackgroundChildImpl::AllocPBackgroundStorageChild(const nsString& aProfilePath)
{
  MOZ_CRASH("PBackgroundStorageChild actors should be manually constructed!");
}

bool
BackgroundChildImpl::DeallocPBackgroundStorageChild(
                                                PBackgroundStorageChild* aActor)
{
  MOZ_ASSERT(aActor);

  StorageDBChild* child = static_cast<StorageDBChild*>(aActor);
  child->ReleaseIPDLReference();
  return true;
}

PPendingIPCBlobChild*
BackgroundChildImpl::AllocPPendingIPCBlobChild(const IPCBlob& aBlob)
{
  return new mozilla::dom::PendingIPCBlobChild(aBlob);
}

bool
BackgroundChildImpl::DeallocPPendingIPCBlobChild(PPendingIPCBlobChild* aActor)
{
  delete aActor;
  return true;
}

PIPCBlobInputStreamChild*
BackgroundChildImpl::AllocPIPCBlobInputStreamChild(const nsID& aID,
                                                   const uint64_t& aSize)
{
  // IPCBlobInputStreamChild is refcounted. Here it's created and in
  // DeallocPIPCBlobInputStreamChild is released.

  RefPtr<mozilla::dom::IPCBlobInputStreamChild> actor =
    new mozilla::dom::IPCBlobInputStreamChild(aID, aSize);
  return actor.forget().take();
}

bool
BackgroundChildImpl::DeallocPIPCBlobInputStreamChild(PIPCBlobInputStreamChild* aActor)
{
  RefPtr<mozilla::dom::IPCBlobInputStreamChild> actor =
    dont_AddRef(static_cast<mozilla::dom::IPCBlobInputStreamChild*>(aActor));
  return true;
}

PFileDescriptorSetChild*
BackgroundChildImpl::AllocPFileDescriptorSetChild(
                                          const FileDescriptor& aFileDescriptor)
{
  return new FileDescriptorSetChild(aFileDescriptor);
}

bool
BackgroundChildImpl::DeallocPFileDescriptorSetChild(
                                                PFileDescriptorSetChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<FileDescriptorSetChild*>(aActor);
  return true;
}

BackgroundChildImpl::PVsyncChild*
BackgroundChildImpl::AllocPVsyncChild()
{
  RefPtr<mozilla::layout::VsyncChild> actor = new mozilla::layout::VsyncChild();
  // There still has one ref-count after return, and it will be released in
  // DeallocPVsyncChild().
  return actor.forget().take();
}

bool
BackgroundChildImpl::DeallocPVsyncChild(PVsyncChild* aActor)
{
  MOZ_ASSERT(aActor);

  // This actor already has one ref-count. Please check AllocPVsyncChild().
  RefPtr<mozilla::layout::VsyncChild> actor =
      dont_AddRef(static_cast<mozilla::layout::VsyncChild*>(aActor));
  return true;
}

PUDPSocketChild*
BackgroundChildImpl::AllocPUDPSocketChild(const OptionalPrincipalInfo& aPrincipalInfo,
                                          const nsCString& aFilter)
{
  MOZ_CRASH("AllocPUDPSocket should not be called");
  return nullptr;
}

bool
BackgroundChildImpl::DeallocPUDPSocketChild(PUDPSocketChild* child)
{

  UDPSocketChild* p = static_cast<UDPSocketChild*>(child);
  p->ReleaseIPDLReference();
  return true;
}

// -----------------------------------------------------------------------------
// BroadcastChannel API
// -----------------------------------------------------------------------------

dom::PBroadcastChannelChild*
BackgroundChildImpl::AllocPBroadcastChannelChild(const PrincipalInfo& aPrincipalInfo,
                                                 const nsCString& aOrigin,
                                                 const nsString& aChannel)
{
  RefPtr<dom::BroadcastChannelChild> agent =
    new dom::BroadcastChannelChild(aOrigin);
  return agent.forget().take();
}

bool
BackgroundChildImpl::DeallocPBroadcastChannelChild(
                                                 PBroadcastChannelChild* aActor)
{
  RefPtr<dom::BroadcastChannelChild> child =
    dont_AddRef(static_cast<dom::BroadcastChannelChild*>(aActor));
  MOZ_ASSERT(child);
  return true;
}

camera::PCamerasChild*
BackgroundChildImpl::AllocPCamerasChild()
{
#ifdef MOZ_WEBRTC
  RefPtr<camera::CamerasChild> agent =
    new camera::CamerasChild();
  return agent.forget().take();
#else
  return nullptr;
#endif
}

bool
BackgroundChildImpl::DeallocPCamerasChild(camera::PCamerasChild *aActor)
{
#ifdef MOZ_WEBRTC
  RefPtr<camera::CamerasChild> child =
      dont_AddRef(static_cast<camera::CamerasChild*>(aActor));
  MOZ_ASSERT(aActor);
#endif
  return true;
}

// -----------------------------------------------------------------------------
// ServiceWorkerManager
// -----------------------------------------------------------------------------

dom::PServiceWorkerManagerChild*
BackgroundChildImpl::AllocPServiceWorkerManagerChild()
{
  RefPtr<dom::workers::ServiceWorkerManagerChild> agent =
    new dom::workers::ServiceWorkerManagerChild();
  return agent.forget().take();
}

bool
BackgroundChildImpl::DeallocPServiceWorkerManagerChild(
                                             PServiceWorkerManagerChild* aActor)
{
  RefPtr<dom::workers::ServiceWorkerManagerChild> child =
    dont_AddRef(static_cast<dom::workers::ServiceWorkerManagerChild*>(aActor));
  MOZ_ASSERT(child);
  return true;
}

// -----------------------------------------------------------------------------
// Cache API
// -----------------------------------------------------------------------------

PCacheStorageChild*
BackgroundChildImpl::AllocPCacheStorageChild(const Namespace& aNamespace,
                                             const PrincipalInfo& aPrincipalInfo)
{
  MOZ_CRASH("CacheStorageChild actor must be provided to PBackground manager");
  return nullptr;
}

bool
BackgroundChildImpl::DeallocPCacheStorageChild(PCacheStorageChild* aActor)
{
  dom::cache::DeallocPCacheStorageChild(aActor);
  return true;
}

PCacheChild*
BackgroundChildImpl::AllocPCacheChild()
{
  return dom::cache::AllocPCacheChild();
}

bool
BackgroundChildImpl::DeallocPCacheChild(PCacheChild* aActor)
{
  dom::cache::DeallocPCacheChild(aActor);
  return true;
}

PCacheStreamControlChild*
BackgroundChildImpl::AllocPCacheStreamControlChild()
{
  return dom::cache::AllocPCacheStreamControlChild();
}

bool
BackgroundChildImpl::DeallocPCacheStreamControlChild(PCacheStreamControlChild* aActor)
{
  dom::cache::DeallocPCacheStreamControlChild(aActor);
  return true;
}

// -----------------------------------------------------------------------------
// MessageChannel/MessagePort API
// -----------------------------------------------------------------------------

dom::PMessagePortChild*
BackgroundChildImpl::AllocPMessagePortChild(const nsID& aUUID,
                                            const nsID& aDestinationUUID,
                                            const uint32_t& aSequenceID)
{
  RefPtr<dom::MessagePortChild> agent = new dom::MessagePortChild();
  return agent.forget().take();
}

bool
BackgroundChildImpl::DeallocPMessagePortChild(PMessagePortChild* aActor)
{
  RefPtr<dom::MessagePortChild> child =
    dont_AddRef(static_cast<dom::MessagePortChild*>(aActor));
  MOZ_ASSERT(child);
  return true;
}

PChildToParentStreamChild*
BackgroundChildImpl::AllocPChildToParentStreamChild()
{
  MOZ_CRASH("PChildToParentStreamChild actors should be manually constructed!");
}

bool
BackgroundChildImpl::DeallocPChildToParentStreamChild(PChildToParentStreamChild* aActor)
{
  delete aActor;
  return true;
}

PParentToChildStreamChild*
BackgroundChildImpl::AllocPParentToChildStreamChild()
{
  return mozilla::ipc::AllocPParentToChildStreamChild();
}

bool
BackgroundChildImpl::DeallocPParentToChildStreamChild(PParentToChildStreamChild* aActor)
{
  delete aActor;
  return true;
}

PAsmJSCacheEntryChild*
BackgroundChildImpl::AllocPAsmJSCacheEntryChild(
                               const dom::asmjscache::OpenMode& aOpenMode,
                               const dom::asmjscache::WriteParams& aWriteParams,
                               const PrincipalInfo& aPrincipalInfo)
{
  MOZ_CRASH("PAsmJSCacheEntryChild actors should be manually constructed!");
}

bool
BackgroundChildImpl::DeallocPAsmJSCacheEntryChild(PAsmJSCacheEntryChild* aActor)
{
  MOZ_ASSERT(aActor);

  dom::asmjscache::DeallocEntryChild(aActor);
  return true;
}

BackgroundChildImpl::PQuotaChild*
BackgroundChildImpl::AllocPQuotaChild()
{
  MOZ_CRASH("PQuotaChild actor should be manually constructed!");
}

bool
BackgroundChildImpl::DeallocPQuotaChild(PQuotaChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

dom::PFileSystemRequestChild*
BackgroundChildImpl::AllocPFileSystemRequestChild(const FileSystemParams& aParams)
{
  MOZ_CRASH("Should never get here!");
  return nullptr;
}

bool
BackgroundChildImpl::DeallocPFileSystemRequestChild(PFileSystemRequestChild* aActor)
{
  // The reference is increased in FileSystemTaskBase::Start of
  // FileSystemTaskBase.cpp. We should decrease it after IPC.
  RefPtr<dom::FileSystemTaskChildBase> child =
    dont_AddRef(static_cast<dom::FileSystemTaskChildBase*>(aActor));
  return true;
}

// Gamepad API Background IPC
dom::PGamepadEventChannelChild*
BackgroundChildImpl::AllocPGamepadEventChannelChild()
{
  MOZ_CRASH("PGamepadEventChannelChild actor should be manually constructed!");
  return nullptr;
}

bool
BackgroundChildImpl::DeallocPGamepadEventChannelChild(PGamepadEventChannelChild* aActor)
{
  MOZ_ASSERT(aActor);
  delete static_cast<dom::GamepadEventChannelChild*>(aActor);
  return true;
}

dom::PGamepadTestChannelChild*
BackgroundChildImpl::AllocPGamepadTestChannelChild()
{
  MOZ_CRASH("PGamepadTestChannelChild actor should be manually constructed!");
  return nullptr;
}

bool
BackgroundChildImpl::DeallocPGamepadTestChannelChild(PGamepadTestChannelChild* aActor)
{
  MOZ_ASSERT(aActor);
  delete static_cast<dom::GamepadTestChannelChild*>(aActor);
  return true;
}

#ifdef EARLY_BETA_OR_EARLIER
void
BackgroundChildImpl::OnChannelReceivedMessage(const Message& aMsg)
{
  if (aMsg.type() == layout::PVsync::MessageType::Msg_Notify__ID) {
    // Not really necessary to look at the message payload, it will be
    // <0.5ms away from TimeStamp::Now()
    SchedulerGroup::MarkVsyncReceived();
  }
}
#endif

dom::PWebAuthnTransactionChild*
BackgroundChildImpl::AllocPWebAuthnTransactionChild()
{
  MOZ_CRASH("PWebAuthnTransaction actor should be manually constructed!");
  return nullptr;
}

bool
BackgroundChildImpl::DeallocPWebAuthnTransactionChild(PWebAuthnTransactionChild* aActor)
{
  MOZ_ASSERT(aActor);
  RefPtr<dom::WebAuthnTransactionChild> child =
    dont_AddRef(static_cast<dom::WebAuthnTransactionChild*>(aActor));
  return true;
}

net::PHttpBackgroundChannelChild*
BackgroundChildImpl::AllocPHttpBackgroundChannelChild(const uint64_t& aChannelId)
{
  MOZ_CRASH("PHttpBackgroundChannelChild actor should be manually constructed!");
  return nullptr;
}

bool
BackgroundChildImpl::DeallocPHttpBackgroundChannelChild(PHttpBackgroundChannelChild* aActor)
{
  // The reference is increased in BackgroundChannelCreateCallback::ActorCreated
  // of HttpBackgroundChannelChild.cpp. We should decrease it after IPC
  // destroyed.
  RefPtr<net::HttpBackgroundChannelChild> child =
    dont_AddRef(static_cast<net::HttpBackgroundChannelChild*>(aActor));
  return true;
}

mozilla::ipc::IPCResult
BackgroundChildImpl::RecvDispatchLocalStorageChange(
                                            const nsString& aDocumentURI,
                                            const nsString& aKey,
                                            const nsString& aOldValue,
                                            const nsString& aNewValue,
                                            const PrincipalInfo& aPrincipalInfo,
                                            const bool& aIsPrivate)
{
  if (!NS_IsMainThread()) {
    return IPC_OK();
  }

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(aPrincipalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }

  LocalStorage::DispatchStorageEvent(aDocumentURI, aKey, aOldValue, aNewValue,
                                     principal, aIsPrivate, nullptr, true);

  return IPC_OK();
}

bool
BackgroundChildImpl::GetMessageSchedulerGroups(const Message& aMsg, nsTArray<RefPtr<SchedulerGroup>>& aGroups)
{
  if (aMsg.type() == layout::PVsync::MessageType::Msg_Notify__ID) {
    MOZ_ASSERT(NS_IsMainThread());
    aGroups.Clear();
    if (dom::TabChild::HasActiveTabs()) {
      for (dom::TabChild* tabChild : dom::TabChild::GetActiveTabs()) {
        aGroups.AppendElement(tabChild->TabGroup());
      }
    }
    return true;
  }

  return false;
}

} // namespace ipc
} // namespace mozilla

mozilla::ipc::IPCResult
TestChild::Recv__delete__(const nsCString& aTestArg)
{
  MOZ_RELEASE_ASSERT(aTestArg == mTestArg,
                     "BackgroundTest message was corrupted!");

  return IPC_OK();
}
