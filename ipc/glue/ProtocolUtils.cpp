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

#include "mozilla/IntegerPrintfMacros.h"

#include "mozilla/ipc/ProtocolUtils.h"

#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/Transport.h"
#include "mozilla/recordreplay/ChildIPC.h"
#include "mozilla/recordreplay/ParentIPC.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Unused.h"
#include "nsPrintfCString.h"

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
#  include "mozilla/sandboxTarget.h"
#endif

#if defined(XP_WIN)
#  include "aclapi.h"
#  include "sddl.h"

#  include "mozilla/TypeTraits.h"
#endif

#include "nsAutoPtr.h"

using namespace IPC;

using base::GetCurrentProcId;
using base::ProcessHandle;
using base::ProcessId;

namespace mozilla {

#if defined(XP_WIN)
// Generate RAII classes for LPTSTR and PSECURITY_DESCRIPTOR.
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedLPTStr,
                                          RemovePointer<LPTSTR>::Type,
                                          ::LocalFree)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(
    ScopedPSecurityDescriptor, RemovePointer<PSECURITY_DESCRIPTOR>::Type,
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

class ChannelOpened : public IPC::Message {
 public:
  ChannelOpened(TransportDescriptor aDescriptor, ProcessId aOtherProcess,
                ProtocolId aProtocol,
                NestedLevel aNestedLevel = NOT_NESTED)
      : IPC::Message(MSG_ROUTING_CONTROL,  // these only go to top-level actors
                     CHANNEL_OPENED_MESSAGE_TYPE, 0,
                     HeaderFlags(aNestedLevel)) {
    IPC::WriteParam(this, aDescriptor);
    IPC::WriteParam(this, aOtherProcess);
    IPC::WriteParam(this, static_cast<uint32_t>(aProtocol));
  }

  static bool Read(const IPC::Message& aMsg, TransportDescriptor* aDescriptor,
                   ProcessId* aOtherProcess, ProtocolId* aProtocol) {
    PickleIterator iter(aMsg);
    if (!IPC::ReadParam(&aMsg, &iter, aDescriptor) ||
        !IPC::ReadParam(&aMsg, &iter, aOtherProcess) ||
        !IPC::ReadParam(&aMsg, &iter, reinterpret_cast<uint32_t*>(aProtocol))) {
      return false;
    }
    aMsg.EndRead(iter);
    return true;
  }
};

nsresult Bridge(const PrivateIPDLInterface&, MessageChannel* aParentChannel,
                ProcessId aParentPid, MessageChannel* aChildChannel,
                ProcessId aChildPid, ProtocolId aProtocol,
                ProtocolId aChildProtocol) {
  if (!aParentPid || !aChildPid) {
    return NS_ERROR_INVALID_ARG;
  }

  TransportDescriptor parentSide, childSide;
  nsresult rv;
  if (NS_FAILED(rv = CreateTransport(aParentPid, &parentSide, &childSide))) {
    return rv;
  }

  if (!aParentChannel->Send(
          new ChannelOpened(parentSide, aChildPid, aProtocol,
                            IPC::Message::NESTED_INSIDE_CPOW))) {
    CloseDescriptor(parentSide);
    CloseDescriptor(childSide);
    return NS_ERROR_BRIDGE_OPEN_PARENT;
  }

  if (!aChildChannel->Send(
          new ChannelOpened(childSide, aParentPid, aChildProtocol,
                            IPC::Message::NESTED_INSIDE_CPOW))) {
    CloseDescriptor(parentSide);
    CloseDescriptor(childSide);
    return NS_ERROR_BRIDGE_OPEN_CHILD;
  }

  return NS_OK;
}

bool Open(const PrivateIPDLInterface&, MessageChannel* aOpenerChannel,
          ProcessId aOtherProcessId, Transport::Mode aOpenerMode,
          ProtocolId aProtocol, ProtocolId aChildProtocol) {
  bool isParent = (Transport::MODE_SERVER == aOpenerMode);
  ProcessId thisPid = GetCurrentProcId();
  ProcessId parentId = isParent ? thisPid : aOtherProcessId;
  ProcessId childId = !isParent ? thisPid : aOtherProcessId;
  if (!parentId || !childId) {
    return false;
  }

  TransportDescriptor parentSide, childSide;
  if (NS_FAILED(CreateTransport(parentId, &parentSide, &childSide))) {
    return false;
  }

  Message* parentMsg = new ChannelOpened(parentSide, childId, aProtocol);
  Message* childMsg = new ChannelOpened(childSide, parentId, aChildProtocol);
  nsAutoPtr<Message> messageForUs(isParent ? parentMsg : childMsg);
  nsAutoPtr<Message> messageForOtherSide(!isParent ? parentMsg : childMsg);
  if (!aOpenerChannel->Echo(messageForUs.forget()) ||
      !aOpenerChannel->Send(messageForOtherSide.forget())) {
    CloseDescriptor(parentSide);
    CloseDescriptor(childSide);
    return false;
  }
  return true;
}

bool UnpackChannelOpened(const PrivateIPDLInterface&, const Message& aMsg,
                         TransportDescriptor* aTransport,
                         ProcessId* aOtherProcess, ProtocolId* aProtocol) {
  return ChannelOpened::Read(aMsg, aTransport, aOtherProcess, aProtocol);
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

bool StateTransition(bool aIsDelete, LivenessState* aNext) {
  switch (*aNext) {
    case LivenessState::Null:
      if (aIsDelete) {
        *aNext = LivenessState::Dead;
      }
      break;
    case LivenessState::Dead:
      return false;
    default:
      return false;
  }
  return true;
}

bool ReEntrantDeleteStateTransition(bool aIsDelete, bool aIsDeleteReply,
                                    ReEntrantDeleteLivenessState* aNext) {
  switch (*aNext) {
    case ReEntrantDeleteLivenessState::Null:
      if (aIsDelete) {
        *aNext = ReEntrantDeleteLivenessState::Dying;
      }
      break;
    case ReEntrantDeleteLivenessState::Dead:
      return false;
    case ReEntrantDeleteLivenessState::Dying:
      if (aIsDeleteReply) {
        *aNext = ReEntrantDeleteLivenessState::Dead;
      }
      break;
    default:
      return false;
  }
  return true;
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

  if (listener->GetProtocolTypeId() != aProtocolTypeId) {
    MismatchedActorTypeError(aActorDescription);
    return Nothing();
  }

  return Some(listener);
}

int32_t IProtocol::ManagedState::Register(IProtocol* aRouted) {
  return mProtocol->Manager()->Register(aRouted);
}

int32_t IProtocol::ManagedState::RegisterID(IProtocol* aRouted, int32_t aId) {
  return mProtocol->Manager()->RegisterID(aRouted, aId);
}

IProtocol* IProtocol::ManagedState::Lookup(int32_t aId) {
  return mProtocol->Manager()->Lookup(aId);
}

void IProtocol::ManagedState::Unregister(int32_t aId) {
  if (mProtocol->mId == aId) {
    mProtocol->mId = kFreedActorId;
  }
  mProtocol->Manager()->Unregister(aId);
}

Shmem::SharedMemory* IProtocol::ManagedState::CreateSharedMemory(
    size_t aSize, SharedMemory::SharedMemoryType aType, bool aUnsafe,
    int32_t* aId) {
  return mProtocol->Manager()->CreateSharedMemory(aSize, aType, aUnsafe, aId);
}

Shmem::SharedMemory* IProtocol::ManagedState::LookupSharedMemory(int32_t aId) {
  return mProtocol->Manager()->LookupSharedMemory(aId);
}

bool IProtocol::ManagedState::IsTrackingSharedMemory(
    Shmem::SharedMemory* aSegment) {
  return mProtocol->Manager()->IsTrackingSharedMemory(aSegment);
}

bool IProtocol::ManagedState::DestroySharedMemory(Shmem& aShmem) {
  return mProtocol->Manager()->DestroySharedMemory(aShmem);
}

const MessageChannel* IProtocol::ManagedState::GetIPCChannel() const {
  return mChannel;
}

MessageChannel* IProtocol::ManagedState::GetIPCChannel() { return mChannel; }

ProcessId IProtocol::OtherPid() const { return Manager()->OtherPid(); }

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
}

void IProtocol::SetManagerAndRegister(IProtocol* aManager) {
  // Set the manager prior to registering so registering properly inherits
  // the manager's event target.
  SetManager(aManager);

  aManager->Register(this);

  mState->SetIPCChannel(aManager->GetIPCChannel());
}

void IProtocol::SetManagerAndRegister(IProtocol* aManager, int32_t aId) {
  // Set the manager prior to registering so registering properly inherits
  // the manager's event target.
  SetManager(aManager);

  aManager->RegisterID(this, aId);

  mState->SetIPCChannel(aManager->GetIPCChannel());
}

void IProtocol::SetEventTargetForActor(IProtocol* aActor,
                                       nsIEventTarget* aEventTarget) {
  // Make sure we have a manager for the internal method to access.
  aActor->SetManager(this);
  mState->SetEventTargetForActor(aActor, aEventTarget);
}

void IProtocol::ReplaceEventTargetForActor(IProtocol* aActor,
                                           nsIEventTarget* aEventTarget) {
  // Ensure the actor has been registered.
  MOZ_ASSERT(aActor->Manager());
  mState->ReplaceEventTargetForActor(aActor, aEventTarget);
}

void IProtocol::SetEventTargetForRoute(int32_t aRoute,
                                       nsIEventTarget* aEventTarget) {
  mState->SetEventTargetForRoute(aRoute, aEventTarget);
}

nsIEventTarget* IProtocol::GetActorEventTarget() {
  return mState->GetActorEventTarget();
}

already_AddRefed<nsIEventTarget> IProtocol::GetActorEventTarget(
    IProtocol* aActor) {
  return mState->GetActorEventTarget(aActor);
}

nsIEventTarget* IProtocol::ManagedState::GetActorEventTarget() {
  // We should only call this function when this actor has been registered and
  // is not unregistered yet.
  MOZ_RELEASE_ASSERT(mProtocol->mId != kNullActorId &&
                     mProtocol->mId != kFreedActorId);
  RefPtr<nsIEventTarget> target = GetActorEventTarget(mProtocol);
  return target;
}

void IProtocol::ManagedState::SetEventTargetForActor(
    IProtocol* aActor, nsIEventTarget* aEventTarget) {
  // Go directly through the state so we don't try to redundantly (and
  // wrongly) call SetManager() on aActor.
  mProtocol->Manager()->mState->SetEventTargetForActor(aActor, aEventTarget);
}

void IProtocol::ManagedState::ReplaceEventTargetForActor(
    IProtocol* aActor, nsIEventTarget* aEventTarget) {
  mProtocol->Manager()->ReplaceEventTargetForActor(aActor, aEventTarget);
}

void IProtocol::ManagedState::SetEventTargetForRoute(
    int32_t aRoute, nsIEventTarget* aEventTarget) {
  mProtocol->Manager()->SetEventTargetForRoute(aRoute, aEventTarget);
}

already_AddRefed<nsIEventTarget> IProtocol::ManagedState::GetActorEventTarget(
    IProtocol* aActor) {
  return mProtocol->Manager()->GetActorEventTarget(aActor);
}

IToplevelProtocol::IToplevelProtocol(const char* aName, ProtocolId aProtoId,
                                     Side aSide)
    : IProtocol(aSide, MakeUnique<ToplevelState>(aName, this, aSide)),
      mProtocolId(aProtoId),
      mOtherPid(mozilla::ipc::kInvalidProcessId),
      mIsMainThreadProtocol(false) {}

IToplevelProtocol::~IToplevelProtocol() {
  mState = nullptr;
  if (mTrans) {
    RefPtr<DeleteTask<Transport>> task =
        new DeleteTask<Transport>(mTrans.release());
    XRE_GetIOMessageLoop()->PostTask(task.forget());
  }
}

base::ProcessId IToplevelProtocol::OtherPid() const {
  base::ProcessId pid = OtherPidMaybeInvalid();
  MOZ_RELEASE_ASSERT(pid != kInvalidProcessId);
  return pid;
}

base::ProcessId IToplevelProtocol::OtherPidMaybeInvalid() const {
  return mOtherPid;
}

void IToplevelProtocol::SetOtherProcessId(base::ProcessId aOtherPid) {
  // When recording an execution, all communication we do is forwarded from
  // the middleman to the parent process, so use its pid instead of the
  // middleman's pid.
  if (recordreplay::IsRecordingOrReplaying() &&
      aOtherPid == recordreplay::child::MiddlemanProcessId()) {
    mOtherPid = recordreplay::child::ParentProcessId();
  } else {
    mOtherPid = aOtherPid;
  }
}

bool IToplevelProtocol::Open(mozilla::ipc::Transport* aTransport,
                             base::ProcessId aOtherPid, MessageLoop* aThread,
                             mozilla::ipc::Side aSide) {
  SetOtherProcessId(aOtherPid);
  return GetIPCChannel()->Open(aTransport, aThread, aSide);
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

bool IToplevelProtocol::OpenWithAsyncPid(mozilla::ipc::Transport* aTransport,
                                         MessageLoop* aThread,
                                         mozilla::ipc::Side aSide) {
  return GetIPCChannel()->Open(aTransport, aThread, aSide);
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

int32_t IToplevelProtocol::ToplevelState::NextId() {
  // Genreate the next ID to use for a shared memory or protocol. Parent and
  // Child sides of the protocol use different pools, and actors created in the
  // middleman need to use a distinct pool as well.
  int32_t tag = 0;
  if (recordreplay::IsMiddleman()) {
    tag |= 1 << 0;
  }
  if (mProtocol->GetSide() == ParentSide) {
    tag |= 1 << 1;
  }

  // Check any overflow
  MOZ_RELEASE_ASSERT(mLastLocalId < (1 << 29));

  // Compute the ID to use with the low two bits as our tag, and the remaining
  // bits as a monotonic.
  return (++mLastLocalId << 2) | tag;
}

int32_t IToplevelProtocol::ToplevelState::Register(IProtocol* aRouted) {
  if (aRouted->Id() != kNullActorId && aRouted->Id() != kFreedActorId) {
    // If there's already an ID, just return that.
    return aRouted->Id();
  }
  int32_t id = NextId();
  mActorMap.AddWithID(aRouted, id);
  aRouted->SetId(id);

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

int32_t IToplevelProtocol::ToplevelState::RegisterID(IProtocol* aRouted,
                                                     int32_t aId) {
  mActorMap.AddWithID(aRouted, aId);
  aRouted->SetId(aId);
  return aId;
}

IProtocol* IToplevelProtocol::ToplevelState::Lookup(int32_t aId) {
  return mActorMap.Lookup(aId);
}

void IToplevelProtocol::ToplevelState::Unregister(int32_t aId) {
  mActorMap.Remove(aId);

  MutexAutoLock lock(mEventTargetMutex);
  mEventTargetMap.RemoveIfPresent(aId);
}

IToplevelProtocol::ToplevelState::ToplevelState(const char* aName,
                                                IToplevelProtocol* aProtocol,
                                                Side aSide)
    : ProtocolState(),
      mProtocol(aProtocol),
      mLastLocalId(0),
      mEventTargetMutex("ProtocolEventTargetMutex"),
      mChannel(aName, aProtocol) {}

Shmem::SharedMemory* IToplevelProtocol::ToplevelState::CreateSharedMemory(
    size_t aSize, Shmem::SharedMemory::SharedMemoryType aType, bool aUnsafe,
    Shmem::id_t* aId) {
  // XXX the mProtocol uses here should go away!
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
      mProtocol->OtherPidMaybeInvalid();
#else
      mProtocol->OtherPid();
#endif

  Message* descriptor =
      shmem.ShareTo(Shmem::PrivateIPDLCaller(), pid, MSG_ROUTING_CONTROL);
  if (!descriptor) {
    return nullptr;
  }
  Unused << mProtocol->GetIPCChannel()->Send(descriptor);

  *aId = shmem.Id(Shmem::PrivateIPDLCaller());
  Shmem::SharedMemory* rawSegment = segment.get();
  mShmemMap.AddWithID(segment.forget().take(), *aId);
  return rawSegment;
}

Shmem::SharedMemory* IToplevelProtocol::ToplevelState::LookupSharedMemory(
    Shmem::id_t aId) {
  return mShmemMap.Lookup(aId);
}

bool IToplevelProtocol::ToplevelState::IsTrackingSharedMemory(
    Shmem::SharedMemory* segment) {
  return mShmemMap.HasData(segment);
}

bool IToplevelProtocol::ToplevelState::DestroySharedMemory(Shmem& shmem) {
  Shmem::id_t aId = shmem.Id(Shmem::PrivateIPDLCaller());
  Shmem::SharedMemory* segment = LookupSharedMemory(aId);
  if (!segment) {
    return false;
  }

  Message* descriptor =
      shmem.UnshareFrom(Shmem::PrivateIPDLCaller(), MSG_ROUTING_CONTROL);

  mShmemMap.Remove(aId);
  Shmem::Dealloc(Shmem::PrivateIPDLCaller(), segment);

  MessageChannel* channel = mProtocol->GetIPCChannel();
  if (!channel->CanSend()) {
    delete descriptor;
    return true;
  }

  return descriptor && channel->Send(descriptor);
}

void IToplevelProtocol::ToplevelState::DeallocShmems() {
  for (IDMap<SharedMemory*>::const_iterator cit = mShmemMap.begin();
       cit != mShmemMap.end(); ++cit) {
    Shmem::Dealloc(Shmem::PrivateIPDLCaller(), cit->second);
  }
  mShmemMap.Clear();
}

bool IToplevelProtocol::ToplevelState::ShmemCreated(const Message& aMsg) {
  Shmem::id_t id;
  RefPtr<Shmem::SharedMemory> rawmem(
      Shmem::OpenExisting(Shmem::PrivateIPDLCaller(), aMsg, &id, true));
  if (!rawmem) {
    return false;
  }
  mShmemMap.AddWithID(rawmem.forget().take(), id);
  return true;
}

bool IToplevelProtocol::ToplevelState::ShmemDestroyed(const Message& aMsg) {
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

already_AddRefed<nsIEventTarget>
IToplevelProtocol::ToplevelState::GetMessageEventTarget(const Message& aMsg) {
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
      target = mProtocol->GetConstructedEventTarget(aMsg);
    }

    mEventTargetMap.AddWithID(target, handle.mId);
  } else if (!target) {
    // We don't need the lock after this point.
    lock.reset();

    target = mProtocol->GetSpecificMessageEventTarget(aMsg);
  }

  return target.forget();
}

already_AddRefed<nsIEventTarget>
IToplevelProtocol::ToplevelState::GetActorEventTarget(IProtocol* aActor) {
  MOZ_RELEASE_ASSERT(aActor->Id() != kNullActorId &&
                     aActor->Id() != kFreedActorId);

  MutexAutoLock lock(mEventTargetMutex);
  nsCOMPtr<nsIEventTarget> target = mEventTargetMap.Lookup(aActor->Id());
  return target.forget();
}

nsIEventTarget* IToplevelProtocol::ToplevelState::GetActorEventTarget() {
  // The EventTarget of a ToplevelProtocol shall never be set.
  return nullptr;
}

void IToplevelProtocol::ToplevelState::SetEventTargetForActor(
    IProtocol* aActor, nsIEventTarget* aEventTarget) {
  // The EventTarget of a ToplevelProtocol shall never be set.
  MOZ_RELEASE_ASSERT(aActor != mProtocol);

  // We should only call this function on actors that haven't been used for IPC
  // code yet. Otherwise we'll be posting stuff to the wrong event target before
  // we're called.
  MOZ_RELEASE_ASSERT(aActor->Id() == kNullActorId ||
                     aActor->Id() == kFreedActorId);

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

void IToplevelProtocol::ToplevelState::ReplaceEventTargetForActor(
    IProtocol* aActor, nsIEventTarget* aEventTarget) {
  // The EventTarget of a ToplevelProtocol shall never be set.
  MOZ_RELEASE_ASSERT(aActor != mProtocol);

  int32_t id = aActor->Id();
  // The ID of the actor should have existed.
  MOZ_RELEASE_ASSERT(id != kNullActorId && id != kFreedActorId);

  MutexAutoLock lock(mEventTargetMutex);
  mEventTargetMap.ReplaceWithID(aEventTarget, id);
}

void IToplevelProtocol::ToplevelState::SetEventTargetForRoute(
    int32_t aRoute, nsIEventTarget* aEventTarget) {
  MOZ_RELEASE_ASSERT(aRoute != mProtocol->Id());
  MOZ_RELEASE_ASSERT(aRoute != kNullActorId && aRoute != kFreedActorId);

  MutexAutoLock lock(mEventTargetMutex);
  MOZ_ASSERT(!mEventTargetMap.Lookup(aRoute));
  mEventTargetMap.AddWithID(aEventTarget, aRoute);
}

const MessageChannel* IToplevelProtocol::ToplevelState::GetIPCChannel() const {
  return ProtocolState::mChannel ? ProtocolState::mChannel : &mChannel;
}

MessageChannel* IToplevelProtocol::ToplevelState::GetIPCChannel() {
  return ProtocolState::mChannel ? ProtocolState::mChannel : &mChannel;
}

}  // namespace ipc
}  // namespace mozilla
