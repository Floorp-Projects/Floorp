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
  void AllocateAfterDelay();
  void AllocateOnIdle();
  void AllocateNow();
  already_AddRefed<ContentParent> Take();
  bool Provide(ContentParent* aParent);

private:
  static mozilla::StaticRefPtr<PreallocatedProcessManagerImpl> sSingleton;

  PreallocatedProcessManagerImpl();
  ~PreallocatedProcessManagerImpl() {}
  DISALLOW_EVIL_CONSTRUCTORS(PreallocatedProcessManagerImpl);

  void Init();

  void RereadPrefs();
  void Enable();
  void Disable();
  void CloseProcess();

  void ObserveProcessShutdown(nsISupports* aSubject);

  bool mEnabled;
  bool mShutdown;
  RefPtr<ContentParent> mPreallocatedProcess;
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

  // First time when we init sSingleton, the pref database might not be in a
  // reliable state (we are too early), so despite dom.ipc.processPrelaunch.enabled
  // is set to true Preferences::GetBool will return false (it cannot find the pref).
  // Since Init() above will be called only once, and the pref will not be changed,
  // the manger will stay disabled. To prevent that let's re-read the pref each time
  // someone accessing the manager singleton. This is a hack but this is not a hot code
  // so it should be fine.
  sSingleton->RereadPrefs();

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
PreallocatedProcessManagerImpl::AllocateAfterDelay()
{
  if (!mEnabled || mPreallocatedProcess || mShutdown) {
    return;
  }

  // Originally AllocateOnIdle() was post here, but since the gecko parent
  // message loop in practice never goes idle, that didn't work out well.
  // Let's just launch the process after the delay.
  NS_DelayedDispatchToCurrentThread(
    NewRunnableMethod("PreallocatedProcessManagerImpl::AllocateNow",
                      this,
                      &PreallocatedProcessManagerImpl::AllocateNow),
    Preferences::GetUint("dom.ipc.processPrelaunch.delayMs",
                         DEFAULT_ALLOCATE_DELAY));
}

void
PreallocatedProcessManagerImpl::AllocateOnIdle()
{
  if (!mEnabled || mPreallocatedProcess || mShutdown) {
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
  if (!mEnabled || mPreallocatedProcess || mShutdown ||
      ContentParent::IsMaxProcessCountReached(NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE))) {
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
  if (!mPreallocatedProcess) {
    return;
  }

  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(props);

  uint64_t childID = CONTENT_PROCESS_ID_UNKNOWN;
  props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"), &childID);
  NS_ENSURE_TRUE_VOID(childID != CONTENT_PROCESS_ID_UNKNOWN);

  if (childID == mPreallocatedProcess->ChildID()) {
    mPreallocatedProcess = nullptr;
  }
}

inline PreallocatedProcessManagerImpl* GetPPMImpl()
{
  return PreallocatedProcessManagerImpl::Singleton();
}

/* static */ void
PreallocatedProcessManager::AllocateAfterDelay()
{
  GetPPMImpl()->AllocateAfterDelay();
}

/* static */ void
PreallocatedProcessManager::AllocateOnIdle()
{
  GetPPMImpl()->AllocateOnIdle();
}

/* static */ void
PreallocatedProcessManager::AllocateNow()
{
  GetPPMImpl()->AllocateNow();
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
