/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* BarProps are the collection of little properties of DOM windows whose
   only property of their own is "visible".  They describe the window
   chrome which can be made visible or not through JavaScript by setting
   the appropriate property (window.menubar.visible)
*/

#ifndef nsBarProps_h___
#define nsBarProps_h___

#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIDOMBarProp.h"
#include "nsIWeakReference.h"

class nsGlobalWindow;
class nsIWebBrowserChrome;

// Script "BarProp" object
class nsBarProp : public nsIDOMBarProp
{
public:
  explicit nsBarProp(nsGlobalWindow *aWindow);
  virtual ~nsBarProp();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetVisibleByFlag(bool *aVisible, PRUint32 aChromeFlag);
  NS_IMETHOD SetVisibleByFlag(bool aVisible, PRUint32 aChromeFlag);

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
class nsMenubarProp : public nsBarProp
{
public:
  explicit nsMenubarProp(nsGlobalWindow *aWindow);
  virtual ~nsMenubarProp();

  NS_DECL_NSIDOMBARPROP
};

// Script "toolbar" object
class nsToolbarProp : public nsBarProp
{
public:
  explicit nsToolbarProp(nsGlobalWindow *aWindow);
  virtual ~nsToolbarProp();

  NS_DECL_NSIDOMBARPROP
};

// Script "locationbar" object
class nsLocationbarProp : public nsBarProp
{
public:
  explicit nsLocationbarProp(nsGlobalWindow *aWindow);
  virtual ~nsLocationbarProp();

  NS_DECL_NSIDOMBARPROP
};

// Script "personalbar" object
class nsPersonalbarProp : public nsBarProp
{
public:
  explicit nsPersonalbarProp(nsGlobalWindow *aWindow);
  virtual ~nsPersonalbarProp();

  NS_DECL_NSIDOMBARPROP
};

// Script "statusbar" object
class nsStatusbarProp : public nsBarProp
{
public:
  explicit nsStatusbarProp(nsGlobalWindow *aWindow);
  virtual ~nsStatusbarProp();

  NS_DECL_NSIDOMBARPROP
};

// Script "scrollbars" object
class nsScrollbarsProp : public nsBarProp
{
public:
  explicit nsScrollbarsProp(nsGlobalWindow *aWindow);
  virtual ~nsScrollbarsProp();

  NS_DECL_NSIDOMBARPROP
};

#endif /* nsBarProps_h___ */

