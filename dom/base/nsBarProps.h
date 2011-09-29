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

