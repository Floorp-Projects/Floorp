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

#ifndef nsIDOMSelectionListener_h__
#define nsIDOMSelectionListener_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMDocument;
class nsIDOMSelection;

#define NS_IDOMSELECTIONLISTENER_IID \
 { 0xa6cf90e2, 0x15b3, 0x11d2,             \
        { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }		

class nsIDOMSelectionListener : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMSELECTIONLISTENER_IID; return iid; }
  enum {
    NO_REASON = 0,
    DRAG_REASON = 1,
    MOUSEDOWN_REASON = 2,
    MOUSEUP_REASON = 4,
    KEYPRESS_REASON = 8,
    SELECTALL_REASON = 16
  };

  NS_IMETHOD    NotifySelectionChanged(nsIDOMDocument* aDoc, nsIDOMSelection* aSel, PRInt16 aReason)=0;
};


#define NS_DECL_IDOMSELECTIONLISTENER   \
  NS_IMETHOD    NotifySelectionChanged(nsIDOMDocument* aDoc, nsIDOMSelection* aSel, PRInt16 aReason);  \



#define NS_FORWARD_IDOMSELECTIONLISTENER(_to)  \
  NS_IMETHOD    NotifySelectionChanged(nsIDOMDocument* aDoc, nsIDOMSelection* aSel, PRInt16 aReason) { return _to NotifySelectionChanged(aDoc, aSel, aReason); }  \


extern "C" NS_DOM nsresult NS_InitSelectionListenerClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptSelectionListener(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMSelectionListener_h__
