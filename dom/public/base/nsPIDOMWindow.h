/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#ifndef nsPIDOMWindow_h__
#define nsPIDOMWindow_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMLocation.h"

class nsIDocShell;
class nsIDOMWindow;

#define NS_PIDOMWINDOW_IID \
{ 0x3aa80781, 0x7e6a, 0x11d3, { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }


class nsPIDOMWindow : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_PIDOMWINDOW_IID; return iid; }

  NS_IMETHOD GetPrivateParent(nsPIDOMWindow** aResult)=0;
  NS_IMETHOD GetPrivateRoot(nsIDOMWindow** aResult)=0;

  NS_IMETHOD GetLocation(nsIDOMLocation** aLocation) = 0;
//  NS_IMETHOD GetDocShell(nsIDocShell **aDocShell) =0;// XXX This may be temporary - rods

  NS_IMETHOD SetXPConnectObject(const PRUnichar* aProperty, nsISupports* aXPConnectObj)=0;
  NS_IMETHOD GetXPConnectObject(const PRUnichar* aProperty, nsISupports** aXPConnectObj)=0;
  
  // This is private because activate/deactivate events are not part of the DOM spec.
  NS_IMETHOD Activate() = 0;
  NS_IMETHOD Deactivate() = 0;
};

#endif // nsPIDOMWindow_h__
