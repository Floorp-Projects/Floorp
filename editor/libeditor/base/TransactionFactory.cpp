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
#include "InsertTableTxn.h"
#include "InsertTableCellTxn.h"
#include "InsertTableColumnTxn.h"
#include "InsertTableRowTxn.h"
#include "DeleteTableTxn.h"
#include "DeleteTableCellTxn.h"
#include "DeleteTableColumnTxn.h"
#include "DeleteTableRowTxn.h"
#include "JoinTableCellsTxn.h"

static NS_DEFINE_IID(kEditAggregateTxnIID,  EDIT_AGGREGATE_TXN_IID);
static NS_DEFINE_IID(kPlaceholderTxnIID,    PLACEHOLDER_TXN_IID);
static NS_DEFINE_IID(kInsertTextTxnIID,     INSERT_TEXT_TXN_IID);
static NS_DEFINE_IID(kDeleteTextTxnIID,     DELETE_TEXT_TXN_IID);
static NS_DEFINE_IID(kCreateElementTxnIID,  CREATE_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kInsertElementTxnIID,  INSERT_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kDeleteElementTxnIID,  DELETE_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kDeleteRangeTxnIID,    DELETE_RANGE_TXN_IID);
static NS_DEFINE_IID(kChangeAttributeTxnIID,CHANGE_ATTRIBUTE_TXN_IID);
static NS_DEFINE_IID(kSplitElementTxnIID,   SPLIT_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kJoinElementTxnIID,    JOIN_ELEMENT_TXN_IID);
static NS_DEFINE_IID(kInsertTableTxnIID,       INSERT_TABLE_TXN_IID);
static NS_DEFINE_IID(kInsertTableCellTxnIID,   INSERT_CELL_TXN_IID);
static NS_DEFINE_IID(kInsertTableColumnTxnIID, INSERT_COLUMN_TXN_IID);
static NS_DEFINE_IID(kInsertTableRowTxnIID,    INSERT_ROW_TXN_IID);
static NS_DEFINE_IID(kDeleteTableTxnIID,       DELETE_TABLE_TXN_IID);
static NS_DEFINE_IID(kDeleteTableCellTxnIID,   DELETE_CELL_TXN_IID);
static NS_DEFINE_IID(kDeleteTableColumnTxnIID, DELETE_COLUMN_TXN_IID);
static NS_DEFINE_IID(kDeleteTableRowTxnIID,    DELETE_ROW_TXN_IID);
static NS_DEFINE_IID(kJoinTableCellsTxnIID,    JOIN_CELLS_TXN_IID);

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
  else if (aTxnType.Equals(kInsertElementTxnIID))
    *aResult = new InsertElementTxn();
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
  else if (aTxnType.Equals(kEditAggregateTxnIID))
    *aResult = new EditAggregateTxn();
  else if (aTxnType.Equals(kPlaceholderTxnIID))
    *aResult = new PlaceholderTxn();
  else
    result = NS_ERROR_NO_INTERFACE;
  
  if (NS_SUCCEEDED(result) && nsnull==*aResult)
    result = NS_ERROR_OUT_OF_MEMORY;

  if (NS_SUCCEEDED(result))
    NS_ADDREF(*aResult);

  return result;
}


