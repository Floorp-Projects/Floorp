/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Unused.h"
#include "nsIWidget.h"
#include "nsServiceManagerUtils.h"
#include "ScreenManagerParent.h"
#include "ContentProcessManager.h"

namespace mozilla {
namespace dom {

static const char *sScreenManagerContractID = "@mozilla.org/gfx/screenmanager;1";

ScreenManagerParent::ScreenManagerParent(uint32_t* aNumberOfScreens,
                                         float* aSystemDefaultScale,
                                         bool* aSuccess)
{
  mScreenMgr = do_GetService(sScreenManagerContractID);
  if (!mScreenMgr) {
    MOZ_CRASH("Couldn't get nsIScreenManager from ScreenManagerParent.");
  }

  Unused << RecvRefresh(aNumberOfScreens, aSystemDefaultScale, aSuccess);
}

mozilla::ipc::IPCResult
ScreenManagerParent::RecvRefresh(uint32_t* aNumberOfScreens,
                                 float* aSystemDefaultScale,
                                 bool* aSuccess)
{
  *aSuccess = false;

  nsresult rv = mScreenMgr->GetNumberOfScreens(aNumberOfScreens);
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  rv = mScreenMgr->GetSystemDefaultScale(aSystemDefaultScale);
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  *aSuccess = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
ScreenManagerParent::RecvScreenRefresh(const uint32_t& aId,
                                       ScreenDetails* aRetVal,
                                       bool* aSuccess)
{
  *aSuccess = false;

  nsCOMPtr<nsIScreen> screen;
  nsresult rv = mScreenMgr->ScreenForId(aId, getter_AddRefs(screen));
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  ScreenDetails details;
  Unused << ExtractScreenDetails(screen, details);

  *aRetVal = details;
  *aSuccess = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
ScreenManagerParent::RecvGetPrimaryScreen(ScreenDetails* aRetVal,
                                          bool* aSuccess)
{
  *aSuccess = false;

  nsCOMPtr<nsIScreen> screen;
  nsresult rv = mScreenMgr->GetPrimaryScreen(getter_AddRefs(screen));

  NS_ENSURE_SUCCESS(rv, IPC_OK());

  ScreenDetails details;
  if (!ExtractScreenDetails(screen, details)) {
    return IPC_OK();
  }

  *aRetVal = details;
  *aSuccess = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
ScreenManagerParent::RecvScreenForRect(const int32_t& aLeft,
                                       const int32_t& aTop,
                                       const int32_t& aWidth,
                                       const int32_t& aHeight,
                                       ScreenDetails* aRetVal,
                                       bool* aSuccess)
{
  *aSuccess = false;

  nsCOMPtr<nsIScreen> screen;
  nsresult rv = mScreenMgr->ScreenForRect(aLeft, aTop, aWidth, aHeight, getter_AddRefs(screen));

  NS_ENSURE_SUCCESS(rv, IPC_OK());

  ScreenDetails details;
  if (!ExtractScreenDetails(screen, details)) {
    return IPC_OK();
  }

  *aRetVal = details;
  *aSuccess = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
ScreenManagerParent::RecvScreenForBrowser(const TabId& aTabId,
                                          ScreenDetails* aRetVal,
                                          bool* aSuccess)
{
  *aSuccess = false;
#ifdef MOZ_VALGRIND
  // Zero this so that Valgrind doesn't complain when we send it to another
  // process.
  memset(aRetVal, 0, sizeof(ScreenDetails));
#endif

  // Find the mWidget associated with the tabparent, and then return
  // the nsIScreen it's on.
  ContentParent* cp = static_cast<ContentParent*>(this->Manager());
  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  RefPtr<TabParent> tabParent =
    cpm->GetTopLevelTabParentByProcessAndTabId(cp->ChildID(), aTabId);
  if(!tabParent){
    return IPC_FAIL_NO_REASON(this);
  }

  nsCOMPtr<nsIWidget> widget = tabParent->GetWidget();

  nsCOMPtr<nsIScreen> screen;
  if (widget && widget->GetNativeData(NS_NATIVE_WINDOW)) {
    mScreenMgr->ScreenForNativeWidget(widget->GetNativeData(NS_NATIVE_WINDOW),
                                      getter_AddRefs(screen));
  } else {
    nsresult rv = mScreenMgr->GetPrimaryScreen(getter_AddRefs(screen));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return IPC_OK();
    }
  }

  NS_ENSURE_TRUE(screen, IPC_OK());

  ScreenDetails details;
  if (!ExtractScreenDetails(screen, details)) {
    return IPC_OK();
  }

  *aRetVal = details;
  *aSuccess = true;
  return IPC_OK();
}

bool
ScreenManagerParent::ExtractScreenDetails(nsIScreen* aScreen, ScreenDetails &aDetails)
{
  if (!aScreen) {
    return false;
  }

  uint32_t id;
  nsresult rv = aScreen->GetId(&id);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.id() = id;

  nsIntRect rect;
  rv = aScreen->GetRect(&rect.x, &rect.y, &rect.width, &rect.height);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.rect() = rect;

  nsIntRect rectDisplayPix;
  rv = aScreen->GetRectDisplayPix(&rectDisplayPix.x, &rectDisplayPix.y,
                                  &rectDisplayPix.width, &rectDisplayPix.height);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.rectDisplayPix() = rectDisplayPix;

  nsIntRect availRect;
  rv = aScreen->GetAvailRect(&availRect.x, &availRect.y, &availRect.width,
                             &availRect.height);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.availRect() = availRect;

  nsIntRect availRectDisplayPix;
  rv = aScreen->GetAvailRectDisplayPix(&availRectDisplayPix.x,
                                       &availRectDisplayPix.y,
                                       &availRectDisplayPix.width,
                                       &availRectDisplayPix.height);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.availRectDisplayPix() = availRectDisplayPix;

  int32_t pixelDepth = 0;
  rv = aScreen->GetPixelDepth(&pixelDepth);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.pixelDepth() = pixelDepth;

  int32_t colorDepth = 0;
  rv = aScreen->GetColorDepth(&colorDepth);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.colorDepth() = colorDepth;

  double contentsScaleFactor = 1.0;
  rv = aScreen->GetContentsScaleFactor(&contentsScaleFactor);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.contentsScaleFactor() = contentsScaleFactor;

  return true;
}

void
ScreenManagerParent::ActorDestroy(ActorDestroyReason why)
{
}

} // namespace dom
} // namespace mozilla

