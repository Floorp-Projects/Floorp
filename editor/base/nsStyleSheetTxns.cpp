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


#include "nsEditor.h"

#include "nsIPresShell.h"
#include "nsICSSStyleSheet.h"
#include "nsIStyleSet.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"


#include "nsStyleSheetTxns.h"



AddStyleSheetTxn::AddStyleSheetTxn()
:  EditTxn()
,  mEditor(NULL)
{
  SetTransactionDescriptionID( kTransactionID );
  /* log description initialized in parent constructor */
}

AddStyleSheetTxn::~AddStyleSheetTxn()
{
}

NS_IMETHODIMP
AddStyleSheetTxn::Init(nsIEditor *aEditor, nsICSSStyleSheet *aSheet)
{
  if (!aEditor)
    return NS_ERROR_INVALID_ARG;

  if (!aSheet)
    return NS_ERROR_INVALID_ARG;

  mEditor = aEditor;
  mSheet = do_QueryInterface(aSheet);
  
  return NS_OK;
}


NS_IMETHODIMP
AddStyleSheetTxn::Do()
{
  if (!mEditor || !mSheet)
    return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsISelectionController> selCon;
  mEditor->GetSelectionController(getter_AddRefs(selCon));
  if (!selCon)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIPresShell> presShell;
  presShell = do_QueryInterface(selCon);

  if (!presShell)
    return NS_ERROR_UNEXPECTED;
  
  nsCOMPtr<nsIStyleSet> styleSet;
  nsresult rv = presShell->GetStyleSet(getter_AddRefs(styleSet));

  if (NS_SUCCEEDED(rv) && styleSet)
  {
    nsCOMPtr<nsIStyleSheet> styleSheet     = do_QueryInterface(mSheet);

    if (styleSheet)
    {
      nsCOMPtr<nsIDocument> document;
      rv = presShell->GetDocument(getter_AddRefs(document));

      if (NS_SUCCEEDED(rv) && document)
        document->AddStyleSheet(styleSheet);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
AddStyleSheetTxn::Undo()
{
  if (!mEditor || !mSheet)
    return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsISelectionController> selCon;
  mEditor->GetSelectionController(getter_AddRefs(selCon));
  if (!selCon)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIPresShell> presShell;
  presShell = do_QueryInterface(selCon);
  if (!presShell)
    return NS_ERROR_UNEXPECTED;
  
  nsCOMPtr<nsIStyleSet> styleSet;
  nsresult rv = presShell->GetStyleSet(getter_AddRefs(styleSet));

  if (NS_SUCCEEDED(rv) && styleSet)
  {
    styleSet->RemoveDocStyleSheet(mSheet);

    nsCOMPtr<nsIDocumentObserver> observer = do_QueryInterface(presShell);
    nsCOMPtr<nsIStyleSheet> styleSheet     = do_QueryInterface(mSheet);
    nsCOMPtr<nsIDocument> document;

    rv = presShell->GetDocument(getter_AddRefs(document));

    if (NS_SUCCEEDED(rv) && document && observer && styleSheet)
      rv = observer->StyleSheetRemoved(document, styleSheet);
  }
  
  return rv;
}

NS_IMETHODIMP
AddStyleSheetTxn::Redo()
{
   return Do();
}

NS_IMETHODIMP
AddStyleSheetTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  // set out param default value
  if (!aDidMerge)
    return NS_ERROR_NULL_POINTER;
    
  *aDidMerge = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTxn::GetUndoString(nsString *aString)
{
  if (aString)
  {
    aString->AssignWithConversion("Remove Style Sheet");
  }
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTxn::GetRedoString(nsString *aString)
{
  if (aString)
  {
    aString->AssignWithConversion("Add Style Sheet");
  }
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif


RemoveStyleSheetTxn::RemoveStyleSheetTxn()
:  EditTxn()
,  mEditor(NULL)
{
  SetTransactionDescriptionID( kTransactionID );
}

RemoveStyleSheetTxn::~RemoveStyleSheetTxn()
{
}

NS_IMETHODIMP
RemoveStyleSheetTxn::Init(nsIEditor *aEditor, nsICSSStyleSheet *aSheet)
{
  if (!aEditor)
    return NS_ERROR_INVALID_ARG;

  if (!aSheet)
    return NS_ERROR_INVALID_ARG;

  mEditor = aEditor;
  mSheet = do_QueryInterface(aSheet);
  
  return NS_OK;
}


NS_IMETHODIMP
RemoveStyleSheetTxn::Do()
{
  if (!mEditor || !mSheet)
    return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsISelectionController> selCon;
  mEditor->GetSelectionController(getter_AddRefs(selCon));
  if (!selCon)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIPresShell> presShell;
  presShell = do_QueryInterface(selCon);
  if (!presShell)
    return NS_ERROR_UNEXPECTED;
  
  nsCOMPtr<nsIStyleSet> styleSet;
  nsresult rv = presShell->GetStyleSet(getter_AddRefs(styleSet));

  if (NS_SUCCEEDED(rv) && styleSet)
  {
    styleSet->RemoveDocStyleSheet(mSheet);

    nsCOMPtr<nsIDocumentObserver> observer = do_QueryInterface(presShell);
    nsCOMPtr<nsIStyleSheet> styleSheet     = do_QueryInterface(mSheet);
    nsCOMPtr<nsIDocument> document;

    rv = presShell->GetDocument(getter_AddRefs(document));

    if (NS_SUCCEEDED(rv) && document && observer && styleSheet)
      rv = observer->StyleSheetRemoved(document, styleSheet);
  }
  
  return rv;
}

NS_IMETHODIMP
RemoveStyleSheetTxn::Undo()
{
  if (!mEditor || !mSheet)
    return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsISelectionController> selCon;
  mEditor->GetSelectionController(getter_AddRefs(selCon));
  if (!selCon)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIPresShell> presShell;
  presShell = do_QueryInterface(selCon);
  if (!presShell)
    return NS_ERROR_UNEXPECTED;
  
  nsCOMPtr<nsIStyleSet> styleSet;
  nsresult rv = presShell->GetStyleSet(getter_AddRefs(styleSet));

  if (NS_SUCCEEDED(rv) && styleSet)
  {
    nsCOMPtr<nsIStyleSheet> styleSheet     = do_QueryInterface(mSheet);

    if (styleSheet)
    {
      nsCOMPtr<nsIDocument> document;
      rv = presShell->GetDocument(getter_AddRefs(document));

      if (NS_SUCCEEDED(rv) && document)
        document->AddStyleSheet(styleSheet);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
RemoveStyleSheetTxn::Redo()
{
   return Do();
}

NS_IMETHODIMP
RemoveStyleSheetTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  // set out param default value
  if (!aDidMerge)
    return NS_ERROR_NULL_POINTER;
    
  *aDidMerge = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTxn::GetUndoString(nsString *aString)
{
  if (aString)
  {
    aString->AssignWithConversion("Add Style Sheet");
  }
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTxn::GetRedoString(nsString *aString)
{
  if (aString)
  {
    aString->AssignWithConversion("Remove Style Sheet");
  }
  return NS_OK;
}
