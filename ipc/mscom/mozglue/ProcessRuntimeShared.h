/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_ProcessRuntimeShared_h
#define mozilla_mscom_ProcessRuntimeShared_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

namespace mozilla {
namespace mscom {
namespace detail {

enum class ProcessInitState : uint32_t {
  Uninitialized = 0,
  PartialSecurityInitialized,
  PartialGlobalOptions,
  FullyInitialized,
};

MFBT_API ProcessInitState& BeginProcessRuntimeInit();
MFBT_API void EndProcessRuntimeInit();

}  // namespace detail

class MOZ_RAII ProcessInitLock final {
 public:
  ProcessInitLock() : mInitState(detail::BeginProcessRuntimeInit()) {}

  ~ProcessInitLock() { detail::EndProcessRuntimeInit(); }

  detail::ProcessInitState GetInitState() const { return mInitState; }

  void SetInitState(const detail::ProcessInitState aNewState) {
    MOZ_DIAGNOSTIC_ASSERT(aNewState > mInitState);
    mInitState = aNewState;
  }

  ProcessInitLock(const ProcessInitLock&) = delete;
  ProcessInitLock(ProcessInitLock&&) = delete;
  ProcessInitLock operator=(const ProcessInitLock&) = delete;
  ProcessInitLock operator=(ProcessInitLock&&) = delete;

 private:
  detail::ProcessInitState& mInitState;
};

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_ProcessRuntimeShared_h
