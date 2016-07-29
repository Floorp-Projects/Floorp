/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZChild.h"

#include "mozilla/dom/TabChild.h"
#include "mozilla/layers/APZCCallbackHelper.h"

namespace mozilla {
namespace layers {

/**
 * There are cases where we try to create the APZChild before the corresponding
 * TabChild has been created, we use an observer for the "tab-child-created"
 * topic to set the TabChild in the APZChild when it has been created.
 */
class TabChildCreatedObserver : public nsIObserver
{
public:
  TabChildCreatedObserver(APZChild* aAPZChild, const dom::TabId& aTabId)
    : mAPZChild(aAPZChild),
      mTabId(aTabId)
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

private:
  virtual ~TabChildCreatedObserver()
  {}

  // TabChildCreatedObserver is owned by mAPZChild, and mAPZChild outlives its
  // TabChildCreatedObserver, so the raw pointer is fine.
  APZChild* mAPZChild;
  dom::TabId mTabId;
};

NS_IMPL_ISUPPORTS(TabChildCreatedObserver, nsIObserver)

NS_IMETHODIMP
TabChildCreatedObserver::Observe(nsISupports* aSubject,
                                 const char* aTopic,
                                 const char16_t* aData)
{
  MOZ_ASSERT(strcmp(aTopic, "tab-child-created") == 0);

  nsCOMPtr<nsITabChild> tabChild(do_QueryInterface(aSubject));
  NS_ENSURE_TRUE(tabChild, NS_ERROR_FAILURE);

  dom::TabChild* browser = static_cast<dom::TabChild*>(tabChild.get());
  if (browser->GetTabId() == mTabId) {
    mAPZChild->SetBrowser(browser);
  }
  return NS_OK;
}

APZChild*
APZChild::Create(const dom::TabId& aTabId)
{
  RefPtr<dom::TabChild> browser = dom::TabChild::FindTabChild(aTabId);
  nsAutoPtr<APZChild> apz(new APZChild);
  if (browser) {
    apz->SetBrowser(browser);
  } else {
    RefPtr<TabChildCreatedObserver> observer =
      new TabChildCreatedObserver(apz, aTabId);
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (!os ||
        NS_FAILED(os->AddObserver(observer, "tab-child-created", false))) {
      return nullptr;
    }
    apz->SetObserver(observer);
  }

  return apz.forget();
}

APZChild::APZChild()
  : mDestroyed(false)
{
}

APZChild::~APZChild()
{
  if (mObserver) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    os->RemoveObserver(mObserver, "tab-child-created");
  } else if (mBrowser) {
    mBrowser->SetAPZChild(nullptr);
  }
}

bool
APZChild::RecvUpdateFrame(const FrameMetrics& aFrameMetrics)
{
  return mBrowser->UpdateFrame(aFrameMetrics);
}

bool
APZChild::RecvHandleTap(const TapType& aType,
                        const LayoutDevicePoint& aPoint,
                        const Modifiers& aModifiers,
                        const ScrollableLayerGuid& aGuid,
                        const uint64_t& aInputBlockId,
                        const bool& aCallTakeFocusForClickFromTap)
{
  mBrowser->HandleTap(aType, aPoint, aModifiers, aGuid,
      aInputBlockId, aCallTakeFocusForClickFromTap);
  return true;
}

bool
APZChild::RecvNotifyAPZStateChange(const ViewID& aViewId,
                                   const APZStateChange& aChange,
                                   const int& aArg)
{
  return mBrowser->NotifyAPZStateChange(aViewId, aChange, aArg);
}

bool
APZChild::RecvNotifyFlushComplete()
{
  nsCOMPtr<nsIPresShell> shell;
  if (nsCOMPtr<nsIDocument> doc = mBrowser->GetDocument()) {
    shell = doc->GetShell();
  }
  APZCCallbackHelper::NotifyFlushComplete(shell.get());
  return true;
}

bool
APZChild::RecvDestroy()
{
  mDestroyed = true;
  if (mBrowser) {
    mBrowser->SetAPZChild(nullptr);
    mBrowser = nullptr;
  }
  PAPZChild::Send__delete__(this);
  return true;
}

void
APZChild::SetObserver(nsIObserver* aObserver)
{
  MOZ_ASSERT(!mBrowser);
  mObserver = aObserver;
}

void
APZChild::SetBrowser(dom::TabChild* aBrowser)
{
  MOZ_ASSERT(!mBrowser);
  if (mObserver) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    os->RemoveObserver(mObserver, "tab-child-created");
    mObserver = nullptr;
  }
  // We might get the tab-child-created notification after we receive a
  // Destroy message from the parent. In that case we don't want to install
  // ourselves with the browser.
  if (!mDestroyed) {
    mBrowser = aBrowser;
    mBrowser->SetAPZChild(this);
  }
}

} // namespace layers
} // namespace mozilla
