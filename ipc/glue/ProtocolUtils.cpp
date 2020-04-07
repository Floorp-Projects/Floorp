/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/process_util.h"
#include "base/task.h"

#ifdef OS_POSIX
#  include <errno.h>
#endif
#include <type_traits>

#include "mozilla/IntegerPrintfMacros.h"

#include "mozilla/ipc/ProtocolUtils.h"

#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/Transport.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Unused.h"
#include "nsPrintfCString.h"

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
#  include "mozilla/sandboxTarget.h"
#endif

#if defined(XP_WIN)
#  include "aclapi.h"
#  include "sddl.h"
#endif

using namespace IPC;

using base::GetCurrentProcId;
using base::ProcessHandle;
using base::ProcessId;

namespace mozilla {

#if defined(XP_WIN)
// Generate RAII classes for LPTSTR and PSECURITY_DESCRIPTOR.
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedLPTStr,
                                          std::remove_pointer_t<LPTSTR>,
                                          ::LocalFree)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(
    ScopedPSecurityDescriptor, std::remove_pointer_t<PSECURITY_DESCRIPTOR>,
    ::LocalFree)
#endif

namespace ipc {

IPCResult IPCResult::Fail(NotNull<IProtocol*> actor, const char* where,
                          const char* why) {
  // Calls top-level protocol to handle the error.
  nsPrintfCString errorMsg("%s %s\n", where, why);
  actor->GetIPCChannel()->Listener()->ProcessingError(
      HasResultCodes::MsgProcessingError, errorMsg.get());
  return IPCResult(false);
}

#if defined(XP_WIN)
bool DuplicateHandle(HANDLE aSourceHandle, DWORD aTargetProcessId,
                     HANDLE* aTargetHandle, DWORD aDesiredAccess,
                     DWORD aOptions) {
  // If our process is the target just duplicate the handle.
  if (aTargetProcessId == base::GetCurrentProcId()) {
    return !!::DuplicateHandle(::GetCurrentProcess(), aSourceHandle,
                               ::GetCurrentProcess(), aTargetHandle,
                               aDesiredAccess, false, aOptions);
  }

#  if defined(MOZ_SANDBOX)
  // Try the broker next (will fail if not sandboxed).
  if (SandboxTarget::Instance()->BrokerDuplicateHandle(
          aSourceHandle, aTargetProcessId, aTargetHandle, aDesiredAccess,
          aOptions)) {
    return true;
  }
#  endif

  // Finally, see if we already have access to the process.
  ScopedProcessHandle targetProcess(
      OpenProcess(PROCESS_DUP_HANDLE, FALSE, aTargetProcessId));
  if (!targetProcess) {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::IPCTransportFailureReason,
        NS_LITERAL_CSTRING("Failed to open target process."));
    return false;
  }

  return !!::DuplicateHandle(::GetCurrentProcess(), aSourceHandle,
                             targetProcess, aTargetHandle, aDesiredAccess,
                             FALSE, aOptions);
}
#endif

void AnnotateSystemError() {
  int64_t error = 0;
#if defined(XP_WIN)
  error = ::GetLastError();
#elif defined(OS_POSIX)
  error = errno;
#endif
  if (error) {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::IPCSystemError,
        nsPrintfCString("%" PRId64, error));
  }
}

#if defined(XP_MACOSX)
void AnnotateCrashReportWithErrno(CrashReporter::Annotation tag, int error) {
  CrashReporter::AnnotateCrashReport(tag, error);
}
#endif  // defined(XP_MACOSX)

void LogMessageForProtocol(const char* aTopLevelProtocol,
                           base::ProcessId aOtherPid,
                           const char* aContextDescription, uint32_t aMessageId,
                           MessageDirection aDirection) {
  nsPrintfCString logMessage(
      "[time: %" PRId64 "][%d%s%d] [%s] %s %s\n", PR_Now(),
      base::GetCurrentProcId(),
      aDirection == MessageDirection::eReceiving ? "<-" : "->", aOtherPid,
      aTopLevelProtocol, aContextDescription,
      StringFromIPCMessageType(aMessageId));
#ifdef ANDROID
  __android_log_write(ANDROID_LOG_INFO, "GeckoIPC", logMessage.get());
#endif
  fputs(logMessage.get(), stderr);
}

void ProtocolErrorBreakpoint(const char* aMsg) {
  // Bugs that generate these error messages can be tough to
  // reproduce.  Log always in the hope that someone finds the error
  // message.
  printf_stderr("IPDL protocol error: %s\n", aMsg);
}

void FatalError(const char* aMsg, bool aIsParent) {
#ifndef FUZZING
  ProtocolErrorBreakpoint(aMsg);
#endif

  nsAutoCString formattedMessage("IPDL error: \"");
  formattedMessage.AppendASCII(aMsg);
  if (aIsParent) {
    // We're going to crash the parent process because at this time
    // there's no other really nice way of getting a minidump out of
    // this process if we're off the main thread.
    formattedMessage.AppendLiteral("\". Intentionally crashing.");
    NS_ERROR(formattedMessage.get());
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::IPCFatalErrorMsg, nsDependentCString(aMsg));
    AnnotateSystemError();
#ifndef FUZZING
    MOZ_CRASH("IPC FatalError in the parent process!");
#endif
  } else {
    formattedMessage.AppendLiteral("\". abort()ing as a result.");
#ifndef FUZZING
    MOZ_CRASH_UNSAFE(formattedMessage.get());
#endif
  }
}

void LogicError(const char* aMsg) { MOZ_CRASH_UNSAFE(aMsg); }

void ActorIdReadError(const char* aActorDescription) {
#ifndef FUZZING
  MOZ_CRASH_UNSAFE_PRINTF("Error deserializing id for %s", aActorDescription);
#endif
}

void BadActorIdError(const char* aActorDescription) {
  nsPrintfCString message("bad id for %s", aActorDescription);
  ProtocolErrorBreakpoint(message.get());
}

void ActorLookupError(const char* aActorDescription) {
  nsPrintfCString message("could not lookup id for %s", aActorDescription);
  ProtocolErrorBreakpoint(message.get());
}

void MismatchedActorTypeError(const char* aActorDescription) {
  nsPrintfCString message("actor that should be of type %s has different type",
                          aActorDescription);
  ProtocolErrorBreakpoint(message.get());
}

void UnionTypeReadError(const char* aUnionName) {
  MOZ_CRASH_UNSAFE_PRINTF("error deserializing type of union %s", aUnionName);
}

void ArrayLengthReadError(const char* aElementName) {
  MOZ_CRASH_UNSAFE_PRINTF("error deserializing length of %s[]", aElementName);
}

void SentinelReadError(const char* aClassName) {
  MOZ_CRASH_UNSAFE_PRINTF("incorrect sentinel when reading %s", aClassName);
}

void TableToArray(const nsTHashtable<nsPtrHashKey<void>>& aTable,
                  nsTArray<void*>& aArray) {
  uint32_t i = 0;
  void** elements = aArray.AppendElements(aTable.Count());
  for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
    elements[i] = iter.Get()->GetKey();
    ++i;
  }
}

ActorLifecycleProxy::ActorLifecycleProxy(IProtocol* aActor) : mActor(aActor) {
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mActor->CanSend(),
             "Cannot create LifecycleProxy for non-connected actor!");

  // Take a reference to our manager's lifecycle proxy to try to hold it &
  // ensure it doesn't die before us.
  if (mActor->mManager) {
    mManager = mActor->mManager->mLifecycleProxy;
  }

  // Record that we've taken our first reference to our actor.
  mActor->ActorAlloc();
}

ActorLifecycleProxy::~ActorLifecycleProxy() {
  // When the LifecycleProxy's lifetime has come to an end, it means that the
  // actor should have its `Dealloc` method called on it. In a well-behaved
  // actor, this will release the IPC-held reference to the actor.
  //
  // If the actor has already died before the `LifecycleProxy`, the `IProtocol`
  // destructor below will clear our reference to it, preventing us from
  // performing a use-after-free here.
  if (!mActor) {
    return;
  }

  // Clear our actor's state back to inactive, and then invoke ActorDealloc.
  MOZ_ASSERT(mActor->mLinkStatus == LinkStatus::Destroyed,
             "Deallocating non-destroyed actor!");
  mActor->mLifecycleProxy = nullptr;
  mActor->mLinkStatus = LinkStatus::Inactive;
  mActor->ActorDealloc();
  mActor = nullptr;
}

IProtocol::~IProtocol() {
  // If the actor still has a lifecycle proxy when it is being torn down, it
  // means that IPC was not given control over the lifecycle of the actor
  // correctly. Usually this means that the actor was destroyed while IPC is
  // calling a message handler for it, and the actor incorrectly frees itself
  // during that operation.
  //
  // As this happens unfortunately frequently, due to many odd protocols in
  // Gecko, simply emit a warning and clear the weak backreference from our
  // LifecycleProxy back to us.
  if (mLifecycleProxy) {
    // FIXME: It would be nice to have this print out the name of the
    // misbehaving actor, to help people notice it's their fault!
    NS_WARNING(
        "Actor destructor called before IPC lifecycle complete!\n"
        "References to this actor may unexpectedly dangle!");

    mLifecycleProxy->mActor = nullptr;

    // If we are somehow being destroyed while active, make sure that the
    // existing IPC reference has been freed. If the status of the actor is
    // `Destroyed`, the reference has already been freed, and we shouldn't free
    // it a second time.
    MOZ_ASSERT(mLinkStatus != LinkStatus::Inactive);
    if (mLinkStatus != LinkStatus::Destroyed) {
      NS_IF_RELEASE(mLifecycleProxy);
    }
    mLifecycleProxy = nullptr;
  }
}

// The following methods either directly forward to the toplevel protocol, or
// almost directly do.
int32_t IProtocol::Register(IProtocol* aRouted) {
  return mToplevel->Register(aRouted);
}
int32_t IProtocol::RegisterID(IProtocol* aRouted, int32_t aId) {
  return mToplevel->RegisterID(aRouted, aId);
}
IProtocol* IProtocol::Lookup(int32_t aId) { return mToplevel->Lookup(aId); }
void IProtocol::Unregister(int32_t aId) {
  if (aId == mId) {
    mId = kFreedActorId;
  }
  return mToplevel->Unregister(aId);
}

Shmem::SharedMemory* IProtocol::CreateSharedMemory(
    size_t aSize, SharedMemory::SharedMemoryType aType, bool aUnsafe,
    int32_t* aId) {
  return mToplevel->CreateSharedMemory(aSize, aType, aUnsafe, aId);
}
Shmem::SharedMemory* IProtocol::LookupSharedMemory(int32_t aId) {
  return mToplevel->LookupSharedMemory(aId);
}
bool IProtocol::IsTrackingSharedMemory(Shmem::SharedMemory* aSegment) {
  return mToplevel->IsTrackingSharedMemory(aSegment);
}
bool IProtocol::DestroySharedMemory(Shmem& aShmem) {
  return mToplevel->DestroySharedMemory(aShmem);
}

MessageChannel* IProtocol::GetIPCChannel() {
  return mToplevel->GetIPCChannel();
}
const MessageChannel* IProtocol::GetIPCChannel() const {
  return mToplevel->GetIPCChannel();
}

void IProtocol::SetEventTargetForActor(IProtocol* aActor,
                                       nsIEventTarget* aEventTarget) {
  // Make sure we have a manager for the internal method to access.
  aActor->SetManager(this);
  mToplevel->SetEventTargetForActorInternal(aActor, aEventTarget);
}

void IProtocol::ReplaceEventTargetForActor(IProtocol* aActor,
                                           nsIEventTarget* aEventTarget) {
  MOZ_ASSERT(aActor->Manager());
  mToplevel->ReplaceEventTargetForActor(aActor, aEventTarget);
}

void IProtocol::SetEventTargetForRoute(int32_t aRoute,
                                       nsIEventTarget* aEventTarget) {
  mToplevel->SetEventTargetForRoute(aRoute, aEventTarget);
}

nsIEventTarget* IProtocol::GetActorEventTarget() {
  // FIXME: It's a touch sketchy that we don't return a strong reference here.
  RefPtr<nsIEventTarget> target = GetActorEventTarget(this);
  return target;
}
already_AddRefed<nsIEventTarget> IProtocol::GetActorEventTarget(
    IProtocol* aActor) {
  return mToplevel->GetActorEventTarget(aActor);
}

ProcessId IProtocol::OtherPid() const { return mToplevel->OtherPid(); }

void IProtocol::SetId(int32_t aId) {
  MOZ_ASSERT(mId == aId || mLinkStatus == LinkStatus::Inactive);
  mId = aId;
}

Maybe<IProtocol*> IProtocol::ReadActor(const IPC::Message* aMessage,
                                       PickleIterator* aIter, bool aNullable,
                                       const char* aActorDescription,
                                       int32_t aProtocolTypeId) {
  int32_t id;
  if (!IPC::ReadParam(aMessage, aIter, &id)) {
    ActorIdReadError(aActorDescription);
    return Nothing();
  }

  if (id == 1 || (id == 0 && !aNullable)) {
    BadActorIdError(aActorDescription);
    return Nothing();
  }

  if (id == 0) {
    return Some(static_cast<IProtocol*>(nullptr));
  }

  IProtocol* listener = this->Lookup(id);
  if (!listener) {
    ActorLookupError(aActorDescription);
    return Nothing();
  }

  if (listener->GetProtocolId() != aProtocolTypeId) {
    MismatchedActorTypeError(aActorDescription);
    return Nothing();
  }

  return Some(listener);
}

void IProtocol::FatalError(const char* const aErrorMsg) const {
  HandleFatalError(aErrorMsg);
}

void IProtocol::HandleFatalError(const char* aErrorMsg) const {
  if (IProtocol* manager = Manager()) {
    manager->HandleFatalError(aErrorMsg);
    return;
  }

  mozilla::ipc::FatalError(aErrorMsg, mSide == ParentSide);
}

bool IProtocol::AllocShmem(size_t aSize,
                           Shmem::SharedMemory::SharedMemoryType aType,
                           Shmem* aOutMem) {
  Shmem::id_t id;
  Shmem::SharedMemory* rawmem(CreateSharedMemory(aSize, aType, false, &id));
  if (!rawmem) {
    return false;
  }

  *aOutMem = Shmem(Shmem::PrivateIPDLCaller(), rawmem, id);
  return true;
}

bool IProtocol::AllocUnsafeShmem(size_t aSize,
                                 Shmem::SharedMemory::SharedMemoryType aType,
                                 Shmem* aOutMem) {
  Shmem::id_t id;
  Shmem::SharedMemory* rawmem(CreateSharedMemory(aSize, aType, true, &id));
  if (!rawmem) {
    return false;
  }

  *aOutMem = Shmem(Shmem::PrivateIPDLCaller(), rawmem, id);
  return true;
}

bool IProtocol::DeallocShmem(Shmem& aMem) {
  bool ok = DestroySharedMemory(aMem);
#ifdef DEBUG
  if (!ok) {
    if (mSide == ChildSide) {
      FatalError("bad Shmem");
    } else {
      NS_WARNING("bad Shmem");
    }
    return false;
  }
#endif  // DEBUG
  aMem.forget(Shmem::PrivateIPDLCaller());
  return ok;
}

void IProtocol::SetManager(IProtocol* aManager) {
  MOZ_RELEASE_ASSERT(!mManager || mManager == aManager);
  mManager = aManager;
  mToplevel = aManager->mToplevel;
}

void IProtocol::SetManagerAndRegister(IProtocol* aManager) {
  // Set the manager prior to registering so registering properly inherits
  // the manager's event target.
  SetManager(aManager);

  aManager->Register(this);
}

void IProtocol::SetManagerAndRegister(IProtocol* aManager, int32_t aId) {
  // Set the manager prior to registering so registering properly inherits
  // the manager's event target.
  SetManager(aManager);

  aManager->RegisterID(this, aId);
}

bool IProtocol::ChannelSend(IPC::Message* aMsg) {
  UniquePtr<IPC::Message> msg(aMsg);
  if (CanSend()) {
    // NOTE: This send call failing can only occur during toplevel channel
    // teardown. As this is an async call, this isn't reasonable to predict or
    // respond to, so just drop the message on the floor silently.
    GetIPCChannel()->Send(msg.release());
    return true;
  }

  NS_WARNING("IPC message discarded: actor cannot send");
  return false;
}

bool IProtocol::ChannelSend(IPC::Message* aMsg, IPC::Message* aReply) {
  UniquePtr<IPC::Message> msg(aMsg);
  if (CanSend()) {
    return GetIPCChannel()->Send(msg.release(), aReply);
  }

  NS_WARNING("IPC message discarded: actor cannot send");
  return false;
}

bool IProtocol::ChannelCall(IPC::Message* aMsg, IPC::Message* aReply) {
  UniquePtr<IPC::Message> msg(aMsg);
  if (CanSend()) {
    return GetIPCChannel()->Call(msg.release(), aReply);
  }

  NS_WARNING("IPC message discarded: actor cannot send");
  return false;
}

void IProtocol::ActorConnected() {
  if (mLinkStatus != LinkStatus::Inactive) {
    return;
  }

  mLinkStatus = LinkStatus::Connected;

  MOZ_ASSERT(!mLifecycleProxy, "double-connecting live actor");
  mLifecycleProxy = new ActorLifecycleProxy(this);
  NS_ADDREF(mLifecycleProxy);  // Reference freed in DestroySubtree();
}

void IProtocol::DoomSubtree() {
  MOZ_ASSERT(CanSend(), "dooming non-connected actor");
  MOZ_ASSERT(mLifecycleProxy, "dooming zombie actor");

  nsTArray<RefPtr<ActorLifecycleProxy>> managed;
  AllManagedActors(managed);
  for (ActorLifecycleProxy* proxy : managed) {
    // Guard against actor being disconnected or destroyed during previous Doom
    IProtocol* actor = proxy->Get();
    if (actor && actor->CanSend()) {
      actor->DoomSubtree();
    }
  }

  // ActorDoom is called immediately before changing state, this allows messages
  // to be sent during ActorDoom immediately before the channel is closed and
  // sending messages is disabled.
  ActorDoom();
  mLinkStatus = LinkStatus::Doomed;
}

void IProtocol::DestroySubtree(ActorDestroyReason aWhy) {
  MOZ_ASSERT(CanRecv(), "destroying non-connected actor");
  MOZ_ASSERT(mLifecycleProxy, "destroying zombie actor");

  // If we're a managed actor, unregister from our manager
  if (Manager()) {
    Unregister(Id());
  }

  // Destroy subtree
  ActorDestroyReason subtreeWhy = aWhy;
  if (aWhy == Deletion || aWhy == FailedConstructor) {
    subtreeWhy = AncestorDeletion;
  }

  nsTArray<RefPtr<ActorLifecycleProxy>> managed;
  AllManagedActors(managed);
  for (ActorLifecycleProxy* proxy : managed) {
    // Guard against actor being disconnected or destroyed during previous
    // Destroy
    IProtocol* actor = proxy->Get();
    if (actor && actor->CanRecv()) {
      actor->DestroySubtree(subtreeWhy);
    }
  }

  // Ensure that we don't send any messages while we're calling `ActorDestroy`
  // by setting our state to `Doomed`.
  mLinkStatus = LinkStatus::Doomed;

  // The actor is being destroyed, reject any pending responses, invoke
  // `ActorDestroy` to destroy it, and then clear our status to
  // `LinkStatus::Destroyed`.
  GetIPCChannel()->RejectPendingResponsesForActor(this);
  ActorDestroy(aWhy);
  mLinkStatus = LinkStatus::Destroyed;
}

IToplevelProtocol::IToplevelProtocol(const char* aName, ProtocolId aProtoId,
                                     Side aSide)
    : IProtocol(aProtoId, aSide),
      mOtherPid(mozilla::ipc::kInvalidProcessId),
      mLastLocalId(0),
      mEventTargetMutex("ProtocolEventTargetMutex"),
      mChannel(aName, this) {
  mToplevel = this;
}

base::ProcessId IToplevelProtocol::OtherPid() const {
  base::ProcessId pid = OtherPidMaybeInvalid();
  MOZ_RELEASE_ASSERT(pid != kInvalidProcessId);
  return pid;
}

void IToplevelProtocol::SetOtherProcessId(base::ProcessId aOtherPid) {
  mOtherPid = aOtherPid;
}

bool IToplevelProtocol::Open(UniquePtr<Transport> aTransport,
                             base::ProcessId aOtherPid, MessageLoop* aThread,
                             mozilla::ipc::Side aSide) {
  SetOtherProcessId(aOtherPid);
  return GetIPCChannel()->Open(std::move(aTransport), aThread, aSide);
}

bool IToplevelProtocol::Open(MessageChannel* aChannel,
                             MessageLoop* aMessageLoop,
                             mozilla::ipc::Side aSide) {
  SetOtherProcessId(base::GetCurrentProcId());
  return GetIPCChannel()->Open(aChannel, aMessageLoop->SerialEventTarget(),
                               aSide);
}

bool IToplevelProtocol::Open(MessageChannel* aChannel,
                             nsIEventTarget* aEventTarget,
                             mozilla::ipc::Side aSide) {
  SetOtherProcessId(base::GetCurrentProcId());
  return GetIPCChannel()->Open(aChannel, aEventTarget, aSide);
}

bool IToplevelProtocol::OpenOnSameThread(MessageChannel* aChannel, Side aSide) {
  SetOtherProcessId(base::GetCurrentProcId());
  return GetIPCChannel()->OpenOnSameThread(aChannel, aSide);
}

void IToplevelProtocol::Close() { GetIPCChannel()->Close(); }

void IToplevelProtocol::SetReplyTimeoutMs(int32_t aTimeoutMs) {
  GetIPCChannel()->SetReplyTimeoutMs(aTimeoutMs);
}

bool IToplevelProtocol::IsOnCxxStack() const {
  return GetIPCChannel()->IsOnCxxStack();
}

int32_t IToplevelProtocol::NextId() {
  // Genreate the next ID to use for a shared memory or protocol. Parent and
  // Child sides of the protocol use different pools.
  int32_t tag = 0;
  if (GetSide() == ParentSide) {
    tag |= 1 << 1;
  }

  // Check any overflow
  MOZ_RELEASE_ASSERT(mLastLocalId < (1 << 29));

  // Compute the ID to use with the low two bits as our tag, and the remaining
  // bits as a monotonic.
  return (++mLastLocalId << 2) | tag;
}

int32_t IToplevelProtocol::Register(IProtocol* aRouted) {
  if (aRouted->Id() != kNullActorId && aRouted->Id() != kFreedActorId) {
    // If there's already an ID, just return that.
    return aRouted->Id();
  }
  int32_t id = RegisterID(aRouted, NextId());

  // Inherit our event target from our manager.
  if (IProtocol* manager = aRouted->Manager()) {
    MutexAutoLock lock(mEventTargetMutex);
    if (nsCOMPtr<nsIEventTarget> target =
            mEventTargetMap.Lookup(manager->Id())) {
      mEventTargetMap.AddWithID(target, id);
    }
  }

  return id;
}

int32_t IToplevelProtocol::RegisterID(IProtocol* aRouted, int32_t aId) {
  aRouted->SetId(aId);
  aRouted->ActorConnected();
  mActorMap.AddWithID(aRouted, aId);
  return aId;
}

IProtocol* IToplevelProtocol::Lookup(int32_t aId) {
  return mActorMap.Lookup(aId);
}

void IToplevelProtocol::Unregister(int32_t aId) {
  mActorMap.Remove(aId);

  MutexAutoLock lock(mEventTargetMutex);
  mEventTargetMap.RemoveIfPresent(aId);
}

Shmem::SharedMemory* IToplevelProtocol::CreateSharedMemory(
    size_t aSize, Shmem::SharedMemory::SharedMemoryType aType, bool aUnsafe,
    Shmem::id_t* aId) {
  RefPtr<Shmem::SharedMemory> segment(
      Shmem::Alloc(Shmem::PrivateIPDLCaller(), aSize, aType, aUnsafe));
  if (!segment) {
    return nullptr;
  }
  int32_t id = NextId();
  Shmem shmem(Shmem::PrivateIPDLCaller(), segment.get(), id);

  base::ProcessId pid =
#ifdef ANDROID
      // We use OtherPidMaybeInvalid() because on Android this method is
      // actually called on an unconnected protocol, but Android's shared memory
      // implementation doesn't actually use the PID.
      OtherPidMaybeInvalid();
#else
      OtherPid();
#endif

  Message* descriptor =
      shmem.ShareTo(Shmem::PrivateIPDLCaller(), pid, MSG_ROUTING_CONTROL);
  if (!descriptor) {
    return nullptr;
  }
  Unused << GetIPCChannel()->Send(descriptor);

  *aId = shmem.Id(Shmem::PrivateIPDLCaller());
  Shmem::SharedMemory* rawSegment = segment.get();
  mShmemMap.AddWithID(segment.forget().take(), *aId);
  return rawSegment;
}

Shmem::SharedMemory* IToplevelProtocol::LookupSharedMemory(Shmem::id_t aId) {
  return mShmemMap.Lookup(aId);
}

bool IToplevelProtocol::IsTrackingSharedMemory(Shmem::SharedMemory* segment) {
  return mShmemMap.HasData(segment);
}

bool IToplevelProtocol::DestroySharedMemory(Shmem& shmem) {
  Shmem::id_t aId = shmem.Id(Shmem::PrivateIPDLCaller());
  Shmem::SharedMemory* segment = LookupSharedMemory(aId);
  if (!segment) {
    return false;
  }

  Message* descriptor =
      shmem.UnshareFrom(Shmem::PrivateIPDLCaller(), MSG_ROUTING_CONTROL);

  mShmemMap.Remove(aId);
  Shmem::Dealloc(Shmem::PrivateIPDLCaller(), segment);

  MessageChannel* channel = GetIPCChannel();
  if (!channel->CanSend()) {
    delete descriptor;
    return true;
  }

  return descriptor && channel->Send(descriptor);
}

void IToplevelProtocol::DeallocShmems() {
  for (IDMap<SharedMemory*>::const_iterator cit = mShmemMap.begin();
       cit != mShmemMap.end(); ++cit) {
    Shmem::Dealloc(Shmem::PrivateIPDLCaller(), cit->second);
  }
  mShmemMap.Clear();
}

bool IToplevelProtocol::ShmemCreated(const Message& aMsg) {
  Shmem::id_t id;
  RefPtr<Shmem::SharedMemory> rawmem(
      Shmem::OpenExisting(Shmem::PrivateIPDLCaller(), aMsg, &id, true));
  if (!rawmem) {
    return false;
  }
  mShmemMap.AddWithID(rawmem.forget().take(), id);
  return true;
}

bool IToplevelProtocol::ShmemDestroyed(const Message& aMsg) {
  Shmem::id_t id;
  PickleIterator iter = PickleIterator(aMsg);
  if (!IPC::ReadParam(&aMsg, &iter, &id)) {
    return false;
  }
  aMsg.EndRead(iter);

  Shmem::SharedMemory* rawmem = LookupSharedMemory(id);
  if (rawmem) {
    mShmemMap.Remove(id);
    Shmem::Dealloc(Shmem::PrivateIPDLCaller(), rawmem);
  }
  return true;
}

already_AddRefed<nsIEventTarget> IToplevelProtocol::GetMessageEventTarget(
    const Message& aMsg) {
  int32_t route = aMsg.routing_id();

  Maybe<MutexAutoLock> lock;
  lock.emplace(mEventTargetMutex);

  nsCOMPtr<nsIEventTarget> target = mEventTargetMap.Lookup(route);

  if (aMsg.is_constructor()) {
    ActorHandle handle;
    PickleIterator iter = PickleIterator(aMsg);
    if (!IPC::ReadParam(&aMsg, &iter, &handle)) {
      return nullptr;
    }

    // Normally a new actor inherits its event target from its manager. If the
    // manager has no event target, we give the subclass a chance to make a new
    // one.
    if (!target) {
      MutexAutoUnlock unlock(mEventTargetMutex);
      target = GetConstructedEventTarget(aMsg);
    }

#ifdef DEBUG
    // If this function is called more than once for the same message,
    // the actor handle ID will already be in the map and the AddWithID
    // call below will trigger a crash in DEBUG builds. Avoid this by
    // removing the entry first and ASSERTing that if the ID has already
    // been inserted, it matches the provided |aMsg| ID. If the ASSERT fails,
    // the map contains a different event target which is unexpected.
    nsCOMPtr<nsIEventTarget> existingTgt = mEventTargetMap.Lookup(handle.mId);
    MOZ_ASSERT(existingTgt == target || existingTgt == nullptr);
    mEventTargetMap.RemoveIfPresent(handle.mId);
#endif /* DEBUG */

    mEventTargetMap.AddWithID(target, handle.mId);
  } else if (!target) {
    // We don't need the lock after this point.
    lock.reset();

    target = GetSpecificMessageEventTarget(aMsg);
  }

  return target.forget();
}

already_AddRefed<nsIEventTarget> IToplevelProtocol::GetActorEventTarget(
    IProtocol* aActor) {
  MOZ_RELEASE_ASSERT(aActor->Id() != kNullActorId &&
                     aActor->Id() != kFreedActorId);

  MutexAutoLock lock(mEventTargetMutex);
  nsCOMPtr<nsIEventTarget> target = mEventTargetMap.Lookup(aActor->Id());
  return target.forget();
}

nsIEventTarget* IToplevelProtocol::GetActorEventTarget() {
  // The EventTarget of a ToplevelProtocol shall never be set.
  return nullptr;
}

void IToplevelProtocol::SetEventTargetForActorInternal(
    IProtocol* aActor, nsIEventTarget* aEventTarget) {
  // The EventTarget of a ToplevelProtocol shall never be set.
  MOZ_RELEASE_ASSERT(aActor != this);

  // We should only call this function on actors that haven't been used for IPC
  // code yet. Otherwise we'll be posting stuff to the wrong event target before
  // we're called.
  MOZ_RELEASE_ASSERT(aActor->Id() == kNullActorId ||
                     aActor->Id() == kFreedActorId);

  MOZ_ASSERT(aActor->Manager() && aActor->ToplevelProtocol() == this);

  // Register the actor early. When it's registered again, it will keep the same
  // ID.
  int32_t id = Register(aActor);
  aActor->SetId(id);

  MutexAutoLock lock(mEventTargetMutex);
  // FIXME bug 1445121 - sometimes the id is already mapped.
  // (IDMap debug-asserts that the existing state is as expected.)
  bool replace = false;
#ifdef DEBUG
  replace = mEventTargetMap.Lookup(id) != nullptr;
#endif
  if (replace) {
    mEventTargetMap.ReplaceWithID(aEventTarget, id);
  } else {
    mEventTargetMap.AddWithID(aEventTarget, id);
  }
}

void IToplevelProtocol::ReplaceEventTargetForActor(
    IProtocol* aActor, nsIEventTarget* aEventTarget) {
  // The EventTarget of a ToplevelProtocol shall never be set.
  MOZ_RELEASE_ASSERT(aActor != this);

  int32_t id = aActor->Id();
  // The ID of the actor should have existed.
  MOZ_RELEASE_ASSERT(id != kNullActorId && id != kFreedActorId);

  MutexAutoLock lock(mEventTargetMutex);
  mEventTargetMap.ReplaceWithID(aEventTarget, id);
}

void IToplevelProtocol::SetEventTargetForRoute(int32_t aRoute,
                                               nsIEventTarget* aEventTarget) {
  MOZ_RELEASE_ASSERT(aRoute != Id());
  MOZ_RELEASE_ASSERT(aRoute != kNullActorId && aRoute != kFreedActorId);

  MutexAutoLock lock(mEventTargetMutex);
  MOZ_ASSERT(!mEventTargetMap.Lookup(aRoute));
  mEventTargetMap.AddWithID(aEventTarget, aRoute);
}

}  // namespace ipc
}  // namespace mozilla
