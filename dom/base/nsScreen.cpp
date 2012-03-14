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
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsLayoutUtils.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "nsDOMEvent.h"

using namespace mozilla;
using namespace mozilla::dom;

/* static */ bool nsScreen::sInitialized = false;
/* static */ bool nsScreen::sAllowScreenEnabledProperty = false;
/* static */ bool nsScreen::sAllowScreenBrightnessProperty = false;

/* static */ void
nsScreen::Initialize()
{
  MOZ_ASSERT(!sInitialized);
  sInitialized = true;
  Preferences::AddBoolVarCache(&sAllowScreenEnabledProperty,
                               "dom.screenEnabledProperty.enabled");
  Preferences::AddBoolVarCache(&sAllowScreenBrightnessProperty,
                               "dom.screenBrightnessProperty.enabled");
}

void
nsScreen::Invalidate()
{
  hal::UnregisterScreenOrientationObserver(this);
}

/* static */ already_AddRefed<nsScreen>
nsScreen::Create(nsPIDOMWindow* aWindow)
{
  if (!sInitialized) {
    Initialize();
  }

  if (!aWindow->GetDocShell()) {
    return nsnull;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(sgo, nsnull);

  nsRefPtr<nsScreen> screen = new nsScreen();
  screen->BindToOwner(aWindow);
  screen->mDocShell = aWindow->GetDocShell();

  hal::RegisterScreenOrientationObserver(screen);
  hal::GetCurrentScreenOrientation(&(screen->mOrientation));

  return screen.forget();
}

nsScreen::nsScreen()
{
}

nsScreen::~nsScreen()
{
  Invalidate();
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
  return nsLayoutUtils::GetDeviceContextForScreenInfo(mDocShell);
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

namespace {

bool IsWhiteListed(nsIDocShell *aDocShell) {
  nsCOMPtr<nsIDocShellTreeItem> ds = do_QueryInterface(aDocShell);
  if (!ds) {
    return false;
  }

  PRInt32 itemType;
  ds->GetItemType(&itemType);
  if (itemType == nsIDocShellTreeItem::typeChrome) {
    return true;
  }

  nsCOMPtr<nsIDocument> doc = do_GetInterface(aDocShell);
  nsIPrincipal *principal = doc->NodePrincipal();

  nsCOMPtr<nsIURI> principalURI;
  principal->GetURI(getter_AddRefs(principalURI));
  if (nsContentUtils::URIIsChromeOrInPref(principalURI,
                                          "dom.mozScreenWhitelist")) {
    return true;
  }

  return false;
}

} // anonymous namespace

nsresult
nsScreen::GetMozEnabled(bool *aEnabled)
{
  if (!sAllowScreenEnabledProperty || !IsWhiteListed(mDocShell)) {
    *aEnabled = true;
    return NS_OK;
  }

  *aEnabled = hal::GetScreenEnabled();
  return NS_OK;
}

nsresult
nsScreen::SetMozEnabled(bool aEnabled)
{
  if (!sAllowScreenEnabledProperty || !IsWhiteListed(mDocShell)) {
    return NS_OK;
  }

  // TODO bug 707589: When the screen's state changes, all visible windows
  // should fire a visibility change event.
  hal::SetScreenEnabled(aEnabled);
  return NS_OK;
}

nsresult
nsScreen::GetMozBrightness(double *aBrightness)
{
  if (!sAllowScreenEnabledProperty || !IsWhiteListed(mDocShell)) {
    *aBrightness = 1;
    return NS_OK;
  }

  *aBrightness = hal::GetScreenBrightness();
  return NS_OK;
}

nsresult
nsScreen::SetMozBrightness(double aBrightness)
{
  if (!sAllowScreenEnabledProperty || !IsWhiteListed(mDocShell)) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(0 <= aBrightness && aBrightness <= 1, NS_ERROR_INVALID_ARG);
  hal::SetScreenBrightness(aBrightness);
  return NS_OK;
}

void
nsScreen::Notify(const ScreenOrientationWrapper& aOrientation)
{
  ScreenOrientation previousOrientation = mOrientation;
  mOrientation = aOrientation.orientation;

  NS_ASSERTION(mOrientation != eScreenOrientation_Current &&
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
    case eScreenOrientation_Current:
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
