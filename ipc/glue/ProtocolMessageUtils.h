/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef IPC_GLUE_PROTOCOLMESSAGEUTILS_H
#define IPC_GLUE_PROTOCOLMESSAGEUTILS_H

#include <stdint.h>
#include <string>
#include "base/string_util.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_message_utils.h"
#include "ipc/EnumSerializer.h"
#include "mozilla/Assertions.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/ProtocolUtils.h"

class PickleIterator;

namespace mozilla::ipc {
class FileDescriptor;
template <class PFooSide>
class Endpoint;
template <class PFooSide>
class ManagedEndpoint;
template <typename P>
struct IPDLParamTraits;
}  // namespace mozilla::ipc

namespace IPC {

class Message;
class MessageReader;
class MessageWriter;

template <>
struct ParamTraits<Channel::Mode>
    : ContiguousEnumSerializerInclusive<Channel::Mode, Channel::MODE_SERVER,
                                        Channel::MODE_CLIENT> {};

template <>
struct ParamTraits<IPCMessageStart>
    : ContiguousEnumSerializer<IPCMessageStart, IPCMessageStart(0),
                               LastMsgIndex> {};

template <>
struct ParamTraits<mozilla::ipc::ActorHandle> {
  typedef mozilla::ipc::ActorHandle paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    IPC::WriteParam(aWriter, aParam.mId);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    int id;
    if (IPC::ReadParam(aReader, &id)) {
      aResult->mId = id;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(StringPrintf(L"(%d)", aParam.mId));
  }
};

template <>
struct ParamTraits<mozilla::ipc::UntypedEndpoint> {
  using paramType = mozilla::ipc::UntypedEndpoint;

  static void Write(MessageWriter* aWriter, paramType&& aParam);

  static bool Read(MessageReader* aReader, paramType* aResult);

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(StringPrintf(L"Endpoint"));
  }
};

template <class PFooSide>
struct ParamTraits<mozilla::ipc::Endpoint<PFooSide>>
    : ParamTraits<mozilla::ipc::UntypedEndpoint> {};

}  // namespace IPC

namespace mozilla::ipc {

template <>
struct IPDLParamTraits<UntypedManagedEndpoint> {
  using paramType = UntypedManagedEndpoint;

  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    paramType&& aParam);
  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult);
};

template <class PFooSide>
struct IPDLParamTraits<ManagedEndpoint<PFooSide>> {
  using paramType = ManagedEndpoint<PFooSide>;

  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    paramType&& aParam) {
    IPDLParamTraits<UntypedManagedEndpoint>::Write(aWriter, aActor,
                                                   std::move(aParam));
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    return IPDLParamTraits<UntypedManagedEndpoint>::Read(aReader, aActor,
                                                         aResult);
  }
};

template <>
struct IPDLParamTraits<FileDescriptor> {
  typedef FileDescriptor paramType;

  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam);
  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult);
};
}  // namespace mozilla::ipc

#endif  // IPC_GLUE_PROTOCOLMESSAGEUTILS_H
