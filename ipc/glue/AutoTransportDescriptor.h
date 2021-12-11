/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_AutoTransportDescriptor_h
#define mozilla_ipc_AutoTransportDescriptor_h

#include <utility>

#include "mozilla/ipc/Transport.h"
#include "mozilla/Result.h"

namespace mozilla::ipc {

class AutoTransportDescriptor final {
 public:
  AutoTransportDescriptor() = default;
  explicit AutoTransportDescriptor(TransportDescriptor aTransport)
      : mTransport(aTransport), mValid(true) {}
  ~AutoTransportDescriptor();

  AutoTransportDescriptor(AutoTransportDescriptor&& aOther) noexcept;
  AutoTransportDescriptor& operator=(AutoTransportDescriptor&& aOther) noexcept;

  static Result<std::pair<AutoTransportDescriptor, AutoTransportDescriptor>,
                nsresult>
  Create(int32_t aProcIdOne);

  AutoTransportDescriptor Duplicate() const;

  // NOTE: This will consume this transport descriptor, making it invalid.
  UniquePtr<Transport> Open(Transport::Mode aMode);

  explicit operator bool() const { return mValid; }

 private:
  friend struct IPC::ParamTraits<AutoTransportDescriptor>;

  TransportDescriptor mTransport;
  bool mValid = false;
};

}  // namespace mozilla::ipc

namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::AutoTransportDescriptor> {
  using paramType = mozilla::ipc::AutoTransportDescriptor;
  static void Write(Message* aMsg, paramType&& aParam) {
    WriteParam(aMsg, aParam.mValid);
    if (aParam.mValid) {
      // TransportDescriptor's serialization code takes it by `const T&`,
      // however, it actually closes the handles when passed, so we need to mark
      // our state as invalid after sending.
      WriteParam(aMsg, aParam.mTransport);
      aParam.mValid = false;
    }
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool valid = false;
    if (!ReadParam(aMsg, aIter, &valid)) {
      return false;
    }
    if (!valid) {
      *aResult = mozilla::ipc::AutoTransportDescriptor{};
      return true;
    }

    mozilla::ipc::TransportDescriptor descr;
    if (!ReadParam(aMsg, aIter, &descr)) {
      return false;
    }
    *aResult = mozilla::ipc::AutoTransportDescriptor{descr};
    return true;
  }
};

}  // namespace IPC

#endif
