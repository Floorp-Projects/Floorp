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

#ifndef AggregatePlaceholderTxn_h__
#define AggregatePlaceholderTxn_h__

#include "EditAggregateTxn.h"

#define PLACEHOLDER_TXN_IID \
{/* {0CE9FB00-D9D1-11d2-86DE-000064657374} */ \
0x0CE9FB00, 0xD9D1, 0x11d2, \
{0x86, 0xde, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * An aggregate transaction that knows how to absorb all subsequent
 * transactions with the same name.  This transaction does not "Do" anything.
 * But it absorbs other transactions via merge, and can undo/redo the
 * transactions it has absorbed.
 */
class PlaceholderTxn : public EditAggregateTxn
{
public:

private:
  PlaceholderTxn();

public:

  virtual ~PlaceholderTxn();

  NS_IMETHOD Do(void);

  NS_IMETHOD Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  NS_IMETHOD SetAbsorb(PRBool aAbsorb);

  friend class TransactionFactory;

protected:

  PRBool mAbsorb;

};

inline NS_IMETHODIMP PlaceholderTxn::SetAbsorb(PRBool aAbsorb)
{
  mAbsorb = aAbsorb;
  return NS_OK;
};


#endif
