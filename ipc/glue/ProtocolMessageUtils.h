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
#include "mozilla/Assertions.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Transport.h"

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

template <>
struct ParamTraits<mozilla::ipc::ActorHandle> {
  typedef mozilla::ipc::ActorHandle paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    IPC::WriteParam(aMsg, aParam.mId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    int id;
    if (IPC::ReadParam(aMsg, aIter, &id)) {
      aResult->mId = id;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(StringPrintf(L"(%d)", aParam.mId));
  }
};

template <class PFooSide>
struct ParamTraits<mozilla::ipc::Endpoint<PFooSide>> {
  typedef mozilla::ipc::Endpoint<PFooSide> paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    IPC::WriteParam(aMsg, aParam.mValid);
    if (!aParam.mValid) {
      return;
    }

    IPC::WriteParam(aMsg, static_cast<uint32_t>(aParam.mMode));

    // We duplicate the descriptor so that our own file descriptor remains
    // valid after the write. An alternative would be to set
    // aParam.mTransport.mValid to false, but that won't work because aParam
    // is const.
    mozilla::ipc::TransportDescriptor desc =
        mozilla::ipc::DuplicateDescriptor(aParam.mTransport);
    IPC::WriteParam(aMsg, desc);

    IPC::WriteParam(aMsg, aParam.mMyPid);
    IPC::WriteParam(aMsg, aParam.mOtherPid);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    MOZ_RELEASE_ASSERT(!aResult->mValid);

    if (!IPC::ReadParam(aMsg, aIter, &aResult->mValid)) {
      return false;
    }
    if (!aResult->mValid) {
      // Object is empty, but read succeeded.
      return true;
    }

    uint32_t mode;
    if (!IPC::ReadParam(aMsg, aIter, &mode) ||
        !IPC::ReadParam(aMsg, aIter, &aResult->mTransport) ||
        !IPC::ReadParam(aMsg, aIter, &aResult->mMyPid) ||
        !IPC::ReadParam(aMsg, aIter, &aResult->mOtherPid)) {
      return false;
    }
    aResult->mMode = Channel::Mode(mode);
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(StringPrintf(L"Endpoint"));
  }
};

template <class PFooSide>
struct ParamTraits<mozilla::ipc::ManagedEndpoint<PFooSide>> {
  typedef mozilla::ipc::ManagedEndpoint<PFooSide> paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    IPC::WriteParam(aMsg, aParam.mId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    MOZ_RELEASE_ASSERT(aResult->mId == 0);

    if (!IPC::ReadParam(aMsg, aIter, &aResult->mId)) {
      return false;
    }
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(StringPrintf(L"ManagedEndpoint"));
  }
};

}  // namespace IPC

namespace mozilla::ipc {
template <>
struct IPDLParamTraits<FileDescriptor> {
  typedef FileDescriptor paramType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult);
};
}  // namespace mozilla::ipc

#endif  // IPC_GLUE_PROTOCOLMESSAGEUTILS_H
