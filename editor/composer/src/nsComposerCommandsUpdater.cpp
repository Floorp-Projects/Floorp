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
 *   Simon Fraser <sfraser@netscape.com>
 */


#include "nsComposerCommandsUpdater.h"

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsISelection.h"
#include "nsIScriptGlobalObject.h"

#include "nsIInterfaceRequestorUtils.h"

#include "nsICommandManager.h"
#include "nsPICommandUpdater.h"

#include "nsIEditor.h"
#include "nsIDocShell.h"
#include "nsITransactionManager.h"

nsComposerCommandsUpdater::nsComposerCommandsUpdater()
:  mEditor(nsnull)
,	 mDocShell(nsnull)
,  mDirtyState(eStateUninitialized)
,  mSelectionCollapsed(eStateUninitialized)
,  mFirstDoOfFirstUndo(PR_TRUE)
{
	NS_INIT_REFCNT();
}

nsComposerCommandsUpdater::~nsComposerCommandsUpdater()
{
}

NS_IMPL_ISUPPORTS4(nsComposerCommandsUpdater, nsISelectionListener, nsIDocumentStateListener, nsITransactionListener, nsITimerCallback);

#if 0
#pragma mark -
#endif

NS_IMETHODIMP
nsComposerCommandsUpdater::NotifyDocumentCreated()
{
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::NotifyDocumentWillBeDestroyed()
{
  // cancel any outstanding udpate timer
  if (mUpdateTimer)
    mUpdateTimer->Cancel();
  
  return NS_OK;
}


NS_IMETHODIMP
nsComposerCommandsUpdater::NotifyDocumentStateChanged(PRBool aNowDirty)
{
  // update document modified. We should have some other notifications for this too.
  return UpdateDirtyState(aNowDirty);
}

NS_IMETHODIMP
nsComposerCommandsUpdater::NotifySelectionChanged(nsIDOMDocument *, nsISelection *, short)
{
  return PrimeUpdateTimer();
}

#if 0
#pragma mark -
#endif

NS_IMETHODIMP nsComposerCommandsUpdater::WillDo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::DidDo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, nsresult aDoResult)
{
  // only need to update if the status of the Undo menu item changes.
  PRInt32 undoCount;
  aManager->GetNumberOfUndoItems(&undoCount);
  if (undoCount == 1)
  {
    if (mFirstDoOfFirstUndo)
      CallUpdateCommands(NS_ConvertASCIItoUCS2("undo"));
    mFirstDoOfFirstUndo = PR_FALSE;
  }
	
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::WillUndo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::DidUndo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, nsresult aUndoResult)
{
  PRInt32 undoCount;
  aManager->GetNumberOfUndoItems(&undoCount);
  if (undoCount == 0)
    mFirstDoOfFirstUndo = PR_TRUE;    // reset the state for the next do

  CallUpdateCommands(NS_ConvertASCIItoUCS2("undo"));
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::WillRedo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::DidRedo(nsITransactionManager *aManager,  
  nsITransaction *aTransaction, nsresult aRedoResult)
{
  CallUpdateCommands(NS_ConvertASCIItoUCS2("undo"));
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::WillBeginBatch(nsITransactionManager *aManager, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::DidBeginBatch(nsITransactionManager *aManager, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::WillEndBatch(nsITransactionManager *aManager, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::DidEndBatch(nsITransactionManager *aManager, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::WillMerge(nsITransactionManager *aManager,
        nsITransaction *aTopTransaction, nsITransaction *aTransactionToMerge, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsComposerCommandsUpdater::DidMerge(nsITransactionManager *aManager,
  nsITransaction *aTopTransaction, nsITransaction *aTransactionToMerge,
                    PRBool aDidMerge, nsresult aMergeResult)
{
  return NS_OK;
}

#if 0
#pragma mark -
#endif

nsresult
nsComposerCommandsUpdater::SetEditor(nsIEditor* aEditor)
{
  mEditor = aEditor;		// no addreffing here
  return NS_OK;
}

nsresult
nsComposerCommandsUpdater::PrimeUpdateTimer()
{
  nsresult rv = NS_OK;
    
  if (mUpdateTimer)
  {
    // i'd love to be able to just call SetDelay on the existing timer, but
    // i think i have to tear it down and make a new one.
    mUpdateTimer->Cancel();
    mUpdateTimer = NULL;      // free it
  }
  
  mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) return rv;

  const PRUint32 kUpdateTimerDelay = 150;
  return mUpdateTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this), kUpdateTimerDelay);
}


void nsComposerCommandsUpdater::TimerCallback()
{
  // if the selection state has changed, update stuff
  PRBool isCollapsed = SelectionIsCollapsed();
  if (isCollapsed != mSelectionCollapsed)
  {
    CallUpdateCommands(NS_ConvertASCIItoUCS2("select"));
    mSelectionCollapsed = isCollapsed;
  }
  
  CallUpdateCommands(NS_ConvertASCIItoUCS2("style"));
}

nsresult
nsComposerCommandsUpdater::UpdateDirtyState(PRBool aNowDirty)
{
  if (mDirtyState != aNowDirty)
  {
    CallUpdateCommands(NS_ConvertASCIItoUCS2("save"));

    mDirtyState = aNowDirty;
  }
  
  return NS_OK;  
}

nsresult
nsComposerCommandsUpdater::CallUpdateCommands(const nsAReadableString& aCommand)
{
  if (!mDocShell)
  {
    nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
    if (!editor) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMDocument> domDoc;
    editor->GetDocument(getter_AddRefs(domDoc));
    if (!domDoc) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocument> theDoc = do_QueryInterface(domDoc);
    if (!theDoc) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
    theDoc->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));

		nsCOMPtr<nsIDocShell>	docShell;
		scriptGlobalObject->GetDocShell(getter_AddRefs(docShell));
		mDocShell = docShell.get();		
  }

  if (!mDocShell) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsICommandManager>  	commandManager = do_GetInterface(mDocShell);
  nsCOMPtr<nsPICommandUpdater>	commandUpdater = do_QueryInterface(commandManager);
  if (!commandUpdater) return NS_ERROR_FAILURE;
  
  commandUpdater->CommandStatusChanged(NS_LITERAL_STRING("cmd_bold"));
  commandUpdater->CommandStatusChanged(NS_LITERAL_STRING("cmd_italic"));
  commandUpdater->CommandStatusChanged(NS_LITERAL_STRING("cmd_underline"));
  
  return NS_OK;  
}

PRBool
nsComposerCommandsUpdater::SelectionIsCollapsed()
{
  nsresult rv;
  // we don't care too much about failures here.
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISelection> domSelection;
    rv = editor->GetSelection(getter_AddRefs(domSelection));
    if (NS_SUCCEEDED(rv))
    {    
      PRBool selectionCollapsed = PR_FALSE;
      rv = domSelection->GetIsCollapsed(&selectionCollapsed);
      return selectionCollapsed;
    }
  }
  return PR_FALSE;
}

#if 0
#pragma mark -
#endif

void
nsComposerCommandsUpdater::Notify(nsITimer *timer)
{
  NS_ASSERTION(timer == mUpdateTimer.get(), "Hey, this ain't my timer!");
  mUpdateTimer = NULL;    // release my hold  
  TimerCallback();
}

#if 0
#pragma mark -
#endif


nsresult NS_NewComposerCommandsUpdater(nsIEditor* aEditor, nsISelectionListener** aInstancePtrResult)
{
  nsComposerCommandsUpdater* newThang = new nsComposerCommandsUpdater;
  if (!newThang)
    return NS_ERROR_OUT_OF_MEMORY;

  *aInstancePtrResult = nsnull;
  nsresult rv = newThang->SetEditor(aEditor);
  if (NS_FAILED(rv))
  {
    delete newThang;
    return rv;
  }
      
  return newThang->QueryInterface(NS_GET_IID(nsISelectionListener), (void **)aInstancePtrResult);
}
