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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsSOAPFault_h__
#define nsSOAPFault_h__

#include "nsString.h"
#include "nsISOAPFault.h"
#include "nsISecurityCheckedComponent.h"
#include "nsIDOMElement.h"
#include "nsCOMPtr.h"

class nsSOAPFault : public nsISOAPFault,
		    public nsISecurityCheckedComponent
{
public:
  nsSOAPFault(nsIDOMElement* aElement);
  virtual ~nsSOAPFault();

  NS_DECL_ISUPPORTS

  // nsISOAPFault
  NS_DECL_NSISOAPFAULT

  // nsISecurityCheckedComponent
  NS_DECL_NSISECURITYCHECKEDCOMPONENT

protected:
  nsCOMPtr<nsIDOMElement> mFaultElement;
};

#define NS_SOAPFAULT_CID                   \
 { /* cb8cd1ae-1dd1-11b2-9954-e393ae604c08 */      \
 0xcb8cd1ae, 0x1dd2, 0x11b2,                       \
 {0x99, 0x54, 0xe3, 0x93, 0xae, 0x60, 0x4c, 0x08} }
#define NS_SOAPFAULT_CONTRACTID "@mozilla.org/xmlextras/soap/fault;1"
#endif
