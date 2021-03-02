/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is the implementation of SynchronizedSharedMemory for unimplemented
// platforms

#include "mozilla/dom/SynchronizedSharedMemory.h"
#include "mozilla/dom/SynchronizedSharedMemoryRemoteInfo.h"

namespace mozilla::dom {

class SynchronizedSharedMemoryDetail::Impl {};

// static
Maybe<SynchronizedSharedMemoryDetail> SynchronizedSharedMemoryDetail::CreateNew(
    uint32_t) {
  return Nothing{};
}

void SynchronizedSharedMemoryDetail::LockMutex() {
  MOZ_CRASH("Should never be called");
}

void SynchronizedSharedMemoryDetail::UnlockMutex() {
  MOZ_CRASH("Should never be called");
}

void* SynchronizedSharedMemoryDetail::GetPtr() const {
  MOZ_CRASH("Should never be called");
}

// static
Maybe<SynchronizedSharedMemoryDetail>
SynchronizedSharedMemoryDetail::CreateFromRemoteInfo(
    const SynchronizedSharedMemoryRemoteInfo&) {
  return Nothing{};
}

bool SynchronizedSharedMemoryDetail::GenerateRemoteInfo(
    const mozilla::ipc::IProtocol*, SynchronizedSharedMemoryRemoteInfo*) {
  MOZ_CRASH("Should never be called");
}

bool SynchronizedSharedMemoryDetail::GenerateTestRemoteInfo(
    SynchronizedSharedMemoryRemoteInfo*) {
  MOZ_CRASH("Should never be called");
}

SynchronizedSharedMemoryDetail::SynchronizedSharedMemoryDetail() = default;
SynchronizedSharedMemoryDetail::~SynchronizedSharedMemoryDetail() = default;

SynchronizedSharedMemoryDetail::SynchronizedSharedMemoryDetail(
    UniquePtr<Impl> aImpl)
    : mImpl(std::move(aImpl)) {}

SynchronizedSharedMemoryDetail::SynchronizedSharedMemoryDetail(
    SynchronizedSharedMemoryDetail&&) = default;
SynchronizedSharedMemoryDetail& SynchronizedSharedMemoryDetail::operator=(
    SynchronizedSharedMemoryDetail&&) = default;

}  // namespace mozilla::dom
