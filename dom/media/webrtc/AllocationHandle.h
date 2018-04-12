/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AllocationHandle_h
#define AllocationHandle_h

#include <cstdint>
#include <limits>

#include "MediaEnginePrefs.h"
#include "MediaManager.h"
#include "MediaTrackConstraints.h"
#include "mozilla/Assertions.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsString.h"

namespace mozilla {

/**
 * AllocationHandle helps keep track of metadata for allocations of shared
 * MediaEngineSources. That is, for MediaEngineSources that support more than
 * one concurrent allocation.
 */
class AllocationHandle
{
  ~AllocationHandle() = default;

public:
  static uint64_t GetUniqueId()
  {
    static uint64_t sId = 0;

    MOZ_ASSERT(MediaManager::GetIfExists());
    MOZ_ASSERT(MediaManager::GetIfExists()->IsInMediaThread());
    MOZ_RELEASE_ASSERT(sId < std::numeric_limits<decltype(sId)>::max());
    return sId++;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AllocationHandle);

  AllocationHandle() = delete;
  AllocationHandle(const dom::MediaTrackConstraints& aConstraints,
                   const ipc::PrincipalInfo& aPrincipalInfo,
                   const nsString& aDeviceId)
    : mId(GetUniqueId())
    , mDeviceId(aDeviceId)
    , mPrincipalInfo(aPrincipalInfo)
    , mConstraints(aConstraints)
  {}

  const uint64_t mId;
  const nsString mDeviceId;
  const ipc::PrincipalInfo mPrincipalInfo;
  NormalizedConstraints mConstraints;
};

} // namespace mozilla

#endif // AllocationHandle_h
