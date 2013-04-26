/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBarProps.h"

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsGlobalWindow.h"
#include "nsStyleConsts.h"
#include "nsIDocShell.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollable.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDOMWindow.h"
#include "nsDOMClassInfoID.h"
#include "nsContentUtils.h"

//
//  Basic (virtual) BarProp class implementation
//
nsBarProp::nsBarProp(nsGlobalWindow *aWindow)
{
  mDOMWindow = aWindow;
  nsISupports *supwin = static_cast<nsIScriptGlobalObject *>(aWindow);
  mDOMWindowWeakref = do_GetWeakReference(supwin);
}

nsBarProp::~nsBarProp()
{
}


DOMCI_DATA(BarProp, nsBarProp)

// QueryInterface implementation for BarProp
NS_INTERFACE_MAP_BEGIN(nsBarProp)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBarProp)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BarProp)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsBarProp)
NS_IMPL_RELEASE(nsBarProp)

NS_IMETHODIMP
nsBarProp::GetVisibleByFlag(bool *aVisible, uint32_t aChromeFlag)
{
  *aVisible = false;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome = GetBrowserChrome();
  NS_ENSURE_TRUE(browserChrome, NS_OK);

  uint32_t chromeFlags;

  NS_ENSURE_SUCCESS(browserChrome->GetChromeFlags(&chromeFlags),
                    NS_ERROR_FAILURE);
  if (chromeFlags & aChromeFlag)
    *aVisible = true;

  return NS_OK;
}

NS_IMETHODIMP
nsBarProp::SetVisibleByFlag(bool aVisible, uint32_t aChromeFlag)
{
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = GetBrowserChrome();
  NS_ENSURE_TRUE(browserChrome, NS_OK);

  if (!nsContentUtils::IsCallerChrome())
    return NS_OK;

  uint32_t chromeFlags;

  NS_ENSURE_SUCCESS(browserChrome->GetChromeFlags(&chromeFlags),
                    NS_ERROR_FAILURE);
  if (aVisible)
    chromeFlags |= aChromeFlag;
  else
    chromeFlags &= ~aChromeFlag;
  NS_ENSURE_SUCCESS(browserChrome->SetChromeFlags(chromeFlags),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

already_AddRefed<nsIWebBrowserChrome>
nsBarProp::GetBrowserChrome()
{
  // Check that the window is still alive.
  nsCOMPtr<nsIDOMWindow> domwin(do_QueryReferent(mDOMWindowWeakref));
  if (!domwin)
    return nullptr;

  return mDOMWindow->GetWebBrowserChrome();
}

//
// MenubarProp class implementation
//

nsMenubarProp::nsMenubarProp(nsGlobalWindow *aWindow) 
  : nsBarProp(aWindow)
{
}

nsMenubarProp::~nsMenubarProp()
{
}

NS_IMETHODIMP
nsMenubarProp::GetVisible(bool *aVisible)
{
  return nsBarProp::GetVisibleByFlag(aVisible,
                                     nsIWebBrowserChrome::CHROME_MENUBAR);
}

NS_IMETHODIMP
nsMenubarProp::SetVisible(bool aVisible)
{
  return nsBarProp::SetVisibleByFlag(aVisible,
                                     nsIWebBrowserChrome::CHROME_MENUBAR);
}

//
// ToolbarProp class implementation
//

nsToolbarProp::nsToolbarProp(nsGlobalWindow *aWindow)
  : nsBarProp(aWindow)
{
}

nsToolbarProp::~nsToolbarProp()
{
}

NS_IMETHODIMP
nsToolbarProp::GetVisible(bool *aVisible)
{
  return nsBarProp::GetVisibleByFlag(aVisible,
                                     nsIWebBrowserChrome::CHROME_TOOLBAR);
}

NS_IMETHODIMP
nsToolbarProp::SetVisible(bool aVisible)
{
  return nsBarProp::SetVisibleByFlag(aVisible,
                                     nsIWebBrowserChrome::CHROME_TOOLBAR);
}

//
// LocationbarProp class implementation
//

nsLocationbarProp::nsLocationbarProp(nsGlobalWindow *aWindow)
  : nsBarProp(aWindow)
{
}

nsLocationbarProp::~nsLocationbarProp()
{
}

NS_IMETHODIMP
nsLocationbarProp::GetVisible(bool *aVisible)
{
  return
    nsBarProp::GetVisibleByFlag(aVisible,
                                nsIWebBrowserChrome::CHROME_LOCATIONBAR);
}

NS_IMETHODIMP
nsLocationbarProp::SetVisible(bool aVisible)
{
  return
    nsBarProp::SetVisibleByFlag(aVisible,
                                nsIWebBrowserChrome::CHROME_LOCATIONBAR);
}

//
// PersonalbarProp class implementation
//

nsPersonalbarProp::nsPersonalbarProp(nsGlobalWindow *aWindow)
  : nsBarProp(aWindow)
{
}

nsPersonalbarProp::~nsPersonalbarProp()
{
}

NS_IMETHODIMP
nsPersonalbarProp::GetVisible(bool *aVisible)
{
  return
    nsBarProp::GetVisibleByFlag(aVisible,
                                nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);
}

NS_IMETHODIMP
nsPersonalbarProp::SetVisible(bool aVisible)
{
  return
    nsBarProp::SetVisibleByFlag(aVisible,
                                nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);
}

//
// StatusbarProp class implementation
//

nsStatusbarProp::nsStatusbarProp(nsGlobalWindow *aWindow)
  : nsBarProp(aWindow)
{
}

nsStatusbarProp::~nsStatusbarProp()
{
}

NS_IMETHODIMP
nsStatusbarProp::GetVisible(bool *aVisible)
{
  return nsBarProp::GetVisibleByFlag(aVisible,
                                     nsIWebBrowserChrome::CHROME_STATUSBAR);
}

NS_IMETHODIMP
nsStatusbarProp::SetVisible(bool aVisible)
{
  return nsBarProp::SetVisibleByFlag(aVisible,
                                     nsIWebBrowserChrome::CHROME_STATUSBAR);
}

//
// ScrollbarsProp class implementation
//

nsScrollbarsProp::nsScrollbarsProp(nsGlobalWindow *aWindow)
: nsBarProp(aWindow)
{
}

nsScrollbarsProp::~nsScrollbarsProp()
{
}

NS_IMETHODIMP
nsScrollbarsProp::GetVisible(bool *aVisible)
{
  *aVisible = true; // one assumes

  nsCOMPtr<nsIDOMWindow> domwin(do_QueryReferent(mDOMWindowWeakref));
  if (domwin) { // dom window not deleted
    nsCOMPtr<nsIScrollable> scroller =
      do_QueryInterface(mDOMWindow->GetDocShell());

    if (scroller) {
      int32_t prefValue;
      scroller->GetDefaultScrollbarPreferences(
                  nsIScrollable::ScrollOrientation_Y, &prefValue);
      if (prefValue == nsIScrollable::Scrollbar_Never) // try the other way
        scroller->GetDefaultScrollbarPreferences(
                    nsIScrollable::ScrollOrientation_X, &prefValue);

      if (prefValue == nsIScrollable::Scrollbar_Never)
        *aVisible = false;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScrollbarsProp::SetVisible(bool aVisible)
{
  if (!nsContentUtils::IsCallerChrome())
    return NS_OK;

  /* Scrollbars, unlike the other barprops, implement visibility directly
     rather than handing off to the superclass (and from there to the
     chrome window) because scrollbar visibility uniquely applies only
     to the window making the change (arguably. it does now, anyway.)
     and because embedding apps have no interface for implementing this
     themselves, and therefore the implementation must be internal. */

  nsCOMPtr<nsIDOMWindow> domwin(do_QueryReferent(mDOMWindowWeakref));
  if (domwin) { // dom window must still exist. use away.
    nsCOMPtr<nsIScrollable> scroller =
      do_QueryInterface(mDOMWindow->GetDocShell());

    if (scroller) {
      int32_t prefValue;

      if (aVisible) {
        prefValue = nsIScrollable::Scrollbar_Auto;
      } else {
        prefValue = nsIScrollable::Scrollbar_Never;
      }

      scroller->SetDefaultScrollbarPreferences(
                  nsIScrollable::ScrollOrientation_Y, prefValue);
      scroller->SetDefaultScrollbarPreferences(
                  nsIScrollable::ScrollOrientation_X, prefValue);
    }
  }

  /* Notably absent is the part where we notify the chrome window using
     GetBrowserChrome()->SetChromeFlags(). Given the possibility of multiple
     DOM windows (multiple top-level windows, even) within a single
     chrome window, the historical concept of a single "has scrollbars"
     flag in the chrome is inapplicable, and we can't tell at this level
     whether we represent the particular DOM window that makes this decision
     for the chrome.

     So only this object (and its corresponding DOM window) knows whether
     scrollbars are visible. The corresponding chrome window will need to
     ask (one of) its DOM window(s) when it needs to know about scrollbar
     visibility, rather than caching its own copy of that information.
  */

  return NS_OK;
}

