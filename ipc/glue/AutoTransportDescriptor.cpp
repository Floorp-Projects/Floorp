/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/AutoTransportDescriptor.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ipc/Transport.h"

namespace mozilla::ipc {

AutoTransportDescriptor::AutoTransportDescriptor(
    AutoTransportDescriptor&& aOther) noexcept
    : mTransport(aOther.mTransport), mValid(aOther.mValid) {
  aOther.mValid = false;
}

AutoTransportDescriptor& AutoTransportDescriptor::operator=(
    AutoTransportDescriptor&& aOther) noexcept {
  if (mValid) {
    CloseDescriptor(mTransport);
  }
  mValid = std::exchange(aOther.mValid, false);
  mTransport = aOther.mTransport;
  return *this;
}

AutoTransportDescriptor::~AutoTransportDescriptor() {
  if (mValid) {
    CloseDescriptor(mTransport);
  }
}

Result<std::pair<AutoTransportDescriptor, AutoTransportDescriptor>, nsresult>
AutoTransportDescriptor::Create(int32_t aProcIdOne) {
  TransportDescriptor one, two;
  MOZ_TRY(CreateTransport(aProcIdOne, &one, &two));
  return std::pair{AutoTransportDescriptor(one), AutoTransportDescriptor(two)};
}

AutoTransportDescriptor AutoTransportDescriptor::Duplicate() const {
  if (mValid) {
    return AutoTransportDescriptor{DuplicateDescriptor(mTransport)};
  }
  return {};
}

UniquePtr<Transport> AutoTransportDescriptor::Open(Transport::Mode aMode) {
  if (mValid) {
    mValid = false;
    return OpenDescriptor(mTransport, aMode);
  }
  return nullptr;
}

}  // namespace mozilla::ipc
