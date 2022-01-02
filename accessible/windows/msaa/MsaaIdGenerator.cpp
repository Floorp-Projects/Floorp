/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MsaaIdGenerator.h"

#include "mozilla/a11y/AccessibleWrap.h"
#include "mozilla/a11y/MsaaAccessible.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "nsTHashMap.h"
#include "sdnAccessible.h"

static const uint32_t kNumFullIDBits = 31UL;

// These constants may be adjusted to modify the proportion of the Child ID
// allocated to the content ID vs proportion allocated to the unique ID. They
// must always sum to 31, ie. the width of a 32-bit integer less the sign bit.

// NB: kNumContentProcessIDBits must be large enough to successfully hold the
// maximum permitted number of e10s content processes. If the e10s maximum
// number of content processes changes, then kNumContentProcessIDBits must also
// be updated if necessary to accommodate that new value!
static const uint32_t kNumContentProcessIDBits = 8UL;
static const uint32_t kNumUniqueIDBits = (31UL - kNumContentProcessIDBits);

static_assert(
    kNumContentProcessIDBits + kNumUniqueIDBits == 31,
    "Allocation of Content ID bits and Unique ID bits must sum to 31");

namespace mozilla {
namespace a11y {
namespace detail {

typedef nsTHashMap<nsUint64HashKey, uint32_t> ContentParentIdMap;

#pragma pack(push, 1)
union MsaaID {
  int32_t mInt32;
  uint32_t mUInt32;
  struct {
    uint32_t mUniqueID : kNumUniqueIDBits;
    uint32_t mContentProcessID : kNumContentProcessIDBits;
    uint32_t mSignBit : 1;
  } mCracked;
};
#pragma pack(pop)

static uint32_t BuildMsaaID(const uint32_t aID,
                            const uint32_t aContentProcessID) {
  MsaaID id;
  id.mCracked.mSignBit = 0;
  id.mCracked.mUniqueID = aID;
  id.mCracked.mContentProcessID = aContentProcessID;
  return ~id.mUInt32;
}

class MsaaIDCracker {
 public:
  explicit MsaaIDCracker(const uint32_t aMsaaID) { mID.mUInt32 = ~aMsaaID; }

  uint32_t GetContentProcessId() { return mID.mCracked.mContentProcessID; }

  uint32_t GetUniqueId() { return mID.mCracked.mUniqueID; }

 private:
  MsaaID mID;
};

}  // namespace detail

uint32_t MsaaIdGenerator::GetID() {
  if (!mIDSet) {
    // XXX This is only a pointer because we need to decide how many bits we
    // need for the IDSet at runtime. Once the cache pref is gone, this no
    // longer needs to be a pointer.
    mIDSet =
        MakeUnique<IDSet>(StaticPrefs::accessibility_cache_enabled_AtStartup()
                              ? kNumFullIDBits
                              : kNumUniqueIDBits);
  }
  uint32_t id = mIDSet->GetID();
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    MOZ_ASSERT(id <= ((1UL << kNumFullIDBits) - 1UL));
    return ~id;
  }
  MOZ_ASSERT(id <= ((1UL << kNumUniqueIDBits) - 1UL));
  return detail::BuildMsaaID(id, ResolveContentProcessID());
}

bool MsaaIdGenerator::ReleaseID(uint32_t aID) {
  MOZ_ASSERT(aID != MsaaAccessible::kNoID);
  if (!mIDSet) {
    // If we're in the parent process and we're trying to release an id created
    // in a content process, mIDSet might not exist yet. Just ignore this.
    MOZ_ASSERT(XRE_IsParentProcess());
    return false;
  }
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Releasing an id means it can be reused. Reusing ids too quickly can
    // cause problems for clients which process events asynchronously.
    // Therefore, we release ids after a short delay. This doesn't seem to be
    // necessary when the cache is disabled, perhaps because the COM runtime
    // holds references to our objects for longer.
    const uint32_t kReleaseDelay = 1000;
    mIDsToRelease.AppendElement(aID);
    if (mReleaseIDTimer) {
      mReleaseIDTimer->SetDelay(kReleaseDelay);
    } else {
      NS_NewTimerWithCallback(
          getter_AddRefs(mReleaseIDTimer),
          // MsaaIdGenerator's destructor cancels mReleaseIDTimer, so capturing
          // this here is safe.
          [this](nsITimer* aTimer) {
            for (auto id : mIDsToRelease) {
              mIDSet->ReleaseID(~id);
            }
            mIDsToRelease.Clear();
            mReleaseIDTimer = nullptr;
          },
          kReleaseDelay, nsITimer::TYPE_ONE_SHOT,
          "a11y::MsaaIdGenerator::ReleaseIDDelayed");
    }
    return true;
  }
  detail::MsaaIDCracker cracked(aID);
  if (cracked.GetContentProcessId() != ResolveContentProcessID()) {
    return false;
  }
  mIDSet->ReleaseID(cracked.GetUniqueId());
  return true;
}

void MsaaIdGenerator::ReleaseID(NotNull<MsaaAccessible*> aMsaaAcc) {
  // ReleaseID may fail if chrome holds a proxy whose ID was originally
  // generated by a content process. Since ReleaseID only has meaning in the
  // process that originally generated that ID, we ignore ReleaseID calls for
  // any ID that did not come from the current process.
  ReleaseID(aMsaaAcc->GetExistingID());
}

void MsaaIdGenerator::ReleaseID(NotNull<sdnAccessible*> aSdnAcc) {
  Maybe<uint32_t> id = aSdnAcc->ReleaseUniqueID();
  if (id.isSome()) {
    DebugOnly<bool> released = ReleaseID(id.value());
    MOZ_ASSERT(released);
  }
}

bool MsaaIdGenerator::IsChromeID(uint32_t aID) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return true;
  }
  detail::MsaaIDCracker cracked(aID);
  return cracked.GetContentProcessId() == 0;
}

bool MsaaIdGenerator::IsIDForThisContentProcess(uint32_t aID) {
  MOZ_ASSERT(XRE_IsContentProcess());
  detail::MsaaIDCracker cracked(aID);
  return cracked.GetContentProcessId() == ResolveContentProcessID();
}

bool MsaaIdGenerator::IsIDForContentProcess(
    uint32_t aID, dom::ContentParentId aIPCContentProcessId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!StaticPrefs::accessibility_cache_enabled_AtStartup());
  detail::MsaaIDCracker cracked(aID);
  return cracked.GetContentProcessId() ==
         GetContentProcessIDFor(aIPCContentProcessId);
}

bool MsaaIdGenerator::IsSameContentProcessFor(uint32_t aFirstID,
                                              uint32_t aSecondID) {
  MOZ_ASSERT(!StaticPrefs::accessibility_cache_enabled_AtStartup());
  detail::MsaaIDCracker firstCracked(aFirstID);
  detail::MsaaIDCracker secondCracked(aSecondID);
  return firstCracked.GetContentProcessId() ==
         secondCracked.GetContentProcessId();
}

uint32_t MsaaIdGenerator::ResolveContentProcessID() {
  if (XRE_IsParentProcess()) {
    return 0;
  }

  dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
  uint32_t result = contentChild->GetMsaaID();

  MOZ_ASSERT(result);
  return result;
}

/**
 * Each dom::ContentParent has a 64-bit ID. This ID is monotonically increasing
 * with each new content process, so those IDs are effectively single-use. OTOH,
 * MSAA requires 32-bit IDs. Since we only allocate kNumContentProcessIDBits for
 * the content process ID component, the MSAA content process ID value must be
 * reusable. sContentParentIdMap holds the current associations between
 * dom::ContentParent IDs and the MSAA content parent IDs that have been
 * allocated to them.
 */
static StaticAutoPtr<detail::ContentParentIdMap> sContentParentIdMap;

static const uint32_t kBitsPerByte = 8UL;
// Set sContentProcessIdBitmap[0] to 1 to reserve the Chrome process's id
static uint64_t sContentProcessIdBitmap[(1UL << kNumContentProcessIDBits) /
                                        (sizeof(uint64_t) * kBitsPerByte)] = {
    1ULL};
static const uint32_t kBitsPerElement =
    sizeof(sContentProcessIdBitmap[0]) * kBitsPerByte;

uint32_t MsaaIdGenerator::GetContentProcessIDFor(
    dom::ContentParentId aIPCContentProcessID) {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());
  if (!sContentParentIdMap) {
    sContentParentIdMap = new detail::ContentParentIdMap();
    ClearOnShutdown(&sContentParentIdMap);
  }

  return sContentParentIdMap->LookupOrInsertWith(aIPCContentProcessID, [] {
    uint32_t value = 0;
    uint32_t index = 0;
    for (; index < ArrayLength(sContentProcessIdBitmap); ++index) {
      if (sContentProcessIdBitmap[index] == UINT64_MAX) {
        continue;
      }
      uint32_t bitIndex =
          CountTrailingZeroes64(~sContentProcessIdBitmap[index]);
      MOZ_ASSERT(!(sContentProcessIdBitmap[index] & (1ULL << bitIndex)));
      MOZ_ASSERT(bitIndex != 0 || index != 0);
      sContentProcessIdBitmap[index] |= (1ULL << bitIndex);
      value = index * kBitsPerElement + bitIndex;
      break;
    }

    // If we run out of content process IDs, we're in trouble
    MOZ_RELEASE_ASSERT(index < ArrayLength(sContentProcessIdBitmap));

    return value;
  });
}

void MsaaIdGenerator::ReleaseContentProcessIDFor(
    dom::ContentParentId aIPCContentProcessID) {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());
  if (!sContentParentIdMap) {
    // Since Content IDs are generated lazily, ContentParent might attempt
    // to release an ID that was never allocated to begin with.
    return;
  }

  Maybe<uint32_t> mapping = sContentParentIdMap->Extract(aIPCContentProcessID);
  if (!mapping) {
    // Since Content IDs are generated lazily, ContentParent might attempt
    // to release an ID that was never allocated to begin with.
    return;
  }

  uint32_t index = mapping.ref() / kBitsPerElement;
  MOZ_ASSERT(index < ArrayLength(sContentProcessIdBitmap));

  uint64_t mask = 1ULL << (mapping.ref() % kBitsPerElement);
  MOZ_ASSERT(sContentProcessIdBitmap[index] & mask);

  sContentProcessIdBitmap[index] &= ~mask;
}

}  // namespace a11y
}  // namespace mozilla
