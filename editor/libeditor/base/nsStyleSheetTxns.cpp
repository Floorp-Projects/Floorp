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


#include "nsEditor.h"

#include "nsIPresShell.h"
#include "nsICSSStyleSheet.h"
#include "nsIStyleSet.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"


#include "nsStyleSheetTxns.h"



AddStyleSheetTxn::AddStyleSheetTxn()
:  mEditor(NULL)
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
AddStyleSheetTxn::Do()
{
  if (!mEditor || !mSheet)
    return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsIPresShell> presShell;
  mEditor->GetPresShell(getter_AddRefs(presShell));
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
  
  nsCOMPtr<nsIPresShell> presShell;
  mEditor->GetPresShell(getter_AddRefs(presShell));
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
    *aString="Remove Style Sheet";
  }
  return NS_OK;
}

NS_IMETHODIMP
AddStyleSheetTxn::GetRedoString(nsString *aString)
{
  if (aString)
  {
    *aString="Add Style Sheet";
  }
  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#endif


RemoveStyleSheetTxn::RemoveStyleSheetTxn()
:  mEditor(NULL)
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
RemoveStyleSheetTxn::Do()
{
  if (!mEditor || !mSheet)
    return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsIPresShell> presShell;
  mEditor->GetPresShell(getter_AddRefs(presShell));
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
  
  nsCOMPtr<nsIPresShell> presShell;
  mEditor->GetPresShell(getter_AddRefs(presShell));
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
    *aString="Add Style Sheet";
  }
  return NS_OK;
}

NS_IMETHODIMP
RemoveStyleSheetTxn::GetRedoString(nsString *aString)
{
  if (aString)
  {
    *aString="Remove Style Sheet";
  }
  return NS_OK;
}
