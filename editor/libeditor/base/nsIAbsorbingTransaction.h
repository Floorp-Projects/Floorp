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

#ifndef nsIAbsorbingTransaction_h__
#define nsIAbsorbingTransaction_h__

#include "nsISupports.h"

/*
Transaction interface to outside world
*/

#define NS_IABSORBINGTRANSACTION_IID \
{ /* a6cf910b-15b3-11d2-932e-00805f8add32 */ \
    0xa6cf910b, 0x15b3, 0x11d2,              \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsIAtom.h"
#include "nsIDOMNode.h"

/**
 * A transaction interface mixin - for transactions that can support. 
 * the placeholder absorbtion idiom. 
 */
class nsIAbsorbingTransaction  : public nsISupports{
public:

  static const nsIID& GetIID() { static nsIID iid = NS_IABSORBINGTRANSACTION_IID; return iid; }

  NS_IMETHOD Init(nsWeakPtr aPresShellWeak, nsIAtom *aName, nsIDOMNode *aStartNode, PRInt32 aStartOffset)=0;
  
  NS_IMETHOD EndPlaceHolderBatch()=0;
  
  NS_IMETHOD GetTxnName(nsIAtom **aName)=0;

  NS_IMETHOD GetStartNodeAndOffset(nsCOMPtr<nsIDOMNode> *aTxnStartNode, PRInt32 *aTxnStartOffset)=0;

  NS_IMETHOD ForwardEndBatchTo(nsIAbsorbingTransaction *aForwardingAddress)=0;
  
  NS_IMETHOD Commit()=0;
};

#endif // nsIAbsorbingTransaction_h__

