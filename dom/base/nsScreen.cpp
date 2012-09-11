/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "nsScreen.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsDOMClassInfoID.h"
#include "nsIDocShellTreeItem.h"
#include "nsLayoutUtils.h"
#include "nsDOMEvent.h"
#include "nsGlobalWindow.h"
#include "nsJSUtils.h"

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

  int32_t itemType;
  ds->GetItemType(&itemType);
  return itemType == nsIDocShellTreeItem::typeChrome;
}

} // anonymous namespace

/* static */ already_AddRefed<nsScreen>
nsScreen::Create(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(aWindow);

  if (!aWindow->GetDocShell()) {
    return nullptr;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo =
    do_QueryInterface(static_cast<nsPIDOMWindow*>(aWindow));
  NS_ENSURE_TRUE(sgo, nullptr);

  nsRefPtr<nsScreen> screen = new nsScreen();
  screen->BindToOwner(aWindow);

  hal::RegisterScreenConfigurationObserver(screen);
  hal::ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);
  screen->mOrientation = config.orientation();

  return screen.forget();
}

nsScreen::nsScreen()
  : mEventListener(nullptr)
{
}

void
nsScreen::Reset()
{
  hal::UnlockScreenOrientation();

  if (mEventListener) {
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetOwner());
    if (target) {
      target->RemoveSystemEventListener(NS_LITERAL_STRING("mozfullscreenchange"),
                                        mEventListener, true);
    }

    mEventListener = nullptr;
  }
}

nsScreen::~nsScreen()
{
  Reset();
  hal::UnregisterScreenConfigurationObserver(this);
}


DOMCI_DATA(Screen, nsScreen)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsScreen)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsScreen,
                                                  nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsScreen,
                                                nsDOMEventTargetHelper)
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
nsScreen::GetTop(int32_t* aTop)
{
  nsRect rect;
  nsresult rv = GetRect(rect);

  *aTop = rect.y;

  return rv;
}


NS_IMETHODIMP
nsScreen::GetLeft(int32_t* aLeft)
{
  nsRect rect;
  nsresult rv = GetRect(rect);

  *aLeft = rect.x;

  return rv;
}


NS_IMETHODIMP
nsScreen::GetWidth(int32_t* aWidth)
{
  nsRect rect;
  nsresult rv = GetRect(rect);

  *aWidth = rect.width;

  return rv;
}

NS_IMETHODIMP
nsScreen::GetHeight(int32_t* aHeight)
{
  nsRect rect;
  nsresult rv = GetRect(rect);

  *aHeight = rect.height;

  return rv;
}

NS_IMETHODIMP
nsScreen::GetPixelDepth(int32_t* aPixelDepth)
{
  nsDeviceContext* context = GetDeviceContext();

  if (!context) {
    *aPixelDepth = -1;

    return NS_ERROR_FAILURE;
  }

  uint32_t depth;
  context->GetDepth(depth);

  *aPixelDepth = depth;

  return NS_OK;
}

NS_IMETHODIMP
nsScreen::GetColorDepth(int32_t* aColorDepth)
{
  return GetPixelDepth(aColorDepth);
}

NS_IMETHODIMP
nsScreen::GetAvailWidth(int32_t* aAvailWidth)
{
  nsRect rect;
  nsresult rv = GetAvailRect(rect);

  *aAvailWidth = rect.width;

  return rv;
}

NS_IMETHODIMP
nsScreen::GetAvailHeight(int32_t* aAvailHeight)
{
  nsRect rect;
  nsresult rv = GetAvailRect(rect);

  *aAvailHeight = rect.height;

  return rv;
}

NS_IMETHODIMP
nsScreen::GetAvailLeft(int32_t* aAvailLeft)
{
  nsRect rect;
  nsresult rv = GetAvailRect(rect);

  *aAvailLeft = rect.x;

  return rv;
}

NS_IMETHODIMP
nsScreen::GetAvailTop(int32_t* aAvailTop)
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
nsScreen::Notify(const hal::ScreenConfiguration& aConfiguration)
{
  ScreenOrientation previousOrientation = mOrientation;
  mOrientation = aConfiguration.orientation();

  NS_ASSERTION(mOrientation != eScreenOrientation_None &&
               mOrientation != eScreenOrientation_Portrait &&
               mOrientation != eScreenOrientation_Landscape,
               "Invalid orientation value passed to notify method!");

  if (mOrientation != previousOrientation) {
    // TODO: use an helper method, see bug 720768.
    nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nullptr, nullptr);
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
nsScreen::MozLockOrientation(const jsval& aOrientation, JSContext* aCx, bool* aReturn)
{
  *aReturn = false;

  nsAutoTArray<nsString, 8> orientations;
  // Preallocating 8 elements to make it faster.

  if (aOrientation.isString()) {
    nsDependentJSString item;
    item.init(aCx, aOrientation.toString());
    orientations.AppendElement(item);
  } else {
    // If we don't have a string, we must have an Array.
    if (!aOrientation.isObject()) {
      return NS_ERROR_INVALID_ARG;
    }

    JSObject& obj = aOrientation.toObject();
    uint32_t length;
    if (!JS_GetArrayLength(aCx, &obj, &length) || length <= 0) {
      return NS_ERROR_INVALID_ARG;
    }

    orientations.SetCapacity(length);

    for (uint32_t i = 0; i < length; ++i) {
      jsval value;
      NS_ENSURE_TRUE(JS_GetElement(aCx, &obj, i, &value), NS_ERROR_UNEXPECTED);
      if (!value.isString()) {
        return NS_ERROR_INVALID_ARG;
      }

      nsDependentJSString item;
      item.init(aCx, value);
      orientations.AppendElement(item);
    }
  }

  ScreenOrientation orientation = eScreenOrientation_None;

  for (uint32_t i=0; i<orientations.Length(); ++i) {
    nsString& item = orientations[i];

    if (item.EqualsLiteral("portrait")) {
      orientation |= eScreenOrientation_Portrait;
    } else if (item.EqualsLiteral("portrait-primary")) {
      orientation |= eScreenOrientation_PortraitPrimary;
    } else if (item.EqualsLiteral("portrait-secondary")) {
      orientation |= eScreenOrientation_PortraitSecondary;
    } else if (item.EqualsLiteral("landscape")) {
      orientation |= eScreenOrientation_Landscape;
    } else if (item.EqualsLiteral("landscape-primary")) {
      orientation |= eScreenOrientation_LandscapePrimary;
    } else if (item.EqualsLiteral("landscape-secondary")) {
      orientation |= eScreenOrientation_LandscapeSecondary;
    } else {
      // If we don't recognize that the token, we should just return 'false'
      // without throwing.
      return NS_OK;
    }
  }

  // Determine whether we can lock the screen orientation.
  bool canLockOrientation = false;
  do {
    nsCOMPtr<nsPIDOMWindow> owner = GetOwner();
    if (!owner) {
      break;
    }

    // Chrome can always lock the screen orientation.
    if (IsChromeType(owner->GetDocShell())) {
      canLockOrientation = true;
      break;
    }

    nsCOMPtr<nsIDOMDocument> domDoc;
    owner->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    if (!doc) {
      break;
    }

    // Apps can always lock the screen orientation.
    if (doc->NodePrincipal()->GetAppStatus() >=
          nsIPrincipal::APP_STATUS_INSTALLED) {
      canLockOrientation = true;
      break;
    }

    // Other content must be full-screen in order to lock orientation.
    bool fullscreen;
    domDoc->GetMozFullScreen(&fullscreen);
    if (!fullscreen) {
      break;
    }

    // If we're full-screen, register a listener so we learn when we leave
    // full-screen.
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(owner);
    if (!target) {
      break;
    }

    if (!mEventListener) {
      mEventListener = new FullScreenEventListener();
    }

    target->AddSystemEventListener(NS_LITERAL_STRING("mozfullscreenchange"),
                                   mEventListener, /* useCapture = */ true);
    canLockOrientation = true;
  } while(0);

  if (canLockOrientation) {
    *aReturn = hal::LockScreenOrientation(orientation);
  }

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
