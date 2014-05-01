/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BarProps.h"
#include "mozilla/dom/BarPropBinding.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIDocShell.h"
#include "nsIScrollable.h"
#include "nsIWebBrowserChrome.h"

namespace mozilla {
namespace dom {

//
//  Basic (virtual) BarProp class implementation
//
BarProp::BarProp(nsGlobalWindow* aWindow)
  : mDOMWindow(aWindow)
{
  MOZ_ASSERT(aWindow->IsInnerWindow());
  SetIsDOMBinding();
}

BarProp::~BarProp()
{
}

nsPIDOMWindow*
BarProp::GetParentObject() const
{
  return mDOMWindow;
}

JSObject*
BarProp::WrapObject(JSContext* aCx)
{
  return BarPropBinding::Wrap(aCx, this);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BarProp, mDOMWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BarProp)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BarProp)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BarProp)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

bool
BarProp::GetVisibleByFlag(uint32_t aChromeFlag, ErrorResult& aRv)
{
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = GetBrowserChrome();
  NS_ENSURE_TRUE(browserChrome, false);

  uint32_t chromeFlags;

  if (NS_FAILED(browserChrome->GetChromeFlags(&chromeFlags))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  return (chromeFlags & aChromeFlag);
}

void
BarProp::SetVisibleByFlag(bool aVisible, uint32_t aChromeFlag,
                          ErrorResult& aRv)
{
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = GetBrowserChrome();
  NS_ENSURE_TRUE_VOID(browserChrome);

  if (!nsContentUtils::IsCallerChrome()) {
    return;
  }

  uint32_t chromeFlags;

  if (NS_FAILED(browserChrome->GetChromeFlags(&chromeFlags))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (aVisible)
    chromeFlags |= aChromeFlag;
  else
    chromeFlags &= ~aChromeFlag;

  if (NS_FAILED(browserChrome->SetChromeFlags(chromeFlags))) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

already_AddRefed<nsIWebBrowserChrome>
BarProp::GetBrowserChrome()
{
  if (!mDOMWindow) {
    return nullptr;
  }

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

bool
MenubarProp::GetVisible(ErrorResult& aRv)
{
  return BarProp::GetVisibleByFlag(nsIWebBrowserChrome::CHROME_MENUBAR, aRv);
}

void
MenubarProp::SetVisible(bool aVisible, ErrorResult& aRv)
{
  BarProp::SetVisibleByFlag(aVisible, nsIWebBrowserChrome::CHROME_MENUBAR, aRv);
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

bool
ToolbarProp::GetVisible(ErrorResult& aRv)
{
  return BarProp::GetVisibleByFlag(nsIWebBrowserChrome::CHROME_TOOLBAR, aRv);
}

void
ToolbarProp::SetVisible(bool aVisible, ErrorResult& aRv)
{
  BarProp::SetVisibleByFlag(aVisible, nsIWebBrowserChrome::CHROME_TOOLBAR,
                            aRv);
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

bool
LocationbarProp::GetVisible(ErrorResult& aRv)
{
  return BarProp::GetVisibleByFlag(nsIWebBrowserChrome::CHROME_LOCATIONBAR,
                                   aRv);
}

void
LocationbarProp::SetVisible(bool aVisible, ErrorResult& aRv)
{
  BarProp::SetVisibleByFlag(aVisible, nsIWebBrowserChrome::CHROME_LOCATIONBAR,
                            aRv);
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

bool
PersonalbarProp::GetVisible(ErrorResult& aRv)
{
  return BarProp::GetVisibleByFlag(nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR,
                                   aRv);
}

void
PersonalbarProp::SetVisible(bool aVisible, ErrorResult& aRv)
{
  BarProp::SetVisibleByFlag(aVisible,
                            nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR,
                            aRv);
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

bool
StatusbarProp::GetVisible(ErrorResult& aRv)
{
  return BarProp::GetVisibleByFlag(nsIWebBrowserChrome::CHROME_STATUSBAR, aRv);
}

void
StatusbarProp::SetVisible(bool aVisible, ErrorResult& aRv)
{
  return BarProp::SetVisibleByFlag(aVisible,
                                   nsIWebBrowserChrome::CHROME_STATUSBAR, aRv);
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

bool
ScrollbarsProp::GetVisible(ErrorResult& aRv)
{
  if (!mDOMWindow) {
    return true;
  }

  nsCOMPtr<nsIScrollable> scroller =
    do_QueryInterface(mDOMWindow->GetDocShell());

  if (!scroller) {
    return true;
  }

  int32_t prefValue;
  scroller->GetDefaultScrollbarPreferences(
              nsIScrollable::ScrollOrientation_Y, &prefValue);
  if (prefValue != nsIScrollable::Scrollbar_Never) {
    return true;
  }

  scroller->GetDefaultScrollbarPreferences(
              nsIScrollable::ScrollOrientation_X, &prefValue);
  return prefValue != nsIScrollable::Scrollbar_Never;
}

void
ScrollbarsProp::SetVisible(bool aVisible, ErrorResult& aRv)
{
  if (!nsContentUtils::IsCallerChrome()) {
    return;
  }

  /* Scrollbars, unlike the other barprops, implement visibility directly
     rather than handing off to the superclass (and from there to the
     chrome window) because scrollbar visibility uniquely applies only
     to the window making the change (arguably. it does now, anyway.)
     and because embedding apps have no interface for implementing this
     themselves, and therefore the implementation must be internal. */

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
}

} // namespace dom
} // namespace mozilla

