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
 *   Simon Fraser   <sfraser@netscape.com>
 *   Michael Judge  <mjudge@netscape.com>
 *   Charles Manske <cmanske@netscape.com>
 */

#include "nsIDOMWindow.h"
#include "nsComposerCommandsUpdater.h"
#include "nsIServiceManager.h"
#include "nsIDOMDocument.h"
//#include "nsIDocument.h"
#include "nsISelection.h"
#include "nsIScriptGlobalObject.h"

#include "nsIInterfaceRequestorUtils.h"

#include "nsICommandManager.h"
#include "nsPICommandUpdater.h"

#include "nsIEditor.h"
#include "nsIDocShell.h"
#include "nsITransactionManager.h"

nsComposerCommandsUpdater::nsComposerCommandsUpdater()
:  mDOMWindow(nsnull)
,  mDocShell(nsnull)
,  mDirtyState(eStateUninitialized)
,  mSelectionCollapsed(eStateUninitialized)
,  mFirstDoOfFirstUndo(PR_TRUE)
{
	NS_INIT_ISUPPORTS();
}

nsComposerCommandsUpdater::~nsComposerCommandsUpdater()
{
}

NS_IMPL_ISUPPORTS4(nsComposerCommandsUpdater, nsISelectionListener,
                   nsIDocumentStateListener, nsITransactionListener, nsITimerCallback);

#if 0
#pragma mark -
#endif

NS_IMETHODIMP
nsComposerCommandsUpdater::NotifyDocumentCreated()
{
  // Trigger an nsIObserve notification that the document has been created
  UpdateOneCommand("obs_documentCreated");
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::NotifyDocumentWillBeDestroyed()
{
  // cancel any outstanding udpate timer
  if (mUpdateTimer)
    mUpdateTimer->Cancel();
  
  // Trigger an nsIObserve notification that the document will be destroyed
  UpdateOneCommand("obs_documentWillBeDestroyed");
  return NS_OK;
}


NS_IMETHODIMP
nsComposerCommandsUpdater::NotifyDocumentStateChanged(PRBool aNowDirty)
{
  // update document modified. We should have some other notifications for this too.
  return UpdateDirtyState(aNowDirty);
}

NS_IMETHODIMP
nsComposerCommandsUpdater::NotifySelectionChanged(nsIDOMDocument *,
                                                  nsISelection *, short)
{
  return PrimeUpdateTimer();
}

#if 0
#pragma mark -
#endif

NS_IMETHODIMP
nsComposerCommandsUpdater::WillDo(nsITransactionManager *aManager,
                                  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::DidDo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, nsresult aDoResult)
{
  // only need to update if the status of the Undo menu item changes.
  PRInt32 undoCount;
  aManager->GetNumberOfUndoItems(&undoCount);
  if (undoCount == 1)
  {
    if (mFirstDoOfFirstUndo)
      UpdateCommandGroup(NS_LITERAL_STRING("undo"));
    mFirstDoOfFirstUndo = PR_FALSE;
  }
	
  return NS_OK;
}

NS_IMETHODIMP 
nsComposerCommandsUpdater::WillUndo(nsITransactionManager *aManager,
                                    nsITransaction *aTransaction,
                                    PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::DidUndo(nsITransactionManager *aManager,
                                   nsITransaction *aTransaction,
                                   nsresult aUndoResult)
{
  PRInt32 undoCount;
  aManager->GetNumberOfUndoItems(&undoCount);
  if (undoCount == 0)
    mFirstDoOfFirstUndo = PR_TRUE;    // reset the state for the next do

  UpdateCommandGroup(NS_LITERAL_STRING("undo"));
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::WillRedo(nsITransactionManager *aManager,
                                    nsITransaction *aTransaction,
                                    PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::DidRedo(nsITransactionManager *aManager,  
                                   nsITransaction *aTransaction,
                                   nsresult aRedoResult)
{
  UpdateCommandGroup(NS_LITERAL_STRING("undo"));
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::WillBeginBatch(nsITransactionManager *aManager,
                                          PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::DidBeginBatch(nsITransactionManager *aManager,
                                         nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::WillEndBatch(nsITransactionManager *aManager,
                                        PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::DidEndBatch(nsITransactionManager *aManager, 
                                       nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::WillMerge(nsITransactionManager *aManager,
                                     nsITransaction *aTopTransaction,
                                     nsITransaction *aTransactionToMerge,
                                     PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::DidMerge(nsITransactionManager *aManager,
                                    nsITransaction *aTopTransaction,
                                    nsITransaction *aTransactionToMerge,
                                    PRBool aDidMerge, nsresult aMergeResult)
{
  return NS_OK;
}

#if 0
#pragma mark -
#endif

nsresult
nsComposerCommandsUpdater::Init(nsIDOMWindow* aDOMWindow)
{
  NS_ENSURE_ARG(aDOMWindow);
  mDOMWindow = aDOMWindow;

  nsCOMPtr<nsIScriptGlobalObject> scriptObject(do_QueryInterface(aDOMWindow));
  if (scriptObject)
  {
    nsCOMPtr<nsIDocShell> docShell;
    scriptObject->GetDocShell(getter_AddRefs(docShell));
    mDocShell = docShell.get();		
  }
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
  return mUpdateTimer->InitWithCallback(NS_STATIC_CAST(nsITimerCallback*, this),
                                        kUpdateTimerDelay,
                                        nsITimer::TYPE_ONE_SHOT);
}


void nsComposerCommandsUpdater::TimerCallback()
{
  // if the selection state has changed, update stuff
  PRBool isCollapsed = SelectionIsCollapsed();
  if (isCollapsed != mSelectionCollapsed)
  {
    UpdateCommandGroup(NS_LITERAL_STRING("select"));
    mSelectionCollapsed = isCollapsed;
  }
  
  UpdateCommandGroup(NS_LITERAL_STRING("style"));
}

nsresult
nsComposerCommandsUpdater::UpdateDirtyState(PRBool aNowDirty)
{
  if (mDirtyState != aNowDirty)
  {
    UpdateCommandGroup(NS_LITERAL_STRING("save"));
    mDirtyState = aNowDirty;
  }
  
  return NS_OK;  
}

nsresult
nsComposerCommandsUpdater::UpdateCommandGroup(const nsAString& aCommandGroup)
{
  if (!mDocShell) return NS_ERROR_FAILURE;

  nsCOMPtr<nsICommandManager> commandManager = do_GetInterface(mDocShell);
  nsCOMPtr<nsPICommandUpdater> commandUpdater = do_QueryInterface(commandManager);
  if (!commandUpdater) return NS_ERROR_FAILURE;

  
  // This hardcoded list of commands is temporary.
  // This code should use nsIControllerCommandGroup.
  if (aCommandGroup.Equals(NS_LITERAL_STRING("undo")))
  {
    commandUpdater->CommandStatusChanged("cmd_undo");
    commandUpdater->CommandStatusChanged("cmd_redo");
  }
  else if (aCommandGroup.Equals(NS_LITERAL_STRING("select")) ||
           aCommandGroup.Equals(NS_LITERAL_STRING("style")))
  {
    commandUpdater->CommandStatusChanged("cmd_bold");
    commandUpdater->CommandStatusChanged("cmd_italic");
    commandUpdater->CommandStatusChanged("cmd_underline");
    commandUpdater->CommandStatusChanged("cmd_tt");

    commandUpdater->CommandStatusChanged("cmd_strikethrough");
    commandUpdater->CommandStatusChanged("cmd_superscript");
    commandUpdater->CommandStatusChanged("cmd_subscript");
    commandUpdater->CommandStatusChanged("cmd_nobreak");

    commandUpdater->CommandStatusChanged("cmd_em");
    commandUpdater->CommandStatusChanged("cmd_strong");
    commandUpdater->CommandStatusChanged("cmd_cite");
    commandUpdater->CommandStatusChanged("cmd_abbr");
    commandUpdater->CommandStatusChanged("cmd_acronym");
    commandUpdater->CommandStatusChanged("cmd_code");
    commandUpdater->CommandStatusChanged("cmd_samp");
    commandUpdater->CommandStatusChanged("cmd_var");
   
    commandUpdater->CommandStatusChanged("cmd_increaseFont");
    commandUpdater->CommandStatusChanged("cmd_decreaseFont");

    commandUpdater->CommandStatusChanged("cmd_paragraphState");
    commandUpdater->CommandStatusChanged("cmd_fontFace");
    commandUpdater->CommandStatusChanged("cmd_fontColor");
    commandUpdater->CommandStatusChanged("cmd_backgroundColor");
    commandUpdater->CommandStatusChanged("cmd_highlight");
  }  
  else if (aCommandGroup.Equals(NS_LITERAL_STRING("save")))
  {
    // save commands (most are not in C++)
    commandUpdater->CommandStatusChanged("cmd_setDocumentModified");
    commandUpdater->CommandStatusChanged("cmd_save");
  }
  return NS_OK;  
}

nsresult
nsComposerCommandsUpdater::UpdateOneCommand(const char *aCommand)
{
  if (!mDocShell) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsICommandManager>  	commandManager = do_GetInterface(mDocShell);
  nsCOMPtr<nsPICommandUpdater>	commandUpdater = do_QueryInterface(commandManager);
  if (!commandUpdater) return NS_ERROR_FAILURE;

  commandUpdater->CommandStatusChanged(aCommand);

  return NS_OK;  
}

PRBool
nsComposerCommandsUpdater::SelectionIsCollapsed()
{
  if (!mDOMWindow) return PR_TRUE;

  nsCOMPtr<nsISelection> domSelection;
  if (NS_SUCCEEDED(mDOMWindow->GetSelection(getter_AddRefs(domSelection))))
  {    
    PRBool selectionCollapsed = PR_FALSE;
    domSelection->GetIsCollapsed(&selectionCollapsed);
    return selectionCollapsed;
  }

  return PR_FALSE;
}

#if 0
#pragma mark -
#endif

nsresult
nsComposerCommandsUpdater::Notify(nsITimer *timer)
{
  NS_ASSERTION(timer == mUpdateTimer.get(), "Hey, this ain't my timer!");
  mUpdateTimer = NULL;    // release my hold  
  TimerCallback();
  return NS_OK;
}

#if 0
#pragma mark -
#endif


nsresult
NS_NewComposerCommandsUpdater(nsISelectionListener** aInstancePtrResult)
{
  nsComposerCommandsUpdater* newThang = new nsComposerCommandsUpdater;
  if (!newThang)
    return NS_ERROR_OUT_OF_MEMORY;

  return newThang->QueryInterface(NS_GET_IID(nsISelectionListener),
                                  (void **)aInstancePtrResult);
}
