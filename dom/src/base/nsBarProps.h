/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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

class nsIWebBrowserChrome;

// Script "BarProp" object
class BarPropImpl : public nsIDOMBarProp {
public:
   BarPropImpl();
   virtual ~BarPropImpl();

   NS_DECL_ISUPPORTS

   NS_IMETHOD SetWebBrowserChrome(nsIWebBrowserChrome* aBrowserChrome);

   NS_IMETHOD GetVisibleByFlag(PRBool *aVisible, PRUint32 aChromeFlag);
   NS_IMETHOD SetVisibleByFlag(PRBool aVisible, PRUint32 aChromeFlag);

protected:
   // Weak Reference
   nsIWebBrowserChrome* mBrowserChrome;
};

// Script "menubar" object
class MenubarPropImpl : public BarPropImpl {
public:
   MenubarPropImpl();
   virtual ~MenubarPropImpl();

   NS_DECL_NSIDOMBARPROP
};

// Script "toolbar" object
class ToolbarPropImpl : public BarPropImpl {
public:
   ToolbarPropImpl();
   virtual ~ToolbarPropImpl();

   NS_DECL_NSIDOMBARPROP
};

// Script "locationbar" object
class LocationbarPropImpl : public BarPropImpl {
public:
   LocationbarPropImpl();
   virtual ~LocationbarPropImpl();

   NS_DECL_NSIDOMBARPROP
};

// Script "personalbar" object
class PersonalbarPropImpl : public BarPropImpl {
public:
   PersonalbarPropImpl();
   virtual ~PersonalbarPropImpl();

   NS_DECL_NSIDOMBARPROP
};

// Script "statusbar" object
class StatusbarPropImpl : public BarPropImpl {
public:
   StatusbarPropImpl();
   virtual ~StatusbarPropImpl();

   NS_DECL_NSIDOMBARPROP
};

// Script "scrollbars" object
class ScrollbarsPropImpl : public BarPropImpl {
public:
   ScrollbarsPropImpl();
   virtual ~ScrollbarsPropImpl();

   NS_DECL_NSIDOMBARPROP
};

#endif /* nsBarProps_h___ */
