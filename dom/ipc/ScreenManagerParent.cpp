/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabParent.h"
#include "mozilla/unused.h"
#include "nsIWidget.h"
#include "nsServiceManagerUtils.h"
#include "ScreenManagerParent.h"

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

  unused << AnswerRefresh(aNumberOfScreens, aSystemDefaultScale, aSuccess);
}

bool
ScreenManagerParent::AnswerRefresh(uint32_t* aNumberOfScreens,
                                   float* aSystemDefaultScale,
                                   bool* aSuccess)
{
  *aSuccess = false;

  nsresult rv = mScreenMgr->GetNumberOfScreens(aNumberOfScreens);
  if (NS_FAILED(rv)) {
    return true;
  }

  rv = mScreenMgr->GetSystemDefaultScale(aSystemDefaultScale);
  if (NS_FAILED(rv)) {
    return true;
  }

  *aSuccess = true;
  return true;
}

bool
ScreenManagerParent::AnswerScreenRefresh(const uint32_t& aId,
                                         ScreenDetails* aRetVal,
                                         bool* aSuccess)
{
  *aSuccess = false;

  nsCOMPtr<nsIScreen> screen;
  nsresult rv = mScreenMgr->ScreenForId(aId, getter_AddRefs(screen));
  if (NS_FAILED(rv)) {
    return true;
  }

  ScreenDetails details;
  unused << ExtractScreenDetails(screen, details);

  *aRetVal = details;
  *aSuccess = true;
  return true;
}

bool
ScreenManagerParent::AnswerGetPrimaryScreen(ScreenDetails* aRetVal,
                                            bool* aSuccess)
{
  *aSuccess = false;

  nsCOMPtr<nsIScreen> screen;
  nsresult rv = mScreenMgr->GetPrimaryScreen(getter_AddRefs(screen));

  NS_ENSURE_SUCCESS(rv, true);

  ScreenDetails details;
  if (!ExtractScreenDetails(screen, details)) {
    return true;
  }

  *aRetVal = details;
  *aSuccess = true;
  return true;
}

bool
ScreenManagerParent::AnswerScreenForRect(const int32_t& aLeft,
                                         const int32_t& aTop,
                                         const int32_t& aWidth,
                                         const int32_t& aHeight,
                                         ScreenDetails* aRetVal,
                                         bool* aSuccess)
{
  *aSuccess = false;

  nsCOMPtr<nsIScreen> screen;
  nsresult rv = mScreenMgr->ScreenForRect(aLeft, aTop, aWidth, aHeight, getter_AddRefs(screen));

  NS_ENSURE_SUCCESS(rv, true);

  ScreenDetails details;
  if (!ExtractScreenDetails(screen, details)) {
    return true;
  }

  *aRetVal = details;
  *aSuccess = true;
  return true;
}

bool
ScreenManagerParent::AnswerScreenForBrowser(PBrowserParent* aBrowser,
                                            ScreenDetails* aRetVal,
                                            bool* aSuccess)
{
  *aSuccess = false;

  // Find the mWidget associated with the tabparent, and then return
  // the nsIScreen it's on.
  TabParent* tabParent = static_cast<TabParent*>(aBrowser);
  nsCOMPtr<nsIWidget> widget = tabParent->GetWidget();
  if (!widget) {
    return true;
  }

  nsCOMPtr<nsIScreen> screen;
  if (widget->GetNativeData(NS_NATIVE_WINDOW)) {
    mScreenMgr->ScreenForNativeWidget(widget->GetNativeData(NS_NATIVE_WINDOW),
                                      getter_AddRefs(screen));
  }

  NS_ENSURE_TRUE(screen, true);

  ScreenDetails details;
  if (!ExtractScreenDetails(screen, details)) {
    return true;
  }

  *aRetVal = details;
  *aSuccess = true;
  return true;
}

bool
ScreenManagerParent::ExtractScreenDetails(nsIScreen* aScreen, ScreenDetails &aDetails)
{
  uint32_t id;
  nsresult rv = aScreen->GetId(&id);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.id() = id;

  nsIntRect rect;
  rv = aScreen->GetRect(&rect.x, &rect.y, &rect.width, &rect.height);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.rect() = rect;

  nsIntRect availRect;
  rv = aScreen->GetAvailRect(&availRect.x, &availRect.y, &availRect.width,
                             &availRect.height);
  NS_ENSURE_SUCCESS(rv, false);
  aDetails.availRect() = availRect;

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

