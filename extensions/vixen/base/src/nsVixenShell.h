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

#ifndef __vixenShell_h__
#define __vixenShell_h__
#endif

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIVixenShell.h"

#define NS_VIXENSHELL_CID \
{ 0x5029a73a, 0x8ff6, 0x4d30, {0x99, 0xdf, 0xf5, 0x3c, 0xd4, 0x4f, 0x25, 0x42} }

#define NS_VIXENSHELL_CONTRACTID "@mozilla.org/vixen/shell;1"

class VxChangeAttributeTxn;
class VxRemoveAttributeTxn;
class VxInsertElementTxn;
class VxRemoveElementTxn;
class nsITransaction;
class nsIDOMElement;

class nsVixenShell : public nsIVixenShell
{
public:
    nsVixenShell();
    ~nsVixenShell();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIVIXENSHELL

public:
    // Execute Transactions
    NS_IMETHOD Do(nsITransaction* aTxn);

    // Create transactions for the Transaction Manager
    NS_IMETHOD CreateTxnForSetAttribute(nsIDOMElement* aElement, 
                                        const nsString& aAttribute, 
                                        const nsString& aValue, 
                                        VxChangeAttributeTxn** aTxn);
    
    NS_IMETHOD CreateTxnForRemoveAttribute(nsIDOMElement* aElement,
                                           const nsString& aAttribute,
                                           VxRemoveAttributeTxn** aTxn);

    NS_IMETHOD CreateTxnForInsertElement(nsIDOMElement* aElement,
                                         nsIDOMElement* aParent,
                                         PRInt32 aPos,
                                         VxInsertElementTxn** aTxn);

    NS_IMETHOD CreateTxnForRemoveElement(nsIDOMElement* aElement,
                                         VxRemoveElementTxn** aTxn);

};

