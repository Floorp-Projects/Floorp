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

#ifndef nsIAbsorbingTransaction_h__
#define nsIAbsorbingTransaction_h__

#include "nsISupports.h"

/*
Transaction interface to outside world
*/

#define NS_IABSORBINGTRANSACTION_IID \
{ /* a6cf9116-15b3-11d2-932e-00805f8add32 */ \
    0xa6cf9116, \
    0x15b3, \
    0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

class nsSelectionState;
class nsIEditor;
class nsIAtom;

/**
 * A transaction interface mixin - for transactions that can support. 
 * the placeholder absorbtion idiom. 
 */
class nsIAbsorbingTransaction  : public nsISupports{
public:

  static const nsIID& GetIID() { static nsIID iid = NS_IABSORBINGTRANSACTION_IID; return iid; }

  NS_IMETHOD Init(nsIAtom *aName, nsSelectionState *aSelState, nsIEditor *aEditor)=0;
  
  NS_IMETHOD EndPlaceHolderBatch()=0;
  
  NS_IMETHOD GetTxnName(nsIAtom **aName)=0;

  NS_IMETHOD StartSelectionEquals(nsSelectionState *aSelState, PRBool *aResult)=0;

  NS_IMETHOD ForwardEndBatchTo(nsIAbsorbingTransaction *aForwardingAddress)=0;
  
  NS_IMETHOD Commit()=0;
};

#endif // nsIAbsorbingTransaction_h__

