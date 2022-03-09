/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/ScopedPort.h"
#include "mozilla/ipc/NodeController.h"
#include "chrome/common/ipc_message_utils.h"

namespace mozilla::ipc {

void ScopedPort::Reset() {
  if (mValid) {
    mController->ClosePort(mPort);
  }
  mValid = false;
  mPort = {};
  mController = nullptr;
}

auto ScopedPort::Release() -> PortRef {
  if (!mValid) {
    return {};
  }
  mValid = false;
  mController = nullptr;
  return std::exchange(mPort, PortRef{});
}

ScopedPort::ScopedPort() = default;

ScopedPort::~ScopedPort() { Reset(); }

ScopedPort::ScopedPort(PortRef aPort, NodeController* aController)
    : mValid(true), mPort(std::move(aPort)), mController(aController) {
  MOZ_ASSERT(mPort.is_valid() && mController);
}

ScopedPort::ScopedPort(ScopedPort&& aOther)
    : mValid(std::exchange(aOther.mValid, false)),
      mPort(std::move(aOther.mPort)),
      mController(std::move(aOther.mController)) {}

ScopedPort& ScopedPort::operator=(ScopedPort&& aOther) {
  if (this != &aOther) {
    Reset();
    mValid = std::exchange(aOther.mValid, false);
    mPort = std::move(aOther.mPort);
    mController = std::move(aOther.mController);
  }
  return *this;
}

}  // namespace mozilla::ipc

void IPC::ParamTraits<mozilla::ipc::ScopedPort>::Write(MessageWriter* aWriter,
                                                       paramType&& aParam) {
  aWriter->WriteBool(aParam.IsValid());
  if (!aParam.IsValid()) {
    return;
  }
  aWriter->WritePort(std::move(aParam));
}

bool IPC::ParamTraits<mozilla::ipc::ScopedPort>::Read(MessageReader* aReader,
                                                      paramType* aResult) {
  bool isValid = false;
  if (!aReader->ReadBool(&isValid)) {
    return false;
  }
  if (!isValid) {
    *aResult = {};
    return true;
  }
  return aReader->ConsumePort(aResult);
}
