/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 * Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Ningjie Chen <chenn@email.uc.edu>
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

#include "nsIMEStateManager.h"
#include "nsCOMPtr.h"
#include "nsIWidget.h"
#include "nsIViewManager.h"
#include "nsIViewObserver.h"
#include "nsIPresShell.h"
#include "nsISupports.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIEditorDocShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIDOMWindow.h"
#include "nsContentUtils.h"
#include "nsINode.h"
#include "nsIFrame.h"
#include "nsRange.h"
#include "nsIDOMRange.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionListener.h"
#include "nsISelectionController.h"
#include "nsIMutationObserver.h"
#include "nsContentEventHandler.h"

/******************************************************************/
/* nsIMEStateManager                                              */
/******************************************************************/

nsIContent*    nsIMEStateManager::sContent      = nsnull;
nsPresContext* nsIMEStateManager::sPresContext  = nsnull;
PRBool         nsIMEStateManager::sInstalledMenuKeyboardListener = PR_FALSE;

nsTextStateManager* nsIMEStateManager::sTextStateObserver = nsnull;

nsresult
nsIMEStateManager::OnDestroyPresContext(nsPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  if (aPresContext != sPresContext)
    return NS_OK;
  nsCOMPtr<nsIWidget> widget = GetWidget(sPresContext);
  if (widget) {
    PRUint32 newState = GetNewIMEState(sPresContext, nsnull);
    SetIMEState(newState, widget);
  }
  sContent = nsnull;
  sPresContext = nsnull;
  OnTextStateBlur(nsnull, nsnull);
  return NS_OK;
}

nsresult
nsIMEStateManager::OnRemoveContent(nsPresContext* aPresContext,
                                   nsIContent* aContent)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  if (!sPresContext || !sContent ||
      aPresContext != sPresContext ||
      aContent != sContent)
    return NS_OK;

  // Current IME transaction should commit
  nsCOMPtr<nsIWidget> widget = GetWidget(sPresContext);
  if (widget) {
    nsresult rv = widget->CancelIMEComposition();
    if (NS_FAILED(rv))
      widget->ResetInputState();
    PRUint32 newState = GetNewIMEState(sPresContext, nsnull);
    SetIMEState(newState, widget);
  }

  sContent = nsnull;
  sPresContext = nsnull;

  return NS_OK;
}

nsresult
nsIMEStateManager::OnChangeFocus(nsPresContext* aPresContext,
                                 nsIContent* aContent)
{
  NS_ENSURE_ARG_POINTER(aPresContext);

  nsCOMPtr<nsIWidget> widget = GetWidget(aPresContext);
  if (!widget) {
    return NS_OK;
  }

  PRUint32 newState = GetNewIMEState(aPresContext, aContent);
  if (aPresContext == sPresContext && aContent == sContent) {
    // actual focus isn't changing, but if IME enabled state is changing,
    // we should do it.
    PRUint32 newEnabledState = newState & nsIContent::IME_STATUS_MASK_ENABLED;
    if (newEnabledState == 0) {
      // the enabled state isn't changing, we should do nothing.
      return NS_OK;
    }
    PRUint32 enabled;
    if (NS_FAILED(widget->GetIMEEnabled(&enabled))) {
      // this platform doesn't support IME controlling
      return NS_OK;
    }
    if (enabled ==
        nsContentUtils::GetWidgetStatusFromIMEStatus(newEnabledState)) {
      // the enabled state isn't changing.
      return NS_OK;
    }
  }

  // Current IME transaction should commit
  if (sPresContext) {
    nsCOMPtr<nsIWidget> oldWidget;
    if (sPresContext == aPresContext)
      oldWidget = widget;
    else
      oldWidget = GetWidget(sPresContext);
    if (oldWidget)
      oldWidget->ResetInputState();
  }

  if (newState != nsIContent::IME_STATUS_NONE) {
    // Update IME state for new focus widget
    SetIMEState(newState, widget);
  }

  sPresContext = aPresContext;
  sContent = aContent;

  return NS_OK;
}

void
nsIMEStateManager::OnInstalledMenuKeyboardListener(PRBool aInstalling)
{
  sInstalledMenuKeyboardListener = aInstalling;
  OnChangeFocus(sPresContext, sContent);
}

void
nsIMEStateManager::UpdateIMEState(PRUint32 aNewIMEState)
{
  if (!sPresContext) {
    NS_WARNING("ISM doesn't know which editor has focus");
    return;
  }
  NS_PRECONDITION(aNewIMEState != 0, "aNewIMEState doesn't specify new state.");
  nsCOMPtr<nsIWidget> widget = GetWidget(sPresContext);
  if (!widget) {
    NS_WARNING("focused widget is not found");
    return;
  }

  // Don't update IME state when enabled state isn't actually changed.
  PRUint32 currentEnabledState;
  nsresult rv = widget->GetIMEEnabled(&currentEnabledState);
  if (NS_FAILED(rv)) {
    return; // This platform doesn't support controling the IME state.
  }
  PRUint32 newEnabledState = aNewIMEState & nsIContent::IME_STATUS_MASK_ENABLED;
  if (currentEnabledState ==
        nsContentUtils::GetWidgetStatusFromIMEStatus(newEnabledState)) {
    return;
  }

  // commit current composition
  widget->ResetInputState();

  SetIMEState(aNewIMEState, widget);
}

PRUint32
nsIMEStateManager::GetNewIMEState(nsPresContext* aPresContext,
                                  nsIContent*    aContent)
{
  // On Printing or Print Preview, we don't need IME.
  if (aPresContext->Type() == nsPresContext::eContext_PrintPreview ||
      aPresContext->Type() == nsPresContext::eContext_Print) {
    return nsIContent::IME_STATUS_DISABLE;
  }

  if (sInstalledMenuKeyboardListener)
    return nsIContent::IME_STATUS_DISABLE;

  if (!aContent) {
    // Even if there are no focused content, the focused document might be
    // editable, such case is design mode.
    nsIDocument* doc = aPresContext->Document();
    if (doc && doc->HasFlag(NODE_IS_EDITABLE))
      return nsIContent::IME_STATUS_ENABLE;
    return nsIContent::IME_STATUS_DISABLE;
  }

  return aContent->GetDesiredIMEState();
}

void
nsIMEStateManager::SetIMEState(PRUint32 aState,
                               nsIWidget* aWidget)
{
  if (aState & nsIContent::IME_STATUS_MASK_ENABLED) {
    PRUint32 state =
      nsContentUtils::GetWidgetStatusFromIMEStatus(aState);
    aWidget->SetIMEEnabled(state);
  }
  if (aState & nsIContent::IME_STATUS_MASK_OPENED) {
    PRBool open = !!(aState & nsIContent::IME_STATUS_OPEN);
    aWidget->SetIMEOpenState(open);
  }
}

nsIWidget*
nsIMEStateManager::GetWidget(nsPresContext* aPresContext)
{
  nsIPresShell* shell = aPresContext->GetPresShell();
  NS_ENSURE_TRUE(shell, nsnull);

  nsIViewManager* vm = shell->GetViewManager();
  if (!vm)
    return nsnull;
  nsCOMPtr<nsIWidget> widget = nsnull;
  nsresult rv = vm->GetRootWidget(getter_AddRefs(widget));
  NS_ENSURE_SUCCESS(rv, nsnull);
  return widget;
}


// nsTextStateManager notifies widget of any text and selection changes
//  in the currently focused editor
// sTextStateObserver points to the currently active nsTextStateManager
// sTextStateObserver is null if there is no focused editor

class nsTextStateManager : public nsISelectionListener,
                           public nsStubMutationObserver
{
public:
  nsTextStateManager();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONLISTENER
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  nsresult Init(nsIWidget* aWidget,
                nsPresContext* aPresContext,
                nsINode* aNode,
                PRBool aWantUpdates);
  void     Destroy(void);

  nsCOMPtr<nsIWidget>            mWidget;
  nsCOMPtr<nsISelection>         mSel;
  nsCOMPtr<nsIContent>           mRootContent;
  nsCOMPtr<nsINode>              mEditableNode;
  PRBool                         mDestroying;

private:
  void NotifyContentAdded(nsINode* aContainer, PRInt32 aStart, PRInt32 aEnd);
};

nsTextStateManager::nsTextStateManager()
{
  mDestroying = PR_FALSE;
}

nsresult
nsTextStateManager::Init(nsIWidget* aWidget,
                         nsPresContext* aPresContext,
                         nsINode* aNode,
                         PRBool aWantUpdates)
{
  mWidget = aWidget;

  if (!aWantUpdates) {
    mEditableNode = aNode;
    return NS_OK;
  }

  nsIPresShell* presShell = aPresContext->PresShell();

  // get selection and root content
  nsCOMPtr<nsISelectionController> selCon;
  if (aNode->IsNodeOfType(nsINode::eCONTENT)) {
    nsIFrame* frame = static_cast<nsIContent*>(aNode)->GetPrimaryFrame();
    NS_ENSURE_TRUE(frame, NS_ERROR_UNEXPECTED);

    frame->GetSelectionController(aPresContext,
                                  getter_AddRefs(selCon));
  } else {
    // aNode is a document
    selCon = do_QueryInterface(presShell);
  }
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelection> sel;
  nsresult rv = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                     getter_AddRefs(sel));
  NS_ENSURE_TRUE(sel, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMRange> selDomRange;
  rv = sel->GetRangeAt(0, getter_AddRefs(selDomRange));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIRange> selRange(do_QueryInterface(selDomRange));
  NS_ENSURE_TRUE(selRange && selRange->GetStartParent(), NS_ERROR_UNEXPECTED);

  mRootContent = selRange->GetStartParent()->
                     GetSelectionRootContent(presShell);
  if (!mRootContent && aNode->IsNodeOfType(nsINode::eDOCUMENT)) {
    // The document node is editable, but there are no contents, this document
    // is not editable.
    return NS_ERROR_NOT_AVAILABLE;
  }
  NS_ENSURE_TRUE(mRootContent, NS_ERROR_UNEXPECTED);

  // add text change observer
  mRootContent->AddMutationObserver(this);

  // add selection change listener
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(sel));
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_UNEXPECTED);
  rv = selPrivate->AddSelectionListener(this);
  NS_ENSURE_SUCCESS(rv, rv);
  mSel = sel;

  mEditableNode = aNode;
  return NS_OK;
}

void
nsTextStateManager::Destroy(void)
{
  if (mSel) {
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mSel));
    if (selPrivate)
      selPrivate->RemoveSelectionListener(this);
    mSel = nsnull;
  }
  if (mRootContent) {
    mRootContent->RemoveMutationObserver(this);
    mRootContent = nsnull;
  }
  mEditableNode = nsnull;
  mWidget = nsnull;
}

NS_IMPL_ISUPPORTS2(nsTextStateManager,
                   nsIMutationObserver,
                   nsISelectionListener)

// Helper class, used for selection change notification
class SelectionChangeEvent : public nsRunnable {
public:
  SelectionChangeEvent(nsIWidget *widget)
    : mWidget(widget)
  {
  }

  NS_IMETHOD Run() {
    mWidget->OnIMESelectionChange();
    return NS_OK;
  }

private:
  nsCOMPtr<nsIWidget> mWidget;
};

nsresult
nsTextStateManager::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                           nsISelection* aSel,
                                           PRInt16 aReason)
{
  PRInt32 count = 0;
  nsresult rv = aSel->GetRangeCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);
  if (count > 0) {
    nsContentUtils::AddScriptRunner(new SelectionChangeEvent(mWidget));
  }
  return NS_OK;
}

// Helper class, used for text change notification
class TextChangeEvent : public nsRunnable {
public:
  TextChangeEvent(nsIWidget *widget,
                  PRUint32 start, PRUint32 oldEnd, PRUint32 newEnd)
    : mWidget(widget)
    , mStart(start)
    , mOldEnd(oldEnd)
    , mNewEnd(newEnd)
  {
  }

  NS_IMETHOD Run() {
    mWidget->OnIMETextChange(mStart, mOldEnd, mNewEnd);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIWidget> mWidget;
  PRUint32 mStart, mOldEnd, mNewEnd;
};

void
nsTextStateManager::CharacterDataChanged(nsIDocument* aDocument,
                                         nsIContent* aContent,
                                         CharacterDataChangeInfo* aInfo)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "character data changed for non-text node");

  PRUint32 offset = 0;
  // get offsets of change and fire notification
  if (NS_FAILED(nsContentEventHandler::GetFlatTextOffsetOfRange(
                    mRootContent, aContent, aInfo->mChangeStart, &offset)))
    return;

  PRUint32 oldEnd = offset + aInfo->mChangeEnd - aInfo->mChangeStart;
  PRUint32 newEnd = offset + aInfo->mReplaceLength;

  nsContentUtils::AddScriptRunner(
      new TextChangeEvent(mWidget, offset, oldEnd, newEnd));
}

void
nsTextStateManager::NotifyContentAdded(nsINode* aContainer,
                                       PRInt32 aStartIndex,
                                       PRInt32 aEndIndex)
{
  PRUint32 offset = 0, newOffset = 0;
  if (NS_FAILED(nsContentEventHandler::GetFlatTextOffsetOfRange(
                    mRootContent, aContainer, aStartIndex, &offset)))
    return;

  // get offset at the end of the last added node
  if (NS_FAILED(nsContentEventHandler::GetFlatTextOffsetOfRange(
                    aContainer->GetChildAt(aStartIndex),
                    aContainer, aEndIndex, &newOffset)))
    return;

  // fire notification
  if (newOffset)
    nsContentUtils::AddScriptRunner(
        new TextChangeEvent(mWidget, offset, offset, offset + newOffset));
}

void
nsTextStateManager::ContentAppended(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aFirstNewContent,
                                    PRInt32 aNewIndexInContainer)
{
  NotifyContentAdded(aContainer, aNewIndexInContainer,
                     aContainer->GetChildCount());
}

void
nsTextStateManager::ContentInserted(nsIDocument* aDocument,
                                     nsIContent* aContainer,
                                     nsIContent* aChild,
                                     PRInt32 aIndexInContainer)
{
  NotifyContentAdded(NODE_FROM(aContainer, aDocument),
                     aIndexInContainer, aIndexInContainer + 1);
}

void
nsTextStateManager::ContentRemoved(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   PRInt32 aIndexInContainer,
                                   nsIContent* aPreviousSibling)
{
  PRUint32 offset = 0, childOffset = 1;
  if (NS_FAILED(nsContentEventHandler::GetFlatTextOffsetOfRange(
                    mRootContent, NODE_FROM(aContainer, aDocument),
                    aIndexInContainer, &offset)))
    return;

  // get offset at the end of the deleted node
  if (aChild->IsNodeOfType(nsINode::eTEXT))
    childOffset = aChild->TextLength();
  else if (0 < aChild->GetChildCount())
    childOffset = aChild->GetChildCount();

  if (NS_FAILED(nsContentEventHandler::GetFlatTextOffsetOfRange(
                    aChild, aChild, childOffset, &childOffset)))
    return;

  // fire notification
  if (childOffset)
    nsContentUtils::AddScriptRunner(
        new TextChangeEvent(mWidget, offset, offset + childOffset, offset));
}

static nsINode* GetRootEditableNode(nsPresContext* aPresContext,
                                    nsIContent* aContent)
{
  if (aContent) {
    nsINode* root = nsnull;
    nsINode* node = aContent;
    while (node && node->IsEditable()) {
      root = node;
      node = node->GetParent();
    }
    return root;
  }
  if (aPresContext) {
    nsIDocument* document = aPresContext->Document();
    if (document && document->IsEditable())
      return document;
  }
  return nsnull;
}

nsresult
nsIMEStateManager::OnTextStateBlur(nsPresContext* aPresContext,
                                   nsIContent* aContent)
{
  if (!sTextStateObserver || sTextStateObserver->mDestroying ||
      sTextStateObserver->mEditableNode ==
          GetRootEditableNode(aPresContext, aContent))
    return NS_OK;

  sTextStateObserver->mDestroying = PR_TRUE;
  sTextStateObserver->mWidget->OnIMEFocusChange(PR_FALSE);
  sTextStateObserver->Destroy();
  NS_RELEASE(sTextStateObserver);
  return NS_OK;
}

nsresult
nsIMEStateManager::OnTextStateFocus(nsPresContext* aPresContext,
                                    nsIContent* aContent)
{
  if (sTextStateObserver) return NS_OK;

  nsINode *editableNode = GetRootEditableNode(aPresContext, aContent);
  if (!editableNode) return NS_OK;

  nsIPresShell* shell = aPresContext->GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_NOT_AVAILABLE);
  
  nsIViewManager* vm = shell->GetViewManager();
  NS_ENSURE_TRUE(vm, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIWidget> widget;
  nsresult rv = vm->GetRootWidget(getter_AddRefs(widget));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_NOT_AVAILABLE);
  if (!widget) {
    return NS_OK; // Sometimes, there are no widgets.
  }

  rv = widget->OnIMEFocusChange(PR_TRUE);
  if (rv == NS_ERROR_NOT_IMPLEMENTED)
    return NS_OK;
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool wantUpdates = rv != NS_SUCCESS_IME_NO_UPDATES;

  // OnIMEFocusChange may cause focus and sTextStateObserver to change
  // In that case return and keep the current sTextStateObserver
  NS_ENSURE_TRUE(!sTextStateObserver, NS_OK);

  sTextStateObserver = new nsTextStateManager();
  NS_ENSURE_TRUE(sTextStateObserver, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(sTextStateObserver);
  rv = sTextStateObserver->Init(widget, aPresContext,
                                editableNode, wantUpdates);
  if (NS_FAILED(rv)) {
    sTextStateObserver->mDestroying = PR_TRUE;
    sTextStateObserver->Destroy();
    NS_RELEASE(sTextStateObserver);
    widget->OnIMEFocusChange(PR_FALSE);
    return rv;
  }
  return NS_OK;
}

nsresult
nsIMEStateManager::GetFocusSelectionAndRoot(nsISelection** aSel,
                                            nsIContent** aRoot)
{
  if (!sTextStateObserver || !sTextStateObserver->mEditableNode)
    return NS_ERROR_NOT_AVAILABLE;

  NS_ASSERTION(sTextStateObserver->mSel && sTextStateObserver->mRootContent,
               "uninitialized text state observer");
  NS_ADDREF(*aSel = sTextStateObserver->mSel);
  NS_ADDREF(*aRoot = sTextStateObserver->mRootContent);
  return NS_OK;
}
