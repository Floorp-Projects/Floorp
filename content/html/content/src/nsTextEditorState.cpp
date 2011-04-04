/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
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
 * The Original Code is Mozilla.org client code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan@mozilla.com> (Original Author)
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

#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIView.h"
#include "nsCaret.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsITextControlFrame.h" 
#include "nsIPlaintextEditor.h"
#include "nsIDOMDocument.h"
#include "nsContentCreatorFunctions.h"
#include "nsTextControlFrame.h"
#include "nsIControllers.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsITransactionManager.h"
#include "nsIControllerContext.h"
#include "nsAttrValue.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMKeyListener.h"
#include "nsIEditorObserver.h"
#include "nsINativeKeyBindings.h"
#include "nsIDocumentEncoder.h"
#include "nsISelectionPrivate.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMEventGroup.h"
#include "nsIEditor.h"
#include "nsTextEditRules.h"

#include "nsTextEditorState.h"

using namespace mozilla::dom;

static NS_DEFINE_CID(kTextEditorCID, NS_TEXTEDITOR_CID);
static NS_DEFINE_CID(kFrameSelectionCID, NS_FRAMESELECTION_CID);

static nsINativeKeyBindings *sNativeInputBindings = nsnull;
static nsINativeKeyBindings *sNativeTextAreaBindings = nsnull;

struct SelectionState {
  PRInt32 mStart;
  PRInt32 mEnd;
};

class RestoreSelectionState : public nsRunnable {
public:
  RestoreSelectionState(nsTextControlFrame *aFrame, PRInt32 aStart, PRInt32 aEnd)
    : mFrame(aFrame),
      mWeakFrame(aFrame),
      mStart(aStart),
      mEnd(aEnd)
  {
  }

  NS_IMETHOD Run() {
    if (mWeakFrame.IsAlive()) {
      // SetSelectionRange leads to Selection::AddRange which flushes Layout -
      // need to block script to avoid nested PrepareEditor calls (bug 642800).
      nsAutoScriptBlocker scriptBlocker;
      mFrame->SetSelectionRange(mStart, mEnd);
    }
    return NS_OK;
  }

private:
  nsTextControlFrame* mFrame;
  nsWeakFrame mWeakFrame;
  PRInt32 mStart;
  PRInt32 mEnd;
};

/*static*/
PRBool
nsITextControlElement::GetWrapPropertyEnum(nsIContent* aContent,
  nsITextControlElement::nsHTMLTextWrap& aWrapProp)
{
  // soft is the default; "physical" defaults to soft as well because all other
  // browsers treat it that way and there is no real reason to maintain physical
  // and virtual as separate entities if no one else does.  Only hard and off
  // do anything different.
  aWrapProp = eHTMLTextWrap_Soft; // the default

  nsAutoString wrap;
  if (aContent->IsHTML()) {
    static nsIContent::AttrValuesArray strings[] =
      {&nsGkAtoms::HARD, &nsGkAtoms::OFF, nsnull};

    switch (aContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::wrap,
                                      strings, eIgnoreCase)) {
      case 0: aWrapProp = eHTMLTextWrap_Hard; break;
      case 1: aWrapProp = eHTMLTextWrap_Off; break;
    }

    return PR_TRUE;
  }

  return PR_FALSE;
}

static PRBool
SuppressEventHandlers(nsPresContext* aPresContext)
{
  PRBool suppressHandlers = PR_FALSE;

  if (aPresContext)
  {
    // Right now we only suppress event handlers and controller manipulation
    // when in a print preview or print context!

    // In the current implementation, we only paginate when
    // printing or in print preview.

    suppressHandlers = aPresContext->IsPaginated();
  }

  return suppressHandlers;
}

class nsAnonDivObserver : public nsStubMutationObserver
{
public:
  nsAnonDivObserver(nsTextEditorState* aTextEditorState)
  : mTextEditorState(aTextEditorState) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

private:
  nsTextEditorState* mTextEditorState;
};

class nsTextInputSelectionImpl : public nsSupportsWeakReference
                               , public nsISelectionController
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsTextInputSelectionImpl, nsISelectionController)

  nsTextInputSelectionImpl(nsFrameSelection *aSel, nsIPresShell *aShell, nsIContent *aLimiter);
  ~nsTextInputSelectionImpl(){}

  void SetScrollableFrame(nsIScrollableFrame *aScrollableFrame);
  nsFrameSelection* GetConstFrameSelection()
    { return mFrameSelection; }

  //NSISELECTIONCONTROLLER INTERFACES
  NS_IMETHOD SetDisplaySelection(PRInt16 toggle);
  NS_IMETHOD GetDisplaySelection(PRInt16 *_retval);
  NS_IMETHOD SetSelectionFlags(PRInt16 aInEnable);
  NS_IMETHOD GetSelectionFlags(PRInt16 *aOutEnable);
  NS_IMETHOD GetSelection(PRInt16 type, nsISelection **_retval);
  NS_IMETHOD ScrollSelectionIntoView(PRInt16 aType, PRInt16 aRegion, PRInt16 aFlags);
  NS_IMETHOD RepaintSelection(PRInt16 type);
  NS_IMETHOD RepaintSelection(nsPresContext* aPresContext, SelectionType aSelectionType);
  NS_IMETHOD SetCaretEnabled(PRBool enabled);
  NS_IMETHOD SetCaretReadOnly(PRBool aReadOnly);
  NS_IMETHOD GetCaretEnabled(PRBool *_retval);
  NS_IMETHOD GetCaretVisible(PRBool *_retval);
  NS_IMETHOD SetCaretVisibilityDuringSelection(PRBool aVisibility);
  NS_IMETHOD CharacterMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD CharacterExtendForDelete();
  NS_IMETHOD CharacterExtendForBackspace();
  NS_IMETHOD WordMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD WordExtendForDelete(PRBool aForward);
  NS_IMETHOD LineMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD IntraLineMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD PageMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD CompleteScroll(PRBool aForward);
  NS_IMETHOD CompleteMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD ScrollPage(PRBool aForward);
  NS_IMETHOD ScrollLine(PRBool aForward);
  NS_IMETHOD ScrollHorizontal(PRBool aLeft);
  NS_IMETHOD SelectAll(void);
  NS_IMETHOD CheckVisibility(nsIDOMNode *node, PRInt16 startOffset, PRInt16 EndOffset, PRBool *_retval);

private:
  nsCOMPtr<nsFrameSelection> mFrameSelection;
  nsCOMPtr<nsIContent>       mLimiter;
  nsIScrollableFrame        *mScrollFrame;
  nsWeakPtr mPresShellWeak;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsTextInputSelectionImpl, nsISelectionController)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsTextInputSelectionImpl, nsISelectionController)
NS_INTERFACE_TABLE_HEAD(nsTextInputSelectionImpl)
  NS_INTERFACE_TABLE3(nsTextInputSelectionImpl,
                      nsISelectionController,
                      nsISelectionDisplay,
                      nsISupportsWeakReference)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsTextInputSelectionImpl)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_2(nsTextInputSelectionImpl, mFrameSelection, mLimiter)


// BEGIN nsTextInputSelectionImpl

nsTextInputSelectionImpl::nsTextInputSelectionImpl(nsFrameSelection *aSel,
                                                   nsIPresShell *aShell,
                                                   nsIContent *aLimiter)
  : mScrollFrame(nsnull)
{
  if (aSel && aShell)
  {
    mFrameSelection = aSel;//we are the owner now!
    mLimiter = aLimiter;
    mFrameSelection->Init(aShell, mLimiter);
    mPresShellWeak = do_GetWeakReference(aShell);
  }
}

void
nsTextInputSelectionImpl::SetScrollableFrame(nsIScrollableFrame *aScrollableFrame)
{
  mScrollFrame = aScrollableFrame;
  if (!mScrollFrame && mFrameSelection) {
    mFrameSelection->DisconnectFromPresShell();
    mFrameSelection = nsnull;
  }
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetDisplaySelection(PRInt16 aToggle)
{
  if (!mFrameSelection)
    return NS_ERROR_NULL_POINTER;
  
  mFrameSelection->SetDisplaySelection(aToggle);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetDisplaySelection(PRInt16 *aToggle)
{
  if (!mFrameSelection)
    return NS_ERROR_NULL_POINTER;

  *aToggle = mFrameSelection->GetDisplaySelection();
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetSelectionFlags(PRInt16 aToggle)
{
  return NS_OK;//stub this out. not used in input
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetSelectionFlags(PRInt16 *aOutEnable)
{
  *aOutEnable = nsISelectionDisplay::DISPLAY_TEXT;
  return NS_OK; 
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetSelection(PRInt16 type, nsISelection **_retval)
{
  if (!mFrameSelection)
    return NS_ERROR_NULL_POINTER;
    
  *_retval = mFrameSelection->GetSelection(type);
  
  if (!(*_retval))
    return NS_ERROR_FAILURE;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollSelectionIntoView(PRInt16 aType, PRInt16 aRegion, PRInt16 aFlags)
{
  if (!mFrameSelection) 
    return NS_ERROR_FAILURE; 

  return mFrameSelection->ScrollSelectionIntoView(aType, aRegion, aFlags);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::RepaintSelection(PRInt16 type)
{
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;

  return mFrameSelection->RepaintSelection(type);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::RepaintSelection(nsPresContext* aPresContext, SelectionType aSelectionType)
{
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;

  return mFrameSelection->RepaintSelection(aSelectionType);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetCaretEnabled(PRBool enabled)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak);
  if (!shell) return NS_ERROR_FAILURE;

  // tell the pres shell to enable the caret, rather than settings its visibility directly.
  // this way the presShell's idea of caret visibility is maintained.
  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(shell);
  if (!selCon) return NS_ERROR_NO_INTERFACE;
  selCon->SetCaretEnabled(enabled);

  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetCaretReadOnly(PRBool aReadOnly)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    nsRefPtr<nsCaret> caret = shell->GetCaret();
    if (caret) {
      nsISelection* domSel = mFrameSelection->
        GetSelection(nsISelectionController::SELECTION_NORMAL);
      if (domSel)
        caret->SetCaretReadOnly(aReadOnly);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetCaretEnabled(PRBool *_retval)
{
  return GetCaretVisible(_retval);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetCaretVisible(PRBool *_retval)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    nsRefPtr<nsCaret> caret = shell->GetCaret();
    if (caret) {
      nsISelection* domSel = mFrameSelection->
        GetSelection(nsISelectionController::SELECTION_NORMAL);
      if (domSel)
        return caret->GetCaretVisible(_retval);
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetCaretVisibilityDuringSelection(PRBool aVisibility)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    nsRefPtr<nsCaret> caret = shell->GetCaret();
    if (caret) {
      nsISelection* domSel = mFrameSelection->
        GetSelection(nsISelectionController::SELECTION_NORMAL);
      if (domSel)
        caret->SetVisibilityDuringSelection(aVisibility);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CharacterMove(PRBool aForward, PRBool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->CharacterMove(aForward, aExtend);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CharacterExtendForDelete()
{
  if (mFrameSelection)
    return mFrameSelection->CharacterExtendForDelete();
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CharacterExtendForBackspace()
{
  if (mFrameSelection)
    return mFrameSelection->CharacterExtendForBackspace();
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::WordMove(PRBool aForward, PRBool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->WordMove(aForward, aExtend);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::WordExtendForDelete(PRBool aForward)
{
  if (mFrameSelection)
    return mFrameSelection->WordExtendForDelete(aForward);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::LineMove(PRBool aForward, PRBool aExtend)
{
  if (mFrameSelection)
  {
    nsresult result = mFrameSelection->LineMove(aForward, aExtend);
    if (NS_FAILED(result))
      result = CompleteMove(aForward,aExtend);
    return result;
  }
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsTextInputSelectionImpl::IntraLineMove(PRBool aForward, PRBool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->IntraLineMove(aForward, aExtend);
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsTextInputSelectionImpl::PageMove(PRBool aForward, PRBool aExtend)
{
  // expected behavior for PageMove is to scroll AND move the caret
  // and to remain relative position of the caret in view. see Bug 4302.
  if (mScrollFrame)
  {
    mFrameSelection->CommonPageMove(aForward, aExtend, mScrollFrame);
  }
  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  return ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, nsISelectionController::SELECTION_FOCUS_REGION,
                                 nsISelectionController::SCROLL_SYNCHRONOUS);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CompleteScroll(PRBool aForward)
{
  if (!mScrollFrame)
    return NS_ERROR_NOT_INITIALIZED;

  mScrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                         nsIScrollableFrame::WHOLE,
                         nsIScrollableFrame::INSTANT);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CompleteMove(PRBool aForward, PRBool aExtend)
{
  // grab the parent / root DIV for this text widget
  nsIContent* parentDIV = mFrameSelection->GetLimiter();
  if (!parentDIV)
    return NS_ERROR_UNEXPECTED;

  // make the caret be either at the very beginning (0) or the very end
  PRInt32 offset = 0;
  nsFrameSelection::HINT hint = nsFrameSelection::HINTLEFT;
  if (aForward)
  {
    offset = parentDIV->GetChildCount();

    // Prevent the caret from being placed after the last
    // BR node in the content tree!

    if (offset > 0)
    {
      nsIContent *child = parentDIV->GetChildAt(offset - 1);

      if (child->Tag() == nsGkAtoms::br)
      {
        --offset;
        hint = nsFrameSelection::HINTRIGHT; // for Bug 106855
      }
    }
  }

  mFrameSelection->HandleClick(parentDIV, offset, offset, aExtend,
                               PR_FALSE, hint);

  // if we got this far, attempt to scroll no matter what the above result is
  return CompleteScroll(aForward);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollPage(PRBool aForward)
{
  if (!mScrollFrame)
    return NS_ERROR_NOT_INITIALIZED;

  mScrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                         nsIScrollableFrame::PAGES,
                         nsIScrollableFrame::SMOOTH);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollLine(PRBool aForward)
{
  if (!mScrollFrame)
    return NS_ERROR_NOT_INITIALIZED;

  mScrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                         nsIScrollableFrame::LINES,
                         nsIScrollableFrame::SMOOTH);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollHorizontal(PRBool aLeft)
{
  if (!mScrollFrame)
    return NS_ERROR_NOT_INITIALIZED;

  mScrollFrame->ScrollBy(nsIntPoint(aLeft ? -1 : 1, 0),
                         nsIScrollableFrame::LINES,
                         nsIScrollableFrame::SMOOTH);
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SelectAll()
{
  if (mFrameSelection)
    return mFrameSelection->SelectAll();
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CheckVisibility(nsIDOMNode *node, PRInt16 startOffset, PRInt16 EndOffset, PRBool *_retval)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsISelectionController> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    return shell->CheckVisibility(node,startOffset,EndOffset, _retval);
  }
  return NS_ERROR_FAILURE;

}

class nsTextInputListener : public nsISelectionListener,
                            public nsIDOMKeyListener,
                            public nsIEditorObserver,
                            public nsSupportsWeakReference
{
public:
  /** the default constructor
   */ 
  explicit nsTextInputListener(nsITextControlElement* aTxtCtrlElement);
  /** the default destructor. virtual due to the possibility of derivation.
   */
  virtual ~nsTextInputListener();

  /** SetEditor gives an address to the editor that will be accessed
   *  @param aEditor the editor this listener calls for editing operations
   */
  void SetFrame(nsTextControlFrame *aFrame){mFrame = aFrame;}

  void SettingValue(PRBool aValue) { mSettingValue = aValue; }

  NS_DECL_ISUPPORTS

  NS_DECL_NSISELECTIONLISTENER

  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMKeyListener
  NS_IMETHOD KeyDown(nsIDOMEvent *aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent *aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent *aKeyEvent);

  NS_DECL_NSIEDITOROBSERVER

protected:

  nsresult  UpdateTextInputCommands(const nsAString& commandsToUpdate);

  NS_HIDDEN_(nsINativeKeyBindings*) GetKeyBindings();

protected:

  nsWeakFrame mFrame;

  nsITextControlElement* const mTxtCtrlElement;

  PRPackedBool    mSelectionWasCollapsed;
  /**
   * Whether we had undo items or not the last time we got EditAction()
   * notification (when this state changes we update undo and redo menus)
   */
  PRPackedBool    mHadUndoItems;
  /**
   * Whether we had redo items or not the last time we got EditAction()
   * notification (when this state changes we update undo and redo menus)
   */
  PRPackedBool    mHadRedoItems;
  /**
   * Whether we're in the process of a SetValue call, and should therefore
   * refrain from calling OnValueChanged.
   */
  PRPackedBool mSettingValue;
};


/*
 * nsTextInputListener implementation
 */

nsTextInputListener::nsTextInputListener(nsITextControlElement* aTxtCtrlElement)
: mTxtCtrlElement(aTxtCtrlElement)
, mSelectionWasCollapsed(PR_TRUE)
, mHadUndoItems(PR_FALSE)
, mHadRedoItems(PR_FALSE)
, mSettingValue(PR_FALSE)
{
}

nsTextInputListener::~nsTextInputListener() 
{
}

NS_IMPL_ADDREF(nsTextInputListener)
NS_IMPL_RELEASE(nsTextInputListener)

NS_INTERFACE_MAP_BEGIN(nsTextInputListener)
  NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
  NS_INTERFACE_MAP_ENTRY(nsIEditorObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMKeyListener)
NS_INTERFACE_MAP_END

// BEGIN nsIDOMSelectionListener

NS_IMETHODIMP
nsTextInputListener::NotifySelectionChanged(nsIDOMDocument* aDoc, nsISelection* aSel, PRInt16 aReason)
{
  PRBool collapsed;
  if (!mFrame.IsAlive() || !aDoc || !aSel || NS_FAILED(aSel->GetIsCollapsed(&collapsed)))
    return NS_OK;

  // Fire the select event
  // The specs don't exactly say when we should fire the select event.
  // IE: Whenever you add/remove a character to/from the selection. Also
  //     each time for select all. Also if you get to the end of the text 
  //     field you will get new event for each keypress or a continuous 
  //     stream of events if you use the mouse. IE will fire select event 
  //     when the selection collapses to nothing if you are holding down
  //     the shift or mouse button.
  // Mozilla: If we have non-empty selection we will fire a new event for each
  //          keypress (or mouseup) if the selection changed. Mozilla will also
  //          create the event each time select all is called, even if everything
  //          was previously selected, becase technically select all will first collapse
  //          and then extend. Mozilla will never create an event if the selection 
  //          collapses to nothing.
  if (!collapsed && (aReason & (nsISelectionListener::MOUSEUP_REASON | 
                                nsISelectionListener::KEYPRESS_REASON |
                                nsISelectionListener::SELECTALL_REASON)))
  {
    nsIContent* content = mFrame->GetContent();
    if (content) 
    {
      nsCOMPtr<nsIDocument> doc = content->GetDocument();
      if (doc) 
      {
        nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
        if (presShell) 
        {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsEvent event(PR_TRUE, NS_FORM_SELECTED);

          presShell->HandleEventWithTarget(&event, mFrame, content, &status);
        }
      }
    }
  }

  // if the collapsed state did not change, don't fire notifications
  if (collapsed == mSelectionWasCollapsed)
    return NS_OK;
  
  mSelectionWasCollapsed = collapsed;

  if (!mFrame.IsAlive() || !nsContentUtils::IsFocusedContent(mFrame->GetContent()))
    return NS_OK;

  return UpdateTextInputCommands(NS_LITERAL_STRING("select"));
}

// END nsIDOMSelectionListener

// BEGIN nsIDOMKeyListener

NS_IMETHODIMP
nsTextInputListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

static void
DoCommandCallback(const char *aCommand, void *aData)
{
  nsTextControlFrame *frame = static_cast<nsTextControlFrame*>(aData);
  nsIContent *content = frame->GetContent();

  nsCOMPtr<nsIControllers> controllers;
  nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(content);
  if (input) {
    input->GetControllers(getter_AddRefs(controllers));
  } else {
    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textArea =
      do_QueryInterface(content);

    if (textArea) {
      textArea->GetControllers(getter_AddRefs(controllers));
    }
  }

  if (!controllers) {
    NS_WARNING("Could not get controllers");
    return;
  }

  nsCOMPtr<nsIController> controller;
  controllers->GetControllerForCommand(aCommand, getter_AddRefs(controller));
  if (controller) {
    controller->DoCommand(aCommand);
  }
}


NS_IMETHODIMP
nsTextInputListener::KeyDown(nsIDOMEvent *aDOMEvent)
{
  NS_ENSURE_STATE(mFrame.IsAlive());
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aDOMEvent));
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_INVALID_ARG);

  nsNativeKeyEvent nativeEvent;
  nsINativeKeyBindings *bindings = GetKeyBindings();
  if (bindings &&
      nsContentUtils::DOMEventToNativeKeyEvent(keyEvent, &nativeEvent, PR_FALSE)) {
    if (bindings->KeyDown(nativeEvent, DoCommandCallback, mFrame)) {
      aDOMEvent->PreventDefault();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTextInputListener::KeyPress(nsIDOMEvent *aDOMEvent)
{
  NS_ENSURE_STATE(mFrame.IsAlive());
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aDOMEvent));
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_INVALID_ARG);

  nsNativeKeyEvent nativeEvent;
  nsINativeKeyBindings *bindings = GetKeyBindings();
  if (bindings &&
      nsContentUtils::DOMEventToNativeKeyEvent(keyEvent, &nativeEvent, PR_TRUE)) {
    if (bindings->KeyPress(nativeEvent, DoCommandCallback, mFrame)) {
      aDOMEvent->PreventDefault();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTextInputListener::KeyUp(nsIDOMEvent *aDOMEvent)
{
  NS_ENSURE_STATE(mFrame.IsAlive());
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aDOMEvent));
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_INVALID_ARG);

  nsNativeKeyEvent nativeEvent;
  nsINativeKeyBindings *bindings = GetKeyBindings();
  if (bindings &&
      nsContentUtils::DOMEventToNativeKeyEvent(keyEvent, &nativeEvent, PR_FALSE)) {
    if (bindings->KeyUp(nativeEvent, DoCommandCallback, mFrame)) {
      aDOMEvent->PreventDefault();
    }
  }

  return NS_OK;
}
// END nsIDOMKeyListener

// BEGIN nsIEditorObserver

NS_IMETHODIMP
nsTextInputListener::EditAction()
{
  NS_ENSURE_STATE(mFrame.IsAlive());
  nsITextControlFrame* frameBase = do_QueryFrame(mFrame.GetFrame());
  nsTextControlFrame* frame = static_cast<nsTextControlFrame*> (frameBase);
  NS_ASSERTION(frame, "Where is our frame?");
  //
  // Update the undo / redo menus
  //
  nsCOMPtr<nsIEditor> editor;
  frame->GetEditor(getter_AddRefs(editor));

  nsCOMPtr<nsITransactionManager> manager;
  editor->GetTransactionManager(getter_AddRefs(manager));
  NS_ENSURE_TRUE(manager, NS_ERROR_FAILURE);

  // Get the number of undo / redo items
  PRInt32 numUndoItems = 0;
  PRInt32 numRedoItems = 0;
  manager->GetNumberOfUndoItems(&numUndoItems);
  manager->GetNumberOfRedoItems(&numRedoItems);
  if ((numUndoItems && !mHadUndoItems) || (!numUndoItems && mHadUndoItems) ||
      (numRedoItems && !mHadRedoItems) || (!numRedoItems && mHadRedoItems)) {
    // Modify the menu if undo or redo items are different
    UpdateTextInputCommands(NS_LITERAL_STRING("undo"));

    mHadUndoItems = numUndoItems != 0;
    mHadRedoItems = numRedoItems != 0;
  }

  if (!mFrame.IsAlive()) {
    return NS_OK;
  }

  // Make sure we know we were changed (do NOT set this to false if there are
  // no undo items; JS could change the value and we'd still need to save it)
  frame->SetValueChanged(PR_TRUE);

  if (!mSettingValue) {
    mTxtCtrlElement->OnValueChanged(PR_TRUE);
  }

  // Fire input event
  nsCOMPtr<nsIEditor_MOZILLA_2_0_BRANCH> editor20 = do_QueryInterface(editor);
  NS_ASSERTION(editor20, "Something is very wrong!");
  PRBool trusted = PR_FALSE;
  editor20->GetLastKeypressEventTrusted(&trusted);
  frame->FireOnInput(trusted);

  // mFrame may be dead after this, but we don't need to check for it, because
  // we are not uisng it in this function any more.

  return NS_OK;
}

// END nsIEditorObserver


nsresult
nsTextInputListener::UpdateTextInputCommands(const nsAString& commandsToUpdate)
{
  NS_ENSURE_STATE(mFrame.IsAlive());

  nsIContent* content = mFrame->GetContent();
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIDocument> doc = content->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsPIDOMWindow *domWindow = doc->GetWindow();
  NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

  return domWindow->UpdateCommands(commandsToUpdate);
}

nsINativeKeyBindings*
nsTextInputListener::GetKeyBindings()
{
  if (mTxtCtrlElement->IsTextArea()) {
    static PRBool sNoTextAreaBindings = PR_FALSE;

    if (!sNativeTextAreaBindings && !sNoTextAreaBindings) {
      CallGetService(NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "textarea",
                     &sNativeTextAreaBindings);

      if (!sNativeTextAreaBindings) {
        sNoTextAreaBindings = PR_TRUE;
      }
    }

    return sNativeTextAreaBindings;
  }

  static PRBool sNoInputBindings = PR_FALSE;
  if (!sNativeInputBindings && !sNoInputBindings) {
    CallGetService(NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "input",
                   &sNativeInputBindings);

    if (!sNativeInputBindings) {
      sNoInputBindings = PR_TRUE;
    }
  }

  return sNativeInputBindings;
}

// END nsTextInputListener

// nsTextEditorState

nsTextEditorState::nsTextEditorState(nsITextControlElement* aOwningElement)
  : mTextCtrlElement(aOwningElement),
    mBoundFrame(nsnull),
    mTextListener(nsnull),
    mEditorInitialized(PR_FALSE),
    mInitializing(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsTextEditorState);
}

nsTextEditorState::~nsTextEditorState()
{
  MOZ_COUNT_DTOR(nsTextEditorState);
  Clear();
}

void
nsTextEditorState::Clear()
{
  if (mBoundFrame) {
    // Oops, we still have a frame!
    // This should happen when the type of a text input control is being changed
    // to something which is not a text control.  In this case, we should pretend
    // that a frame is being destroyed, and clean up after ourselves properly.
    UnbindFromFrame(mBoundFrame);
    mEditor = nsnull;
  } else {
    // If we have a bound frame around, UnbindFromFrame will call DestroyEditor
    // for us.
    DestroyEditor();
  }
  NS_IF_RELEASE(mTextListener);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsTextEditorState)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsTextEditorState, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsTextEditorState, Release)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(nsTextEditorState)
  tmp->Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(nsTextEditorState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mSelCon, nsISelectionController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mEditor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRootNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPlaceholderDiv)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsFrameSelection*
nsTextEditorState::GetConstFrameSelection() {
  if (mSelCon)
    return mSelCon->GetConstFrameSelection();
  return nsnull;
}

nsIEditor*
nsTextEditorState::GetEditor()
{
  if (!mEditor) {
    nsresult rv = PrepareEditor();
    NS_ENSURE_SUCCESS(rv, nsnull);
  }
  return mEditor;
}

nsISelectionController*
nsTextEditorState::GetSelectionController() const
{
  return mSelCon;
}

// Helper class, used below in BindToFrame().
class PrepareEditorEvent : public nsRunnable {
public:
  PrepareEditorEvent(nsTextEditorState &aState,
                     nsIContent *aOwnerContent,
                     const nsAString &aCurrentValue)
    : mState(aState)
    , mOwnerContent(aOwnerContent)
    , mCurrentValue(aCurrentValue)
  {
  }

  NS_IMETHOD Run() {
    // Transfer the saved value to the editor if we have one
    const nsAString *value = nsnull;
    if (!mCurrentValue.IsEmpty()) {
      value = &mCurrentValue;
    }

    mState.PrepareEditor(value);

    return NS_OK;
  }

private:
  nsTextEditorState &mState;
  nsCOMPtr<nsIContent> mOwnerContent; // strong reference
  nsAutoString mCurrentValue;
};

nsresult
nsTextEditorState::BindToFrame(nsTextControlFrame* aFrame)
{
  NS_ASSERTION(aFrame, "The frame to bind to should be valid");
  NS_ENSURE_ARG_POINTER(aFrame);

  NS_ASSERTION(!mBoundFrame, "Cannot bind twice, need to unbind first");
  NS_ENSURE_TRUE(!mBoundFrame, NS_ERROR_FAILURE);

  // If we'll need to transfer our current value to the editor, save it before
  // binding to the frame.
  nsAutoString currentValue;
  if (mEditor) {
    GetValue(currentValue, PR_TRUE);
  }

  mBoundFrame = aFrame;

  nsIContent *rootNode = GetRootNode();

  nsIPresShell *shell = mBoundFrame->PresContext()->GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  // Create selection
  nsresult rv;
  nsCOMPtr<nsFrameSelection> frameSel;
  frameSel = do_CreateInstance(kFrameSelectionCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a SelectionController
  mSelCon = new nsTextInputSelectionImpl(frameSel, shell, rootNode);
  NS_ENSURE_TRUE(mSelCon, NS_ERROR_OUT_OF_MEMORY);
  mTextListener = new nsTextInputListener(mTextCtrlElement);
  NS_ENSURE_TRUE(mTextListener, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(mTextListener);

  mTextListener->SetFrame(mBoundFrame);
  mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);

  // Get the caret and make it a selection listener.
  nsRefPtr<nsISelection> domSelection;
  if (NS_SUCCEEDED(mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                         getter_AddRefs(domSelection))) &&
      domSelection) {
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(domSelection));
    nsRefPtr<nsCaret> caret = shell->GetCaret();
    nsCOMPtr<nsISelectionListener> listener;
    if (caret) {
      listener = do_QueryInterface(caret);
      if (listener) {
        selPriv->AddSelectionListener(listener);
      }
    }

    selPriv->AddSelectionListener(static_cast<nsISelectionListener*>
                                             (mTextListener));
  }

  // If an editor exists from before, prepare it for usage
  if (mEditor) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
    NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

    // Set the correct direction on the newly created root node
    PRUint32 flags;
    rv = mEditor->GetFlags(&flags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (flags & nsIPlaintextEditor::eEditorRightToLeft) {
      rootNode->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("rtl"), PR_FALSE);
    } else if (flags & nsIPlaintextEditor::eEditorLeftToRight) {
      rootNode->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("ltr"), PR_FALSE);
    } else {
      // otherwise, inherit the content node's direction
    }

    if (!nsContentUtils::AddScriptRunner(
          new PrepareEditorEvent(*this, content, currentValue)))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
nsTextEditorState::PrepareEditor(const nsAString *aValue)
{
  if (!mBoundFrame) {
    // Cannot create an editor without a bound frame.
    // Don't return a failure code, because js callers can't handle that.
    return NS_OK;
  }

  if (mEditorInitialized) {
    // Do not initialize the editor multiple times.
    return NS_OK;
  }

  // Don't attempt to initialize recursively!
  InitializationGuard guard(*this);
  if (guard.IsInitializingRecursively()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Note that we don't check mEditor here, because we might already have one
  // around, in which case we don't create a new one, and we'll just tie the
  // required machinery to it.

  nsPresContext *presContext = mBoundFrame->PresContext();
  nsIPresShell *shell = presContext->GetPresShell();

  // Setup the editor flags
  PRUint32 editorFlags = 0;
  if (IsPlainTextControl())
    editorFlags |= nsIPlaintextEditor::eEditorPlaintextMask;
  if (IsSingleLineTextControl())
    editorFlags |= nsIPlaintextEditor::eEditorSingleLineMask;
  if (IsPasswordTextControl())
    editorFlags |= nsIPlaintextEditor::eEditorPasswordMask;

  // All nsTextControlFrames are widgets
  editorFlags |= nsIPlaintextEditor::eEditorWidgetMask;

  // Use async reflow and painting for text widgets to improve
  // performance.
  editorFlags |= nsIPlaintextEditor::eEditorUseAsyncUpdatesMask;

  PRBool shouldInitializeEditor = PR_FALSE;
  nsCOMPtr<nsIEditor> newEditor; // the editor that we might create
  nsresult rv;
  if (!mEditor) {
    shouldInitializeEditor = PR_TRUE;

    // Create an editor
    newEditor = do_CreateInstance(kTextEditorCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure we clear out the non-breaking space before we initialize the editor
    rv = mBoundFrame->UpdateValueDisplay(PR_FALSE, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    if (aValue || !mEditorInitialized) {
      // Set the correct value in the root node
      rv = mBoundFrame->UpdateValueDisplay(PR_TRUE, !mEditorInitialized, aValue);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    newEditor = mEditor; // just pretend that we have a new editor!
  }

  if (!mEditorInitialized) {
    // Now initialize the editor.
    //
    // NOTE: Conversion of '\n' to <BR> happens inside the
    //       editor's Init() call.

    // Get the DOM document
    nsCOMPtr<nsIDOMDocument> domdoc = do_QueryInterface(shell->GetDocument());
    if (!domdoc)
      return NS_ERROR_FAILURE;

    // What follows is a bit of a hack.  The editor uses the public DOM APIs
    // for its content manipulations, and it causes it to fail some security
    // checks deep inside when initializing.  So we push a null JSContext
    // on the JS stack here to make it clear that we're native code.
    // Note that any script that's directly trying to access our value
    // has to be going through some scriptable object to do that and that
    // already does the relevant security checks.
    nsCxPusher pusher;
    pusher.PushNull();

    rv = newEditor->Init(domdoc, GetRootNode(), mSelCon, editorFlags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Initialize the controller for the editor

  if (!SuppressEventHandlers(presContext)) {
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsIDOMHTMLInputElement> inputElement =
      do_QueryInterface(mTextCtrlElement);
    if (inputElement) {
      rv = inputElement->GetControllers(getter_AddRefs(controllers));
    } else {
      nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textAreaElement =
        do_QueryInterface(mTextCtrlElement);

      if (!textAreaElement)
        return NS_ERROR_FAILURE;

      rv = textAreaElement->GetControllers(getter_AddRefs(controllers));
    }

    NS_ENSURE_SUCCESS(rv, rv);

    if (controllers) {
      PRUint32 numControllers;
      PRBool found = PR_FALSE;
      rv = controllers->GetControllerCount(&numControllers);
      for (PRUint32 i = 0; i < numControllers; i ++) {
        nsCOMPtr<nsIController> controller;
        rv = controllers->GetControllerAt(i, getter_AddRefs(controller));
        if (NS_SUCCEEDED(rv) && controller) {
          nsCOMPtr<nsIControllerContext> editController =
            do_QueryInterface(controller);
          if (editController) {
            editController->SetCommandContext(newEditor);
            found = PR_TRUE;
          }
        }
      }
      if (!found)
        rv = NS_ERROR_FAILURE;
    }
  }

  if (shouldInitializeEditor) {
    // Initialize the plaintext editor
    nsCOMPtr<nsIPlaintextEditor> textEditor(do_QueryInterface(newEditor));
    if (textEditor) {
      // Set up wrapping
      textEditor->SetWrapColumn(GetWrapCols());

      // Set max text field length
      PRInt32 maxLength;
      if (GetMaxLength(&maxLength)) { 
        textEditor->SetMaxTextLength(maxLength);
      }
    }
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
  if (content) {
    rv = newEditor->GetFlags(&editorFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    // Check if the readonly attribute is set.
    if (content->HasAttr(kNameSpaceID_None, nsGkAtoms::readonly))
      editorFlags |= nsIPlaintextEditor::eEditorReadonlyMask;

    // Check if the disabled attribute is set.
    // TODO: call IsDisabled() here!
    if (content->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) 
      editorFlags |= nsIPlaintextEditor::eEditorDisabledMask;

    // Disable the selection if necessary.
    if (editorFlags & nsIPlaintextEditor::eEditorDisabledMask)
      mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);

    newEditor->SetFlags(editorFlags);
  }

  // Get the current value of the textfield from the content.
  // Note that if we've created a new editor, mEditor is null at this stage,
  // so we will get the real value from the content.
  nsAutoString defaultValue;
  if (aValue) {
    defaultValue = *aValue;
  } else {
    GetValue(defaultValue, PR_TRUE);
  }

  if (shouldInitializeEditor) {
    // Hold on to the newly created editor
    mEditor = newEditor;
  }

  // If we have a default value, insert it under the div we created
  // above, but be sure to use the editor so that '*' characters get
  // displayed for password fields, etc. SetValue() will call the
  // editor for us.

  if (!defaultValue.IsEmpty()) {
    // Avoid causing reentrant painting and reflowing by telling the editor
    // that we don't want it to force immediate view refreshes or force
    // immediate reflows during any editor calls.

    rv = newEditor->SetFlags(editorFlags |
                             nsIPlaintextEditor::eEditorUseAsyncUpdatesMask);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now call SetValue() which will make the necessary editor calls to set
    // the default value.  Make sure to turn off undo before setting the default
    // value, and turn it back on afterwards. This will make sure we can't undo
    // past the default value.

    rv = newEditor->EnableUndo(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    SetValue(defaultValue, PR_FALSE);

    rv = newEditor->EnableUndo(PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv),"Transaction Manager must have failed");

    // Now restore the original editor flags.
    rv = newEditor->SetFlags(editorFlags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsITransactionManager> transMgr;
  newEditor->GetTransactionManager(getter_AddRefs(transMgr));
  NS_ENSURE_TRUE(transMgr, NS_ERROR_FAILURE);

  transMgr->SetMaxTransactionCount(nsITextControlElement::DEFAULT_UNDO_CAP);

  if (IsPasswordTextControl()) {
    // Disable undo for password textfields.  Note that we want to do this at
    // the very end of InitEditor, so the calls to EnableUndo when setting the
    // default value don't screw us up.
    // Since changing the control type does a reframe, we don't have to worry
    // about dynamic type changes here.
    newEditor->EnableUndo(PR_FALSE);
  }

  if (!mEditorInitialized) {
    newEditor->PostCreate();
    mEditorInitialized = PR_TRUE;
  }

  if (mTextListener)
    newEditor->AddEditorObserver(mTextListener);

  // Restore our selection after being bound to a new frame
  if (mSelState) {
    nsContentUtils::AddScriptRunner(new RestoreSelectionState(mBoundFrame, mSelState->mStart, mSelState->mEnd));
    mSelState = nsnull;
  }

  return rv;
}

void
nsTextEditorState::DestroyEditor()
{
  // notify the editor that we are going away
  if (mEditorInitialized) {
    if (mTextListener)
      mEditor->RemoveEditorObserver(mTextListener);

    mEditor->PreDestroy(PR_TRUE);
    mEditorInitialized = PR_FALSE;
  }
}

void
nsTextEditorState::UnbindFromFrame(nsTextControlFrame* aFrame)
{
  NS_ENSURE_TRUE(mBoundFrame, );

  // If it was, however, it should be unbounded from the same frame.
  NS_ASSERTION(!aFrame || aFrame == mBoundFrame, "Unbinding from the wrong frame");
  NS_ENSURE_TRUE(!aFrame || aFrame == mBoundFrame, );

  // We need to start storing the value outside of the editor if we're not
  // going to use it anymore, so retrieve it for now.
  nsAutoString value;
  GetValue(value, PR_TRUE);

  // Save our selection state if needed.
  // Note that nsTextControlFrame::GetSelectionRange attempts to initialize the
  // editor before grabbing the range, and because this is not an acceptable
  // side effect for unbinding from a text control frame, we need to call
  // GetSelectionRange before calling DestroyEditor, and only if
  // mEditorInitialized indicates that we actually have an editor available.
  if (mEditorInitialized) {
    mSelState = new SelectionState();
    nsresult rv = mBoundFrame->GetSelectionRange(&mSelState->mStart, &mSelState->mEnd);
    if (NS_FAILED(rv)) {
      mSelState = nsnull;
    }
  }

  // Destroy our editor
  DestroyEditor();

  // Clean up the controller
  if (!SuppressEventHandlers(mBoundFrame->PresContext()))
  {
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsIDOMHTMLInputElement> inputElement =
      do_QueryInterface(mTextCtrlElement);
    if (inputElement)
      inputElement->GetControllers(getter_AddRefs(controllers));
    else
    {
      nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textAreaElement =
        do_QueryInterface(mTextCtrlElement);
      if (textAreaElement) {
        textAreaElement->GetControllers(getter_AddRefs(controllers));
      }
    }

    if (controllers)
    {
      PRUint32 numControllers;
      nsresult rv = controllers->GetControllerCount(&numControllers);
      NS_ASSERTION((NS_SUCCEEDED(rv)), "bad result in gfx text control destructor");
      for (PRUint32 i = 0; i < numControllers; i ++)
      {
        nsCOMPtr<nsIController> controller;
        rv = controllers->GetControllerAt(i, getter_AddRefs(controller));
        if (NS_SUCCEEDED(rv) && controller)
        {
          nsCOMPtr<nsIControllerContext> editController = do_QueryInterface(controller);
          if (editController)
          {
            editController->SetCommandContext(nsnull);
          }
        }
      }
    }
  }

  if (mSelCon) {
    if (mTextListener) {
      nsRefPtr<nsISelection> domSelection;
      if (NS_SUCCEEDED(mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                             getter_AddRefs(domSelection))) &&
          domSelection) {
        nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(domSelection));

        selPriv->RemoveSelectionListener(static_cast<nsISelectionListener*>
                                         (mTextListener));
      }
    }

    mSelCon->SetScrollableFrame(nsnull);
    mSelCon = nsnull;
  }

  if (mTextListener)
  {
    mTextListener->SetFrame(nsnull);

    nsCOMPtr<nsIDOMEventGroup> systemGroup;
    nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
    content->GetSystemEventGroup(getter_AddRefs(systemGroup));
    nsCOMPtr<nsIDOM3EventTarget> dom3Targ = do_QueryInterface(mTextCtrlElement);
    if (dom3Targ) {
      // cast because of ambiguous base
      nsIDOMEventListener *listener = static_cast<nsIDOMKeyListener*>
                                                 (mTextListener);

      dom3Targ->RemoveGroupedEventListener(NS_LITERAL_STRING("keydown"),
                                           listener, PR_FALSE, systemGroup);
      dom3Targ->RemoveGroupedEventListener(NS_LITERAL_STRING("keypress"),
                                           listener, PR_FALSE, systemGroup);
      dom3Targ->RemoveGroupedEventListener(NS_LITERAL_STRING("keyup"),
                                           listener, PR_FALSE, systemGroup);
    }

    NS_RELEASE(mTextListener);
    mTextListener = nsnull;
  }

  mBoundFrame = nsnull;

  // Now that we don't have a frame any more, store the value in the text buffer.
  SetValue(value, PR_FALSE);

  if (mRootNode && mMutationObserver) {
    mRootNode->RemoveMutationObserver(mMutationObserver);
    mMutationObserver = nsnull;
  }

  // Unbind the anonymous content from the tree.
  // We actually hold a reference to the content nodes so that
  // they're not actually destroyed.
  nsContentUtils::DestroyAnonymousContent(&mRootNode);
  nsContentUtils::DestroyAnonymousContent(&mPlaceholderDiv);
}

nsresult
nsTextEditorState::CreateRootNode()
{
  NS_ENSURE_TRUE(!mRootNode, NS_ERROR_UNEXPECTED);
  NS_ENSURE_ARG_POINTER(mBoundFrame);

  nsIPresShell *shell = mBoundFrame->PresContext()->GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsIDocument *doc = shell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // Now create a DIV and add it to the anonymous content child list.
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::div, nsnull,
                                                 kNameSpaceID_XHTML);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = NS_NewHTMLElement(getter_AddRefs(mRootNode), nodeInfo.forget(),
                                  NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the necessary classes on the text control. We use class values
  // instead of a 'style' attribute so that the style comes from a user-agent
  // style sheet and is still applied even if author styles are disabled.
  nsAutoString classValue;
  classValue.AppendLiteral("anonymous-div");
  PRInt32 wrapCols = GetWrapCols();
  if (wrapCols >= 0) {
    classValue.AppendLiteral(" wrap");
  }
  if (!IsSingleLineTextControl()) {
    // We can't just inherit the overflow because setting visible overflow will
    // crash when the number of lines exceeds the height of the textarea and
    // setting -moz-hidden-unscrollable overflow (NS_STYLE_OVERFLOW_CLIP)
    // doesn't paint the caret for some reason.
    const nsStyleDisplay* disp = mBoundFrame->GetStyleDisplay();
    if (disp->mOverflowX != NS_STYLE_OVERFLOW_VISIBLE &&
        disp->mOverflowX != NS_STYLE_OVERFLOW_CLIP) {
      classValue.AppendLiteral(" inherit-overflow");
    }

    mMutationObserver = new nsAnonDivObserver(this);
    NS_ENSURE_TRUE(mMutationObserver, NS_ERROR_OUT_OF_MEMORY);
    mRootNode->AddMutationObserver(mMutationObserver);
  }
  rv = mRootNode->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                          classValue, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mBoundFrame->UpdateValueDisplay(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsTextEditorState::CreatePlaceholderNode()
{
#ifdef DEBUG
  {
    nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
    if (content) {
      nsAutoString placeholderTxt;
      content->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder,
                       placeholderTxt);
      nsContentUtils::RemoveNewlines(placeholderTxt);
      NS_ASSERTION(!placeholderTxt.IsEmpty(), "CreatePlaceholderNode() shouldn't \
be called if @placeholder is the empty string when trimmed from line breaks");
    }
  }
#endif // DEBUG

  NS_ENSURE_TRUE(!mPlaceholderDiv, NS_ERROR_UNEXPECTED);
  NS_ENSURE_ARG_POINTER(mBoundFrame);

  nsIPresShell *shell = mBoundFrame->PresContext()->GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsIDocument *doc = shell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsNodeInfoManager* pNodeInfoManager = doc->NodeInfoManager();
  NS_ENSURE_TRUE(pNodeInfoManager, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  nsCOMPtr<nsIContent> placeholderText;

  // Create a DIV for the placeholder
  // and add it to the anonymous content child list
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = pNodeInfoManager->GetNodeInfo(nsGkAtoms::div, nsnull,
                                           kNameSpaceID_XHTML);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  rv = NS_NewHTMLElement(getter_AddRefs(mPlaceholderDiv), nodeInfo.forget(),
                         NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the text node for the placeholder text before doing anything else
  rv = NS_NewTextNode(getter_AddRefs(placeholderText), pNodeInfoManager);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPlaceholderDiv->AppendChildTo(placeholderText, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // initialize the text
  UpdatePlaceholderText(PR_FALSE);

  return NS_OK;
}

PRBool
nsTextEditorState::GetMaxLength(PRInt32* aMaxLength)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
  NS_ENSURE_TRUE(content, PR_FALSE);
  nsGenericHTMLElement* element = nsGenericHTMLElement::FromContent(content);
  NS_ENSURE_TRUE(element, PR_FALSE);

  const nsAttrValue* attr = element->GetParsedAttr(nsGkAtoms::maxlength);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    *aMaxLength = attr->GetIntegerValue();

    return PR_TRUE;
  }

  return PR_FALSE;
}

void
nsTextEditorState::GetValue(nsAString& aValue, PRBool aIgnoreWrap) const
{
  if (mEditor && mBoundFrame && (mEditorInitialized || !IsSingleLineTextControl())) {
    PRBool canCache = aIgnoreWrap && !IsSingleLineTextControl();
    if (canCache && !mCachedValue.IsEmpty()) {
      aValue = mCachedValue;
      return;
    }

    aValue.Truncate(); // initialize out param

    PRUint32 flags = (nsIDocumentEncoder::OutputLFLineBreak |
                      nsIDocumentEncoder::OutputPreformatted |
                      nsIDocumentEncoder::OutputPersistNBSP);

    if (IsPlainTextControl())
    {
      flags |= nsIDocumentEncoder::OutputBodyOnly;
    }

    if (!aIgnoreWrap) {
      nsITextControlElement::nsHTMLTextWrap wrapProp;
      nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
      if (content &&
          nsITextControlElement::GetWrapPropertyEnum(content, wrapProp) &&
          wrapProp == nsITextControlElement::eHTMLTextWrap_Hard) {
        flags |= nsIDocumentEncoder::OutputWrap;
      }
    }

    // What follows is a bit of a hack.  The problem is that we could be in
    // this method because we're being destroyed for whatever reason while
    // script is executing.  If that happens, editor will run with the
    // privileges of the executing script, which means it may not be able to
    // access its own DOM nodes!  Let's try to deal with that by pushing a null
    // JSContext on the JSContext stack to make it clear that we're native
    // code.  Note that any script that's directly trying to access our value
    // has to be going through some scriptable object to do that and that
    // already does the relevant security checks.
    // XXXbz if we could just get the textContent of our anonymous content (eg
    // if plaintext editor didn't create <br> nodes all over), we wouldn't need
    // this.
    { /* Scope for context pusher */
      nsCxPusher pusher;
      pusher.PushNull();

      mEditor->OutputToString(NS_LITERAL_STRING("text/plain"), flags,
                              aValue);
    }
    if (canCache) {
      mCachedValue = aValue;
    } else {
      mCachedValue.Truncate();
    }
  } else {
    if (!mTextCtrlElement->ValueChanged() || !mValue) {
      mTextCtrlElement->GetDefaultValueFromContent(aValue);
    } else {
      aValue = NS_ConvertUTF8toUTF16(*mValue);
    }
  }
}

void
nsTextEditorState::SetValue(const nsAString& aValue, PRBool aUserInput)
{
  if (mEditor && mBoundFrame) {
    // The InsertText call below might flush pending notifications, which
    // could lead into a scheduled PrepareEditor to be called.  That will
    // lead to crashes (or worse) because we'd be initializing the editor
    // before InsertText returns.  This script blocker makes sure that
    // PrepareEditor cannot be called prematurely.
    nsAutoScriptBlocker scriptBlocker;

    PRBool fireChangeEvent = mBoundFrame->GetFireChangeEventState();
    if (aUserInput) {
      mBoundFrame->SetFireChangeEventState(PR_TRUE);
    }

#ifdef DEBUG
    if (IsSingleLineTextControl()) {
      NS_ASSERTION(mEditorInitialized || mInitializing,
                   "We should never try to use the editor if we're not initialized unless we're being initialized");
    }
#endif

    nsAutoString currentValue;
    if (!mEditorInitialized && IsSingleLineTextControl()) {
      // Grab the current value directly from the text node to make sure that we
      // deal with stale data correctly.
      NS_ASSERTION(mRootNode, "We should have a root node here");
      nsIContent *textContent = mRootNode->GetChildAt(0);
      nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(textContent);
      if (textNode) {
        textNode->GetData(currentValue);
      }
    } else {
      mBoundFrame->GetText(currentValue);
    }

    nsWeakFrame weakFrame(mBoundFrame);

    // this is necessary to avoid infinite recursion
    if (!currentValue.Equals(aValue))
    {
      nsTextControlFrame::ValueSetter valueSetter(mBoundFrame,
                                                  mBoundFrame->mFocusedValue.Equals(currentValue));

      // \r is an illegal character in the dom, but people use them,
      // so convert windows and mac platform linebreaks to \n:
      // Unfortunately aValue is declared const, so we have to copy
      // in order to do this substitution.
      nsString newValue(aValue);
      if (aValue.FindChar(PRUnichar('\r')) != -1) {
        nsContentUtils::PlatformToDOMLineBreaks(newValue);
      }

      nsCOMPtr<nsIDOMDocument> domDoc;
      mEditor->GetDocument(getter_AddRefs(domDoc));
      if (!domDoc) {
        NS_WARNING("Why don't we have a document?");
        return;
      }

      // Time to mess with our security context... See comments in GetValue()
      // for why this is needed.  Note that we have to do this up here, because
      // otherwise SelectAll() will fail.
      { /* Scope for context pusher */
        nsCxPusher pusher;
        pusher.PushNull();

        nsCOMPtr<nsISelection> domSel;
        nsCOMPtr<nsISelectionPrivate> selPriv;
        mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                              getter_AddRefs(domSel));
        if (domSel)
        {
          selPriv = do_QueryInterface(domSel);
          if (selPriv)
            selPriv->StartBatchChanges();
        }

        nsCOMPtr<nsISelectionController> kungFuDeathGrip = mSelCon.get();
        PRUint32 currentLength = currentValue.Length();
        PRUint32 newlength = newValue.Length();
        if (!currentLength ||
            !StringBeginsWith(newValue, currentValue)) {
          // Replace the whole text.
          currentLength = 0;
          mSelCon->SelectAll();
        } else {
          // Collapse selection to the end so that we can append data.
          mBoundFrame->SelectAllOrCollapseToEndOfText(PR_FALSE);
        }
        const nsAString& insertValue =
          StringTail(newValue, newlength - currentLength);
        nsCOMPtr<nsIPlaintextEditor> plaintextEditor = do_QueryInterface(mEditor);
        if (!plaintextEditor || !weakFrame.IsAlive()) {
          NS_WARNING("Somehow not a plaintext editor?");
          return;
        }

        valueSetter.Init();

        // get the flags, remove readonly and disabled, set the value,
        // restore flags
        PRUint32 flags, savedFlags;
        mEditor->GetFlags(&savedFlags);
        flags = savedFlags;
        flags &= ~(nsIPlaintextEditor::eEditorDisabledMask);
        flags &= ~(nsIPlaintextEditor::eEditorReadonlyMask);
        flags |= nsIPlaintextEditor::eEditorUseAsyncUpdatesMask;
        flags |= nsIPlaintextEditor::eEditorDontEchoPassword;
        mEditor->SetFlags(flags);

        mTextListener->SettingValue(PR_TRUE);

        // Also don't enforce max-length here
        PRInt32 savedMaxLength;
        plaintextEditor->GetMaxTextLength(&savedMaxLength);
        plaintextEditor->SetMaxTextLength(-1);

        if (insertValue.IsEmpty()) {
          mEditor->DeleteSelection(nsIEditor::eNone);
        } else {
          plaintextEditor->InsertText(insertValue);
        }

        mTextListener->SettingValue(PR_FALSE);

        if (!weakFrame.IsAlive()) {
          // If the frame was destroyed because of a flush somewhere inside
          // InsertText, mBoundFrame here will be false.  But it's also possible
          // for the frame to go away because of another reason (such as deleting
          // the existing selection -- see bug 574558), in which case we don't
          // need to reset the value here.
          if (!mBoundFrame) {
            SetValue(newValue, PR_FALSE);
          }
          valueSetter.Cancel();
          return;
        }

        if (!IsSingleLineTextControl()) {
          mCachedValue = newValue;
        }

        plaintextEditor->SetMaxTextLength(savedMaxLength);
        mEditor->SetFlags(savedFlags);
        if (selPriv)
          selPriv->EndBatchChanges();
      }
    }

    // This second check _shouldn't_ be necessary, but let's be safe.
    if (!weakFrame.IsAlive()) {
      return;
    }
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(mBoundFrame->GetFirstChild(nsnull));
    if (scrollableFrame)
    {
      // Scroll the upper left corner of the text control's
      // content area back into view.
      scrollableFrame->ScrollTo(nsPoint(0, 0), nsIScrollableFrame::INSTANT);
    }

    if (aUserInput) {
      mBoundFrame->SetFireChangeEventState(fireChangeEvent);
    }
  } else {
    if (!mValue) {
      mValue = new nsCString;
    }
    nsString value(aValue);
    nsContentUtils::PlatformToDOMLineBreaks(value);
    CopyUTF16toUTF8(value, *mValue);

    // Update the frame display if needed
    if (mBoundFrame) {
      mBoundFrame->UpdateValueDisplay(PR_TRUE);
    }
  }

  // If we've reached the point where the root node has been created, we
  // can assume that it's safe to notify.
  ValueWasChanged(!!mRootNode);

  mTextCtrlElement->OnValueChanged(!!mRootNode);
}

void
nsTextEditorState::InitializeKeyboardEventListeners()
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);

  //register key listeners
  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  content->GetSystemEventGroup(getter_AddRefs(systemGroup));
  nsCOMPtr<nsIDOM3EventTarget> dom3Targ = do_QueryInterface(content);
  if (dom3Targ) {
    // cast because of ambiguous base
    nsIDOMEventListener *listener = static_cast<nsIDOMKeyListener*>
                                               (mTextListener);

    dom3Targ->AddGroupedEventListener(NS_LITERAL_STRING("keydown"),
                                      listener, PR_FALSE, systemGroup);
    dom3Targ->AddGroupedEventListener(NS_LITERAL_STRING("keypress"),
                                      listener, PR_FALSE, systemGroup);
    dom3Targ->AddGroupedEventListener(NS_LITERAL_STRING("keyup"),
                                      listener, PR_FALSE, systemGroup);
  }

  mSelCon->SetScrollableFrame(do_QueryFrame(mBoundFrame->GetFirstChild(nsnull)));
}

/* static */ void
nsTextEditorState::ShutDown()
{
  NS_IF_RELEASE(sNativeTextAreaBindings);
  NS_IF_RELEASE(sNativeInputBindings);
}

void
nsTextEditorState::ValueWasChanged(PRBool aNotify)
{
  // placeholder management
  if (!mPlaceholderDiv) {
    return;
  }

  PRBool showPlaceholder = PR_FALSE;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
  if (!nsContentUtils::IsFocusedContent(content)) {
    // If the content is focused, we don't care about the changes because
    // the placeholder is going to be hidden/shown on blur.
    nsAutoString valueString;
    GetValue(valueString, PR_TRUE);
    showPlaceholder = valueString.IsEmpty();
  }
  SetPlaceholderClass(showPlaceholder, aNotify);
}

void
nsTextEditorState::UpdatePlaceholderText(PRBool aNotify)
{
  NS_ASSERTION(mPlaceholderDiv, "This function should not be called if "
                                "mPlaceholderDiv isn't set");

  // If we don't have a placeholder div, there's nothing to do.
  if (!mPlaceholderDiv)
    return;

  nsAutoString placeholderValue;

  nsCOMPtr<nsIContent> content = do_QueryInterface(mTextCtrlElement);
  content->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder, placeholderValue);
  nsContentUtils::RemoveNewlines(placeholderValue);
  NS_ASSERTION(mPlaceholderDiv->GetChildAt(0), "placeholder div has no child");
  mPlaceholderDiv->GetChildAt(0)->SetText(placeholderValue, aNotify);
  ValueWasChanged(aNotify);
}

void
nsTextEditorState::SetPlaceholderClass(PRBool aVisible,
                                       PRBool aNotify)
{
  NS_ASSERTION(mPlaceholderDiv, "This function should not be called if "
                                "mPlaceholderDiv isn't set");

  // No need to do anything if we don't have a frame yet
  if (!mBoundFrame)
    return;

  nsAutoString classValue;

  classValue.Assign(NS_LITERAL_STRING("anonymous-div placeholder"));

  if (!aVisible)
    classValue.AppendLiteral(" hidden");

  nsIContent* placeholderDiv = GetPlaceholderNode();
  NS_ENSURE_TRUE(placeholderDiv, );

  placeholderDiv->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                          classValue, aNotify);
}

NS_IMPL_ISUPPORTS1(nsAnonDivObserver, nsIMutationObserver)

void
nsAnonDivObserver::CharacterDataChanged(nsIDocument*             aDocument,
                                        nsIContent*              aContent,
                                        CharacterDataChangeInfo* aInfo)
{
  mTextEditorState->ClearValueCache();
}

void
nsAnonDivObserver::ContentAppended(nsIDocument* aDocument,
                                   nsIContent*  aContainer,
                                   nsIContent*  aFirstNewContent,
                                   PRInt32      /* unused */)
{
  mTextEditorState->ClearValueCache();
}

void
nsAnonDivObserver::ContentInserted(nsIDocument* aDocument,
                                   nsIContent*  aContainer,
                                   nsIContent*  aChild,
                                   PRInt32      /* unused */)
{
  mTextEditorState->ClearValueCache();
}

void
nsAnonDivObserver::ContentRemoved(nsIDocument* aDocument,
                                  nsIContent*  aContainer,
                                  nsIContent*  aChild,
                                  PRInt32      aIndexInContainer,
                                  nsIContent*  aPreviousSibling)
{
  mTextEditorState->ClearValueCache();
}
