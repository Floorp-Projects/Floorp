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

#include "TransactionFactory.h"
// transactions this factory knows how to build
#include "EditAggregateTxn.h"
#include "PlaceholderTxn.h"
#include "InsertTextTxn.h"
#include "DeleteTextTxn.h"
#include "CreateElementTxn.h"
#include "InsertElementTxn.h"
#include "DeleteElementTxn.h"
#include "DeleteRangeTxn.h"
#include "ChangeAttributeTxn.h"
#include "SplitElementTxn.h"
#include "JoinElementTxn.h"
#include "nsStyleSheetTxns.h"
#include "IMETextTxn.h"
#include "IMECommitTxn.h"

TransactionFactory::TransactionFactory()
{
}

TransactionFactory::~TransactionFactory()
{
}

nsresult
TransactionFactory::GetNewTransaction(REFNSIID aTxnType, EditTxn **aResult)
{
  nsresult result = NS_OK;
  *aResult = nsnull;
  if (aTxnType.Equals(InsertTextTxn::GetCID()))
    *aResult = new InsertTextTxn();
  else if (aTxnType.Equals(DeleteTextTxn::GetCID()))
    *aResult = new DeleteTextTxn();
  else if (aTxnType.Equals(CreateElementTxn::GetCID()))
    *aResult = new CreateElementTxn();
  else if (aTxnType.Equals(InsertElementTxn::GetCID()))
    *aResult = new InsertElementTxn();
  else if (aTxnType.Equals(DeleteElementTxn::GetCID()))
    *aResult = new DeleteElementTxn();
  else if (aTxnType.Equals(DeleteRangeTxn::GetCID()))
    *aResult = new DeleteRangeTxn();
  else if (aTxnType.Equals(ChangeAttributeTxn::GetCID()))
    *aResult = new ChangeAttributeTxn();
  else if (aTxnType.Equals(SplitElementTxn::GetCID()))
    *aResult = new SplitElementTxn();
  else if (aTxnType.Equals(JoinElementTxn::GetCID()))
    *aResult = new JoinElementTxn();
  else if (aTxnType.Equals(EditAggregateTxn::GetCID()))
    *aResult = new EditAggregateTxn();
  else if (aTxnType.Equals(IMETextTxn::GetCID()))
    *aResult = new IMETextTxn();
  else if (aTxnType.Equals(IMECommitTxn::GetCID()))
    *aResult = new IMECommitTxn();
  else if (aTxnType.Equals(AddStyleSheetTxn::GetCID()))
    *aResult = new AddStyleSheetTxn();
  else if (aTxnType.Equals(RemoveStyleSheetTxn::GetCID()))
    *aResult = new RemoveStyleSheetTxn();
  else if (aTxnType.Equals(PlaceholderTxn::GetCID()))
    *aResult = new PlaceholderTxn();
  else
    result = NS_ERROR_NO_INTERFACE;
  
  if (NS_SUCCEEDED(result) && nsnull==*aResult)
    result = NS_ERROR_OUT_OF_MEMORY;

  if (NS_SUCCEEDED(result))
    NS_ADDREF(*aResult);

  return result;
}


