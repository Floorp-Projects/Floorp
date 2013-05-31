/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BarProps.h"

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

DOMCI_DATA(BarProp, mozilla::dom::BarProp)

namespace mozilla {
namespace dom {

//
//  Basic (virtual) BarProp class implementation
//
BarProp::BarProp(nsGlobalWindow *aWindow)
{
  mDOMWindow = aWindow;
  nsISupports *supwin = static_cast<nsIScriptGlobalObject *>(aWindow);
  mDOMWindowWeakref = do_GetWeakReference(supwin);
}

BarProp::~BarProp()
{
}

// QueryInterface implementation for BarProp
NS_INTERFACE_MAP_BEGIN(BarProp)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBarProp)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BarProp)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(BarProp)
NS_IMPL_RELEASE(BarProp)

NS_IMETHODIMP
BarProp::GetVisibleByFlag(bool *aVisible, uint32_t aChromeFlag)
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
BarProp::SetVisibleByFlag(bool aVisible, uint32_t aChromeFlag)
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
BarProp::GetBrowserChrome()
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

MenubarProp::MenubarProp(nsGlobalWindow *aWindow)
  : BarProp(aWindow)
{
}

MenubarProp::~MenubarProp()
{
}

NS_IMETHODIMP
MenubarProp::GetVisible(bool *aVisible)
{
  return BarProp::GetVisibleByFlag(aVisible,
                                   nsIWebBrowserChrome::CHROME_MENUBAR);
}

NS_IMETHODIMP
MenubarProp::SetVisible(bool aVisible)
{
  return BarProp::SetVisibleByFlag(aVisible,
                                   nsIWebBrowserChrome::CHROME_MENUBAR);
}

//
// ToolbarProp class implementation
//

ToolbarProp::ToolbarProp(nsGlobalWindow *aWindow)
  : BarProp(aWindow)
{
}

ToolbarProp::~ToolbarProp()
{
}

NS_IMETHODIMP
ToolbarProp::GetVisible(bool *aVisible)
{
  return BarProp::GetVisibleByFlag(aVisible,
                                   nsIWebBrowserChrome::CHROME_TOOLBAR);
}

NS_IMETHODIMP
ToolbarProp::SetVisible(bool aVisible)
{
  return BarProp::SetVisibleByFlag(aVisible,
                                   nsIWebBrowserChrome::CHROME_TOOLBAR);
}

//
// LocationbarProp class implementation
//

LocationbarProp::LocationbarProp(nsGlobalWindow *aWindow)
  : BarProp(aWindow)
{
}

LocationbarProp::~LocationbarProp()
{
}

NS_IMETHODIMP
LocationbarProp::GetVisible(bool *aVisible)
{
  return
    BarProp::GetVisibleByFlag(aVisible,
                              nsIWebBrowserChrome::CHROME_LOCATIONBAR);
}

NS_IMETHODIMP
LocationbarProp::SetVisible(bool aVisible)
{
  return
    BarProp::SetVisibleByFlag(aVisible,
                              nsIWebBrowserChrome::CHROME_LOCATIONBAR);
}

//
// PersonalbarProp class implementation
//

PersonalbarProp::PersonalbarProp(nsGlobalWindow *aWindow)
  : BarProp(aWindow)
{
}

PersonalbarProp::~PersonalbarProp()
{
}

NS_IMETHODIMP
PersonalbarProp::GetVisible(bool *aVisible)
{
  return
    BarProp::GetVisibleByFlag(aVisible,
                              nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);
}

NS_IMETHODIMP
PersonalbarProp::SetVisible(bool aVisible)
{
  return
    BarProp::SetVisibleByFlag(aVisible,
                              nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);
}

//
// StatusbarProp class implementation
//

StatusbarProp::StatusbarProp(nsGlobalWindow *aWindow)
  : BarProp(aWindow)
{
}

StatusbarProp::~StatusbarProp()
{
}

NS_IMETHODIMP
StatusbarProp::GetVisible(bool *aVisible)
{
  return BarProp::GetVisibleByFlag(aVisible,
                                   nsIWebBrowserChrome::CHROME_STATUSBAR);
}

NS_IMETHODIMP
StatusbarProp::SetVisible(bool aVisible)
{
  return BarProp::SetVisibleByFlag(aVisible,
                                   nsIWebBrowserChrome::CHROME_STATUSBAR);
}

//
// ScrollbarsProp class implementation
//

ScrollbarsProp::ScrollbarsProp(nsGlobalWindow *aWindow)
: BarProp(aWindow)
{
}

ScrollbarsProp::~ScrollbarsProp()
{
}

NS_IMETHODIMP
ScrollbarsProp::GetVisible(bool *aVisible)
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
ScrollbarsProp::SetVisible(bool aVisible)
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

} // namespace dom
} // namespace mozilla

