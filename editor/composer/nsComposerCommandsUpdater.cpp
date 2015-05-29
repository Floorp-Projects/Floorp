/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"           // for operator new
#include "nsAString.h"
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsComposerCommandsUpdater.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsError.h"                    // for NS_OK, NS_ERROR_FAILURE, etc
#include "nsICommandManager.h"          // for nsICommandManager
#include "nsID.h"                       // for NS_GET_IID, etc
#include "nsIDOMWindow.h"               // for nsIDOMWindow
#include "nsIDocShell.h"                // for nsIDocShell
#include "nsIInterfaceRequestorUtils.h"  // for do_GetInterface
#include "nsISelection.h"               // for nsISelection
#include "nsITransactionManager.h"      // for nsITransactionManager
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsPICommandUpdater.h"         // for nsPICommandUpdater
#include "nsPIDOMWindow.h"              // for nsPIDOMWindow

class nsIDOMDocument;
class nsITransaction;

nsComposerCommandsUpdater::nsComposerCommandsUpdater()
:  mDirtyState(eStateUninitialized)
,  mSelectionCollapsed(eStateUninitialized)
,  mFirstDoOfFirstUndo(true)
{
}

nsComposerCommandsUpdater::~nsComposerCommandsUpdater()
{
  // cancel any outstanding update timer
  if (mUpdateTimer)
  {
    mUpdateTimer->Cancel();
  }
}

NS_IMPL_ISUPPORTS(nsComposerCommandsUpdater, nsISelectionListener,
                  nsIDocumentStateListener, nsITransactionListener, nsITimerCallback)

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
  // cancel any outstanding update timer
  if (mUpdateTimer)
  {
    mUpdateTimer->Cancel();
    mUpdateTimer = nullptr;
  }

  // We can't call this right now; it is too late in some cases and the window
  // is already partially destructed (e.g. JS objects may be gone).
#if 0
  // Trigger an nsIObserve notification that the document will be destroyed
  UpdateOneCommand("obs_documentWillBeDestroyed");
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsComposerCommandsUpdater::NotifyDocumentStateChanged(bool aNowDirty)
{
  // update document modified. We should have some other notifications for this too.
  return UpdateDirtyState(aNowDirty);
}

NS_IMETHODIMP
nsComposerCommandsUpdater::NotifySelectionChanged(nsIDOMDocument *,
                                                  nsISelection *, int16_t)
{
  return PrimeUpdateTimer();
}

#if 0
#pragma mark -
#endif

NS_IMETHODIMP
nsComposerCommandsUpdater::WillDo(nsITransactionManager *aManager,
                                  nsITransaction *aTransaction, bool *aInterrupt)
{
  *aInterrupt = false;
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::DidDo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, nsresult aDoResult)
{
  // only need to update if the status of the Undo menu item changes.
  int32_t undoCount;
  aManager->GetNumberOfUndoItems(&undoCount);
  if (undoCount == 1)
  {
    if (mFirstDoOfFirstUndo)
      UpdateCommandGroup(NS_LITERAL_STRING("undo"));
    mFirstDoOfFirstUndo = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::WillUndo(nsITransactionManager *aManager,
                                    nsITransaction *aTransaction,
                                    bool *aInterrupt)
{
  *aInterrupt = false;
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::DidUndo(nsITransactionManager *aManager,
                                   nsITransaction *aTransaction,
                                   nsresult aUndoResult)
{
  int32_t undoCount;
  aManager->GetNumberOfUndoItems(&undoCount);
  if (undoCount == 0)
    mFirstDoOfFirstUndo = true;    // reset the state for the next do

  UpdateCommandGroup(NS_LITERAL_STRING("undo"));
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::WillRedo(nsITransactionManager *aManager,
                                    nsITransaction *aTransaction,
                                    bool *aInterrupt)
{
  *aInterrupt = false;
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
                                          bool *aInterrupt)
{
  *aInterrupt = false;
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
                                        bool *aInterrupt)
{
  *aInterrupt = false;
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
                                     bool *aInterrupt)
{
  *aInterrupt = false;
  return NS_OK;
}

NS_IMETHODIMP
nsComposerCommandsUpdater::DidMerge(nsITransactionManager *aManager,
                                    nsITransaction *aTopTransaction,
                                    nsITransaction *aTransactionToMerge,
                                    bool aDidMerge, nsresult aMergeResult)
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
  mDOMWindow = do_GetWeakReference(aDOMWindow);

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aDOMWindow));
  if (window)
  {
    mDocShell = do_GetWeakReference(window->GetDocShell());
  }
  return NS_OK;
}

nsresult
nsComposerCommandsUpdater::PrimeUpdateTimer()
{
  if (!mUpdateTimer)
  {
    nsresult rv = NS_OK;
    mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  const uint32_t kUpdateTimerDelay = 150;
  return mUpdateTimer->InitWithCallback(static_cast<nsITimerCallback*>(this),
                                        kUpdateTimerDelay,
                                        nsITimer::TYPE_ONE_SHOT);
}


void nsComposerCommandsUpdater::TimerCallback()
{
  // if the selection state has changed, update stuff
  bool isCollapsed = SelectionIsCollapsed();
  if (static_cast<int8_t>(isCollapsed) != mSelectionCollapsed)
  {
    UpdateCommandGroup(NS_LITERAL_STRING("select"));
    mSelectionCollapsed = isCollapsed;
  }

  // isn't this redundant with the UpdateCommandGroup above?
  // can we just nuke the above call? or create a meta command group?
  UpdateCommandGroup(NS_LITERAL_STRING("style"));
}

nsresult
nsComposerCommandsUpdater::UpdateDirtyState(bool aNowDirty)
{
  if (mDirtyState != static_cast<int8_t>(aNowDirty))
  {
    UpdateCommandGroup(NS_LITERAL_STRING("save"));
    UpdateCommandGroup(NS_LITERAL_STRING("undo"));
    mDirtyState = aNowDirty;
  }

  return NS_OK;
}

nsresult
nsComposerCommandsUpdater::UpdateCommandGroup(const nsAString& aCommandGroup)
{
  nsCOMPtr<nsPICommandUpdater> commandUpdater = GetCommandUpdater();
  NS_ENSURE_TRUE(commandUpdater, NS_ERROR_FAILURE);


  // This hardcoded list of commands is temporary.
  // This code should use nsIControllerCommandGroup.
  if (aCommandGroup.EqualsLiteral("undo"))
  {
    commandUpdater->CommandStatusChanged("cmd_undo");
    commandUpdater->CommandStatusChanged("cmd_redo");
  }
  else if (aCommandGroup.EqualsLiteral("select") ||
           aCommandGroup.EqualsLiteral("style"))
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
  else if (aCommandGroup.EqualsLiteral("save"))
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
  nsCOMPtr<nsPICommandUpdater> commandUpdater = GetCommandUpdater();
  NS_ENSURE_TRUE(commandUpdater, NS_ERROR_FAILURE);

  commandUpdater->CommandStatusChanged(aCommand);

  return NS_OK;
}

bool
nsComposerCommandsUpdater::SelectionIsCollapsed()
{
  nsCOMPtr<nsIDOMWindow> domWindow = do_QueryReferent(mDOMWindow);
  NS_ENSURE_TRUE(domWindow, true);

  nsCOMPtr<nsISelection> domSelection;
  if (NS_SUCCEEDED(domWindow->GetSelection(getter_AddRefs(domSelection))) && domSelection)
  {
    bool selectionCollapsed = false;
    domSelection->GetIsCollapsed(&selectionCollapsed);
    return selectionCollapsed;
  }

  NS_WARNING("nsComposerCommandsUpdater::SelectionIsCollapsed - no domSelection");

  return false;
}

already_AddRefed<nsPICommandUpdater>
nsComposerCommandsUpdater::GetCommandUpdater()
{
  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShell);
  NS_ENSURE_TRUE(docShell, nullptr);
  nsCOMPtr<nsICommandManager> manager = do_GetInterface(docShell);
  nsCOMPtr<nsPICommandUpdater> updater = do_QueryInterface(manager);
  return updater.forget();
}

#if 0
#pragma mark -
#endif

nsresult
nsComposerCommandsUpdater::Notify(nsITimer *timer)
{
  NS_ASSERTION(timer == mUpdateTimer.get(), "Hey, this ain't my timer!");
  TimerCallback();
  return NS_OK;
}

#if 0
#pragma mark -
#endif


nsresult
NS_NewComposerCommandsUpdater(nsISelectionListener** aInstancePtrResult)
{
  nsRefPtr<nsComposerCommandsUpdater> newThang = new nsComposerCommandsUpdater;
  newThang.forget(aInstancePtrResult);
  return NS_OK;
}
