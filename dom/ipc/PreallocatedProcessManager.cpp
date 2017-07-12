/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PreallocatedProcessManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsIPropertyBag2.h"
#include "ProcessPriorityManager.h"
#include "nsServiceManagerUtils.h"

// This number is fairly arbitrary ... the intention is to put off
// launching another app process until the last one has finished
// loading its content, to reduce CPU/memory/IO contention.
#define DEFAULT_ALLOCATE_DELAY 1000

using namespace mozilla;
using namespace mozilla::hal;
using namespace mozilla::dom;

namespace mozilla {

/**
 * This singleton class implements the static methods on
 * PreallocatedProcessManager.
 */
class PreallocatedProcessManagerImpl final
  : public nsIObserver
{
public:
  static PreallocatedProcessManagerImpl* Singleton();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // See comments on PreallocatedProcessManager for these methods.
  void AddBlocker(ContentParent* aParent);
  void RemoveBlocker(ContentParent* aParent);
  already_AddRefed<ContentParent> Take();
  bool Provide(ContentParent* aParent);

private:
  static mozilla::StaticRefPtr<PreallocatedProcessManagerImpl> sSingleton;

  PreallocatedProcessManagerImpl();
  ~PreallocatedProcessManagerImpl() {}
  DISALLOW_EVIL_CONSTRUCTORS(PreallocatedProcessManagerImpl);

  void Init();

  bool CanAllocate();
  void AllocateAfterDelay();
  void AllocateOnIdle();
  void AllocateNow();

  void RereadPrefs();
  void Enable();
  void Disable();
  void CloseProcess();

  void ObserveProcessShutdown(nsISupports* aSubject);

  bool mEnabled;
  bool mShutdown;
  RefPtr<ContentParent> mPreallocatedProcess;
  nsTHashtable<nsUint64HashKey> mBlockers;
};

/* static */ StaticRefPtr<PreallocatedProcessManagerImpl>
PreallocatedProcessManagerImpl::sSingleton;

/* static */ PreallocatedProcessManagerImpl*
PreallocatedProcessManagerImpl::Singleton()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sSingleton) {
    sSingleton = new PreallocatedProcessManagerImpl();
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }

  return sSingleton;
}

NS_IMPL_ISUPPORTS(PreallocatedProcessManagerImpl, nsIObserver)

PreallocatedProcessManagerImpl::PreallocatedProcessManagerImpl()
  : mEnabled(false)
  , mShutdown(false)
{}

void
PreallocatedProcessManagerImpl::Init()
{
  Preferences::AddStrongObserver(this, "dom.ipc.processPrelaunch.enabled");
  // We have to respect processCount at all time. This is especially important
  // for testing.
  Preferences::AddStrongObserver(this, "dom.ipc.processCount");
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(this, "ipc:content-shutdown",
                    /* weakRef = */ false);
    os->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                    /* weakRef = */ false);
    os->AddObserver(this, "profile-change-teardown",
                    /* weakRef = */ false);
  }
  RereadPrefs();
}

NS_IMETHODIMP
PreallocatedProcessManagerImpl::Observe(nsISupports* aSubject,
                                        const char* aTopic,
                                        const char16_t* aData)
{
  if (!strcmp("ipc:content-shutdown", aTopic)) {
    ObserveProcessShutdown(aSubject);
  } else if (!strcmp("nsPref:changed", aTopic)) {
    // The only other observer we registered was for our prefs.
    RereadPrefs();
  } else if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic) ||
             !strcmp("profile-change-teardown", aTopic)) {
    Preferences::RemoveObserver(this, "dom.ipc.processPrelaunch.enabled");
    Preferences::RemoveObserver(this, "dom.ipc.processCount");
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, "ipc:content-shutdown");
      os->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      os->RemoveObserver(this, "profile-change-teardown");
    }
    mShutdown = true;
    CloseProcess();
  } else {
    MOZ_ASSERT(false);
  }

  return NS_OK;
}

void
PreallocatedProcessManagerImpl::RereadPrefs()
{
  if (mozilla::BrowserTabsRemoteAutostart() &&
      Preferences::GetBool("dom.ipc.processPrelaunch.enabled")) {
    Enable();
  } else {
    Disable();
  }

  if (ContentParent::IsMaxProcessCountReached(NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE))) {
    CloseProcess();
  }
}

already_AddRefed<ContentParent>
PreallocatedProcessManagerImpl::Take()
{
  if (!mEnabled || mShutdown) {
    return nullptr;
  }

  if (mPreallocatedProcess) {
    // The preallocated process is taken. Let's try to start up a new one soon.
    AllocateOnIdle();
  }

  return mPreallocatedProcess.forget();
}

bool
PreallocatedProcessManagerImpl::Provide(ContentParent* aParent)
{
  if (mEnabled && !mShutdown && !mPreallocatedProcess) {
    mPreallocatedProcess = aParent;
  }

  // We might get a call from both NotifyTabDestroying and NotifyTabDestroyed with the same
  // ContentParent. Returning true here for both calls is important to avoid the cached process
  // to be destroyed.
  return aParent == mPreallocatedProcess;
}

void
PreallocatedProcessManagerImpl::Enable()
{
  if (mEnabled) {
    return;
  }

  mEnabled = true;
  AllocateAfterDelay();
}

void
PreallocatedProcessManagerImpl::AddBlocker(ContentParent* aParent)
{
  uint64_t childID = aParent->ChildID();
  MOZ_ASSERT(!mBlockers.Contains(childID));
  mBlockers.PutEntry(childID);
}

void
PreallocatedProcessManagerImpl::RemoveBlocker(ContentParent* aParent)
{
  uint64_t childID = aParent->ChildID();
  MOZ_ASSERT(mBlockers.Contains(childID));
  mBlockers.RemoveEntry(childID);
  if (!mPreallocatedProcess && mBlockers.IsEmpty()) {
    AllocateAfterDelay();
  }
}

bool
PreallocatedProcessManagerImpl::CanAllocate()
{
  return mEnabled &&
         mBlockers.IsEmpty() &&
         !mPreallocatedProcess &&
         !mShutdown &&
         !ContentParent::IsMaxProcessCountReached(NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE));
}

void
PreallocatedProcessManagerImpl::AllocateAfterDelay()
{
  if (!mEnabled) {
    return;
  }

  NS_DelayedDispatchToCurrentThread(
    NewRunnableMethod("PreallocatedProcessManagerImpl::AllocateOnIdle",
                      this,
                      &PreallocatedProcessManagerImpl::AllocateOnIdle),
    Preferences::GetUint("dom.ipc.processPrelaunch.delayMs",
                         DEFAULT_ALLOCATE_DELAY));
}

void
PreallocatedProcessManagerImpl::AllocateOnIdle()
{
  if (!mEnabled) {
    return;
  }

  NS_IdleDispatchToCurrentThread(
    NewRunnableMethod("PreallocatedProcessManagerImpl::AllocateNow",
                      this,
                      &PreallocatedProcessManagerImpl::AllocateNow));
}

void
PreallocatedProcessManagerImpl::AllocateNow()
{
  if (!CanAllocate()) {
    if (mEnabled && !mShutdown && !mPreallocatedProcess && !mBlockers.IsEmpty()) {
      // If it's too early to allocate a process let's retry later.
      AllocateAfterDelay();
    }
    return;
  }

  mPreallocatedProcess = ContentParent::PreallocateProcess();
}

void
PreallocatedProcessManagerImpl::Disable()
{
  if (!mEnabled) {
    return;
  }

  mEnabled = false;
  CloseProcess();
}

void
PreallocatedProcessManagerImpl::CloseProcess()
{
  if (mPreallocatedProcess) {
    mPreallocatedProcess->ShutDownProcess(ContentParent::SEND_SHUTDOWN_MESSAGE);
    mPreallocatedProcess = nullptr;
  }
}

void
PreallocatedProcessManagerImpl::ObserveProcessShutdown(nsISupports* aSubject)
{
  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(props);

  uint64_t childID = CONTENT_PROCESS_ID_UNKNOWN;
  props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"), &childID);
  NS_ENSURE_TRUE_VOID(childID != CONTENT_PROCESS_ID_UNKNOWN);

  if (mPreallocatedProcess && childID == mPreallocatedProcess->ChildID()) {
    mPreallocatedProcess = nullptr;
  }

  mBlockers.RemoveEntry(childID);
}

inline PreallocatedProcessManagerImpl* GetPPMImpl()
{
  return PreallocatedProcessManagerImpl::Singleton();
}

/* static */ void
PreallocatedProcessManager::AddBlocker(ContentParent* aParent)
{
  GetPPMImpl()->AddBlocker(aParent);
}

/* static */ void
PreallocatedProcessManager::RemoveBlocker(ContentParent* aParent)
{
  GetPPMImpl()->RemoveBlocker(aParent);
}

/* static */ already_AddRefed<ContentParent>
PreallocatedProcessManager::Take()
{
  return GetPPMImpl()->Take();
}

/* static */ bool
PreallocatedProcessManager::Provide(ContentParent* aParent)
{
  return GetPPMImpl()->Provide(aParent);
}

} // namespace mozilla
