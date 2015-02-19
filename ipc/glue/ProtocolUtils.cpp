/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/process_util.h"

#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Transport.h"

using namespace IPC;

using base::GetCurrentProcessHandle;
using base::GetProcId;
using base::ProcessHandle;
using base::ProcessId;

namespace mozilla {
namespace ipc {

#ifdef MOZ_IPDL_TESTS
bool IToplevelProtocol::sAllowNonMainThreadUse;
#endif

IToplevelProtocol::~IToplevelProtocol()
{
  MOZ_ASSERT(NS_IsMainThread() || AllowNonMainThreadUse());
  mOpenActors.clear();
}

void IToplevelProtocol::AddOpenedActor(IToplevelProtocol* aActor)
{
  MOZ_ASSERT(NS_IsMainThread() || AllowNonMainThreadUse());

#ifdef DEBUG
  for (const IToplevelProtocol* actor = mOpenActors.getFirst();
       actor;
       actor = actor->getNext()) {
    NS_ASSERTION(actor != aActor,
                 "Open the same protocol for more than one time");
  }
#endif

  mOpenActors.insertBack(aActor);
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
  for (IToplevelProtocol* actor = aTemplate->GetFirstOpenedActors();
       actor;
       actor = actor->getNext()) {
    IToplevelProtocol* newactor = actor->CloneToplevel(aFds, aPeerProcess, aCtx);
    AddOpenedActor(newactor);
  }
}

#ifdef MOZ_IPDL_TESTS
void
IToplevelProtocol::SetAllowNonMainThreadUse()
{
  sAllowNonMainThreadUse = true;
}
#endif

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

bool
Bridge(const PrivateIPDLInterface&,
       MessageChannel* aParentChannel, ProcessHandle aParentProcess,
       MessageChannel* aChildChannel, ProcessHandle aChildProcess,
       ProtocolId aProtocol, ProtocolId aChildProtocol)
{
  ProcessId parentId = GetProcId(aParentProcess);
  ProcessId childId = GetProcId(aChildProcess);
  if (!parentId || !childId) {
    return false;
  }

  TransportDescriptor parentSide, childSide;
  if (!CreateTransport(aParentProcess, aChildProcess,
                       &parentSide, &childSide)) {
    return false;
  }

  if (!aParentChannel->Send(new ChannelOpened(parentSide,
                                              childId,
                                              aProtocol,
                                              IPC::Message::PRIORITY_URGENT)) ||
      !aChildChannel->Send(new ChannelOpened(childSide,
                                             parentId,
                                             aChildProtocol,
                                             IPC::Message::PRIORITY_URGENT))) {
    CloseDescriptor(parentSide);
    CloseDescriptor(childSide);
    return false;
  }
  return true;
}

bool
Open(const PrivateIPDLInterface&,
     MessageChannel* aOpenerChannel, ProcessHandle aOtherProcess,
     Transport::Mode aOpenerMode,
     ProtocolId aProtocol, ProtocolId aChildProtocol)
{
  bool isParent = (Transport::MODE_SERVER == aOpenerMode);
  ProcessHandle thisHandle = GetCurrentProcessHandle();
  ProcessHandle parentHandle = isParent ? thisHandle : aOtherProcess;
  ProcessHandle childHandle = !isParent ? thisHandle : aOtherProcess;
  ProcessId parentId = GetProcId(parentHandle);
  ProcessId childId = GetProcId(childHandle);
  if (!parentId || !childId) {
    return false;
  }

  TransportDescriptor parentSide, childSide;
  if (!CreateTransport(parentHandle, childHandle,
                       &parentSide, &childSide)) {
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

void
ProtocolErrorBreakpoint(const char* aMsg)
{
    // Bugs that generate these error messages can be tough to
    // reproduce.  Log always in the hope that someone finds the error
    // message.
    printf_stderr("IPDL protocol error: %s\n", aMsg);
}

void
FatalError(const char* aProtocolName, const char* aMsg,
           ProcessHandle aHandle, bool aIsParent)
{
  ProtocolErrorBreakpoint(aMsg);

  nsAutoCString formattedMessage("IPDL error [");
  formattedMessage.AppendASCII(aProtocolName);
  formattedMessage.AppendLiteral("]: \"");
  formattedMessage.AppendASCII(aMsg);
  if (aIsParent) {
    formattedMessage.AppendLiteral("\". Killing child side as a result.");
    NS_ERROR(formattedMessage.get());

    if (aHandle != kInvalidProcessHandle &&
        !base::KillProcess(aHandle, base::PROCESS_END_KILLED_BY_USER, false)) {
      NS_ERROR("May have failed to kill child!");
    }
  } else {
    formattedMessage.AppendLiteral("\". abort()ing as a result.");
    NS_RUNTIMEABORT(formattedMessage.get());
  }
}

} // namespace ipc
} // namespace mozilla
