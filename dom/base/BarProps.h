/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* BarProps are the collection of little properties of DOM windows whose
   only property of their own is "visible".  They describe the window
   chrome which can be made visible or not through JavaScript by setting
   the appropriate property (window.menubar.visible)
*/

#ifndef mozilla_dom_BarProps_h
#define mozilla_dom_BarProps_h

#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIDOMBarProp.h"
#include "nsIWeakReference.h"

class nsGlobalWindow;
class nsIWebBrowserChrome;

namespace mozilla {
namespace dom {

// Script "BarProp" object
class BarProp : public nsIDOMBarProp
{
public:
  explicit BarProp(nsGlobalWindow *aWindow);
  virtual ~BarProp();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetVisibleByFlag(bool *aVisible, uint32_t aChromeFlag);
  NS_IMETHOD SetVisibleByFlag(bool aVisible, uint32_t aChromeFlag);

protected:
  already_AddRefed<nsIWebBrowserChrome> GetBrowserChrome();

  nsGlobalWindow             *mDOMWindow;
  nsCOMPtr<nsIWeakReference>  mDOMWindowWeakref;
  /* Note the odd double reference to the owning global window.
     Since the corresponding DOM window nominally owns this object,
     but refcounted ownership of this object can be handed off to
     owners unknown, we need a weak ref back to the DOM window.
     However we also need access to properties of the DOM Window
     that aren't available through interfaces. Then it's either
     a weak ref and some skanky casting, or this funky double ref.
     Funky beats skanky, so here we are. */
};

// Script "menubar" object
class MenubarProp : public BarProp
{
public:
  explicit MenubarProp(nsGlobalWindow *aWindow);
  virtual ~MenubarProp();

  NS_DECL_NSIDOMBARPROP
};

// Script "toolbar" object
class ToolbarProp : public BarProp
{
public:
  explicit ToolbarProp(nsGlobalWindow *aWindow);
  virtual ~ToolbarProp();

  NS_DECL_NSIDOMBARPROP
};

// Script "locationbar" object
class LocationbarProp : public BarProp
{
public:
  explicit LocationbarProp(nsGlobalWindow *aWindow);
  virtual ~LocationbarProp();

  NS_DECL_NSIDOMBARPROP
};

// Script "personalbar" object
class PersonalbarProp : public BarProp
{
public:
  explicit PersonalbarProp(nsGlobalWindow *aWindow);
  virtual ~PersonalbarProp();

  NS_DECL_NSIDOMBARPROP
};

// Script "statusbar" object
class StatusbarProp : public BarProp
{
public:
  explicit StatusbarProp(nsGlobalWindow *aWindow);
  virtual ~StatusbarProp();

  NS_DECL_NSIDOMBARPROP
};

// Script "scrollbars" object
class ScrollbarsProp : public BarProp
{
public:
  explicit ScrollbarsProp(nsGlobalWindow *aWindow);
  virtual ~ScrollbarsProp();

  NS_DECL_NSIDOMBARPROP
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_BarProps_h */

