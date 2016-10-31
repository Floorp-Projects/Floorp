/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/process_util.h"
#include "base/task.h"

#ifdef OS_POSIX
#include <errno.h>
#endif

#include "mozilla/ipc/ProtocolUtils.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/Transport.h"
#include "mozilla/StaticMutex.h"
#include "nsPrintfCString.h"

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
#define TARGET_SANDBOX_EXPORTS
#include "mozilla/sandboxTarget.h"
#endif

#if defined(MOZ_CRASHREPORTER) && defined(XP_WIN)
#include "aclapi.h"
#include "sddl.h"

#include "mozilla/TypeTraits.h"
#endif

#include "nsAutoPtr.h"

using namespace IPC;

using base::GetCurrentProcId;
using base::ProcessHandle;
using base::ProcessId;

namespace mozilla {

#if defined(MOZ_CRASHREPORTER) && defined(XP_WIN)
// Generate RAII classes for LPTSTR and PSECURITY_DESCRIPTOR.
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedLPTStr, \
                                          RemovePointer<LPTSTR>::Type, \
                                          ::LocalFree)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPSecurityDescriptor, \
                                          RemovePointer<PSECURITY_DESCRIPTOR>::Type, \
                                          ::LocalFree)
#endif

namespace ipc {

IToplevelProtocol::IToplevelProtocol(ProtocolId aProtoId, Side aSide)
 : IProtocol(aSide),
   mProtocolId(aProtoId)
{
}

IToplevelProtocol::~IToplevelProtocol()
{
  if (mTrans) {
    RefPtr<DeleteTask<Transport>> task = new DeleteTask<Transport>(mTrans.release());
    XRE_GetIOMessageLoop()->PostTask(task.forget());
  }
}

class ChannelOpened : public IPC::Message
{
public:
  ChannelOpened(TransportDescriptor aDescriptor,
                ProcessId aOtherProcess,
                ProtocolId aProtocol,
                NestedLevel aNestedLevel = NOT_NESTED)
    : IPC::Message(MSG_ROUTING_CONTROL, // these only go to top-level actors
                   CHANNEL_OPENED_MESSAGE_TYPE,
                   aNestedLevel)
  {
    IPC::WriteParam(this, aDescriptor);
    IPC::WriteParam(this, aOtherProcess);
    IPC::WriteParam(this, static_cast<uint32_t>(aProtocol));
  }

  static bool Read(const IPC::Message& aMsg,
                   TransportDescriptor* aDescriptor,
                   ProcessId* aOtherProcess,
                   ProtocolId* aProtocol)
  {
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

nsresult
Bridge(const PrivateIPDLInterface&,
       MessageChannel* aParentChannel, ProcessId aParentPid,
       MessageChannel* aChildChannel, ProcessId aChildPid,
       ProtocolId aProtocol, ProtocolId aChildProtocol)
{
  if (!aParentPid || !aChildPid) {
    return NS_ERROR_INVALID_ARG;
  }

  TransportDescriptor parentSide, childSide;
  nsresult rv;
  if (NS_FAILED(rv = CreateTransport(aParentPid, &parentSide, &childSide))) {
    return rv;
  }

  if (!aParentChannel->Send(new ChannelOpened(parentSide,
                                              aChildPid,
                                              aProtocol,
                                              IPC::Message::NESTED_INSIDE_CPOW))) {
    CloseDescriptor(parentSide);
    CloseDescriptor(childSide);
    return NS_ERROR_BRIDGE_OPEN_PARENT;
  }

  if (!aChildChannel->Send(new ChannelOpened(childSide,
                                            aParentPid,
                                            aChildProtocol,
                                            IPC::Message::NESTED_INSIDE_CPOW))) {
    CloseDescriptor(parentSide);
    CloseDescriptor(childSide);
    return NS_ERROR_BRIDGE_OPEN_CHILD;
  }

  return NS_OK;
}

bool
Open(const PrivateIPDLInterface&,
     MessageChannel* aOpenerChannel, ProcessId aOtherProcessId,
     Transport::Mode aOpenerMode,
     ProtocolId aProtocol, ProtocolId aChildProtocol)
{
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

bool
UnpackChannelOpened(const PrivateIPDLInterface&,
                    const Message& aMsg,
                    TransportDescriptor* aTransport,
                    ProcessId* aOtherProcess,
                    ProtocolId* aProtocol)
{
  return ChannelOpened::Read(aMsg, aTransport, aOtherProcess, aProtocol);
}

#if defined(XP_WIN)
bool DuplicateHandle(HANDLE aSourceHandle,
                     DWORD aTargetProcessId,
                     HANDLE* aTargetHandle,
                     DWORD aDesiredAccess,
                     DWORD aOptions) {
  // If our process is the target just duplicate the handle.
  if (aTargetProcessId == base::GetCurrentProcId()) {
    return !!::DuplicateHandle(::GetCurrentProcess(), aSourceHandle,
                               ::GetCurrentProcess(), aTargetHandle,
                               aDesiredAccess, false, aOptions);

  }

#if defined(MOZ_SANDBOX)
  // Try the broker next (will fail if not sandboxed).
  if (SandboxTarget::Instance()->BrokerDuplicateHandle(aSourceHandle,
                                                       aTargetProcessId,
                                                       aTargetHandle,
                                                       aDesiredAccess,
                                                       aOptions)) {
    return true;
  }
#endif

  // Finally, see if we already have access to the process.
  ScopedProcessHandle targetProcess(OpenProcess(PROCESS_DUP_HANDLE,
                                                FALSE,
                                                aTargetProcessId));
  if (!targetProcess) {
#ifdef MOZ_CRASHREPORTER
    CrashReporter::AnnotateCrashReport(
      NS_LITERAL_CSTRING("IPCTransportFailureReason"),
      NS_LITERAL_CSTRING("Failed to open target process."));
#endif
    return false;
  }

  return !!::DuplicateHandle(::GetCurrentProcess(), aSourceHandle,
                              targetProcess, aTargetHandle,
                              aDesiredAccess, FALSE, aOptions);
}
#endif

#ifdef MOZ_CRASHREPORTER
void
AnnotateSystemError()
{
  int64_t error = 0;
#if defined(XP_WIN)
  error = ::GetLastError();
#elif defined(OS_POSIX)
  error = errno;
#endif
  if (error) {
    CrashReporter::AnnotateCrashReport(
      NS_LITERAL_CSTRING("IPCSystemError"),
      nsPrintfCString("%lld", error));
  }
}
#endif

#if defined(MOZ_CRASHREPORTER) && defined(XP_MACOSX)
void
AnnotateCrashReportWithErrno(const char* tag, int error)
{
  CrashReporter::AnnotateCrashReport(
    nsCString(tag),
    nsPrintfCString("%d", error));
}
#endif

void
LogMessageForProtocol(const char* aTopLevelProtocol, base::ProcessId aOtherPid,
                      const char* aContextDescription,
                      uint32_t aMessageId,
                      MessageDirection aDirection)
{
  nsPrintfCString logMessage("[time: %" PRId64 "][%d%s%d] [%s] %s %s\n",
                             PR_Now(), base::GetCurrentProcId(),
                             aDirection == MessageDirection::eReceiving ? "<-" : "->",
                             aOtherPid, aTopLevelProtocol,
                             aContextDescription,
                             StringFromIPCMessageType(aMessageId));
#ifdef ANDROID
  __android_log_write(ANDROID_LOG_INFO, "GeckoIPC", logMessage.get());
#endif
  fputs(logMessage.get(), stderr);
}

void
ProtocolErrorBreakpoint(const char* aMsg)
{
    // Bugs that generate these error messages can be tough to
    // reproduce.  Log always in the hope that someone finds the error
    // message.
    printf_stderr("IPDL protocol error: %s\n", aMsg);
}

void
FatalError(const char* aProtocolName, const char* aMsg, bool aIsParent)
{
  ProtocolErrorBreakpoint(aMsg);

  nsAutoCString formattedMessage("IPDL error [");
  formattedMessage.AppendASCII(aProtocolName);
  formattedMessage.AppendLiteral("]: \"");
  formattedMessage.AppendASCII(aMsg);
  if (aIsParent) {
#ifdef MOZ_CRASHREPORTER
    // We're going to crash the parent process because at this time
    // there's no other really nice way of getting a minidump out of
    // this process if we're off the main thread.
    formattedMessage.AppendLiteral("\". Intentionally crashing.");
    NS_ERROR(formattedMessage.get());
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("IPCFatalErrorProtocol"),
                                       nsDependentCString(aProtocolName));
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("IPCFatalErrorMsg"),
                                       nsDependentCString(aMsg));
    AnnotateSystemError();
#endif
    MOZ_CRASH("IPC FatalError in the parent process!");
  } else {
    formattedMessage.AppendLiteral("\". abort()ing as a result.");
    NS_RUNTIMEABORT(formattedMessage.get());
  }
}

void
LogicError(const char* aMsg)
{
  NS_RUNTIMEABORT(aMsg);
}

void
ActorIdReadError(const char* aActorDescription)
{
  nsPrintfCString message("Error deserializing id for %s", aActorDescription);
  NS_RUNTIMEABORT(message.get());
}

void
BadActorIdError(const char* aActorDescription)
{
  nsPrintfCString message("bad id for %s", aActorDescription);
  ProtocolErrorBreakpoint(message.get());
}

void
ActorLookupError(const char* aActorDescription)
{
  nsPrintfCString message("could not lookup id for %s", aActorDescription);
  ProtocolErrorBreakpoint(message.get());
}

void
MismatchedActorTypeError(const char* aActorDescription)
{
  nsPrintfCString message("actor that should be of type %s has different type",
                          aActorDescription);
  ProtocolErrorBreakpoint(message.get());
}

void
UnionTypeReadError(const char* aUnionName)
{
  nsPrintfCString message("error deserializing type of union %s", aUnionName);
  NS_RUNTIMEABORT(message.get());
}

void ArrayLengthReadError(const char* aElementName)
{
  nsPrintfCString message("error deserializing length of %s[]", aElementName);
  NS_RUNTIMEABORT(message.get());
}

void
TableToArray(const nsTHashtable<nsPtrHashKey<void>>& aTable,
             nsTArray<void*>& aArray)
{
  uint32_t i = 0;
  void** elements = aArray.AppendElements(aTable.Count());
  for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
    elements[i] = iter.Get()->GetKey();
    ++i;
  }
}

Maybe<IProtocol*>
IProtocol::ReadActor(const IPC::Message* aMessage, PickleIterator* aIter, bool aNullable,
                     const char* aActorDescription, int32_t aProtocolTypeId)
{
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

int32_t
IProtocol::Register(IProtocol* aRouted)
{
  return Manager()->Register(aRouted);
}

int32_t
IProtocol::RegisterID(IProtocol* aRouted, int32_t aId)
{
  return Manager()->RegisterID(aRouted, aId);
}

IProtocol*
IProtocol::Lookup(int32_t aId)
{
  return Manager()->Lookup(aId);
}

void
IProtocol::Unregister(int32_t aId)
{
  Manager()->Unregister(aId);
}

Shmem::SharedMemory*
IProtocol::CreateSharedMemory(size_t aSize,
                              SharedMemory::SharedMemoryType aType,
                              bool aUnsafe,
                              int32_t* aId)
{
  return Manager()->CreateSharedMemory(aSize, aType, aUnsafe, aId);
}

Shmem::SharedMemory*
IProtocol::LookupSharedMemory(int32_t aId)
{
  return Manager()->LookupSharedMemory(aId);
}

bool
IProtocol::IsTrackingSharedMemory(Shmem::SharedMemory* aSegment)
{
  return Manager()->IsTrackingSharedMemory(aSegment);
}

bool
IProtocol::DestroySharedMemory(Shmem& aShmem)
{
  return Manager()->DestroySharedMemory(aShmem);
}

ProcessId
IProtocol::OtherPid() const
{
  return Manager()->OtherPid();
}

} // namespace ipc
} // namespace mozilla
