/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* BarProps are the collection of little properties of DOM windows whose
   only property of their own is "visible".  They describe the window
   chrome which can be made visible or not through JavaScript by setting
   the appropriate property (window.menubar.visible)
*/

#ifndef nsBarProps_h___
#define nsBarProps_h___

#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMBarProp.h"

class nsIBrowserWindow;

// Script "BarProp" object
class BarPropImpl : public nsIScriptObjectOwner, public nsIDOMBarProp {
public:
  BarPropImpl();
  virtual ~BarPropImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  NS_IMETHOD_(void) SetBrowserWindow(nsIBrowserWindow *aBrowser);

  NS_IMETHOD        GetVisible(PRBool *aVisible, PRUint32 aChromeFlag);
  NS_IMETHOD        SetVisible(PRBool aVisible, PRUint32 aChromeFlag);

protected:
  nsIBrowserWindow* mBrowser;
  void *mScriptObject;
};

// Script "menubar" object
class MenubarPropImpl : public BarPropImpl {
public:
  MenubarPropImpl();
  virtual ~MenubarPropImpl();

  NS_IMETHOD        GetVisible(PRBool *aVisible);
  NS_IMETHOD        SetVisible(PRBool aVisible);
};

// Script "toolbar" object
class ToolbarPropImpl : public BarPropImpl {
public:
  ToolbarPropImpl();
  virtual ~ToolbarPropImpl();

  NS_IMETHOD        GetVisible(PRBool *aVisible);
  NS_IMETHOD        SetVisible(PRBool aVisible);
};

// Script "locationbar" object
class LocationbarPropImpl : public BarPropImpl {
public:
  LocationbarPropImpl();
  virtual ~LocationbarPropImpl();

  NS_IMETHOD        GetVisible(PRBool *aVisible);
  NS_IMETHOD        SetVisible(PRBool aVisible);
};

// Script "personalbar" object
class PersonalbarPropImpl : public BarPropImpl {
public:
  PersonalbarPropImpl();
  virtual ~PersonalbarPropImpl();

  NS_IMETHOD        GetVisible(PRBool *aVisible);
  NS_IMETHOD        SetVisible(PRBool aVisible);
};

// Script "statusbar" object
class StatusbarPropImpl : public BarPropImpl {
public:
  StatusbarPropImpl();
  virtual ~StatusbarPropImpl();

  NS_IMETHOD        GetVisible(PRBool *aVisible);
  NS_IMETHOD        SetVisible(PRBool aVisible);
};

// Script "scrollbars" object
class ScrollbarsPropImpl : public BarPropImpl {
public:
  ScrollbarsPropImpl();
  virtual ~ScrollbarsPropImpl();

  NS_IMETHOD        GetVisible(PRBool *aVisible);
  NS_IMETHOD        SetVisible(PRBool aVisible);
};

#endif /* nsBarProps_h___ */
