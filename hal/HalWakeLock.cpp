/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "mozilla/HalWakeLock.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsIPropertyBag2.h"
#include "nsObserverService.h"

using namespace mozilla::hal;

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

} // hal
} // mozilla

namespace mozilla {
namespace hal_impl {

namespace {
struct LockCount {
  LockCount()
    : numLocks(0)
    , numHidden(0)
  {}
  uint32_t numLocks;
  uint32_t numHidden;
};
typedef nsDataHashtable<nsUint64HashKey, LockCount> ProcessLockTable;
typedef nsClassHashtable<nsStringHashKey, ProcessLockTable> LockTable;

static int sActiveListeners = 0;
static StaticAutoPtr<LockTable> sLockTable;
static bool sInitialized = false;
static bool sIsShuttingDown = false;

static PLDHashOperator
CountWakeLocks(const uint64_t& aKey, LockCount aCount, void* aUserArg)
{
  MOZ_ASSERT(aUserArg);

  LockCount* totalCount = static_cast<LockCount*>(aUserArg);
  totalCount->numLocks += aCount.numLocks;
  totalCount->numHidden += aCount.numHidden;

  return PL_DHASH_NEXT;
}

static PLDHashOperator
RemoveChildFromList(const nsAString& aKey, nsAutoPtr<ProcessLockTable>& aTable,
                    void* aUserArg)
{
  MOZ_ASSERT(aUserArg);

  PLDHashOperator op = PL_DHASH_NEXT;
  uint64_t childID = *static_cast<uint64_t*>(aUserArg);
  if (aTable->Get(childID, NULL)) {
    aTable->Remove(childID);
    if (sActiveListeners) {
      LockCount totalCount;
      WakeLockInformation info;
      aTable->EnumerateRead(CountWakeLocks, &totalCount);
      if (!totalCount.numLocks) {
        op = PL_DHASH_REMOVE;
      }
      info.numLocks() = totalCount.numLocks;
      info.numHidden() = totalCount.numHidden;
      info.topic() = aKey;
      NotifyWakeLockChange(info);
    }
  }

  return op;
}

class ClearHashtableOnShutdown MOZ_FINAL : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(ClearHashtableOnShutdown, nsIObserver)

NS_IMETHODIMP
ClearHashtableOnShutdown::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* data)
{
  MOZ_ASSERT(!strcmp(aTopic, "xpcom-shutdown"));

  sIsShuttingDown = true;
  sLockTable = nullptr;

  return NS_OK;
}

class CleanupOnContentShutdown MOZ_FINAL : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(CleanupOnContentShutdown, nsIObserver)

NS_IMETHODIMP
CleanupOnContentShutdown::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* data)
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
    sLockTable->Enumerate(RemoveChildFromList, &childID);
  } else {
    NS_WARNING("ipc:content-shutdown message without childID property");
  }
  return NS_OK;
}

static void
Init()
{
  sLockTable = new LockTable();
  sLockTable->Init();
  sInitialized = true;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(new ClearHashtableOnShutdown(), "xpcom-shutdown", false);
    obs->AddObserver(new CleanupOnContentShutdown(), "ipc:content-shutdown", false);
  }
}
} // anonymous namespace

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
ModifyWakeLockInternal(const nsAString& aTopic,
                       hal::WakeLockControl aLockAdjust,
                       hal::WakeLockControl aHiddenAdjust,
                       uint64_t aProcessID)
{
  MOZ_ASSERT(NS_IsMainThread());

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
    table->Init();
    sLockTable->Put(aTopic, table);
  } else {
    table->Get(aProcessID, &processCount);
    table->EnumerateRead(CountWakeLocks, &totalCount);
  }

  MOZ_ASSERT(processCount.numLocks >= processCount.numHidden);
  MOZ_ASSERT(aLockAdjust >= 0 || processCount.numLocks > 0);
  MOZ_ASSERT(aHiddenAdjust >= 0 || processCount.numHidden > 0);
  MOZ_ASSERT(totalCount.numLocks >= totalCount.numHidden);
  MOZ_ASSERT(aLockAdjust >= 0 || totalCount.numLocks > 0);
  MOZ_ASSERT(aHiddenAdjust >= 0 || totalCount.numHidden > 0);

  WakeLockState oldState = ComputeWakeLockState(totalCount.numLocks, totalCount.numHidden);

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

  WakeLockState newState = ComputeWakeLockState(totalCount.numLocks, totalCount.numHidden);

  if (sActiveListeners && oldState != newState) {
    WakeLockInformation info;
    info.numLocks() = totalCount.numLocks;
    info.numHidden() = totalCount.numHidden;
    info.topic() = aTopic;
    NotifyWakeLockChange(info);
  }
}

void
GetWakeLockInfo(const nsAString& aTopic, WakeLockInformation* aWakeLockInfo)
{
  if (sIsShuttingDown) {
    NS_WARNING("You don't want to get wake lock information during xpcom-shutdown!");
    return;
  }
  if (!sInitialized) {
    Init();
  }

  ProcessLockTable* table = sLockTable->Get(aTopic);
  if (!table) {
    aWakeLockInfo->numLocks() = 0;
    aWakeLockInfo->numHidden() = 0;
    aWakeLockInfo->topic() = aTopic;
    return;
  }
  LockCount totalCount;
  table->EnumerateRead(CountWakeLocks, &totalCount);
  aWakeLockInfo->numLocks() = totalCount.numLocks;
  aWakeLockInfo->numHidden() = totalCount.numHidden;
  aWakeLockInfo->topic() = aTopic;
}

} // hal_impl
} // mozilla
