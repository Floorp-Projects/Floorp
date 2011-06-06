/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "base/process_util.h"

#include "mozilla/ipc/AsyncChannel.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Transport.h"

using namespace base;
using namespace IPC;

namespace mozilla {
namespace ipc {

class ChannelOpened : public IPC::Message
{
public:
  ChannelOpened(TransportDescriptor aDescriptor,
                ProcessId aOtherProcess,
                ProtocolId aProtocol)
    : IPC::Message(MSG_ROUTING_CONTROL, // these only go to top-level actors
                   CHANNEL_OPENED_MESSAGE_TYPE,
                   PRIORITY_NORMAL)
  {
    IPC::WriteParam(this, aDescriptor);
    IPC::WriteParam(this, aOtherProcess);
    IPC::WriteParam(this, static_cast<uint32>(aProtocol));
  }

  static bool Read(const IPC::Message& aMsg,
                   TransportDescriptor* aDescriptor,
                   ProcessId* aOtherProcess,
                   ProtocolId* aProtocol)
  {
    void* iter = nsnull;
    if (!IPC::ReadParam(&aMsg, &iter, aDescriptor) ||
        !IPC::ReadParam(&aMsg, &iter, aOtherProcess) ||
        !IPC::ReadParam(&aMsg, &iter, reinterpret_cast<uint32*>(aProtocol))) {
      return false;
    }
    aMsg.EndRead(iter);
    return true;
  }
};

bool
Bridge(const PrivateIPDLInterface&,
       AsyncChannel* aParentChannel, ProcessHandle aParentProcess,
       AsyncChannel* aChildChannel, ProcessHandle aChildProcess,
       ProtocolId aProtocol)
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
                                              aProtocol)) ||
      !aChildChannel->Send(new ChannelOpened(childSide,
                                             parentId,
                                             aProtocol))) {
    CloseDescriptor(parentSide);
    CloseDescriptor(childSide);
    return false;
  }
  return true;
}

bool
Open(const PrivateIPDLInterface&,
     AsyncChannel* aOpenerChannel, ProcessHandle aOtherProcess,
     Transport::Mode aOpenerMode,
     ProtocolId aProtocol)
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
  Message* childMsg = new ChannelOpened(childSide, parentId, aProtocol);
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

} // namespace ipc
} // namespace mozilla
