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
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
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


#include "nsEditor.h"

#include "nsIPresShell.h"
#include "nsICSSStyleSheet.h"
#include "nsIStyleSet.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsISelectionController.h"


#include "nsStyleSheetTxns.h"



AddStyleSheetTxn::AddStyleSheetTxn()
:  EditTxn()
,  mEditor(NULL)
{
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
AddStyleSheetTxn::DoTransaction()
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
AddStyleSheetTxn::UndoTransaction()
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
AddStyleSheetTxn::RedoTransaction()
{
   return DoTransaction();
}

NS_IMETHODIMP
AddStyleSheetTxn::Merge(nsITransaction *aTransaction, PRBool *aDidMerge)
{
  // set out param default value
  if (!aDidMerge)
    return NS_ERROR_NULL_POINTER;
    
  *aDidMerge = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTxn::GetTxnDescription(nsAWritableString& aString)
{
  aString.Assign(NS_LITERAL_STRING("AddStyleSheetTxn"));
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif


RemoveStyleSheetTxn::RemoveStyleSheetTxn()
:  EditTxn()
,  mEditor(NULL)
{
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
RemoveStyleSheetTxn::DoTransaction()
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
RemoveStyleSheetTxn::UndoTransaction()
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
RemoveStyleSheetTxn::RedoTransaction()
{
   return DoTransaction();
}

NS_IMETHODIMP
RemoveStyleSheetTxn::Merge(nsITransaction *aTransaction, PRBool *aDidMerge)
{
  // set out param default value
  if (!aDidMerge)
    return NS_ERROR_NULL_POINTER;
    
  *aDidMerge = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTxn::GetTxnDescription(nsAWritableString& aString)
{
  aString.Assign(NS_LITERAL_STRING("RemoveStyleSheetTxn"));
  return NS_OK;
}
