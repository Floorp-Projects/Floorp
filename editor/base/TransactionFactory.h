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

#ifndef TransactionFactory_h__
#define TransactionFactory_h__

#include "nsISupports.h"

class EditTxn;

/**
 * This class instantiates and optionally recycles edit transactions
 * A recycler would be a separate static object, since this class does not get instantiated
 */
class TransactionFactory
{
protected:
  TransactionFactory();
  virtual ~TransactionFactory();

public:
  /** return a transaction object of aTxnType, refcounted 
    * @return NS_ERROR_NO_INTERFACE if aTxnType is unknown, 
    *         NS_ERROR_OUT_OF_MEMORY if the allocations fails.
    */
  static nsresult GetNewTransaction(REFNSIID aTxnType, EditTxn **aResult);

};

#endif
