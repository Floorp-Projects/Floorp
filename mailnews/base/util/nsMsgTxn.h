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
 * Copyright (C) 1998, 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMsgTxn_h__
#define nsMsgTxn_h__

#include "nsITransaction.h"
#include "msgCore.h"
#include "nsCOMPtr.h"
#include "nsIMsgWindow.h"

#define NS_MESSAGETRANSACTION_IID \
{ /* da621b30-1efc-11d3-abe4-00805f8ac968 */ \
    0xda621b30, 0x1efc, 0x11d3, \
	{ 0xab, 0xe4, 0x00, 0x80, 0x5f, 0x8a, 0xc9, 0x68 } }
/**
 * base class for all message undo/redo transactions.
 */
class NS_MSG_BASE nsMsgTxn : public nsITransaction
{
    NS_DECL_ISUPPORTS 

    nsMsgTxn();
    virtual ~nsMsgTxn();

    NS_IMETHOD DoTransaction(void);

    NS_IMETHOD UndoTransaction(void) = 0;

    NS_IMETHOD RedoTransaction(void) = 0;
    
    NS_IMETHOD GetIsTransient(PRBool *aIsTransient);

    NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

    NS_IMETHOD GetMsgWindow(nsIMsgWindow **msgWindow);
    NS_IMETHOD SetMsgWindow(nsIMsgWindow *msgWindow);
    NS_IMETHOD SetTransactionType(PRUint32 txnType);
    NS_IMETHOD GetTransactionType(PRUint32 *txnType);

protected:
    nsCOMPtr<nsIMsgWindow> m_msgWindow;
    PRUint32 m_txnType;
};

#endif
