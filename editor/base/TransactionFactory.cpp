/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#ifndef MOZILLA_PLAINTEXT_EDITOR_ONLY
#include "SetDocTitleTxn.h"
#endif // MOZILLA_PLAINTEXT_EDITOR_ONLY

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
  else if (aTxnType.Equals(AddStyleSheetTxn::GetCID()))
    *aResult = new AddStyleSheetTxn();
  else if (aTxnType.Equals(RemoveStyleSheetTxn::GetCID()))
    *aResult = new RemoveStyleSheetTxn();
#ifndef MOZILLA_PLAINTEXT_EDITOR_ONLY
  else if (aTxnType.Equals(SetDocTitleTxn::GetCID()))
    *aResult = new SetDocTitleTxn();
#endif // MOZILLA_PLAINTEXT_EDITOR_ONLY
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


