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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef IMECommitTxn_h__
#define IMECommitTxn_h__

#include "EditTxn.h"
#include "nsIDOMCharacterData.h"
#include "nsCOMPtr.h"

// {9C4994A1-281C-11d3-9EA3-0060089FE59B}
#define IME_COMMIT_TXN_CID					\
{ 0x9c4994a1, 0x281c, 0x11d3, 				\
{ 0x9e, 0xa3, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b }}


/**
  * A transaction representing an IME commit operation
  */
class IMECommitTxn : public EditTxn
{
public:
  static const nsIID& GetCID() { static nsIID iid = IME_COMMIT_TXN_CID; return iid; }

  virtual ~IMECommitTxn();

  static nsIAtom *gIMECommitTxnName;
	
  /** initialize the transaction
    */
  NS_IMETHOD Init(void);

private:
	
	IMECommitTxn();

public:
	
  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

// nsISupports declarations

  // override QueryInterface to handle IMECommitTxn request
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  /** must be called before any IMECommitTxn is instantiated */
  static nsresult ClassInit();

  /** must be called once we are guaranteed all IMECommitTxn have completed */
  static nsresult ClassShutdown();

protected:

  friend class TransactionFactory;

  friend class nsDerivedSafe<IMECommitTxn>; // work around for a compiler bug

};

#endif
