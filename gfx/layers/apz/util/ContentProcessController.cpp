/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentProcessController.h"

#include "mozilla/dom/TabChild.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZChild.h"

#include "InputData.h"                  // for InputData

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
  TabChildCreatedObserver(ContentProcessController* aController, const dom::TabId& aTabId)
    : mController(aController),
      mTabId(aTabId)
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

private:
  virtual ~TabChildCreatedObserver()
  {}

  // TabChildCreatedObserver is owned by mController, and mController outlives its
  // TabChildCreatedObserver, so the raw pointer is fine.
  ContentProcessController* mController;
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
    mController->SetBrowser(browser);
  }
  return NS_OK;
}

APZChild*
ContentProcessController::Create(const dom::TabId& aTabId)
{
  RefPtr<dom::TabChild> browser = dom::TabChild::FindTabChild(aTabId);

  ContentProcessController* controller = new ContentProcessController();

  nsAutoPtr<APZChild> apz(new APZChild(controller));

  if (browser) {

    controller->SetBrowser(browser);

  } else {

    RefPtr<TabChildCreatedObserver> observer =
      new TabChildCreatedObserver(controller, aTabId);
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (!os ||
        NS_FAILED(os->AddObserver(observer, "tab-child-created", false))) {
      return nullptr;
    }
    controller->SetObserver(observer);

  }

  return apz.forget();
}

ContentProcessController::ContentProcessController()
    : mBrowser(nullptr)
{
}
ContentProcessController::~ContentProcessController()
{
  if (mObserver) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    os->RemoveObserver(mObserver, "tab-child-created");
  } else if (mBrowser) {
    mBrowser->SetAPZChild(nullptr);
  }
}

void
ContentProcessController::SetObserver(nsIObserver* aObserver)
{
  MOZ_ASSERT(!mBrowser);
  mObserver = aObserver;
}

void
ContentProcessController::SetBrowser(dom::TabChild* aBrowser)
{
  MOZ_ASSERT(!mBrowser);
  mBrowser = aBrowser;

  if (mObserver) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    os->RemoveObserver(mObserver, "tab-child-created");
    mObserver = nullptr;
  }
}
void
ContentProcessController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  if (mBrowser) {
    mBrowser->UpdateFrame(aFrameMetrics);
  }
}

void
ContentProcessController::HandleTap(
                        TapType aType,
                        const LayoutDevicePoint& aPoint,
                        Modifiers aModifiers,
                        const ScrollableLayerGuid& aGuid,
                        uint64_t aInputBlockId)
{
  if (mBrowser) {
    mBrowser->HandleTap(aType, aPoint - mBrowser->GetChromeDisplacement(), aModifiers, aGuid,
        aInputBlockId, (aType == TapType::eSingleTap));
  }
}

void
ContentProcessController::NotifyAPZStateChange(
                                  const ScrollableLayerGuid& aGuid,
                                  APZStateChange aChange,
                                  int aArg)
{
  if (mBrowser) {
    mBrowser->NotifyAPZStateChange(aGuid.mScrollId, aChange, aArg);
  }
}

void
ContentProcessController::NotifyMozMouseScrollEvent(
                                  const FrameMetrics::ViewID& aScrollId,
                                  const nsString& aEvent)
{
  if (mBrowser) {
    APZCCallbackHelper::NotifyMozMouseScrollEvent(aScrollId, aEvent);
  }
}

void
ContentProcessController::NotifyFlushComplete()
{
  if (mBrowser) {
    nsCOMPtr<nsIPresShell> shell;
    if (nsCOMPtr<nsIDocument> doc = mBrowser->GetDocument()) {
      shell = doc->GetShell();
    }
    APZCCallbackHelper::NotifyFlushComplete(shell.get());
  }
}

void
ContentProcessController::PostDelayedTask(already_AddRefed<Runnable> aRunnable, int aDelayMs)
{
  MOZ_ASSERT_UNREACHABLE("ContentProcessController should only be used remotely.");
}

bool
ContentProcessController::IsRepaintThread()
{
  return NS_IsMainThread();
}

void
ContentProcessController::DispatchToRepaintThread(already_AddRefed<Runnable> aTask)
{
  NS_DispatchToMainThread(Move(aTask));
}

} // namespace layers
} // namespace mozilla
