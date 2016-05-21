/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/process_util.h"

#ifdef OS_POSIX
#include <errno.h>
#endif

#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Transport.h"
#include "mozilla/StaticMutex.h"

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
#define TARGET_SANDBOX_EXPORTS
#include "mozilla/sandboxTarget.h"
#endif

#if defined(MOZ_CRASHREPORTER) && defined(XP_WIN)
#include "aclapi.h"
#include "sddl.h"

#include "mozilla/TypeTraits.h"
#endif

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

ProtocolCloneContext::ProtocolCloneContext()
  : mNeckoParent(nullptr)
{}

ProtocolCloneContext::~ProtocolCloneContext()
{}

void ProtocolCloneContext::SetContentParent(ContentParent* aContentParent)
{
  mContentParent = aContentParent;
}

static StaticMutex gProtocolMutex;

IToplevelProtocol::IToplevelProtocol(ProtocolId aProtoId)
 : mOpener(nullptr)
 , mProtocolId(aProtoId)
 , mTrans(nullptr)
{
}

IToplevelProtocol::~IToplevelProtocol()
{
  StaticMutexAutoLock al(gProtocolMutex);

  for (IToplevelProtocol* actor = mOpenActors.getFirst();
       actor;
       actor = actor->getNext()) {
    actor->mOpener = nullptr;
  }

  mOpenActors.clear();

  if (mOpener) {
      removeFrom(mOpener->mOpenActors);
  }
}

void
IToplevelProtocol::AddOpenedActorLocked(IToplevelProtocol* aActor)
{
  gProtocolMutex.AssertCurrentThreadOwns();

#ifdef DEBUG
  for (const IToplevelProtocol* actor = mOpenActors.getFirst();
       actor;
       actor = actor->getNext()) {
    NS_ASSERTION(actor != aActor,
                 "Open the same protocol for more than one time");
  }
#endif

  aActor->mOpener = this;
  mOpenActors.insertBack(aActor);
}

void
IToplevelProtocol::AddOpenedActor(IToplevelProtocol* aActor)
{
  StaticMutexAutoLock al(gProtocolMutex);
  AddOpenedActorLocked(aActor);
}

void
IToplevelProtocol::GetOpenedActorsLocked(nsTArray<IToplevelProtocol*>& aActors)
{
  gProtocolMutex.AssertCurrentThreadOwns();

  for (IToplevelProtocol* actor = mOpenActors.getFirst();
       actor;
       actor = actor->getNext()) {
    aActors.AppendElement(actor);
  }
}

void
IToplevelProtocol::GetOpenedActors(nsTArray<IToplevelProtocol*>& aActors)
{
  StaticMutexAutoLock al(gProtocolMutex);
  GetOpenedActorsLocked(aActors);
}

size_t
IToplevelProtocol::GetOpenedActorsUnsafe(IToplevelProtocol** aActors, size_t aActorsMax)
{
  size_t count = 0;
  for (IToplevelProtocol* actor = mOpenActors.getFirst();
       actor;
       actor = actor->getNext()) {
    MOZ_RELEASE_ASSERT(count < aActorsMax);
    aActors[count++] = actor;
  }
  return count;
}

IToplevelProtocol*
IToplevelProtocol::CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                                 base::ProcessHandle aPeerProcess,
                                 ProtocolCloneContext* aCtx)
{
  NS_NOTREACHED("Clone() for this protocol actor is not implemented");
  return nullptr;
}

void
IToplevelProtocol::CloneOpenedToplevels(IToplevelProtocol* aTemplate,
                                        const InfallibleTArray<ProtocolFdMapping>& aFds,
                                        base::ProcessHandle aPeerProcess,
                                        ProtocolCloneContext* aCtx)
{
  StaticMutexAutoLock al(gProtocolMutex);

  nsTArray<IToplevelProtocol*> actors;
  aTemplate->GetOpenedActorsLocked(actors);

  for (size_t i = 0; i < actors.Length(); i++) {
    IToplevelProtocol* newactor = actors[i]->CloneToplevel(aFds, aPeerProcess, aCtx);
    AddOpenedActorLocked(newactor);
  }
}

class ChannelOpened : public IPC::Message
{
public:
  ChannelOpened(TransportDescriptor aDescriptor,
                ProcessId aOtherProcess,
                ProtocolId aProtocol,
                PriorityValue aPriority = PRIORITY_NORMAL)
    : IPC::Message(MSG_ROUTING_CONTROL, // these only go to top-level actors
                   CHANNEL_OPENED_MESSAGE_TYPE,
                   aPriority)
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
    void* iter = nullptr;
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
                                              IPC::Message::PRIORITY_URGENT))) {
    CloseDescriptor(parentSide);
    CloseDescriptor(childSide);
    return NS_ERROR_BRIDGE_OPEN_PARENT;
  }

  if (!aChildChannel->Send(new ChannelOpened(childSide,
                                            aParentPid,
                                            aChildProtocol,
                                            IPC::Message::PRIORITY_URGENT))) {
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
#ifdef MOZ_CRASH_REPORTER
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

void
AnnotateProcessInformation(base::ProcessId aPid)
{
#ifdef XP_WIN
  ScopedProcessHandle processHandle(
    OpenProcess(READ_CONTROL|PROCESS_QUERY_INFORMATION, FALSE, aPid));
  if (!processHandle) {
    CrashReporter::AnnotateCrashReport(
      NS_LITERAL_CSTRING("IPCExtraSystemError"),
      nsPrintfCString("Failed to get information of process %d, error(%d)",
                      aPid,
                      ::GetLastError()));
    return;
  }

  DWORD exitCode = 0;
  if (!::GetExitCodeProcess(processHandle, &exitCode)) {
    CrashReporter::AnnotateCrashReport(
      NS_LITERAL_CSTRING("IPCExtraSystemError"),
      nsPrintfCString("Failed to get exit information of process %d, error(%d)",
                      aPid,
                      ::GetLastError()));
    return;
  }

  if (exitCode != STILL_ACTIVE) {
    CrashReporter::AnnotateCrashReport(
      NS_LITERAL_CSTRING("IPCExtraSystemError"),
      nsPrintfCString("Process %d is not alive. Exit code: %d",
                      aPid,
                      exitCode));
    return;
  }

  ScopedPSecurityDescriptor secDesc(nullptr);
  PSID ownerSid = nullptr;
  DWORD rv = ::GetSecurityInfo(processHandle,
                               SE_KERNEL_OBJECT,
                               OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                               &ownerSid,
                               nullptr,
                               nullptr,
                               nullptr,
                               &secDesc.rwget());
  if (rv != ERROR_SUCCESS) {
    // GetSecurityInfo() failed.
    CrashReporter::AnnotateCrashReport(
      NS_LITERAL_CSTRING("IPCExtraSystemError"),
      nsPrintfCString("Failed to get security information of process %d,"
                      " error(%d)",
                      aPid,
                      rv));
    return;
  }

  ScopedLPTStr ownerSidStr(nullptr);
  nsString annotation{};
  annotation.AppendLiteral("Owner: ");
  if (::ConvertSidToStringSid(ownerSid, &ownerSidStr.rwget())) {
    annotation.Append(ownerSidStr.get());
  }

  ScopedLPTStr secDescStr(nullptr);
  annotation.AppendLiteral(", Security Descriptor: ");
  if (::ConvertSecurityDescriptorToStringSecurityDescriptor(secDesc,
                                                            SDDL_REVISION_1,
                                                            DACL_SECURITY_INFORMATION,
                                                            &secDescStr.rwget(),
                                                            nullptr)) {
    annotation.Append(secDescStr.get());
  }

  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("IPCExtraSystemError"),
    NS_ConvertUTF16toUTF8(annotation));
#endif
}
#endif

void
LogMessageForProtocol(const char* aTopLevelProtocol, base::ProcessId aOtherPid,
                      const char* aContextDescription,
                      const char* aMessageDescription,
                      MessageDirection aDirection)
{
  nsPrintfCString logMessage("[time: %" PRId64 "][%d%s%d] [%s] %s %s\n",
                             PR_Now(), base::GetCurrentProcId(),
                             aDirection == MessageDirection::eReceiving ? "<-" : "->",
                             aOtherPid, aTopLevelProtocol,
                             aContextDescription,
                             aMessageDescription);
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

} // namespace ipc
} // namespace mozilla
