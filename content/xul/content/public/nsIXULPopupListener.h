/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIXULPopupListener_h__
#define nsIXULPopupListener_h__

// {2C453161-0942-11d3-BF87-00105A1B0627}
#define NS_IXULPOPUPLISTENER_IID \
{ 0x2c453161, 0x942, 0x11d3, { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }

class nsIDOMElement;

typedef enum {
    eXULPopupType_popup,
    eXULPopupType_context,
    eXULPopupType_tooltip,
    eXULPopupType_blur
} XULPopupType;

class nsIXULPopupListener: public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IXULPOPUPLISTENER_IID; return iid; }

    NS_IMETHOD Init(nsIDOMElement* anElement, const XULPopupType& aPopupType) = 0;
};

extern nsresult
NS_NewXULPopupListener(nsIXULPopupListener** result);

#endif // nsIXULPopupListener_h__
