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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMAbstractView_h__
#define nsIDOMAbstractView_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMDocument;

#define NS_IDOMABSTRACTVIEW_IID \
 { 0xf51ebade, 0x8b1a, 0x11d3, \
  { 0xaa, 0xe7, 0x00, 0x10, 0x83, 0x01, 0x23, 0xb4 } } 

class nsIDOMAbstractView : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMABSTRACTVIEW_IID; return iid; }

  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument)=0;
};


#define NS_DECL_IDOMABSTRACTVIEW   \
  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument);  \



#define NS_FORWARD_IDOMABSTRACTVIEW(_to)  \
  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument) { return _to GetDocument(aDocument); } \


#endif // nsIDOMAbstractView_h__
