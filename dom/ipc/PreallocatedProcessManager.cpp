/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PreallocatedProcessManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/ContentParent.h"

using namespace mozilla;
using namespace mozilla::hal;
using namespace mozilla::dom;

namespace {

/**
 * This singleton class implements the static methods on
 * PreallocatedProcessManager.
 */
class PreallocatedProcessManagerImpl MOZ_FINAL
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

private:
  static mozilla::StaticRefPtr<PreallocatedProcessManagerImpl> sSingleton;

  PreallocatedProcessManagerImpl();
  DISALLOW_EVIL_CONSTRUCTORS(PreallocatedProcessManagerImpl);

  void Init();

  void RereadPrefs();
  void Enable();
  void Disable();

  void ObserveProcessShutdown(nsISupports* aSubject);

  bool mEnabled;
  nsRefPtr<ContentParent> mPreallocatedAppProcess;
};

/* static */ StaticRefPtr<PreallocatedProcessManagerImpl>
PreallocatedProcessManagerImpl::sSingleton;

/* static */ PreallocatedProcessManagerImpl*
PreallocatedProcessManagerImpl::Singleton()
{
  if (!sSingleton) {
    sSingleton = new PreallocatedProcessManagerImpl();
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }

  return sSingleton;
}

NS_IMPL_ISUPPORTS1(PreallocatedProcessManagerImpl, nsIObserver)

PreallocatedProcessManagerImpl::PreallocatedProcessManagerImpl()
  : mEnabled(false)
{}

void
PreallocatedProcessManagerImpl::Init()
{
  Preferences::AddStrongObserver(this, "dom.ipc.processPrelaunch.enabled");
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(this, "ipc:content-shutdown",
                    /* weakRef = */ false);
  }
  RereadPrefs();
}

NS_IMETHODIMP
PreallocatedProcessManagerImpl::Observe(nsISupports* aSubject,
                                        const char* aTopic,
                                        const PRUnichar* aData)
{
  if (!strcmp("ipc:content-shutdown", aTopic)) {
    ObserveProcessShutdown(aSubject);
  } else if (!strcmp("nsPref:changed", aTopic)) {
    // The only other observer we registered was for our prefs.
    RereadPrefs();
  } else {
    MOZ_ASSERT(false);
  }

  return NS_OK;
}

void
PreallocatedProcessManagerImpl::RereadPrefs()
{
  if (Preferences::GetBool("dom.ipc.processPrelaunch.enabled")) {
    Enable();
  } else {
    Disable();
  }
}

already_AddRefed<ContentParent>
PreallocatedProcessManagerImpl::Take()
{
  return mPreallocatedAppProcess.forget();
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
  if (!mEnabled || mPreallocatedAppProcess) {
    return;
  }

  MessageLoop::current()->PostDelayedTask(
    FROM_HERE,
    NewRunnableMethod(this, &PreallocatedProcessManagerImpl::AllocateOnIdle),
    Preferences::GetUint("dom.ipc.processPrelaunch.delayMs", 1000));
}

void
PreallocatedProcessManagerImpl::AllocateOnIdle()
{
  if (!mEnabled || mPreallocatedAppProcess) {
    return;
  }

  MessageLoop::current()->PostIdleTask(
    FROM_HERE,
    NewRunnableMethod(this, &PreallocatedProcessManagerImpl::AllocateNow));
}

void
PreallocatedProcessManagerImpl::AllocateNow()
{
  if (!mEnabled || mPreallocatedAppProcess) {
    return;
  }

  mPreallocatedAppProcess = ContentParent::PreallocateAppProcess();
}

void
PreallocatedProcessManagerImpl::Disable()
{
  if (!mEnabled) {
    return;
  }

  mEnabled = false;

  if (mPreallocatedAppProcess) {
    mPreallocatedAppProcess->ShutDown();
    mPreallocatedAppProcess = nullptr;
  }
}

void
PreallocatedProcessManagerImpl::ObserveProcessShutdown(nsISupports* aSubject)
{
  if (!mPreallocatedAppProcess) {
    return;
  }

  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(props);

  uint64_t childID = CONTENT_PROCESS_ID_UNKNOWN;
  props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"), &childID);
  NS_ENSURE_TRUE_VOID(childID != CONTENT_PROCESS_ID_UNKNOWN);

  if (childID == mPreallocatedAppProcess->ChildID()) {
    mPreallocatedAppProcess = nullptr;
  }
}

} // anonymous namespace

namespace mozilla {

/* static */ void
PreallocatedProcessManager::AllocateAfterDelay()
{
  PreallocatedProcessManagerImpl::Singleton()->AllocateAfterDelay();
}

/* static */ void
PreallocatedProcessManager::AllocateOnIdle()
{
  PreallocatedProcessManagerImpl::Singleton()->AllocateOnIdle();
}

/* static */ void
PreallocatedProcessManager::AllocateNow()
{
  PreallocatedProcessManagerImpl::Singleton()->AllocateNow();
}

/* static */ already_AddRefed<ContentParent>
PreallocatedProcessManager::Take()
{
  return PreallocatedProcessManagerImpl::Singleton()->Take();
}

} // namespace mozilla
