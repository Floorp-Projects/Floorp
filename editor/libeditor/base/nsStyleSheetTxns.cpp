/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsEditor.h"

#include "nsIPresShell.h"
#include "nsICSSStyleSheet.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsISelectionController.h"


#include "nsStyleSheetTxns.h"

static void
AddStyleSheet(nsIEditor* aEditor, nsIStyleSheet* aSheet)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  aEditor->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (doc) {
    doc->BeginUpdate(UPDATE_STYLE);
    doc->AddStyleSheet(aSheet);
    doc->EndUpdate(UPDATE_STYLE);
  }
}

static void
RemoveStyleSheet(nsIEditor *aEditor, nsIStyleSheet *aSheet)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  aEditor->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (doc) {
    doc->BeginUpdate(UPDATE_STYLE);
    doc->RemoveStyleSheet(aSheet);
    doc->EndUpdate(UPDATE_STYLE);
  }
}

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
  
  AddStyleSheet(mEditor, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTxn::UndoTransaction()
{
  if (!mEditor || !mSheet)
    return NS_ERROR_NOT_INITIALIZED;
  
  RemoveStyleSheet(mEditor, mSheet);
  return NS_OK;
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
AddStyleSheetTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("AddStyleSheetTxn");
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

  RemoveStyleSheet(mEditor, mSheet);
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTxn::UndoTransaction()
{
  if (!mEditor || !mSheet)
    return NS_ERROR_NOT_INITIALIZED;

  AddStyleSheet(mEditor, mSheet);
  return NS_OK;
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
RemoveStyleSheetTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("RemoveStyleSheetTxn");
  return NS_OK;
}
