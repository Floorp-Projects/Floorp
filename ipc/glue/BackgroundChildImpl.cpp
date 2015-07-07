/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundChildImpl.h"

#include "ActorsChild.h" // IndexedDB
#include "BroadcastChannelChild.h"
#include "ServiceWorkerManagerChild.h"
#include "FileDescriptorSetChild.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/PBlobChild.h"
#include "mozilla/dom/cache/ActorUtils.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBFactoryChild.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/MessagePortChild.h"
#include "mozilla/ipc/PBackgroundTestChild.h"
#include "mozilla/layout/VsyncChild.h"
#include "mozilla/net/PUDPSocketChild.h"
#include "mozilla/dom/network/UDPSocketChild.h"
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
  ~TestChild()
  {
    MOZ_COUNT_DTOR(TestChild);
  }

public:
  virtual bool
  Recv__delete__(const nsCString& aTestArg) override;
};

} // anonymous namespace

namespace mozilla {
namespace ipc {

using mozilla::dom::UDPSocketChild;
using mozilla::net::PUDPSocketChild;

using mozilla::dom::cache::PCacheChild;
using mozilla::dom::cache::PCacheStorageChild;
using mozilla::dom::cache::PCacheStreamControlChild;

// -----------------------------------------------------------------------------
// BackgroundChildImpl::ThreadLocal
// -----------------------------------------------------------------------------

BackgroundChildImpl::
ThreadLocal::ThreadLocal()
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

  // This is just MOZ_CRASH() un-inlined so that we can pass the result code as
  // a string. MOZ_CRASH() only supports string literals at the moment.
  MOZ_ReportCrash(abortMessage.get(), __FILE__, __LINE__); MOZ_REALLY_CRASH();
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

auto
BackgroundChildImpl::AllocPBlobChild(const BlobConstructorParams& aParams)
  -> PBlobChild*
{
  MOZ_ASSERT(aParams.type() != BlobConstructorParams::T__None);

  return mozilla::dom::BlobChild::Create(this, aParams);
}

bool
BackgroundChildImpl::DeallocPBlobChild(PBlobChild* aActor)
{
  MOZ_ASSERT(aActor);

  mozilla::dom::BlobChild::Destroy(aActor);
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
  nsRefPtr<mozilla::layout::VsyncChild> actor = new mozilla::layout::VsyncChild();
  // There still has one ref-count after return, and it will be released in
  // DeallocPVsyncChild().
  return actor.forget().take();
}

bool
BackgroundChildImpl::DeallocPVsyncChild(PVsyncChild* aActor)
{
  MOZ_ASSERT(aActor);

  // This actor already has one ref-count. Please check AllocPVsyncChild().
  nsRefPtr<mozilla::layout::VsyncChild> actor =
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
                                                 const nsString& aChannel,
                                                 const bool& aPrivateBrowsing)
{
  nsRefPtr<dom::BroadcastChannelChild> agent =
    new dom::BroadcastChannelChild(aOrigin);
  return agent.forget().take();
}

bool
BackgroundChildImpl::DeallocPBroadcastChannelChild(
                                                 PBroadcastChannelChild* aActor)
{
  nsRefPtr<dom::BroadcastChannelChild> child =
    dont_AddRef(static_cast<dom::BroadcastChannelChild*>(aActor));
  MOZ_ASSERT(child);
  return true;
}

// -----------------------------------------------------------------------------
// ServiceWorkerManager
// -----------------------------------------------------------------------------

dom::PServiceWorkerManagerChild*
BackgroundChildImpl::AllocPServiceWorkerManagerChild()
{
  nsRefPtr<dom::workers::ServiceWorkerManagerChild> agent =
    new dom::workers::ServiceWorkerManagerChild();
  return agent.forget().take();
}

bool
BackgroundChildImpl::DeallocPServiceWorkerManagerChild(
                                             PServiceWorkerManagerChild* aActor)
{
  nsRefPtr<dom::workers::ServiceWorkerManagerChild> child =
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
  nsRefPtr<dom::MessagePortChild> agent = new dom::MessagePortChild();
  return agent.forget().take();
}

bool
BackgroundChildImpl::DeallocPMessagePortChild(PMessagePortChild* aActor)
{
  nsRefPtr<dom::MessagePortChild> child =
    dont_AddRef(static_cast<dom::MessagePortChild*>(aActor));
  MOZ_ASSERT(child);
  return true;
}

} // namespace ipc
} // namespace mozilla

bool
TestChild::Recv__delete__(const nsCString& aTestArg)
{
  MOZ_RELEASE_ASSERT(aTestArg == mTestArg,
                     "BackgroundTest message was corrupted!");

  return true;
}
