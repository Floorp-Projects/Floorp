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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMDocumentView_h__
#define nsIDOMDocumentView_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMAbstractView;

#define NS_IDOMDOCUMENTVIEW_IID \
 { 0x1acdb2ba, 0x1dd2, 0x11b2, \
  { 0x95, 0xbc, 0x95, 0x42, 0x49, 0x5d, 0x25, 0x69 } } 

class NS_NO_VTABLE nsIDOMDocumentView : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMDOCUMENTVIEW_IID; return iid; }

  NS_IMETHOD    GetDefaultView(nsIDOMAbstractView** aDefaultView)=0;
};


#define NS_DECL_IDOMDOCUMENTVIEW   \
  NS_IMETHOD    GetDefaultView(nsIDOMAbstractView** aDefaultView);  \



#define NS_FORWARD_IDOMDOCUMENTVIEW(_to)  \
  NS_IMETHOD    GetDefaultView(nsIDOMAbstractView** aDefaultView) { return _to GetDefaultView(aDefaultView); } \


#endif // nsIDOMDocumentView_h__
