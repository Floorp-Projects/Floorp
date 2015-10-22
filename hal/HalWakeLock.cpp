/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "mozilla/HalWakeLock.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/ContentParent.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsIPropertyBag2.h"
#include "nsIObserverService.h"

using namespace mozilla;
using namespace mozilla::hal;

namespace {

struct LockCount {
  LockCount()
    : numLocks(0)
    , numHidden(0)
  {}
  uint32_t numLocks;
  uint32_t numHidden;
  nsTArray<uint64_t> processes;
};

typedef nsDataHashtable<nsUint64HashKey, LockCount> ProcessLockTable;
typedef nsClassHashtable<nsStringHashKey, ProcessLockTable> LockTable;

int sActiveListeners = 0;
StaticAutoPtr<LockTable> sLockTable;
bool sInitialized = false;
bool sIsShuttingDown = false;

WakeLockInformation
WakeLockInfoFromLockCount(const nsAString& aTopic, const LockCount& aLockCount)
{
  // TODO: Once we abandon b2g18, we can switch this to use the
  // WakeLockInformation constructor, which is better because it doesn't let us
  // forget to assign a param.  For now we have to do it this way, because
  // b2g18 doesn't have the nsTArray <--> InfallibleTArray conversion (bug
  // 819791).

  WakeLockInformation info;
  info.topic() = aTopic;
  info.numLocks() = aLockCount.numLocks;
  info.numHidden() = aLockCount.numHidden;
  info.lockingProcesses().AppendElements(aLockCount.processes);
  return info;
}

static void
CountWakeLocks(ProcessLockTable* aTable, LockCount* aTotalCount)
{
  for (auto iter = aTable->Iter(); !iter.Done(); iter.Next()) {
    const uint64_t& key = iter.Key();
    LockCount count = iter.UserData();

    aTotalCount->numLocks += count.numLocks;
    aTotalCount->numHidden += count.numHidden;

    // This is linear in the number of processes, but that should be small.
    if (!aTotalCount->processes.Contains(key)) {
      aTotalCount->processes.AppendElement(key);
    }
  }
}

class ClearHashtableOnShutdown final : public nsIObserver {
  ~ClearHashtableOnShutdown() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS(ClearHashtableOnShutdown, nsIObserver)

NS_IMETHODIMP
ClearHashtableOnShutdown::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* data)
{
  MOZ_ASSERT(!strcmp(aTopic, "xpcom-shutdown"));

  sIsShuttingDown = true;
  sLockTable = nullptr;

  return NS_OK;
}

class CleanupOnContentShutdown final : public nsIObserver {
  ~CleanupOnContentShutdown() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS(CleanupOnContentShutdown, nsIObserver)

NS_IMETHODIMP
CleanupOnContentShutdown::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* data)
{
  MOZ_ASSERT(!strcmp(aTopic, "ipc:content-shutdown"));

  if (sIsShuttingDown) {
    return NS_OK;
  }

  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  if (!props) {
    NS_WARNING("ipc:content-shutdown message without property bag as subject");
    return NS_OK;
  }

  uint64_t childID = 0;
  nsresult rv = props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"),
                                           &childID);
  if (NS_SUCCEEDED(rv)) {
    for (auto iter = sLockTable->Iter(); !iter.Done(); iter.Next()) {
      nsAutoPtr<ProcessLockTable>& table = iter.Data();

      if (table->Get(childID, nullptr)) {
        table->Remove(childID);

        LockCount totalCount;
        CountWakeLocks(table, &totalCount);

        if (sActiveListeners) {
          NotifyWakeLockChange(WakeLockInfoFromLockCount(iter.Key(),
                                                         totalCount));
        }

        if (totalCount.numLocks == 0) {
          iter.Remove();
        }
      }
    }
  } else {
    NS_WARNING("ipc:content-shutdown message without childID property");
  }
  return NS_OK;
}

void
Init()
{
  sLockTable = new LockTable();
  sInitialized = true;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(new ClearHashtableOnShutdown(), "xpcom-shutdown", false);
    obs->AddObserver(new CleanupOnContentShutdown(), "ipc:content-shutdown", false);
  }
}

} // namespace

namespace mozilla {

namespace hal {

WakeLockState
ComputeWakeLockState(int aNumLocks, int aNumHidden)
{
  if (aNumLocks == 0) {
    return WAKE_LOCK_STATE_UNLOCKED;
  } else if (aNumLocks == aNumHidden) {
    return WAKE_LOCK_STATE_HIDDEN;
  } else {
    return WAKE_LOCK_STATE_VISIBLE;
  }
}

} // namespace hal

namespace hal_impl {

void
EnableWakeLockNotifications()
{
  sActiveListeners++;
}

void
DisableWakeLockNotifications()
{
  sActiveListeners--;
}

void
ModifyWakeLock(const nsAString& aTopic,
               hal::WakeLockControl aLockAdjust,
               hal::WakeLockControl aHiddenAdjust,
               uint64_t aProcessID)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aProcessID != CONTENT_PROCESS_ID_UNKNOWN);

  if (sIsShuttingDown) {
    return;
  }
  if (!sInitialized) {
    Init();
  }

  ProcessLockTable* table = sLockTable->Get(aTopic);
  LockCount processCount;
  LockCount totalCount;
  if (!table) {
    table = new ProcessLockTable();
    sLockTable->Put(aTopic, table);
  } else {
    table->Get(aProcessID, &processCount);
    CountWakeLocks(table, &totalCount);
  }

  MOZ_ASSERT(processCount.numLocks >= processCount.numHidden);
  MOZ_ASSERT(aLockAdjust >= 0 || processCount.numLocks > 0);
  MOZ_ASSERT(aHiddenAdjust >= 0 || processCount.numHidden > 0);
  MOZ_ASSERT(totalCount.numLocks >= totalCount.numHidden);
  MOZ_ASSERT(aLockAdjust >= 0 || totalCount.numLocks > 0);
  MOZ_ASSERT(aHiddenAdjust >= 0 || totalCount.numHidden > 0);

  WakeLockState oldState = ComputeWakeLockState(totalCount.numLocks, totalCount.numHidden);
  bool processWasLocked = processCount.numLocks > 0;

  processCount.numLocks += aLockAdjust;
  processCount.numHidden += aHiddenAdjust;

  totalCount.numLocks += aLockAdjust;
  totalCount.numHidden += aHiddenAdjust;

  if (processCount.numLocks) {
    table->Put(aProcessID, processCount);
  } else {
    table->Remove(aProcessID);
  }
  if (!totalCount.numLocks) {
    sLockTable->Remove(aTopic);
  }

  if (sActiveListeners &&
      (oldState != ComputeWakeLockState(totalCount.numLocks,
                                        totalCount.numHidden) ||
       processWasLocked != (processCount.numLocks > 0))) {

    WakeLockInformation info;
    hal::GetWakeLockInfo(aTopic, &info);
    NotifyWakeLockChange(info);
  }
}

void
GetWakeLockInfo(const nsAString& aTopic, WakeLockInformation* aWakeLockInfo)
{
  if (sIsShuttingDown) {
    NS_WARNING("You don't want to get wake lock information during xpcom-shutdown!");
    *aWakeLockInfo = WakeLockInformation();
    return;
  }
  if (!sInitialized) {
    Init();
  }

  ProcessLockTable* table = sLockTable->Get(aTopic);
  if (!table) {
    *aWakeLockInfo = WakeLockInfoFromLockCount(aTopic, LockCount());
    return;
  }
  LockCount totalCount;
  CountWakeLocks(table, &totalCount);
  *aWakeLockInfo = WakeLockInfoFromLockCount(aTopic, totalCount);
}

} // namespace hal_impl
} // namespace mozilla
