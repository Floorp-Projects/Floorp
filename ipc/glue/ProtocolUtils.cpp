/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/process_util.h"
#include "base/task.h"

#ifdef XP_UNIX
#  include <errno.h>
#endif
#include <type_traits>

#include "mozilla/IntegerPrintfMacros.h"

#include "mozilla/ipc/ProtocolMessageUtils.h"
#include "mozilla/ipc/ProtocolUtils.h"

#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/StaticMutex.h"
#if defined(DEBUG) || defined(FUZZING)
#  include "mozilla/Tokenizer.h"
#endif
#include "mozilla/Unused.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
#  include "mozilla/sandboxTarget.h"
#endif

#if defined(XP_WIN)
#  include "aclapi.h"
#  include "sddl.h"
#endif

#ifdef FUZZING_SNAPSHOT
#  include "mozilla/fuzzing/IPCFuzzController.h"
#endif

using namespace IPC;

using base::GetCurrentProcId;
using base::ProcessHandle;
using base::ProcessId;

namespace mozilla {
namespace ipc {

/* static */
IPCResult IPCResult::FailImpl(NotNull<IProtocol*> actor, const char* where,
                              const char* why) {
  // Calls top-level protocol to handle the error.
  nsPrintfCString errorMsg("%s %s\n", where, why);
  actor->GetIPCChannel()->Listener()->ProcessingError(
      HasResultCodes::MsgProcessingError, errorMsg.get());

#if defined(DEBUG) && !defined(FUZZING)
  // We do not expect IPC_FAIL to ever happen in normal operations. If this
  // happens in DEBUG, we most likely see some behavior during a test we should
  // really investigate.
  nsPrintfCString crashMsg(
      "Use IPC_FAIL only in an "
      "unrecoverable, unexpected state: %s",
      errorMsg.get());
  // We already leak the same information potentially on child process failures
  // even in release, and here we are only in DEBUG.
  MOZ_CRASH_UNSAFE(crashMsg.get());
#else
  return IPCResult(false);
#endif
}

/* static */
IPCResult IPCResult::FailForTesting(NotNull<IProtocol*> actor,
                                    const char* where, const char* why) {
  return IPCResult(false);
}

void AnnotateSystemError() {
  uint32_t error = 0;
#if defined(XP_WIN)
  error = ::GetLastError();
#else
  error = errno;
#endif
  if (error) {
    CrashReporter::RecordAnnotationU32(
        CrashReporter::Annotation::IPCSystemError, error);
  }
}

#if defined(XP_MACOSX)
void AnnotateCrashReportWithErrno(CrashReporter::Annotation tag, int error) {
  CrashReporter::RecordAnnotationU32(tag, static_cast<uint32_t>(error));
}
#endif  // defined(XP_MACOSX)

#if defined(DEBUG) || defined(FUZZING)
// If aTopLevelProtocol matches any token in aFilter, return true.
//
// aTopLevelProtocol is a protocol name, without the "Parent" / "Child" suffix.
// aSide indicates whether we're logging parent-side or child-side activity.
//
// aFilter is a list of protocol names separated by commas and/or
// spaces. These may include the "Child" / "Parent" suffix, or omit
// the suffix to log activity on both sides.
//
// This overload is for testability; application code should use the single-
// argument version (defined in the ProtocolUtils.h) which takes the filter from
// the environment.
bool LoggingEnabledFor(const char* aTopLevelProtocol, Side aSide,
                       const char* aFilter) {
  if (!aFilter) {
    return false;
  }
  if (strcmp(aFilter, "1") == 0) {
    return true;
  }

  const char kDelimiters[] = ", ";
  Tokenizer tokens(aFilter, kDelimiters);
  Tokenizer::Token t;
  while (tokens.Next(t)) {
    if (t.Type() == Tokenizer::TOKEN_WORD) {
      auto filter = t.AsString();

      // Since aTopLevelProtocol never includes the "Parent" / "Child" suffix,
      // this will only occur when filter doesn't include it either, meaning
      // that we should log activity on both sides.
      if (filter == aTopLevelProtocol) {
        return true;
      }

      if (aSide == ParentSide &&
          StringEndsWith(filter, nsDependentCString("Parent")) &&
          Substring(filter, 0, filter.Length() - 6) == aTopLevelProtocol) {
        return true;
      }

      if (aSide == ChildSide &&
          StringEndsWith(filter, nsDependentCString("Child")) &&
          Substring(filter, 0, filter.Length() - 5) == aTopLevelProtocol) {
        return true;
      }
    }
  }

  return false;
}
#endif  // defined(DEBUG) || defined(FUZZING)

void LogMessageForProtocol(const char* aTopLevelProtocol,
                           base::ProcessId aOtherPid,
                           const char* aContextDescription, uint32_t aMessageId,
                           MessageDirection aDirection) {
  nsPrintfCString logMessage(
      "[time: %" PRId64 "][%" PRIPID "%s%" PRIPID "] [%s] %s %s\n", PR_Now(),
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

void PickleFatalError(const char* aMsg, IProtocol* aActor) {
  if (aActor) {
    aActor->FatalError(aMsg);
  } else {
    FatalError(aMsg, false);
  }
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
    CrashReporter::RecordAnnotationCString(
        CrashReporter::Annotation::IPCFatalErrorMsg, aMsg);
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

ActorLifecycleProxy::ActorLifecycleProxy(IProtocol* aActor) : mActor(aActor) {
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mActor->CanSend(),
             "Cannot create LifecycleProxy for non-connected actor!");

  // Record that we've taken our first reference to our actor.
  mActor->ActorAlloc();
}

WeakActorLifecycleProxy* ActorLifecycleProxy::GetWeakProxy() {
  if (!mWeakProxy) {
    mWeakProxy = new WeakActorLifecycleProxy(this);
  }
  return mWeakProxy;
}

ActorLifecycleProxy::~ActorLifecycleProxy() {
  if (mWeakProxy) {
    mWeakProxy->mProxy = nullptr;
    mWeakProxy = nullptr;
  }

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

WeakActorLifecycleProxy::WeakActorLifecycleProxy(ActorLifecycleProxy* aProxy)
    : mProxy(aProxy), mActorEventTarget(GetCurrentSerialEventTarget()) {}

WeakActorLifecycleProxy::~WeakActorLifecycleProxy() {
  MOZ_DIAGNOSTIC_ASSERT(!mProxy, "Destroyed before mProxy was cleared?");
}

IProtocol* WeakActorLifecycleProxy::Get() const {
  MOZ_DIAGNOSTIC_ASSERT(mActorEventTarget->IsOnCurrentThread());
  return mProxy ? mProxy->Get() : nullptr;
}

WeakActorLifecycleProxy* IProtocol::GetWeakLifecycleProxy() {
  return mLifecycleProxy ? mLifecycleProxy->GetWeakProxy() : nullptr;
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
    MOZ_ASSERT(mLinkStatus != LinkStatus::Inactive);
    NS_WARNING(
        nsPrintfCString("Actor destructor for '%s%s' called before IPC "
                        "lifecycle complete!\n"
                        "References to this actor may unexpectedly dangle!",
                        GetProtocolName(), StringFromIPCSide(GetSide()))
            .get());

    mLifecycleProxy->mActor = nullptr;
    mLifecycleProxy = nullptr;
  }
}

// The following methods either directly forward to the toplevel protocol, or
// almost directly do.
IProtocol* IProtocol::Lookup(int32_t aId) { return mToplevel->Lookup(aId); }

Shmem::SharedMemory* IProtocol::CreateSharedMemory(size_t aSize, bool aUnsafe,
                                                   int32_t* aId) {
  return mToplevel->CreateSharedMemory(aSize, aUnsafe, aId);
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

nsISerialEventTarget* IProtocol::GetActorEventTarget() {
  return GetIPCChannel()->GetWorkerEventTarget();
}

Maybe<IProtocol*> IProtocol::ReadActor(IPC::MessageReader* aReader,
                                       bool aNullable,
                                       const char* aActorDescription,
                                       int32_t aProtocolTypeId) {
  int32_t id;
  if (!IPC::ReadParam(aReader, &id)) {
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

void IProtocol::FatalError(const char* const aErrorMsg) {
  HandleFatalError(aErrorMsg);
}

void IProtocol::HandleFatalError(const char* aErrorMsg) {
  if (IProtocol* manager = Manager()) {
    manager->HandleFatalError(aErrorMsg);
    return;
  }

  mozilla::ipc::FatalError(aErrorMsg, mSide == ParentSide);
  if (CanSend()) {
    GetIPCChannel()->InduceConnectionError();
  }
}

bool IProtocol::AllocShmem(size_t aSize, Shmem* aOutMem) {
  if (!CanSend()) {
    NS_WARNING(
        "Shmem not allocated.  Cannot communicate with the other actor.");
    return false;
  }

  Shmem::id_t id;
  Shmem::SharedMemory* rawmem(CreateSharedMemory(aSize, false, &id));
  if (!rawmem) {
    return false;
  }

  *aOutMem = Shmem(rawmem, id, aSize, false);
  return true;
}

bool IProtocol::AllocUnsafeShmem(size_t aSize, Shmem* aOutMem) {
  if (!CanSend()) {
    NS_WARNING(
        "Shmem not allocated.  Cannot communicate with the other actor.");
    return false;
  }

  Shmem::id_t id;
  Shmem::SharedMemory* rawmem(CreateSharedMemory(aSize, true, &id));
  if (!rawmem) {
    return false;
  }

  *aOutMem = Shmem(rawmem, id, aSize, true);
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
  aMem.forget();
  return ok;
}

void IProtocol::SetManager(IRefCountedProtocol* aManager) {
  MOZ_RELEASE_ASSERT(!mManager || mManager == aManager);
  mManager = aManager;
  mToplevel = aManager->mToplevel;
}

bool IProtocol::SetManagerAndRegister(IRefCountedProtocol* aManager,
                                      int32_t aId) {
  MOZ_RELEASE_ASSERT(mLinkStatus == LinkStatus::Inactive,
                     "Actor must be inactive to SetManagerAndRegister");

  // Set to `false` if the actor is to be torn down after registration.
  bool success = true;

  // Set the manager prior to registering so registering properly inherits
  // the manager's event target.
  SetManager(aManager);

  mId = aId == kNullActorId ? mToplevel->NextId() : aId;
  while (mToplevel->mActorMap.Contains(mId)) {
    // The ID already existing is an error case, but we want to proceed with
    // registration so that we can tear down the actor cleanly - generate a new
    // ID for that case.
    NS_WARNING("Actor already exists with the selected ID!");
    mId = mToplevel->NextId();
    success = false;
  }

  RefPtr<ActorLifecycleProxy> proxy = ActorConnected();
  mToplevel->mActorMap.InsertOrUpdate(mId, proxy);
  MOZ_ASSERT(proxy->Get() == this);

  UntypedManagedContainer* container =
      aManager->GetManagedActors(GetProtocolId());
  if (container) {
    container->Insert(this);
  } else {
    NS_WARNING("Manager does not manage actors with this ProtocolId");
    success = false;
  }

  // If our manager is already dying, mark ourselves as doomed as well.
  if (aManager && aManager->mLinkStatus != LinkStatus::Connected) {
    mLinkStatus = LinkStatus::Doomed;
    if (aManager->mLinkStatus != LinkStatus::Doomed) {
      // Our manager is already fully dead, make sure we call
      // `ActorDisconnected`.
      success = false;
    }
  }

  // If setting the manager failed, call `ActorDisconnected` and return false.
  if (!success) {
    ActorDisconnected(FailedConstructor);
    MOZ_ASSERT(mLinkStatus == LinkStatus::Destroyed);
    return false;
  }
  return true;
}

void IProtocol::UnlinkManager() {
  mToplevel = nullptr;
  mManager = nullptr;
}

bool IProtocol::ChannelSend(UniquePtr<IPC::Message> aMsg) {
  if (CanSend()) {
    // NOTE: This send call failing can only occur during toplevel channel
    // teardown. As this is an async call, this isn't reasonable to predict or
    // respond to, so just drop the message on the floor silently.
    GetIPCChannel()->Send(std::move(aMsg));
    return true;
  }

  WarnMessageDiscarded(aMsg.get());
  return false;
}

bool IProtocol::ChannelSend(UniquePtr<IPC::Message> aMsg,
                            UniquePtr<IPC::Message>* aReply) {
  if (CanSend()) {
    return GetIPCChannel()->Send(std::move(aMsg), aReply);
  }

  WarnMessageDiscarded(aMsg.get());
  return false;
}

#ifdef DEBUG
void IProtocol::WarnMessageDiscarded(IPC::Message* aMsg) {
  NS_WARNING(nsPrintfCString("IPC message '%s' discarded: actor cannot send",
                             aMsg->name())
                 .get());
}
#endif

uint32_t IProtocol::AllManagedActorsCount() const {
  uint32_t total = 0;
  for (ProtocolId id : ManagedProtocolIds()) {
    total += GetManagedActors(id)->Count();
  }
  return total;
}

already_AddRefed<ActorLifecycleProxy> IProtocol::ActorConnected() {
  if (mLinkStatus != LinkStatus::Inactive) {
    return nullptr;
  }

#ifdef FUZZING_SNAPSHOT
  fuzzing::IPCFuzzController::instance().OnActorConnected(this);
#endif

  mLinkStatus = LinkStatus::Connected;

  MOZ_ASSERT(!mLifecycleProxy, "double-connecting live actor");
  RefPtr<ActorLifecycleProxy> proxy = new ActorLifecycleProxy(this);
  mLifecycleProxy = proxy;
  return proxy.forget();
}

void IProtocol::ActorDisconnected(ActorDestroyReason aWhy) {
  MOZ_ASSERT(mLifecycleProxy, "destroying zombie actor");
  // If the actor has already been marked as `Destroyed`, there's nothing to do.
  if (mLinkStatus != LinkStatus::Connected &&
      mLinkStatus != LinkStatus::Doomed) {
    return;
  }

  // Mark the entire subtree as doomed so that no further messages can be
  // sent/recieved, and newly created managed actors are immediately marked as
  // doomed on creation.
  DoomSubtree();

  // Perform the steps to fully destroy an actor after it has been unregistered
  // from its manager.
  auto doActorDestroy = [toplevel = mToplevel, ipcChannel = GetIPCChannel()](
                            IProtocol* actor, ActorDestroyReason why) {
    MOZ_ASSERT(actor->mLinkStatus == LinkStatus::Doomed,
               "Actor must be doomed when calling doActorDestroy");
    MOZ_ASSERT(actor->AllManagedActorsCount() == 0,
               "All managed actors must have been destroyed first");

    // Mark the actor as Destroyed, ensuring we can't re-enter `ActorDestroy`,
    // even if an callback spins a nested event loop.
    actor->mLinkStatus = LinkStatus::Destroyed;

#ifdef FUZZING_SNAPSHOT
    fuzzing::IPCFuzzController::instance().OnActorDestroyed(actor);
#endif

    int32_t id = actor->mId;
    if (IProtocol* manager = actor->Manager()) {
      actor->mId = kFreedActorId;
      auto entry = toplevel->mActorMap.Lookup(id);
      MOZ_DIAGNOSTIC_ASSERT(entry && *entry == actor->GetLifecycleProxy(),
                            "ID must be present and reference this actor");
      entry.Remove();
      if (auto* container = manager->GetManagedActors(actor->GetProtocolId())) {
        container->EnsureRemoved(actor);
      }
    }

    ipcChannel->RejectPendingResponsesForActor(id);
    actor->ActorDestroy(why);
  };

  // Hold all ActorLifecycleProxy instances for managed actors until we return.
  nsTArray<RefPtr<ActorLifecycleProxy>> proxyHolder;
  proxyHolder.AppendElement(GetLifecycleProxy());

  // Invoke `ActorDestroy` for all managed actors in the subtree. These are
  // handled one at a time, so that new actors which are potentially registered
  // during `ActorDestroy` callbacks are not missed.
  ActorDestroyReason subtreeWhy = aWhy;
  if (aWhy == Deletion || aWhy == FailedConstructor) {
    subtreeWhy = AncestorDeletion;
  }
  while (IProtocol* actor = PeekManagedActor()) {
    // If the selected actor manages other actors, destroy those first.
    while (IProtocol* inner = actor->PeekManagedActor()) {
      actor = inner;
    }

    proxyHolder.AppendElement(actor->GetLifecycleProxy());
    doActorDestroy(actor, subtreeWhy);
  }

  // Destroy ourselves if we were not not otherwise destroyed while destroying
  // managed actors.
  if (mLinkStatus == LinkStatus::Doomed) {
    doActorDestroy(this, aWhy);
  }
}

void IProtocol::DoomSubtree() {
  MOZ_ASSERT(
      mLinkStatus == LinkStatus::Connected || mLinkStatus == LinkStatus::Doomed,
      "Invalid link status for SetDoomed");
  for (ProtocolId id : ManagedProtocolIds()) {
    for (IProtocol* actor : *GetManagedActors(id)) {
      actor->DoomSubtree();
    }
  }
  mLinkStatus = LinkStatus::Doomed;
}

IProtocol* IProtocol::PeekManagedActor() const {
  for (ProtocolId id : ManagedProtocolIds()) {
    const UntypedManagedContainer& container = *GetManagedActors(id);
    if (!container.IsEmpty()) {
      // Return the last element first, to reduce the copying required when
      // removing it.
      return *(container.end() - 1);
    }
  }
  return nullptr;
}

IToplevelProtocol::IToplevelProtocol(const char* aName, ProtocolId aProtoId,
                                     Side aSide)
    : IRefCountedProtocol(aProtoId, aSide),
      mOtherPid(base::kInvalidProcessId),
      mLastLocalId(0),
      mChannel(aName, this) {
  mToplevel = this;
}

void IToplevelProtocol::SetOtherProcessId(base::ProcessId aOtherPid) {
  mOtherPid = aOtherPid;
}

bool IToplevelProtocol::Open(ScopedPort aPort, const nsID& aMessageChannelId,
                             base::ProcessId aOtherPid,
                             nsISerialEventTarget* aEventTarget) {
  SetOtherProcessId(aOtherPid);
  return GetIPCChannel()->Open(std::move(aPort), mSide, aMessageChannelId,
                               aEventTarget);
}

bool IToplevelProtocol::Open(IToplevelProtocol* aTarget,
                             nsISerialEventTarget* aEventTarget,
                             mozilla::ipc::Side aSide) {
  SetOtherProcessId(base::GetCurrentProcId());
  aTarget->SetOtherProcessId(base::GetCurrentProcId());
  return GetIPCChannel()->Open(aTarget->GetIPCChannel(), aEventTarget, aSide);
}

bool IToplevelProtocol::OpenOnSameThread(IToplevelProtocol* aTarget,
                                         Side aSide) {
  SetOtherProcessId(base::GetCurrentProcId());
  aTarget->SetOtherProcessId(base::GetCurrentProcId());
  return GetIPCChannel()->OpenOnSameThread(aTarget->GetIPCChannel(), aSide);
}

void IToplevelProtocol::NotifyImpendingShutdown() {
  if (CanRecv()) {
    GetIPCChannel()->NotifyImpendingShutdown();
  }
}

void IToplevelProtocol::Close() { GetIPCChannel()->Close(); }

void IToplevelProtocol::SetReplyTimeoutMs(int32_t aTimeoutMs) {
  GetIPCChannel()->SetReplyTimeoutMs(aTimeoutMs);
}

bool IToplevelProtocol::IsOnCxxStack() const {
  return GetIPCChannel()->IsOnCxxStack();
}

int32_t IToplevelProtocol::NextId() {
  // Generate the next ID to use for a shared memory or protocol. Parent and
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

IProtocol* IToplevelProtocol::Lookup(int32_t aId) {
  if (auto entry = mActorMap.Lookup(aId)) {
    return entry.Data()->Get();
  }
  return nullptr;
}

Shmem::SharedMemory* IToplevelProtocol::CreateSharedMemory(size_t aSize,
                                                           bool aUnsafe,
                                                           Shmem::id_t* aId) {
  RefPtr<Shmem::SharedMemory> segment(Shmem::Alloc(aSize));
  if (!segment) {
    return nullptr;
  }
  int32_t id = NextId();
  Shmem shmem(segment.get(), id, aSize, aUnsafe);

  UniquePtr<Message> descriptor = shmem.MkCreatedMessage(MSG_ROUTING_CONTROL);
  if (!descriptor) {
    return nullptr;
  }
  Unused << GetIPCChannel()->Send(std::move(descriptor));

  *aId = shmem.Id();
  Shmem::SharedMemory* rawSegment = segment.get();
  MOZ_ASSERT(!mShmemMap.Contains(*aId), "Don't insert with an existing ID");
  mShmemMap.InsertOrUpdate(*aId, std::move(segment));
  return rawSegment;
}

Shmem::SharedMemory* IToplevelProtocol::LookupSharedMemory(Shmem::id_t aId) {
  auto entry = mShmemMap.Lookup(aId);
  return entry ? entry.Data().get() : nullptr;
}

bool IToplevelProtocol::IsTrackingSharedMemory(Shmem::SharedMemory* segment) {
  for (const auto& shmem : mShmemMap.Values()) {
    if (segment == shmem) {
      return true;
    }
  }
  return false;
}

bool IToplevelProtocol::DestroySharedMemory(Shmem& shmem) {
  Shmem::id_t aId = shmem.Id();
  Shmem::SharedMemory* segment = LookupSharedMemory(aId);
  if (!segment) {
    return false;
  }

  UniquePtr<Message> descriptor = shmem.MkDestroyedMessage(MSG_ROUTING_CONTROL);

  MOZ_ASSERT(mShmemMap.Contains(aId),
             "Attempting to remove an ID not in the shmem map");
  mShmemMap.Remove(aId);

  MessageChannel* channel = GetIPCChannel();
  if (!channel->CanSend()) {
    return true;
  }

  return descriptor && channel->Send(std::move(descriptor));
}

void IToplevelProtocol::DeallocShmems() { mShmemMap.Clear(); }

bool IToplevelProtocol::ShmemCreated(const Message& aMsg) {
  Shmem::id_t id;
  RefPtr<Shmem::SharedMemory> rawmem(Shmem::OpenExisting(aMsg, &id, true));
  if (!rawmem) {
    return false;
  }
  MOZ_ASSERT(!mShmemMap.Contains(id), "Don't insert with an existing ID");
  mShmemMap.InsertOrUpdate(id, std::move(rawmem));
  return true;
}

bool IToplevelProtocol::ShmemDestroyed(const Message& aMsg) {
  Shmem::id_t id;
  MessageReader reader(aMsg);
  if (!IPC::ReadParam(&reader, &id)) {
    return false;
  }
  reader.EndRead();

  mShmemMap.Remove(id);
  return true;
}

IPDLResolverInner::IPDLResolverInner(UniquePtr<IPC::Message> aReply,
                                     IProtocol* aActor)
    : mReply(std::move(aReply)),
      mWeakProxy(aActor->GetLifecycleProxy()->GetWeakProxy()) {}

void IPDLResolverInner::ResolveOrReject(
    bool aResolve, FunctionRef<void(IPC::Message*, IProtocol*)> aWrite) {
  MOZ_ASSERT(mWeakProxy);
  MOZ_ASSERT(mWeakProxy->ActorEventTarget()->IsOnCurrentThread());
  MOZ_ASSERT(mReply);

  UniquePtr<IPC::Message> reply = std::move(mReply);

  IProtocol* actor = mWeakProxy->Get();
  if (!actor) {
    NS_WARNING(nsPrintfCString("Not resolving response '%s': actor is dead",
                               reply->name())
                   .get());
    return;
  }

  IPC::MessageWriter writer(*reply, actor);
  WriteIPDLParam(&writer, actor, aResolve);
  aWrite(reply.get(), actor);

  actor->ChannelSend(std::move(reply));
}

void IPDLResolverInner::Destroy() {
  if (mReply) {
    NS_PROXY_DELETE_TO_EVENT_TARGET(IPDLResolverInner,
                                    mWeakProxy->ActorEventTarget());
  } else {
    // If we've already been consumed, just delete without proxying. This avoids
    // leaking the resolver if the actor's thread is already dead.
    delete this;
  }
}

IPDLResolverInner::~IPDLResolverInner() {
  if (mReply) {
    NS_WARNING(
        nsPrintfCString(
            "Rejecting reply '%s': resolver dropped without being called",
            mReply->name())
            .get());
    ResolveOrReject(false, [](IPC::Message* aMessage, IProtocol* aActor) {
      IPC::MessageWriter writer(*aMessage, aActor);
      ResponseRejectReason reason = ResponseRejectReason::ResolverDestroyed;
      WriteIPDLParam(&writer, aActor, reason);
    });
  }
}

}  // namespace ipc
}  // namespace mozilla
