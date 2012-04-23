/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Hal.h"
#include "nsScreen.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsDOMClassInfoID.h"
#include "nsIDocShellTreeItem.h"
#include "nsLayoutUtils.h"
#include "nsDOMEvent.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {

bool
IsChromeType(nsIDocShell *aDocShell)
{
  nsCOMPtr<nsIDocShellTreeItem> ds = do_QueryInterface(aDocShell);
  if (!ds) {
    return false;
  }

  PRInt32 itemType;
  ds->GetItemType(&itemType);
  return itemType == nsIDocShellTreeItem::typeChrome;
}

} // anonymous namespace

/* static */ already_AddRefed<nsScreen>
nsScreen::Create(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(aWindow);

  if (!aWindow->GetDocShell()) {
    return nsnull;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo =
    do_QueryInterface(static_cast<nsPIDOMWindow*>(aWindow));
  NS_ENSURE_TRUE(sgo, nsnull);

  nsRefPtr<nsScreen> screen = new nsScreen();
  screen->BindToOwner(aWindow);

  hal::RegisterScreenOrientationObserver(screen);
  hal::GetCurrentScreenOrientation(&(screen->mOrientation));

  return screen.forget();
}

nsScreen::nsScreen()
  : mEventListener(nsnull)
{
}

nsScreen::~nsScreen()
{
  hal::UnregisterScreenOrientationObserver(this);
}


DOMCI_DATA(Screen, nsScreen)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsScreen)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsScreen,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(mozorientationchange)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsScreen,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(mozorientationchange)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// QueryInterface implementation for nsScreen
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsScreen)
  NS_INTERFACE_MAP_ENTRY(nsIDOMScreen)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMScreen)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Screen)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsScreen, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsScreen, nsDOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(nsScreen, mozorientationchange)

NS_IMETHODIMP
nsScreen::GetTop(PRInt32* aTop)
{
  nsRect rect;
  nsresult rv = GetRect(rect);

  *aTop = rect.y;

  return rv;
}


NS_IMETHODIMP
nsScreen::GetLeft(PRInt32* aLeft)
{
  nsRect rect;
  nsresult rv = GetRect(rect);

  *aLeft = rect.x;

  return rv;
}


NS_IMETHODIMP
nsScreen::GetWidth(PRInt32* aWidth)
{
  nsRect rect;
  nsresult rv = GetRect(rect);

  *aWidth = rect.width;

  return rv;
}

NS_IMETHODIMP
nsScreen::GetHeight(PRInt32* aHeight)
{
  nsRect rect;
  nsresult rv = GetRect(rect);

  *aHeight = rect.height;

  return rv;
}

NS_IMETHODIMP
nsScreen::GetPixelDepth(PRInt32* aPixelDepth)
{
  nsDeviceContext* context = GetDeviceContext();

  if (!context) {
    *aPixelDepth = -1;

    return NS_ERROR_FAILURE;
  }

  PRUint32 depth;
  context->GetDepth(depth);

  *aPixelDepth = depth;

  return NS_OK;
}

NS_IMETHODIMP
nsScreen::GetColorDepth(PRInt32* aColorDepth)
{
  return GetPixelDepth(aColorDepth);
}

NS_IMETHODIMP
nsScreen::GetAvailWidth(PRInt32* aAvailWidth)
{
  nsRect rect;
  nsresult rv = GetAvailRect(rect);

  *aAvailWidth = rect.width;

  return rv;
}

NS_IMETHODIMP
nsScreen::GetAvailHeight(PRInt32* aAvailHeight)
{
  nsRect rect;
  nsresult rv = GetAvailRect(rect);

  *aAvailHeight = rect.height;

  return rv;
}

NS_IMETHODIMP
nsScreen::GetAvailLeft(PRInt32* aAvailLeft)
{
  nsRect rect;
  nsresult rv = GetAvailRect(rect);

  *aAvailLeft = rect.x;

  return rv;
}

NS_IMETHODIMP
nsScreen::GetAvailTop(PRInt32* aAvailTop)
{
  nsRect rect;
  nsresult rv = GetAvailRect(rect);

  *aAvailTop = rect.y;

  return rv;
}

nsDeviceContext*
nsScreen::GetDeviceContext()
{
  return nsLayoutUtils::GetDeviceContextForScreenInfo(GetOwner());
}

nsresult
nsScreen::GetRect(nsRect& aRect)
{
  nsDeviceContext *context = GetDeviceContext();

  if (!context) {
    return NS_ERROR_FAILURE;
  }

  context->GetRect(aRect);

  aRect.x = nsPresContext::AppUnitsToIntCSSPixels(aRect.x);
  aRect.y = nsPresContext::AppUnitsToIntCSSPixels(aRect.y);
  aRect.height = nsPresContext::AppUnitsToIntCSSPixels(aRect.height);
  aRect.width = nsPresContext::AppUnitsToIntCSSPixels(aRect.width);

  return NS_OK;
}

nsresult
nsScreen::GetAvailRect(nsRect& aRect)
{
  nsDeviceContext *context = GetDeviceContext();

  if (!context) {
    return NS_ERROR_FAILURE;
  }

  context->GetClientRect(aRect);

  aRect.x = nsPresContext::AppUnitsToIntCSSPixels(aRect.x);
  aRect.y = nsPresContext::AppUnitsToIntCSSPixels(aRect.y);
  aRect.height = nsPresContext::AppUnitsToIntCSSPixels(aRect.height);
  aRect.width = nsPresContext::AppUnitsToIntCSSPixels(aRect.width);

  return NS_OK;
}

void
nsScreen::Notify(const ScreenOrientationWrapper& aOrientation)
{
  ScreenOrientation previousOrientation = mOrientation;
  mOrientation = aOrientation.orientation;

  NS_ASSERTION(mOrientation != eScreenOrientation_None &&
               mOrientation != eScreenOrientation_EndGuard &&
               mOrientation != eScreenOrientation_Portrait &&
               mOrientation != eScreenOrientation_Landscape,
               "Invalid orientation value passed to notify method!");

  if (mOrientation != previousOrientation) {
    // TODO: use an helper method, see bug 720768.
    nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nsnull, nsnull);
    nsresult rv = event->InitEvent(NS_LITERAL_STRING("mozorientationchange"), false, false);
    if (NS_FAILED(rv)) {
      return;
    }

    rv = event->SetTrusted(true);
    if (NS_FAILED(rv)) {
      return;
    }

    bool dummy;
    rv = DispatchEvent(event, &dummy);
    if (NS_FAILED(rv)) {
      return;
    }
  }
}

NS_IMETHODIMP
nsScreen::GetMozOrientation(nsAString& aOrientation)
{
  switch (mOrientation) {
    case eScreenOrientation_None:
    case eScreenOrientation_EndGuard:
    case eScreenOrientation_Portrait:
    case eScreenOrientation_Landscape:
      NS_ASSERTION(false, "Shouldn't be used when getting value!");
      return NS_ERROR_FAILURE;
    case eScreenOrientation_PortraitPrimary:
      aOrientation.AssignLiteral("portrait-primary");
      break;
    case eScreenOrientation_PortraitSecondary:
      aOrientation.AssignLiteral("portrait-secondary");
      break;
    case eScreenOrientation_LandscapePrimary:
      aOrientation.AssignLiteral("landscape-primary");
      break;
    case eScreenOrientation_LandscapeSecondary:
      aOrientation.AssignLiteral("landscape-secondary");
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScreen::MozLockOrientation(const nsAString& aOrientation, bool* aReturn)
{
  ScreenOrientation orientation;

  if (aOrientation.EqualsLiteral("portrait")) {
    orientation = eScreenOrientation_Portrait;
  } else if (aOrientation.EqualsLiteral("portrait-primary")) {
    orientation = eScreenOrientation_PortraitPrimary;
  } else if (aOrientation.EqualsLiteral("portrait-secondary")) {
    orientation = eScreenOrientation_PortraitSecondary;
  } else if (aOrientation.EqualsLiteral("landscape")) {
    orientation = eScreenOrientation_Landscape;
  } else if (aOrientation.EqualsLiteral("landscape-primary")) {
    orientation = eScreenOrientation_LandscapePrimary;
  } else if (aOrientation.EqualsLiteral("landscape-secondary")) {
    orientation = eScreenOrientation_LandscapeSecondary;
  } else {
    *aReturn = false;
    return NS_OK;
  }

  if (!GetOwner()) {
    *aReturn = false;
    return NS_OK;
  }

  if (!IsChromeType(GetOwner()->GetDocShell())) {
    nsCOMPtr<nsIDOMDocument> doc;
    GetOwner()->GetDocument(getter_AddRefs(doc));
    if (!doc) {
      *aReturn = false;
      return NS_OK;
    }

    bool fullscreen;
    doc->GetMozFullScreen(&fullscreen);
    if (!fullscreen) {
      *aReturn = false;
      return NS_OK;
    }

    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetOwner());
    if (!target) {
      *aReturn = false;
      return NS_OK;
    }

    if (!mEventListener) {
      mEventListener = new FullScreenEventListener();
    }

    target->AddSystemEventListener(NS_LITERAL_STRING("mozfullscreenchange"),
                                   mEventListener, true);
  }

  *aReturn = hal::LockScreenOrientation(orientation);
  return NS_OK;
}

NS_IMETHODIMP
nsScreen::MozUnlockOrientation()
{
  hal::UnlockScreenOrientation();
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsScreen::FullScreenEventListener, nsIDOMEventListener)

NS_IMETHODIMP
nsScreen::FullScreenEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
#ifdef DEBUG
  nsAutoString eventType;
  aEvent->GetType(eventType);

  MOZ_ASSERT(eventType.EqualsLiteral("mozfullscreenchange"));
#endif

  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetCurrentTarget(getter_AddRefs(target));

  // We have to make sure that the event we got is the event sent when
  // fullscreen is disabled because we could get one when fullscreen
  // got enabled if the lock call is done at the same moment.
  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(target);
  MOZ_ASSERT(window);

  nsCOMPtr<nsIDOMDocument> doc;
  window->GetDocument(getter_AddRefs(doc));
  // If we have no doc, we will just continue, remove the event and unlock.
  // This is an edge case were orientation lock and fullscreen is meaningless.
  if (doc) {
    bool fullscreen;
    doc->GetMozFullScreen(&fullscreen);
    if (fullscreen) {
      return NS_OK;
    }
  }

  target->RemoveSystemEventListener(NS_LITERAL_STRING("mozfullscreenchange"),
                                    this, true);

  hal::UnlockScreenOrientation();

  return NS_OK;
}
