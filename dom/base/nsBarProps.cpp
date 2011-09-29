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
#include "nsDOMClassInfo.h"

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
nsBarProp::GetVisibleByFlag(bool *aVisible, PRUint32 aChromeFlag)
{
  *aVisible = PR_FALSE;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome = GetBrowserChrome();
  NS_ENSURE_TRUE(browserChrome, NS_OK);

  PRUint32 chromeFlags;

  NS_ENSURE_SUCCESS(browserChrome->GetChromeFlags(&chromeFlags),
                    NS_ERROR_FAILURE);
  if (chromeFlags & aChromeFlag)
    *aVisible = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsBarProp::SetVisibleByFlag(bool aVisible, PRUint32 aChromeFlag)
{
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = GetBrowserChrome();
  NS_ENSURE_TRUE(browserChrome, NS_OK);

  bool enabled = false;

  nsCOMPtr<nsIScriptSecurityManager>
           securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
  if (securityManager)
    securityManager->IsCapabilityEnabled("UniversalBrowserWrite", &enabled);
  if (!enabled)
    return NS_OK;

  PRUint32 chromeFlags;

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
    return nsnull;

  nsIWebBrowserChrome *browserChrome = nsnull;
  mDOMWindow->GetWebBrowserChrome(&browserChrome);
  return browserChrome;
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
  *aVisible = PR_TRUE; // one assumes

  nsCOMPtr<nsIDOMWindow> domwin(do_QueryReferent(mDOMWindowWeakref));
  if (domwin) { // dom window not deleted
    nsCOMPtr<nsIScrollable> scroller =
      do_QueryInterface(mDOMWindow->GetDocShell());

    if (scroller) {
      PRInt32 prefValue;
      scroller->GetDefaultScrollbarPreferences(
                  nsIScrollable::ScrollOrientation_Y, &prefValue);
      if (prefValue == nsIScrollable::Scrollbar_Never) // try the other way
        scroller->GetDefaultScrollbarPreferences(
                    nsIScrollable::ScrollOrientation_X, &prefValue);

      if (prefValue == nsIScrollable::Scrollbar_Never)
        *aVisible = PR_FALSE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScrollbarsProp::SetVisible(bool aVisible)
{
  bool     enabled = false;

  nsCOMPtr<nsIScriptSecurityManager>
           securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
  if (securityManager)
    securityManager->IsCapabilityEnabled("UniversalBrowserWrite", &enabled);
  if (!enabled)
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
      PRInt32 prefValue;

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

