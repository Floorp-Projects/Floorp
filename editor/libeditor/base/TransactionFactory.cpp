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
#include "InsertTextTxn.h"
#include "DeleteTextTxn.h"
#include "CreateElementTxn.h"
#include "DeleteElementTxn.h"
#include "DeleteRangeTxn.h"
#include "ChangeAttributeTxn.h"
#include "SplitElementTxn.h"
#include "JoinElementTxn.h"

static NS_DEFINE_IID(kInsertTextTxnIID,     INSERT_TEXT_TXN_IID);
static NS_DEFINE_IID(kDeleteTextTxnIID,     DELETE_TEXT_TXN_IID);
static NS_DEFINE_IID(kCreateElementTxnIID,  CREATE_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kDeleteElementTxnIID,  DELETE_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kDeleteRangeTxnIID,    DELETE_RANGE_TXN_IID);
static NS_DEFINE_IID(kChangeAttributeTxnIID,CHANGE_ATTRIBUTE_TXN_IID);
static NS_DEFINE_IID(kSplitElementTxnIID,   SPLIT_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kJoinElementTxnIID,    JOIN_ELEMENT_TXN_IID);

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
  if (aTxnType.Equals(kInsertTextTxnIID))
    *aResult = new InsertTextTxn();
  else if (aTxnType.Equals(kDeleteTextTxnIID))
    *aResult = new DeleteTextTxn();
  else if (aTxnType.Equals(kCreateElementTxnIID))
    *aResult = new CreateElementTxn();
  else if (aTxnType.Equals(kDeleteElementTxnIID))
    *aResult = new DeleteElementTxn();
  else if (aTxnType.Equals(kDeleteRangeTxnIID))
    *aResult = new DeleteRangeTxn();
  else if (aTxnType.Equals(kChangeAttributeTxnIID))
    *aResult = new ChangeAttributeTxn();
  else if (aTxnType.Equals(kSplitElementTxnIID))
    *aResult = new SplitElementTxn();
  else if (aTxnType.Equals(kJoinElementTxnIID))
    *aResult = new JoinElementTxn();
  
  if (nsnull==*aResult)
    result = NS_ERROR_INVALID_ARG;

  return result;
}


