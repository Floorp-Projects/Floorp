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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Ross <blakeross@telocity.com>
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
#include "nsTextControlFrame.h"
#include "nsIDocument.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIFormControl.h"
#include "nsIServiceManager.h"
#include "nsIFrameSelection.h"
#include "nsIPlaintextEditor.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsFormControlHelper.h"
#include "nsIDocumentEncoder.h"
#include "nsICaret.h"
#include "nsISelectionListener.h"
#include "nsISelectionPrivate.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIControllerContext.h"
#include "nsIHTMLContent.h"
#include "nsIEditorIMESupport.h"
#include "nsIPhonetic.h"
#include "nsIEditorObserver.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIScrollableView.h"
#include "nsIScrollableFrame.h" //to turn off scroll bars
#include "nsFormControlFrame.h" //for registering accesskeys
#include "nsIDeviceContext.h" // to measure fonts
#include "nsIPresState.h" //for saving state

#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#ifdef USE_QI_IN_SUPPRESS_EVENT_HANDLERS
#include "nsIPrintContext.h"
#include "nsIPrintPreviewContext.h"
#endif // USE_QI_IN_SUPPRESS_EVENT_HANDLERS
#include "nsHTMLAtoms.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsISupportsArray.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIPresShell.h"
#include "nsIComponentManager.h"

#include "nsBoxLayoutState.h"
#include "nsLayoutAtoms.h" //getframetype
//for keylistener for "return" check
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDocument.h" //observe documents to send onchangenotifications
#include "nsIStyleSheet.h"//observe documents to send onchangenotifications
#include "nsIStyleRule.h"//observe documents to send onchangenotifications
#include "nsIDOMEventListener.h"//observe documents to send onchangenotifications
#include "nsGUIEvent.h"
#include "nsIDOMEventGroup.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIEventStateManager.h"

#include "nsIDOMFocusListener.h" //onchange events
#include "nsIDOMCharacterData.h" //for selection setting helper func
#include "nsIDOMNodeList.h" //for selection setting helper func
#include "nsIDOMRange.h" //for selection setting helper func
#include "nsIScriptGlobalObject.h" //needed for notify selection changed to update the menus ect.
#include "nsIDOMWindowInternal.h" //needed for notify selection changed to update the menus ect.
#include "nsITextContent.h" //needed to create initial text control content
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsITextControlElement.h"

#include "nsIEditorObserver.h"
#include "nsITransactionManager.h"
#include "nsIDOMText.h" //for multiline getselection
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"

#ifdef IBMBIDI
#include "nsIBidiKeyboard.h"
#include "nsWidgetsCID.h"
#endif // IBMBIDI

#define DEFAULT_COLUMN_WIDTH 20

#include "nsContentCID.h"
static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);

static NS_DEFINE_CID(kTextEditorCID, NS_TEXTEDITOR_CID);
static NS_DEFINE_CID(kFrameSelectionCID, NS_FRAMESELECTION_CID);

static const PRInt32 DEFAULT_COLS = 20;
static const PRInt32 DEFAULT_ROWS = 1;
static const PRInt32 DEFAULT_ROWS_TEXTAREA = 2;

class nsTextInputListener : public nsISelectionListener,
                            public nsIDOMFocusListener,
                            public nsIEditorObserver,
                            public nsSupportsWeakReference
{
public:
  /** the default constructor
   */ 
  nsTextInputListener();
  /** the default destructor. virtual due to the possibility of derivation.
   */
  virtual ~nsTextInputListener();

  /** SetEditor gives an address to the editor that will be accessed
   *  @param aEditor the editor this listener calls for editing operations
   */
  void SetFrame(nsTextControlFrame *aFrame){mFrame = aFrame;}

  NS_DECL_ISUPPORTS

  NS_DECL_NSISELECTIONLISTENER

  /** nsIDOMFocusListener interfaces 
    * used to propagate focus, blur, and change notifications
    * @see nsIDOMFocusListener
    */
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur (nsIDOMEvent* aEvent);
  /* END interfaces from nsIDOMFocusListener*/

  NS_DECL_NSIEDITOROBSERVER

protected:

  nsresult  UpdateTextInputCommands(const nsAString& commandsToUpdate);

protected:

  nsTextControlFrame* mFrame;  // weak reference
  
  PRPackedBool    mSelectionWasCollapsed;
  PRPackedBool    mKnowSelectionCollapsed;
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
};


/*
 * nsTextEditorListener implementation
 */

nsTextInputListener::nsTextInputListener()
: mFrame(nsnull)
, mSelectionWasCollapsed(PR_TRUE)
, mKnowSelectionCollapsed(PR_FALSE)
, mHadUndoItems(PR_FALSE)
, mHadRedoItems(PR_FALSE)
{
}

nsTextInputListener::~nsTextInputListener() 
{
}

NS_IMPL_ISUPPORTS5(nsTextInputListener, nsISelectionListener,
                   nsIDOMEventListener, nsIDOMFocusListener,
                   nsIEditorObserver, nsISupportsWeakReference)

// BEGIN nsIDOMSelectionListener

NS_IMETHODIMP
nsTextInputListener::NotifySelectionChanged(nsIDOMDocument* aDoc, nsISelection* aSel, PRInt16 aReason)
{
  PRBool collapsed;
  if (!mFrame || !aDoc || !aSel || NS_FAILED(aSel->GetIsCollapsed(&collapsed)))
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
    nsCOMPtr<nsIContent> content;
    mFrame->GetFormContent(*getter_AddRefs(content));
    if (content) 
    {
      nsCOMPtr<nsIDocument> doc = content->GetDocument();
      if (doc) 
      {
        nsIPresShell *presShell = doc->GetShellAt(0);
        if (presShell) 
        {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsEvent event(NS_FORM_SELECTED);

          presShell->HandleEventWithTarget(&event,mFrame,content,NS_EVENT_FLAG_INIT,&status);
        }
      }
    }
  }

  // if the collapsed state did not change, don't fire notifications
  if (mKnowSelectionCollapsed && collapsed == mSelectionWasCollapsed)
    return NS_OK;
  
  mSelectionWasCollapsed = collapsed;
  mKnowSelectionCollapsed = PR_TRUE;

  return UpdateTextInputCommands(NS_LITERAL_STRING("select"));
}

// END nsIDOMSelectionListener

// BEGIN nsIFocusListener

NS_IMETHODIMP
nsTextInputListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTextInputListener::Focus(nsIDOMEvent* aEvent)
{
  if (!mFrame)
    return NS_OK;

  nsCOMPtr<nsIEditor> editor;
  mFrame->GetEditor(getter_AddRefs(editor));
  if (editor) {
    editor->AddEditorObserver(this);
  }

  return mFrame->InitFocusedValue();
}

NS_IMETHODIMP
nsTextInputListener::Blur(nsIDOMEvent* aEvent)
{
  if (!mFrame)
    return NS_OK;

  nsCOMPtr<nsIEditor> editor;
  mFrame->GetEditor(getter_AddRefs(editor));
  if (editor) {
    editor->RemoveEditorObserver(this);
  }

  return mFrame->CheckFireOnChange();
}

// END nsIFocusListener


// BEGIN nsIEditorObserver

NS_IMETHODIMP
nsTextInputListener::EditAction()
{
  //
  // Update the undo / redo menus
  //
  nsCOMPtr<nsIEditor> editor;
  mFrame->GetEditor(getter_AddRefs(editor));

  nsCOMPtr<nsITransactionManager> manager;
  editor->GetTransactionManager(getter_AddRefs(manager));
  NS_ENSURE_TRUE(manager, NS_ERROR_FAILURE);

  // Get the number of undo / redo items
  PRInt32 numUndoItems = 0;
  PRInt32 numRedoItems = 0;
  manager->GetNumberOfUndoItems(&numUndoItems);
  manager->GetNumberOfRedoItems(&numRedoItems);
  if (numUndoItems && !mHadUndoItems || !numUndoItems && mHadUndoItems ||
      numRedoItems && !mHadRedoItems || !numRedoItems && mHadRedoItems) {
    // Modify the menu if undo or redo items are different
    UpdateTextInputCommands(NS_LITERAL_STRING("undo"));

    mHadUndoItems = numUndoItems != 0;
    mHadRedoItems = numRedoItems != 0;
  }

  // Make sure we know we were changed (do NOT set this to false if there are
  // no undo items; JS could change the value and we'd still need to save it)
  mFrame->SetValueChanged(PR_TRUE);

  // Fire input event
  mFrame->FireOnInput();

  return NS_OK;
}

// END nsIEditorObserver


nsresult
nsTextInputListener::UpdateTextInputCommands(const nsAString& commandsToUpdate)
{
  nsIContent* content = mFrame->GetContent();
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIDocument> doc = content->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(doc->GetScriptGlobalObject());
  NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

  return domWindow->UpdateCommands(commandsToUpdate);
}


// END nsTextInputListener

#ifdef XP_MAC
#pragma mark -
#endif
  
class nsTextInputSelectionImpl : public nsSupportsWeakReference, public nsISelectionController, public nsIFrameSelection
{
public:
  NS_DECL_ISUPPORTS

  nsTextInputSelectionImpl(nsIFrameSelection *aSel, nsIPresShell *aShell, nsIContent *aLimiter);
  ~nsTextInputSelectionImpl(){}

  
  //NSISELECTIONCONTROLLER INTERFACES
  NS_IMETHOD SetDisplaySelection(PRInt16 toggle);
  NS_IMETHOD GetDisplaySelection(PRInt16 *_retval);
  NS_IMETHOD SetSelectionFlags(PRInt16 aInEnable);
  NS_IMETHOD GetSelectionFlags(PRInt16 *aOutEnable);
  NS_IMETHOD GetSelection(PRInt16 type, nsISelection **_retval);
  NS_IMETHOD ScrollSelectionIntoView(PRInt16 aType, PRInt16 aRegion, PRBool aIsSynchronous);
  NS_IMETHOD RepaintSelection(PRInt16 type);
  NS_IMETHOD RepaintSelection(nsPresContext* aPresContext, SelectionType aSelectionType);
  NS_IMETHOD SetCaretEnabled(PRBool enabled);
  NS_IMETHOD SetCaretWidth(PRInt16 twips);
  NS_IMETHOD SetCaretReadOnly(PRBool aReadOnly);
  NS_IMETHOD GetCaretEnabled(PRBool *_retval);
  NS_IMETHOD SetCaretVisibilityDuringSelection(PRBool aVisibility);
  NS_IMETHOD CharacterMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD WordMove(PRBool aForward, PRBool aExtend);
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

  // NSIFRAMESELECTION INTERFACES
  NS_IMETHOD Init(nsIFocusTracker *aTracker, nsIContent *aLimiter) ;
  NS_IMETHOD ShutDown() ;
  NS_IMETHOD HandleTextEvent(nsGUIEvent *aGuiEvent) ;
  NS_IMETHOD HandleKeyEvent(nsPresContext* aPresContext, nsGUIEvent *aGuiEvent);
  NS_IMETHOD HandleClick(nsIContent *aNewFocus, PRUint32 aContentOffset, PRUint32 aContentEndOffset , 
                       PRBool aContinueSelection, PRBool aMultipleSelection, PRBool aHint); 
  NS_IMETHOD HandleDrag(nsPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint);
  NS_IMETHOD HandleTableSelection(nsIContent *aParentContent, PRInt32 aContentOffset, PRInt32 aTarget, nsMouseEvent *aMouseEvent);
  NS_IMETHOD StartAutoScrollTimer(nsPresContext *aPresContext, nsIView* aView, nsPoint& aPoint, PRUint32 aDelay);
  NS_IMETHOD StopAutoScrollTimer();
  NS_IMETHOD EnableFrameNotification(PRBool aEnable);
  NS_IMETHOD LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                             SelectionDetails **aReturnDetails, PRBool aSlowCheck);
  NS_IMETHOD SetMouseDownState(PRBool aState);
  NS_IMETHOD GetMouseDownState(PRBool *aState);
  NS_IMETHOD SetDelayCaretOverExistingSelection(PRBool aDelay);
  NS_IMETHOD GetDelayCaretOverExistingSelection(PRBool *aDelay);
  NS_IMETHOD SetDelayedCaretData(nsMouseEvent *aMouseEvent);
  NS_IMETHOD GetDelayedCaretData(nsMouseEvent **aMouseEvent);
  NS_IMETHOD GetLimiter(nsIContent **aLimiterContent);
  NS_IMETHOD GetTableCellSelection(PRBool *aState);
  NS_IMETHOD GetFrameForNodeOffset(nsIContent *aNode, PRInt32 aOffset, HINT aHint, nsIFrame **aReturnFrame, PRInt32 *aReturnOffset);
  NS_IMETHOD AdjustOffsetsFromStyle(nsIFrame *aFrame, PRBool *changeSelection,
      nsIContent** outContent, PRInt32* outStartOffset, PRInt32* outEndOffset);
  NS_IMETHOD GetHint(nsIFrameSelection::HINT *aHint);
  NS_IMETHOD SetHint(nsIFrameSelection::HINT aHint);
  NS_IMETHOD SetScrollableView(nsIScrollableView *aScrollableView);
  NS_IMETHOD GetScrollableView(nsIScrollableView **aScrollableView);
  NS_IMETHOD CommonPageMove(PRBool aForward, PRBool aExtend, nsIScrollableView *aScrollableView, nsIFrameSelection *aFrameSel);
  NS_IMETHOD SetMouseDoubleDown(PRBool aDoubleDown);
  NS_IMETHOD GetMouseDoubleDown(PRBool *aDoubleDown);
  NS_IMETHOD MaintainSelection();
#ifdef IBMBIDI
  NS_IMETHOD GetPrevNextBidiLevels(nsPresContext *aPresContext,
                                   nsIContent *aNode,
                                   PRUint32 aContentOffset,
                                   nsIFrame **aPrevFrame,
                                   nsIFrame **aNextFrame,
                                   PRUint8 *aPrevLevel,
                                   PRUint8 *aNextLevel);
  NS_IMETHOD GetFrameFromLevel(nsPresContext *aPresContext,
                               nsIFrame *aFrameIn,
                               nsDirection aDirection,
                               PRUint8 aBidiLevel,
                               nsIFrame **aFrameOut);
#endif
  //END INTERFACES


  nsWeakPtr &GetPresShell(){return mPresShellWeak;}
private:
  nsCOMPtr<nsIFrameSelection> mFrameSelection;
  nsCOMPtr<nsIContent>        mLimiter;
  nsWeakPtr mPresShellWeak;
#ifdef IBMBIDI
  nsCOMPtr<nsIBidiKeyboard> mBidiKeyboard;
#endif
};

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS3(nsTextInputSelectionImpl, nsISelectionController, nsISupportsWeakReference, nsIFrameSelection)


// BEGIN nsTextInputSelectionImpl

nsTextInputSelectionImpl::nsTextInputSelectionImpl(nsIFrameSelection *aSel, nsIPresShell *aShell, nsIContent *aLimiter)
{
  if (aSel && aShell)
  {
    mFrameSelection = aSel;//we are the owner now!
    nsCOMPtr<nsIFocusTracker> tracker = do_QueryInterface(aShell);
    mLimiter = aLimiter;
    mFrameSelection->Init(tracker, mLimiter);
    mPresShellWeak = do_GetWeakReference(aShell);
#ifdef IBMBIDI
    mBidiKeyboard = do_GetService("@mozilla.org/widget/bidikeyboard;1");
#endif
  }
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetDisplaySelection(PRInt16 aToggle)
{
  if (mFrameSelection)
    return mFrameSelection->SetDisplaySelection(aToggle);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetDisplaySelection(PRInt16 *aToggle)
{
  if (mFrameSelection)
    return mFrameSelection->GetDisplaySelection(aToggle);
  return NS_ERROR_NULL_POINTER;
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
  if (mFrameSelection)
    return mFrameSelection->GetSelection(type, _retval);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollSelectionIntoView(PRInt16 aType, PRInt16 aRegion, PRBool aIsSynchronous)
{
  if (mFrameSelection)
    return mFrameSelection->ScrollSelectionIntoView(aType, aRegion, aIsSynchronous);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::RepaintSelection(PRInt16 type)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
  if (presShell)
  {
    nsCOMPtr<nsPresContext> context;
    if (NS_SUCCEEDED(presShell->GetPresContext(getter_AddRefs(context))) && context)
    {
      return mFrameSelection->RepaintSelection(context, type);
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::RepaintSelection(nsPresContext* aPresContext, SelectionType aSelectionType)
{
  return RepaintSelection(aSelectionType);
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
nsTextInputSelectionImpl::SetCaretWidth(PRInt16 pixels)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    nsCOMPtr<nsICaret> caret;
    if (NS_SUCCEEDED(result = shell->GetCaret(getter_AddRefs(caret))))
    {
        return caret->SetCaretWidth(pixels);
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetCaretReadOnly(PRBool aReadOnly)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    nsCOMPtr<nsICaret> caret;
    if (NS_SUCCEEDED(result = shell->GetCaret(getter_AddRefs(caret))))
    {
      nsCOMPtr<nsISelection> domSel;
      if (NS_SUCCEEDED(result = mFrameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel))))
      {
        return caret->SetCaretReadOnly(aReadOnly);
      }
    }

  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetCaretEnabled(PRBool *_retval)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    nsCOMPtr<nsICaret> caret;
    if (NS_SUCCEEDED(result = shell->GetCaret(getter_AddRefs(caret))))
    {
      nsCOMPtr<nsISelection> domSel;
      if (NS_SUCCEEDED(result = mFrameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel))))
      {
        return caret->GetCaretVisible(_retval);
      }
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
    nsCOMPtr<nsICaret> caret;
    if (NS_SUCCEEDED(result = shell->GetCaret(getter_AddRefs(caret))))
    {
      nsCOMPtr<nsISelection> domSel;
      if (NS_SUCCEEDED(result = mFrameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel))))
      {
        return caret->SetVisibilityDuringSelection(aVisibility);
      }
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
nsTextInputSelectionImpl::WordMove(PRBool aForward, PRBool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->WordMove(aForward, aExtend);
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

  if (mPresShellWeak)
  {
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
    if (!presShell)
      return NS_ERROR_NULL_POINTER;

    //get the scroll view
    nsIScrollableView *scrollableView;
    nsresult result = GetScrollableView(&scrollableView);
    if (NS_FAILED(result))
      return result;

    CommonPageMove(aForward, aExtend, scrollableView, this);
  }
  return ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CompleteScroll(PRBool aForward)
{
  nsIScrollableView *scrollableView;
  nsresult result;
  result = GetScrollableView(&scrollableView);
  if (NS_FAILED(result))
    return result;
  if (!scrollableView)
    return NS_ERROR_NOT_INITIALIZED;

  return scrollableView->ScrollByWhole(!aForward); //TRUE = top, aForward TRUE=bottom
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CompleteMove(PRBool aForward, PRBool aExtend)
{
  // grab the parent / root DIV for this text widget
  nsresult result;
  nsCOMPtr<nsIContent> parentDIV;
  result = GetLimiter(getter_AddRefs(parentDIV));
  if (NS_FAILED(result))
    return result;
  if (!parentDIV)
    return NS_ERROR_UNEXPECTED;

  // make the caret be either at the very beginning (0) or the very end
  PRInt32 offset = 0;
  HINT hint = HINTLEFT;
  if (aForward)
  {
    offset = parentDIV->GetChildCount();

    // Prevent the caret from being placed after the last
    // BR node in the content tree!

    if (offset > 0)
    {
      nsIContent *child = parentDIV->GetChildAt(offset - 1);

      if (child->Tag() == nsHTMLAtoms::br)
      {
        --offset;
        hint = HINTRIGHT; // for Bug 106855
      }
    }
  }

  result = mFrameSelection->HandleClick(parentDIV, offset, offset, aExtend, PR_FALSE, hint);

  // if we got this far, attempt to scroll no matter what the above result is
  return CompleteScroll(aForward);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollPage(PRBool aForward)
{
  nsIScrollableView *scrollableView;
  nsresult result;
  result = GetScrollableView(&scrollableView);
  if (NS_FAILED(result))
    return result;
  if (!scrollableView)
    return NS_ERROR_NOT_INITIALIZED;

  return scrollableView->ScrollByPages(0, aForward ? 1 : -1);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollLine(PRBool aForward)
{
  nsIScrollableView *scrollableView;
  nsresult result;
  result = GetScrollableView(&scrollableView);
  if (NS_FAILED(result))
    return result;
  if (!scrollableView)
    return NS_ERROR_NOT_INITIALIZED;

  // will we have bug #7354 because we aren't forcing an update here?
  return scrollableView->ScrollByLines(0, aForward ? 1 : -1);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollHorizontal(PRBool aLeft)
{
  nsIScrollableView *scrollableView;
  nsresult result;
  result = GetScrollableView(&scrollableView);
  if (NS_FAILED(result))
    return result;
  if (!scrollableView)
    return NS_ERROR_NOT_INITIALIZED;

  // will we have bug #7354 because we aren't forcing an update here?
  return scrollableView->ScrollByLines(aLeft ? -1 : 1, 0);
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


//nsTextInputSelectionImpl::FRAMESELECTIONAPIS

NS_IMETHODIMP
nsTextInputSelectionImpl::Init(nsIFocusTracker *aTracker, nsIContent *aLimiter)
{
  return mFrameSelection->Init(aTracker, aLimiter);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::ShutDown()
{
  return mFrameSelection->ShutDown();
}


NS_IMETHODIMP
nsTextInputSelectionImpl::HandleTextEvent(nsGUIEvent *aGuiEvent)
{
  return mFrameSelection->HandleTextEvent(aGuiEvent);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::HandleKeyEvent(nsPresContext* aPresContext, nsGUIEvent *aGuiEvent)
{
  return mFrameSelection->HandleKeyEvent(aPresContext, aGuiEvent);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::HandleClick(nsIContent *aNewFocus, PRUint32 aContentOffset, PRUint32 aContentEndOffset , 
                     PRBool aContinueSelection, PRBool aMultipleSelection, PRBool aHint)
{
  return mFrameSelection->HandleClick(aNewFocus, aContentOffset, aContentEndOffset , 
                     aContinueSelection, aMultipleSelection, aHint);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::HandleDrag(nsPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint)
{
  return mFrameSelection->HandleDrag(aPresContext, aFrame, aPoint);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::HandleTableSelection(nsIContent *aParentContent, PRInt32 aContentOffset, PRInt32 aTarget, nsMouseEvent *aMouseEvent)
{
  // We should never have a table inside a text control frame!
  NS_ASSERTION(PR_TRUE, "Calling HandleTableSelection inside nsTextControlFrame!");
  return NS_OK;
}


NS_IMETHODIMP
nsTextInputSelectionImpl::StartAutoScrollTimer(nsPresContext *aPresContext, nsIView *aView, nsPoint& aPoint, PRUint32 aDelay)
{
  return mFrameSelection->StartAutoScrollTimer(aPresContext, aView, aPoint, aDelay);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::StopAutoScrollTimer()
{
  return mFrameSelection->StopAutoScrollTimer();
}


NS_IMETHODIMP
nsTextInputSelectionImpl::EnableFrameNotification(PRBool aEnable)
{
  return mFrameSelection->EnableFrameNotification(aEnable);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                           SelectionDetails **aReturnDetails, PRBool aSlowCheck)
{
  return mFrameSelection->LookUpSelection(aContent, aContentOffset, aContentLength, aReturnDetails, aSlowCheck);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::SetMouseDownState(PRBool aState)
{
  return mFrameSelection->SetMouseDownState(aState);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::GetMouseDownState(PRBool *aState)
{
  return mFrameSelection->GetMouseDownState(aState);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetDelayCaretOverExistingSelection(PRBool aDelay)
{
  return mFrameSelection->SetDelayCaretOverExistingSelection(aDelay);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetDelayCaretOverExistingSelection(PRBool *aDelay)
{
  return mFrameSelection->GetDelayCaretOverExistingSelection(aDelay);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetDelayedCaretData(nsMouseEvent *aMouseEvent)
{
  return mFrameSelection->SetDelayedCaretData(aMouseEvent);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetDelayedCaretData(nsMouseEvent **aMouseEvent)
{
  return mFrameSelection->GetDelayedCaretData(aMouseEvent);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetLimiter(nsIContent **aLimiterContent)
{
  return mFrameSelection->GetLimiter(aLimiterContent);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetTableCellSelection(PRBool *aState)
{
  return mFrameSelection->GetTableCellSelection(aState);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::GetFrameForNodeOffset(nsIContent *aNode, PRInt32 aOffset, HINT aHint, nsIFrame **aReturnFrame, PRInt32 *aReturnOffset)
{
  return mFrameSelection->GetFrameForNodeOffset(aNode, aOffset, aHint,aReturnFrame,aReturnOffset);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::AdjustOffsetsFromStyle(nsIFrame *aFrame, PRBool *changeSelection,
      nsIContent** outContent, PRInt32* outStartOffset, PRInt32* outEndOffset)
{
  return mFrameSelection->AdjustOffsetsFromStyle(aFrame, changeSelection, outContent, outStartOffset, outEndOffset);
}

NS_IMETHODIMP nsTextInputSelectionImpl::GetHint(nsIFrameSelection::HINT *aHint)
{
  return mFrameSelection->GetHint(aHint);
}

NS_IMETHODIMP nsTextInputSelectionImpl::SetHint(nsIFrameSelection::HINT aHint)
{
  return mFrameSelection->SetHint(aHint);
}

NS_IMETHODIMP nsTextInputSelectionImpl::SetScrollableView(nsIScrollableView *aScrollableView)
{
  return mFrameSelection->SetScrollableView(aScrollableView);
}

NS_IMETHODIMP nsTextInputSelectionImpl::GetScrollableView(nsIScrollableView **aScrollableView)
{
  return mFrameSelection->GetScrollableView(aScrollableView);
}

NS_IMETHODIMP nsTextInputSelectionImpl::SetMouseDoubleDown(PRBool aDoubleDown)
{
  return mFrameSelection->SetMouseDoubleDown(aDoubleDown);
}

NS_IMETHODIMP nsTextInputSelectionImpl::GetMouseDoubleDown(PRBool *aDoubleDown)
{
  return mFrameSelection->GetMouseDoubleDown(aDoubleDown);
}

NS_IMETHODIMP nsTextInputSelectionImpl::MaintainSelection()
{
  return mFrameSelection->MaintainSelection();
}

NS_IMETHODIMP nsTextInputSelectionImpl::CommonPageMove(PRBool aForward, PRBool aExtend, nsIScrollableView *aScrollableView, nsIFrameSelection *aFrameSel)
{
  return mFrameSelection->CommonPageMove(aForward, aExtend, aScrollableView, this);
}

#ifdef IBMBIDI
NS_IMETHODIMP nsTextInputSelectionImpl::GetPrevNextBidiLevels(nsPresContext *aPresContext,
                                                              nsIContent *aNode,
                                                              PRUint32 aContentOffset,
                                                              nsIFrame **aPrevFrame,
                                                              nsIFrame **aNextFrame,
                                                              PRUint8 *aPrevLevel,
                                                              PRUint8 *aNextLevel)
{
  if (mFrameSelection)
    return mFrameSelection->GetPrevNextBidiLevels(aPresContext, aNode, aContentOffset, aPrevFrame, aNextFrame, aPrevLevel, aNextLevel);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsTextInputSelectionImpl::GetFrameFromLevel(nsPresContext *aPresContext,
                                                          nsIFrame *aFrameIn,
                                                          nsDirection aDirection,
                                                          PRUint8 aBidiLevel,
                                                          nsIFrame **aFrameOut)
{
  if (mFrameSelection)
    return mFrameSelection->GetFrameFromLevel(aPresContext, aFrameIn, aDirection, aBidiLevel, aFrameOut);
  return NS_ERROR_FAILURE;
}
#endif // IBMBIDI

// END   nsTextInputSelectionImpl



nsresult
NS_NewTextControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTextControlFrame* it = new (aPresShell) nsTextControlFrame(aPresShell);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMPL_ADDREF_INHERITED(nsTextControlFrame, nsBoxFrame)
NS_IMPL_RELEASE_INHERITED(nsTextControlFrame, nsBoxFrame)
 

NS_IMETHODIMP
nsTextControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIFormControlFrame))) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIAnonymousContentCreator))) {
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsITextControlFrame))) {
    *aInstancePtr = (void*)(nsITextControlFrame*) this;
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIScrollableViewProvider)) && IsScrollable()) {
    *aInstancePtr = (void*)(nsIScrollableViewProvider*) this;
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIPhonetic))) {
    *aInstancePtr = (void*)(nsIPhonetic*) this;
    return NS_OK;
  }

  return nsBoxFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsTextControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLTextFieldAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

nsTextControlFrame::nsTextControlFrame(nsIPresShell* aShell)
  : nsStackFrame(aShell)
{
  mUseEditor = PR_FALSE;
  mIsProcessing = PR_FALSE;
  mNotifyOnInput = PR_FALSE;
  mSuggestedWidth = NS_FORMSIZE_NOTSET;
  mSuggestedHeight = NS_FORMSIZE_NOTSET;
  mScrollableView = nsnull;
  mDidPreDestroy = PR_FALSE;
}

nsTextControlFrame::~nsTextControlFrame()
{
  //delete mTextListener;
  //delete mTextSelImpl; dont delete this since mSelCon will release it.
}

static PRBool
SuppressEventHandlers(nsPresContext* aPresContext)
{
  PRBool suppressHandlers = PR_FALSE;

  if (aPresContext)
  {
    // Right now we only suppress event handlers and controller manipulation
    // when in a print preview or print context!

#ifdef USE_QI_IN_SUPPRESS_EVENT_HANDLERS

    // Using QI to see if we're printing or print previewing is more
    // accurate, but a bit more heavy weight then just checking
    // the pagination bool, which will return the right answer to us
    // with the current implementation.

    nsCOMPtr<nsIPrintContext> printContext = do_QueryInterface(aPresContext);
    if (printContext)
      suppressHandlers = PR_TRUE;
    else
    {
      nsCOMPtr<nsIPrintPreviewContext> printPreviewContext = do_QueryInterface(aPresContext);
      if (printPreviewContext)
        suppressHandlers = PR_TRUE;
    }

#else

    // In the current implementation, we only paginate when
    // printing or in print preview.

    suppressHandlers = aPresContext->IsPaginated();

#endif
  }

  return suppressHandlers;
}

void
nsTextControlFrame::PreDestroy(nsPresContext* aPresContext)
{
  // notify the editor that we are going away
  if (mEditor)
  {
    // If we were in charge of state before, relinquish it back
    // to the control.
    if (mUseEditor)
    {
      // First get the frame state from the editor
      nsAutoString value;
      GetValue(value, PR_TRUE);

      mUseEditor = PR_FALSE;

      // Next store the frame state in the control
      // (now that mUseEditor is false values get stored
      // in content).
      SetValue(value);
    }
    mEditor->PreDestroy();
  }
  
  // Clean up the controller

  if (!SuppressEventHandlers(aPresContext))
  {
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsIDOMNSHTMLInputElement> inputElement = do_QueryInterface(mContent);
    if (inputElement)
      inputElement->GetControllers(getter_AddRefs(controllers));
    else
    {
      nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textAreaElement = do_QueryInterface(mContent);
      textAreaElement->GetControllers(getter_AddRefs(controllers));
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

  mSelCon = 0;
  mEditor = 0;
  
//unregister self from content
  mTextListener->SetFrame(nsnull);
  nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  if (mTextListener)
  {
    nsCOMPtr<nsIDOMEventReceiver> erP = do_QueryInterface(mContent);
    if (erP)
    {
      erP->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMFocusListener  *,mTextListener), NS_GET_IID(nsIDOMFocusListener));
    }
  }

  mDidPreDestroy = PR_TRUE; 
}

NS_IMETHODIMP 
nsTextControlFrame::Destroy(nsPresContext* aPresContext)
{
  if (!mDidPreDestroy) {
    PreDestroy(aPresContext);
  }
  return nsBoxFrame::Destroy(aPresContext);
}

void 
nsTextControlFrame::RemovedAsPrimaryFrame(nsPresContext* aPresContext)
{
  if (!mDidPreDestroy) {
    PreDestroy(aPresContext);
  }
  else NS_ASSERTION(PR_FALSE, "RemovedAsPrimaryFrame called after PreDestroy");
}

nsIAtom*
nsTextControlFrame::GetType() const 
{ 
  return nsLayoutAtoms::textInputFrame;
} 

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsTextControlFrame::IsSingleLineTextControl() const
{
  PRInt32 type = GetFormControlType();
  return (type == NS_FORM_INPUT_TEXT) || (type == NS_FORM_INPUT_PASSWORD);
}

PRBool nsTextControlFrame::IsTextArea() const
{
  return mContent && mContent->Tag() == nsHTMLAtoms::textarea;
}

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsTextControlFrame::IsPlainTextControl() const
{
  // need to check HTML attribute of mContent and/or CSS.
  return PR_TRUE;
}

PRBool nsTextControlFrame::IsPasswordTextControl() const
{
  return GetFormControlType() == NS_FORM_INPUT_PASSWORD;
}


PRInt32
nsTextControlFrame::GetCols()
{
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent);
  NS_ASSERTION(content, "Content is not HTML content!");

  if (IsTextArea()) {
    nsHTMLValue attr;
    nsresult rv = content->GetHTMLAttribute(nsHTMLAtoms::cols, attr);
    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      PRInt32 cols = attr.GetIntValue();
      // XXX why a default of 1 char, why hide it
      return (cols <= 0) ? 1 : cols;
    }
  } else {
    // Else we know (assume) it is an input with size attr
    nsHTMLValue attr;
    nsresult rv = content->GetHTMLAttribute(nsHTMLAtoms::size, attr);
    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      PRInt32 cols = attr.GetIntValue();
      if (cols > 0) {
        return cols;
      }
    }
  }

  return DEFAULT_COLS;
}


PRInt32
nsTextControlFrame::GetRows()
{
  if (IsTextArea()) {
    nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent);
    NS_ASSERTION(content, "Content is not HTML content!");

    nsHTMLValue attr;
    nsresult rv = content->GetHTMLAttribute(nsHTMLAtoms::rows, attr);
    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      PRInt32 rows = attr.GetIntValue();
      return (rows <= 0) ? DEFAULT_ROWS_TEXTAREA : rows;
    }
    return DEFAULT_ROWS_TEXTAREA;
  }

  return DEFAULT_ROWS;
}


nsresult
nsTextControlFrame::ReflowStandard(nsPresContext*          aPresContext,
                                   nsSize&                  aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus)
{
  // get the css size and let the frame use or override it
  nsSize minSize;
  nsresult rv = CalculateSizeStandard(aPresContext, aReflowState.rendContext,
                                      aDesiredSize, minSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add in the size of the scrollbars for textarea
  if (IsTextArea()) {
    float p2t;
    p2t = aPresContext->PixelsToTwips();

    nsIDeviceContext *dx = aPresContext->DeviceContext();

    float   scale;
    dx->GetCanonicalPixelScale(scale);

    float sbWidth;
    float sbHeight;
    dx->GetScrollBarDimensions(sbWidth, sbHeight);

    nscoord scrollbarWidth  = PRInt32(sbWidth * scale);
    nscoord scrollbarHeight = PRInt32(sbHeight * scale);

    aDesiredSize.height += scrollbarHeight;
    minSize.height      += scrollbarHeight;

    aDesiredSize.width  += scrollbarWidth;
    minSize.width       += scrollbarWidth;
  }
  aDesiredSize.width  += aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;
  aDesiredSize.height += aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  return NS_OK;
}



nsresult
nsTextControlFrame::CalculateSizeStandard(nsPresContext*       aPresContext,
                                          nsIRenderingContext*  aRendContext,
                                          nsSize&               aDesiredSize,
                                          nsSize&               aMinSize)
{
  aDesiredSize.width  = CSS_NOTSET;
  aDesiredSize.height = CSS_NOTSET;

  // Get leading and the Average/MaxAdvance char width 
  nscoord fontHeight  = 0;
  nscoord charWidth   = 0;
  nscoord charMaxAdvance  = 0;

  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult rv = nsFormControlHelper::GetFrameFontFM(this, getter_AddRefs(fontMet));
  NS_ENSURE_SUCCESS(rv, rv);
  aRendContext->SetFont(fontMet);
  fontMet->GetHeight(fontHeight);
  fontMet->GetAveCharWidth(charWidth);
  fontMet->GetMaxAdvance(charMaxAdvance);

  // Set the width equal to the width in characters
  aDesiredSize.width = GetCols() * charWidth;

  // To better match IE, take the maximum character width(in twips) and remove
  // 4 pixels add this on as additional padding(internalPadding). But only do
  // this if charMaxAdvance != charWidth; if they are equal, this is almost
  // certainly a fixed-width font.
  if (charWidth != charMaxAdvance) {
    float p2t;
    p2t = aPresContext->PixelsToTwips();
    nscoord internalPadding = PR_MAX(charMaxAdvance - NSToCoordRound(4 * p2t), 0);
    // round to a multiple of p2t
    nscoord t = NSToCoordRound(p2t); 
    nscoord rest = internalPadding % t; 
    if (rest < t - rest) {
      internalPadding -= rest;
    } else {
      internalPadding += t - rest;
    }
    // Now add the extra padding on (so that small input sizes work well)
    aDesiredSize.width += internalPadding;
  } else {
    // This is to account for the anonymous <br> having a 1 twip width
    // in Full Standards mode, see BRFrame::Reflow and bug 228752.
    if (aPresContext->CompatibilityMode() == eCompatibility_FullStandards) {
      aDesiredSize.width += 1;
    }
  }

  // Set the height equal to total number of rows (times the height of each
  // line, of course)
  aDesiredSize.height = fontHeight * GetRows();

  // Set minimum size equal to desired size.  We are form controls.  We are Gods
  // among elements.  We do not yield for anybody, not even a table cell.  None
  // shall pass.
  aMinSize.width  = aDesiredSize.width;
  aMinSize.height = aDesiredSize.height;

  return NS_OK;
}

void nsTextControlFrame::PostCreateFrames() {
  InitEditor();
}

NS_IMETHODIMP
nsTextControlFrame::CreateFrameFor(nsPresContext*   aPresContext,
                                       nsIContent *      aContent,
                                       nsIFrame**        aFrame)
{
  *aFrame = nsnull;
  return NS_ERROR_FAILURE;
}

nsresult
nsTextControlFrame::InitEditor()
{
  // This method must be called during/after the text
  // control frame's initial reflow to avoid any unintened
  // forced reflows that might result when the editor
  // calls into DOM/layout code while trying to set the
  // initial string.
  //
  // This code used to be called from CreateAnonymousContent(),
  // but when the editor set the initial string, it would trigger
  // a PresShell listener which called FlushPendingNotifications()
  // during frame construction. This was causing other form controls
  // to display wrong values.

  // Check if this method has been called already.
  // If so, just return early.

  if (mUseEditor)
    return NS_OK;

  // If the editor is not here, then we can't use it, now can we?
  if (!mEditor)
    return NS_ERROR_NOT_INITIALIZED;

  // Get the current value of the textfield from the content.
  nsAutoString defaultValue;
  GetValue(defaultValue, PR_TRUE);

  // Turn on mUseEditor so that subsequent calls will use the
  // editor.
  mUseEditor = PR_TRUE;

  // If we have a default value, insert it under the div we created
  // above, but be sure to use the editor so that '*' characters get
  // displayed for password fields, etc. SetValue() will call the
  // editor for us.

  if (!defaultValue.IsEmpty()) {
    PRUint32 editorFlags = 0;

    nsresult rv = mEditor->GetFlags(&editorFlags);

    if (NS_FAILED(rv))
      return rv;

    // Avoid causing reentrant painting and reflowing by telling the editor
    // that we don't want it to force immediate view refreshes or force
    // immediate reflows during any editor calls.

    rv = mEditor->SetFlags(editorFlags |
                           nsIPlaintextEditor::eEditorUseAsyncUpdatesMask);

    if (NS_FAILED(rv))
      return rv;

    // Now call SetValue() which will make the neccessary editor calls to set
    // the default value.  Make sure to turn off undo before setting the default
    // value, and turn it back on afterwards. This will make sure we can't undo
    // past the default value.

    rv = mEditor->EnableUndo(PR_FALSE);

    if (NS_FAILED(rv))
      return rv;

    SetValue(defaultValue);

    rv = mEditor->EnableUndo(PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv),"Transaction Manager must have failed");
    // Now restore the original editor flags.

    rv = mEditor->SetFlags(editorFlags);

    if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTextControlFrame::CreateAnonymousContent(nsPresContext* aPresContext,
                                           nsISupportsArray& aChildList)
{
  // Get the PresShell

  mState |= NS_FRAME_INDEPENDENT_SELECTION;

  nsIPresShell *shell = aPresContext->GetPresShell();

  if (!shell)
    return NS_ERROR_FAILURE;

  // Get the DOM document

  nsCOMPtr<nsIDocument> doc;
  nsresult rv = shell->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(rv))
    return rv;
  if (!doc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDocument> domdoc = do_QueryInterface(doc, &rv);
  if (NS_FAILED(rv))
    return rv;
  if (!domdoc)
    return NS_ERROR_FAILURE;
  
  // Now create a DIV and add it to the anonymous content child list.
  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = doc->NodeInfoManager()->GetNodeInfo(nsHTMLAtoms::div, nsnull,
                                           kNameSpaceID_XHTML,
                                           getter_AddRefs(nodeInfo));

  if (NS_FAILED(rv))
    return rv;

  if (!nodeInfo)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> divContent;
  rv = NS_NewHTMLElement(getter_AddRefs(divContent), nodeInfo);

  if (NS_FAILED(rv))
    return rv;

  if (!divContent)
    return NS_ERROR_FAILURE;

  // Set the div native anonymous, so CSS will be its style language
  // no matter what.
  divContent->SetNativeAnonymous(PR_TRUE);

  // Set the neccessary style attributes on the text control.

  rv = divContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::kClass,
                           NS_LITERAL_STRING("anonymous-div"), PR_FALSE);

  if (!IsSingleLineTextControl()) {
    // We can't just inherit the overflow because setting visible overflow will
    // crash when the number of lines exceeds the height of the textarea and
    // setting -moz-hidden-unscrollable overflow (NS_STYLE_OVERFLOW_HIDDEN)
    // doesn't paint the caret for some reason.
    const nsStyleDisplay* disp = GetStyleDisplay();
    if (disp->mOverflow != NS_STYLE_OVERFLOW_AUTO &&  // this is the default
        disp->mOverflow != NS_STYLE_OVERFLOW_VISIBLE &&
        disp->mOverflow != NS_STYLE_OVERFLOW_HIDDEN) {
      rv = divContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::style,
                               NS_LITERAL_STRING("overflow: inherit;"),
                               PR_FALSE);
    }
  }

  if (NS_FAILED(rv))
    return rv;

  // rv = divContent->SetAttr(kNameSpaceID_None,nsXULAtoms::debug, NS_LITERAL_STRING("true"), PR_FALSE);
  rv = aChildList.AppendElement(divContent);

  if (NS_FAILED(rv))
    return rv;

  // Create an editor

  rv = nsComponentManager::CreateInstance(kTextEditorCID,
                                          nsnull,
                                          NS_GET_IID(nsIEditor), 
                                          getter_AddRefs(mEditor));
  if (NS_FAILED(rv))
    return rv;
  if (!mEditor) 
    return NS_ERROR_OUT_OF_MEMORY;

  // Create selection

  nsCOMPtr<nsIFrameSelection> frameSel;
  rv = nsComponentManager::CreateInstance(kFrameSelectionCID, nsnull,
                                                 NS_GET_IID(nsIFrameSelection),
                                                 getter_AddRefs(frameSel));

  // Create a SelectionController

  mTextSelImpl = new nsTextInputSelectionImpl(frameSel,shell,divContent);
  if (!mTextSelImpl)
    return NS_ERROR_OUT_OF_MEMORY;
  mTextListener = new nsTextInputListener();
  if (!mTextListener)
    return NS_ERROR_OUT_OF_MEMORY;
  mTextListener->SetFrame(this);
  mSelCon =  do_QueryInterface((nsISupports *)(nsISelectionController *)mTextSelImpl);//this will addref it once
  if (!mSelCon)
    return NS_ERROR_NO_INTERFACE;
  mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);

  // Setup the editor flags

  PRUint32 editorFlags = 0;
  if (IsPlainTextControl())
    editorFlags |= nsIPlaintextEditor::eEditorPlaintextMask;
  if (IsSingleLineTextControl())
    editorFlags |= nsIPlaintextEditor::eEditorSingleLineMask;
  if (IsPasswordTextControl())
    editorFlags |= nsIPlaintextEditor::eEditorPasswordMask;

  // All gfxtextcontrolframe2's are widgets
  editorFlags |= nsIPlaintextEditor::eEditorWidgetMask;

  // Use async reflow and painting for text widgets to improve
  // performance.

  // XXX: Using editor async updates exposes bugs 158782, 151882,
  //      and 165130, so we're disabling it for now, until they
  //      can be addressed.
  // editorFlags |= nsIPlaintextEditor::eEditorUseAsyncUpdatesMask;

  // Now initialize the editor.
  //
  // NOTE: Conversion of '\n' to <BR> happens inside the
  //       editor's Init() call.

  rv = mEditor->Init(domdoc, shell, divContent, mSelCon, editorFlags);

  if (NS_FAILED(rv))
    return rv;

  // Initialize the controller for the editor

  if (!SuppressEventHandlers(aPresContext))
  {
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsIDOMNSHTMLInputElement> inputElement = do_QueryInterface(mContent);
    if (inputElement)
      rv = inputElement->GetControllers(getter_AddRefs(controllers));
    else
    {
      nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textAreaElement = do_QueryInterface(mContent);

      if (!textAreaElement)
        return NS_ERROR_FAILURE;

      rv = textAreaElement->GetControllers(getter_AddRefs(controllers));
    }

    if (NS_FAILED(rv))
      return rv;

    if (controllers)
    {
      PRUint32 numControllers;
      PRBool found = PR_FALSE;
      rv = controllers->GetControllerCount(&numControllers);
      for (PRUint32 i = 0; i < numControllers; i ++)
      {
        nsCOMPtr<nsIController> controller;
        rv = controllers->GetControllerAt(i, getter_AddRefs(controller));
        if (NS_SUCCEEDED(rv) && controller)
        {
          nsCOMPtr<nsIControllerContext> editController = do_QueryInterface(controller);
          if (editController)
          {
            editController->SetCommandContext(mEditor);
            found = PR_TRUE;
          }
        }
      }
      if (!found)
        rv = NS_ERROR_FAILURE;
    }
  }

  // Initialize the plaintext editor
  nsCOMPtr<nsIPlaintextEditor> textEditor(do_QueryInterface(mEditor));
  if (textEditor) {
    // Set up wrapping
    if (IsTextArea()) {
      // wrap=off means -1 for wrap width no matter what cols is
      nsFormControlHelper::nsHTMLTextWrap wrapProp;
      nsFormControlHelper::GetWrapPropertyEnum(mContent, wrapProp);
      if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Off) {
        // do not wrap when wrap=off
        textEditor->SetWrapWidth(-1);
      } else {
        // Set wrapping normally otherwise
        textEditor->SetWrapWidth(GetCols());
      }
    } else {
      // Never wrap non-textareas
      textEditor->SetWrapWidth(-1);
    }


    // Set max text field length
    PRInt32 maxLength;
    rv = GetMaxLength(&maxLength);
    if (NS_CONTENT_ATTR_NOT_THERE != rv)
    { 
      textEditor->SetMaxTextLength(maxLength);
    }
  }
    
  // Get the caret and make it a selection listener.

  nsCOMPtr<nsISelection> domSelection;
  if (NS_SUCCEEDED(mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSelection))) && domSelection)
  {
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(domSelection));
    nsCOMPtr<nsICaret> caret;
    nsCOMPtr<nsISelectionListener> listener;
    if (NS_SUCCEEDED(shell->GetCaret(getter_AddRefs(caret))) && caret)
    {
      listener = do_QueryInterface(caret);
      if (listener)
      {
        selPriv->AddSelectionListener(listener);
      }
    }

    selPriv->AddSelectionListener(NS_STATIC_CAST(nsISelectionListener *, mTextListener));
  }
  
  if (mContent)
  {
    rv = mEditor->GetFlags(&editorFlags);

    if (NS_FAILED(rv))
      return rv;

    nsAutoString resultValue;

    // Check if the readonly attribute is set.

    rv = mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::readonly, resultValue);

    if (NS_FAILED(rv))
      return rv;

    if (NS_CONTENT_ATTR_NOT_THERE != rv)
      editorFlags |= nsIPlaintextEditor::eEditorReadonlyMask;

    // Check if the disabled attribute is set.

    rv = mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::disabled, resultValue);

    if (NS_FAILED(rv))
      return rv;

    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
      editorFlags |= nsIPlaintextEditor::eEditorDisabledMask;

    // Disable the caret and selection if neccessary.

    if (editorFlags & nsIPlaintextEditor::eEditorReadonlyMask ||
        editorFlags & nsIPlaintextEditor::eEditorDisabledMask)
    {
      if (mSelCon)
      {
         //do not turn caret enabled off at this time.  the caret will behave 
         //dependant on the focused frame it is in.  disabling it here has
         //an adverse affect on the browser in caret display mode or the editor
         //when a readonly/disabled text form is in the page. bug 141888
         //mSelCon->SetCaretEnabled(PR_FALSE);

        if (editorFlags & nsIPlaintextEditor::eEditorDisabledMask)
          mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
      }

      mEditor->SetFlags(editorFlags);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTextControlFrame::Reflow(nsPresContext*   aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTextControlFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // make sure the the form registers itself on the initial/first reflow
  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
    mNotifyOnInput = PR_TRUE;//its ok to notify now. all has been prepared.
  }

  nsresult rv = nsStackFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  if (NS_SUCCEEDED(rv))
  { // fix for bug 40596, width:auto means the control sets it's mMaxElementWidth to it's default width
    if (aDesiredSize.mComputeMEW)
    {
      const nsStylePosition* stylePosition = GetStylePosition();
      nsStyleUnit widthUnit = stylePosition->mWidth.GetUnit();
      if (eStyleUnit_Auto == widthUnit) {
        aDesiredSize.mMaxElementWidth = aDesiredSize.width;
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsTextControlFrame::Paint(nsPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect,
                              nsFramePaintLayer    aWhichLayer,
                              PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && !isVisible) {
    return NS_OK;
  }
  nsresult rv = NS_OK;
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND) {
    rv = nsStackFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_BACKGROUND);
    if (NS_FAILED(rv)) return rv;
    rv = nsStackFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FLOATS);
    if (NS_FAILED(rv)) return rv;
    rv = nsStackFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FOREGROUND);
  }
  DO_GLOBAL_REFLOW_COUNT_DSP("nsTextControlFrame", &aRenderingContext);
  return rv;
}

NS_IMETHODIMP
nsTextControlFrame::GetFrameForPoint(nsPresContext* aPresContext,
                                         const nsPoint& aPoint,
                                         nsFramePaintLayer aWhichLayer,
                                         nsIFrame** aFrame)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND) {
    rv = nsStackFrame::GetFrameForPoint(aPresContext, aPoint,
                                      NS_FRAME_PAINT_LAYER_FOREGROUND, aFrame);
    if (NS_SUCCEEDED(rv))
      return NS_OK;
    rv = nsStackFrame::GetFrameForPoint(aPresContext, aPoint,
                                        NS_FRAME_PAINT_LAYER_FLOATS, aFrame);
    if (NS_SUCCEEDED(rv))
      return NS_OK;
    rv = nsStackFrame::GetFrameForPoint(aPresContext, aPoint,
                                      NS_FRAME_PAINT_LAYER_BACKGROUND, aFrame);
  }
  return rv;
}

NS_IMETHODIMP
nsTextControlFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mPrefSize)) {
     aSize = mPrefSize;
     return NS_OK;
  }

#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  aSize.width = 0;
  aSize.height = 0;

  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed)
    return NS_OK;

  nsPresContext* presContext = aState.PresContext();
  const nsHTMLReflowState* reflowState = aState.GetReflowState();
  nsSize styleSize(CSS_NOTSET,CSS_NOTSET);
  nsFormControlFrame::GetStyleSize(presContext, *reflowState, styleSize);

  if (!reflowState)
    return NS_OK;

  if (mState & NS_FRAME_FIRST_REFLOW)
    mNotifyOnInput = PR_TRUE; //its ok to notify now. all has been prepared.

  nsReflowStatus status;
  nsresult rv = ReflowStandard(presContext, aSize, *reflowState, status);
  NS_ENSURE_SUCCESS(rv, rv);
  AddInset(aSize);

  mPrefSize = aSize;

#ifdef DEBUG_rods
  {
    nsMargin borderPadding(0,0,0,0);
    GetBorderAndPadding(borderPadding);
    nsSize size(169, 24);
    nsSize actual(aSize.width/15, 
                  aSize.height/15);
    printf("nsGfxText(field) %d,%d  %d,%d  %d,%d\n", 
           size.width, size.height, actual.width, actual.height, actual.width-size.width, actual.height-size.height);  // text field
  }
#endif

  return NS_OK;
}



NS_IMETHODIMP
nsTextControlFrame::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
#define FIX_FOR_BUG_40596
#ifdef FIX_FOR_BUG_40596
  aSize = mMinSize;
  return NS_OK;
#else
  return nsBox::GetMinSize(aState, aSize);
#endif
}

NS_IMETHODIMP
nsTextControlFrame::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  return nsBox::GetMaxSize(aState, aSize);
}

NS_IMETHODIMP
nsTextControlFrame::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  // First calculate the ascent of the text inside
  nsresult rv = nsStackFrame::GetAscent(aState, aAscent);
  NS_ENSURE_SUCCESS(rv, rv);
    
  // Now adjust the ascent for our borders and padding
  aAscent += aState.GetReflowState()->mComputedBorderPadding.top;
  
  return NS_OK;
}

//IMPLEMENTING NS_IFORMCONTROLFRAME
NS_IMETHODIMP
nsTextControlFrame::GetName(nsAString* aResult)
{
  return nsFormControlHelper::GetName(mContent, aResult);
}

NS_IMETHODIMP_(PRInt32)
nsTextControlFrame::GetFormControlType() const
{
  return nsFormControlHelper::GetType(mContent);
}

static PRBool
IsFocusedContent(nsPresContext* aPresContext, nsIContent* aContent)
{
  nsCOMPtr<nsIContent> focusedContent;
  aPresContext->EventStateManager()->
    GetFocusedContent(getter_AddRefs(focusedContent));
  return focusedContent == aContent;
}

void    nsTextControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  if (!aOn || !mSelCon)
    return;

  // onfocus="some_where_else.focus()" can trigger several focus
  // in succession. Here, we only care if we are the winner.
  // @see also nsTextEditorFocusListener::Focus()
  if (!IsFocusedContent(GetPresContext(), mContent))
    return;

  // tell the caret to use our selection

  nsCOMPtr<nsISelection> ourSel;
  mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, 
    getter_AddRefs(ourSel));
  if (!ourSel) return;

  nsIPresShell* presShell = GetPresContext()->GetPresShell();
  nsCOMPtr<nsICaret> caret;
  presShell->GetCaret(getter_AddRefs(caret));
  if (!caret) return;
  caret->SetCaretDOMSelection(ourSel);

  // mutual-exclusion: the selection is either controlled by the
  // document or by the text input/area. Clear any selection in the
  // document since the focus is now on our independent selection.

  nsCOMPtr<nsISelectionController> selCon(do_QueryInterface(presShell));
  nsCOMPtr<nsISelection> docSel;
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
    getter_AddRefs(docSel));
  if (!docSel) return;

  PRBool isCollapsed = PR_FALSE;
  docSel->GetIsCollapsed(&isCollapsed);
  if (!isCollapsed)
    docSel->RemoveAllRanges();
}

void    nsTextControlFrame::ScrollIntoView(nsPresContext* aPresContext)
{
  if (aPresContext) {
    nsIPresShell *presShell = aPresContext->GetPresShell();
    if (presShell) {
      presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    }
  }
}

void    nsTextControlFrame::MouseClicked(nsPresContext* aPresContext){}

nscoord 
nsTextControlFrame::GetVerticalInsidePadding(nsPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(0, aPixToTwip); 
}


//---------------------------------------------------------
nscoord 
nsTextControlFrame::GetHorizontalInsidePadding(nsPresContext* aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return GetVerticalInsidePadding(aPresContext, aPixToTwip, aInnerWidth);
}


NS_IMETHODIMP 
nsTextControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  mSuggestedWidth = aWidth;
  mSuggestedHeight = aHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsTextControlFrame::GetFormContent(nsIContent*& aContent) const
{
  aContent = GetContent();
  NS_IF_ADDREF(aContent);
  return NS_OK;
}

NS_IMETHODIMP nsTextControlFrame::SetProperty(nsPresContext* aPresContext, nsIAtom* aName, const nsAString& aValue)
{
  if (!mIsProcessing)//some kind of lock.
  {
    mIsProcessing = PR_TRUE;
    
    if (nsHTMLAtoms::value == aName) 
    {
      if (mEditor && mUseEditor) {
        // If the editor exists, the control needs to be informed that the value
        // has changed.
        SetValueChanged(PR_TRUE);
      }
      SetValue(aValue);   // set new text value
    }
    else if (nsHTMLAtoms::select == aName && mSelCon)
    {
      // Select all the text.
      //
      // XXX: This is lame, we can't call mEditor->SelectAll()
      //      because that triggers AutoCopies in unix builds.
      //      Instead, we have to call our own homegrown version
      //      of select all which merely builds a range that selects
      //      all of the content and adds that to the selection.

      SelectAllContents();
    }
    mIsProcessing = PR_FALSE;
  }
  return NS_OK;
}      

NS_IMETHODIMP
nsTextControlFrame::GetProperty(nsIAtom* aName, nsAString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If widget is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::value == aName) {
    GetValue(aValue, PR_FALSE);
  }
  return NS_OK;
}  



NS_IMETHODIMP
nsTextControlFrame::GetEditor(nsIEditor **aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);
  *aEditor = mEditor;
  NS_IF_ADDREF(*aEditor);
  return NS_OK;
}

NS_IMETHODIMP
nsTextControlFrame::OwnsValue(PRBool* aOwnsValue)
{
  NS_PRECONDITION(aOwnsValue, "aOwnsValue must be non-null");
  *aOwnsValue = mUseEditor;
  return NS_OK;
}

NS_IMETHODIMP
nsTextControlFrame::GetTextLength(PRInt32* aTextLength)
{
  NS_ENSURE_ARG_POINTER(aTextLength);

  nsAutoString   textContents;
  GetValue(textContents, PR_FALSE);   // this is expensive!
  *aTextLength = textContents.Length();
  return NS_OK;
}

nsresult
nsTextControlFrame::SetSelectionInternal(nsIDOMNode *aStartNode,
                                         PRInt32 aStartOffset,
                                         nsIDOMNode *aEndNode,
                                         PRInt32 aEndOffset)
{
  // Create a new range to represent the new selection.
  // Note that we use a new range to avoid having to do
  // isIncreasing checks to avoid possible errors.

  nsCOMPtr<nsIDOMRange> range = do_CreateInstance(kRangeCID);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  nsresult rv = range->SetStart(aStartNode, aStartOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = range->SetEnd(aEndNode, aEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the selection, clear it and add the new range to it!

  nsCOMPtr<nsISelection> selection;
  mTextSelImpl->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));  
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  rv = selection->RemoveAllRanges();  

  NS_ENSURE_SUCCESS(rv, rv);

  return selection->AddRange(range);
}

nsresult
nsTextControlFrame::SelectAllContents()
{
  if (!mEditor)
    return NS_OK;

  nsCOMPtr<nsIDOMElement> rootElement;
  nsresult rv = mEditor->GetRootElement(getter_AddRefs(rootElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> rootContent = do_QueryInterface(rootElement);
  PRInt32 numChildren = rootContent->GetChildCount();

  if (numChildren > 0) {
    // We never want to place the selection after the last
    // br under the root node!
    nsIContent *child = rootContent->GetChildAt(numChildren - 1);
    if (child) {
      if (child->Tag() == nsHTMLAtoms::br)
        --numChildren;
    }
  }

  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));

  return SetSelectionInternal(rootNode, 0, rootNode, numChildren);
}

nsresult
nsTextControlFrame::SetSelectionEndPoints(PRInt32 aSelStart, PRInt32 aSelEnd)
{
  NS_ASSERTION(aSelStart <= aSelEnd, "Invalid selection offsets!");

  if (aSelStart > aSelEnd)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;

  // Calculate the selection start point.

  nsresult rv = OffsetToDOMPoint(aSelStart, getter_AddRefs(startNode), &startOffset);

  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelStart == aSelEnd) {
    // Collapsed selection, so start and end are the same!
    endNode   = startNode;
    endOffset = startOffset;
  }
  else {
    // Selection isn't collapsed so we have to calculate
    // the end point too.

    rv = OffsetToDOMPoint(aSelEnd, getter_AddRefs(endNode), &endOffset);

    NS_ENSURE_SUCCESS(rv, rv);
  }

  return SetSelectionInternal(startNode, startOffset, endNode, endOffset);
}

NS_IMETHODIMP
nsTextControlFrame::SetSelectionRange(PRInt32 aSelStart, PRInt32 aSelEnd)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_INITIALIZED);
  
  if (aSelStart > aSelEnd) {
    // Simulate what we'd see SetSelectionStart() was called, followed
    // by a SetSelectionEnd().

    aSelStart   = aSelEnd;
  }

  return SetSelectionEndPoints(aSelStart, aSelEnd);
}


NS_IMETHODIMP
nsTextControlFrame::SetSelectionStart(PRInt32 aSelectionStart)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_INITIALIZED);

  PRInt32 selStart = 0, selEnd = 0; 

  nsresult rv = GetSelectionRange(&selStart, &selEnd);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelectionStart > selEnd) {
    // Collapse to the new start point.
    selEnd = aSelectionStart; 
  }

  selStart = aSelectionStart;
  
  return SetSelectionEndPoints(selStart, selEnd);
}

NS_IMETHODIMP
nsTextControlFrame::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_INITIALIZED);
  
  PRInt32 selStart = 0, selEnd = 0; 

  nsresult rv = GetSelectionRange(&selStart, &selEnd);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelectionEnd < selStart) {
    // Collapse to the new end point.
    selStart = aSelectionEnd; 
  }

  selEnd = aSelectionEnd;
  
  return SetSelectionEndPoints(selStart, selEnd);
}

nsresult
nsTextControlFrame::DOMPointToOffset(nsIDOMNode* aNode,
                                     PRInt32 aNodeOffset,
                                     PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aNode && aResult);

  *aResult = 0;

  nsCOMPtr<nsIDOMElement> rootElement;
  mEditor->GetRootElement(getter_AddRefs(rootElement));
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));

  NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNodeList> nodeList;

  nsresult rv = rootNode->GetChildNodes(getter_AddRefs(nodeList));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

  PRUint32 length = 0;
  rv = nodeList->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!length || aNodeOffset < 0)
    return NS_OK;

  PRInt32 i, textOffset = 0;
  PRInt32 lastIndex = (PRInt32)length - 1;

  for (i = 0; i < (PRInt32)length; i++) {
    if (rootNode == aNode && i == aNodeOffset) {
      *aResult = textOffset;
      return NS_OK;
    }

    nsCOMPtr<nsIDOMNode> item;
    rv = nodeList->Item(i, getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMText> domText(do_QueryInterface(item));

    if (domText) {
      PRUint32 textLength = 0;

      rv = domText->GetLength(&textLength);
      NS_ENSURE_SUCCESS(rv, rv);

      if (item == aNode) {
        NS_ASSERTION((aNodeOffset >= 0 && aNodeOffset <= (PRInt32)textLength),
                     "Invalid aNodeOffset!");
        *aResult = textOffset + aNodeOffset;
        return NS_OK;
      }

      textOffset += textLength;
    }
    else {
      // Must be a BR node. If it's not the last BR node
      // under the root, count it as a newline.

      if (i != lastIndex)
        ++textOffset;
    }
  }

  NS_ASSERTION((aNode == rootNode && aNodeOffset == (PRInt32)length),
               "Invalide node offset!");

  *aResult = textOffset;
  
  return NS_OK;
}

nsresult
nsTextControlFrame::OffsetToDOMPoint(PRInt32 aOffset,
                                     nsIDOMNode** aResult,
                                     PRInt32* aPosition)
{
  NS_ENSURE_ARG_POINTER(aResult && aPosition);

  *aResult = nsnull;
  *aPosition = 0;

  nsCOMPtr<nsIDOMElement> rootElement;
  mEditor->GetRootElement(getter_AddRefs(rootElement));
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));

  NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNodeList> nodeList;

  nsresult rv = rootNode->GetChildNodes(getter_AddRefs(nodeList));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

  PRUint32 length = 0;

  rv = nodeList->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!length || aOffset < 0) {
    *aPosition = 0;
    *aResult = rootNode;
    NS_ADDREF(*aResult);
    return NS_OK;
  }

  PRInt32 textOffset = 0;
  PRUint32 lastIndex = length - 1;

  for (PRUint32 i=0; i<length; i++) {
    nsCOMPtr<nsIDOMNode> item;
    rv = nodeList->Item(i, getter_AddRefs(item));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMText> domText(do_QueryInterface(item));

    if (domText) {
      PRUint32 textLength = 0;

      rv = domText->GetLength(&textLength);
      NS_ENSURE_SUCCESS(rv, rv);

      // Check if aOffset falls within this range.
      if (aOffset >= textOffset && aOffset <= textOffset+(PRInt32)textLength) {
        *aPosition = aOffset - textOffset;
        *aResult = item;
        NS_ADDREF(*aResult);
        return NS_OK;
      }

      textOffset += textLength;

      // If there aren't any more siblings after this text node,
      // return the point at the end of this text node!

      if (i == lastIndex) {
        *aPosition = textLength;
        *aResult = item;
        NS_ADDREF(*aResult);
        return NS_OK;
      }
    }
    else {
      // Must be a BR node, count it as a newline.

      if (aOffset == textOffset || i == lastIndex) {
        // We've found the correct position, or aOffset takes us
        // beyond the last child under rootNode, just return the point
        // under rootNode that is in front of this br.

        *aPosition = i;
        *aResult = rootNode;
        NS_ADDREF(*aResult);
        return NS_OK;
      }

      ++textOffset;
    }
  }

  NS_ASSERTION(0, "We should never get here!");

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextControlFrame::GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd)
{
  // make sure we have an editor
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_INITIALIZED);

  *aSelectionStart = 0;
  *aSelectionEnd = 0;

  nsCOMPtr<nsISelection> selection;
  nsresult rv = mTextSelImpl->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));  
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  PRInt32 numRanges = 0;
  selection->GetRangeCount(&numRanges);

  if (numRanges < 1)
    return NS_OK;

  // We only operate on the first range in the selection!

  nsCOMPtr<nsIDOMRange> firstRange;
  rv = selection->GetRangeAt(0, getter_AddRefs(firstRange));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(firstRange, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset = 0, endOffset = 0;

  // Get the start point of the range.

  rv = firstRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

  rv = firstRange->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the end point of the range.

  rv = firstRange->GetEndContainer(getter_AddRefs(endNode));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(endNode, NS_ERROR_FAILURE);

  rv = firstRange->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert the start point to a selection offset.

  rv = DOMPointToOffset(startNode, startOffset, aSelectionStart);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert the end point to a selection offset.

  return DOMPointToOffset(endNode, endOffset, aSelectionEnd);
}


NS_IMETHODIMP
nsTextControlFrame::GetSelectionContr(nsISelectionController **aSelCon)
{
  NS_ENSURE_ARG_POINTER(aSelCon);
  NS_IF_ADDREF(*aSelCon = mSelCon);
  return NS_OK;
}


/////END INTERFACE IMPLEMENTATIONS

////NSIFRAME
NS_IMETHODIMP
nsTextControlFrame::AttributeChanged(nsPresContext* aPresContext,
                                        nsIContent*     aChild,
                                        PRInt32         aNameSpaceID,
                                        nsIAtom*        aAttribute,
                                        PRInt32         aModType)
{
  if (!mEditor || !mSelCon) {return NS_ERROR_NOT_INITIALIZED;}
  nsresult rv = NS_OK;

  if (nsHTMLAtoms::maxlength == aAttribute) 
  {
    PRInt32 maxLength;
    nsresult rv = GetMaxLength(&maxLength);
    
    nsCOMPtr<nsIPlaintextEditor> textEditor = do_QueryInterface(mEditor);
    if (textEditor)
    {
      if (NS_CONTENT_ATTR_NOT_THERE != rv) 
      {  // set the maxLength attribute
          textEditor->SetMaxTextLength(maxLength);
        // if maxLength>docLength, we need to truncate the doc content
      }
      else { // unset the maxLength attribute
          textEditor->SetMaxTextLength(-1);
      }
    }
  } 
  else if (mEditor && nsHTMLAtoms::readonly == aAttribute) 
  {
    nsresult rv = DoesAttributeExist(nsHTMLAtoms::readonly);
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    { // set readonly
      flags |= nsIPlaintextEditor::eEditorReadonlyMask;
      if (mSelCon && IsFocusedContent(aPresContext, mContent))
        mSelCon->SetCaretEnabled(PR_FALSE);
    }
    else 
    { // unset readonly
      flags &= ~(nsIPlaintextEditor::eEditorReadonlyMask);
      if (mSelCon && !(flags & nsIPlaintextEditor::eEditorDisabledMask) &&
          IsFocusedContent(aPresContext, mContent))
        mSelCon->SetCaretEnabled(PR_TRUE);
    }    
    mEditor->SetFlags(flags);
  }
  else if (mEditor && nsHTMLAtoms::disabled == aAttribute) 
  {
    // XXXbryner do we need to check for a null presshell here?
    //           we don't do anything with it.
    nsIPresShell *shell = aPresContext->GetPresShell();
    if (!shell)
      return NS_ERROR_FAILURE;

    rv = DoesAttributeExist(nsHTMLAtoms::disabled);
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    { // set disabled
      flags |= nsIPlaintextEditor::eEditorDisabledMask;
      if (mSelCon)
      {
        mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
        if (IsFocusedContent(aPresContext, mContent))
          mSelCon->SetCaretEnabled(PR_FALSE);
      }
    }
    else 
    { // unset disabled
      flags &= ~(nsIPlaintextEditor::eEditorDisabledMask);
      if (mSelCon)
      {
        mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);
      }
    }    
    mEditor->SetFlags(flags);
  }
  // Allow the base class to handle common attributes supported
  // by all form elements... 
  else {
    rv = nsBoxFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aModType);
  }

  return rv;
}


NS_IMETHODIMP
nsTextControlFrame::GetText(nsString* aText)
{
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;
  if (IsSingleLineTextControl()) {
    // If we're going to remove newlines anyway, ignore the wrap property
    GetValue(*aText, PR_TRUE);
    RemoveNewlines(*aText);
  } else {
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea = do_QueryInterface(mContent);
    if (textArea) {
      if (mEditor) {
        nsCOMPtr<nsIEditorIMESupport> imeSupport = do_QueryInterface(mEditor);
        if (imeSupport)
          imeSupport->ForceCompositionEnd();
      }
      rv = textArea->GetValue(*aText);
    }
  }
  return rv;
}


NS_IMETHODIMP
nsTextControlFrame::GetPhonetic(nsAString& aPhonetic)
{
  aPhonetic.Truncate(0); 
  if (!mEditor)
    return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIEditorIMESupport> imeSupport = do_QueryInterface(mEditor);
  if (imeSupport) {
    nsCOMPtr<nsIPhonetic> phonetic = do_QueryInterface(imeSupport);
    if (phonetic)
      phonetic->GetPhonetic(aPhonetic);
  }
  return NS_OK;
}

///END NSIFRAME OVERLOADS
/////BEGIN PROTECTED METHODS

void nsTextControlFrame::RemoveNewlines(nsString &aString)
{
  // strip CR/LF and null
  static const char badChars[] = {10, 13, 0};
  aString.StripChars(badChars);
}


nsresult
nsTextControlFrame::GetMaxLength(PRInt32* aSize)
{
  *aSize = -1;
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;

  nsCOMPtr<nsIHTMLContent> content(do_QueryInterface(mContent));

  if (content) {
    nsHTMLValue value;
    rv = content->GetHTMLAttribute(nsHTMLAtoms::maxlength, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
  }
  return rv;
}

nsresult
nsTextControlFrame::DoesAttributeExist(nsIAtom *aAtt)
{
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;

  nsCOMPtr<nsIHTMLContent> content(do_QueryInterface(mContent));

  if (content) {
    nsHTMLValue value;
    rv = content->GetHTMLAttribute(aAtt, value);
  }
  return rv;
}

// this is where we propagate a content changed event
void
nsTextControlFrame::FireOnInput()
{
  NS_ASSERTION(mContent, "illegal to call unless we map to a content node");

  if (!mNotifyOnInput) { 
    return; // if notification is turned off, do nothing
  } 
  
  // Dispatch the "input" event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent event(NS_FORM_INPUT);

  // Have the content handle the event, propagating it according to normal
  // DOM rules.
  nsWeakPtr &shell = mTextSelImpl->GetPresShell();
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(shell);
  NS_ASSERTION(presShell, "No pres shell");
  if (!presShell) {
    return;
  }

  nsCOMPtr<nsPresContext> context;
  presShell->GetPresContext(getter_AddRefs(context));
  NS_ASSERTION(context, "No pres context");
  if (!context) {
    return;
  }

  presShell->HandleEventWithTarget(&event, nsnull, mContent,
                                   NS_EVENT_FLAG_INIT, &status); 
}

nsresult
nsTextControlFrame::InitFocusedValue()
{
  return GetText(&mFocusedValue);
}

NS_IMETHODIMP
nsTextControlFrame::CheckFireOnChange()
{
  nsString value;
  GetText(&value);
  if (!mFocusedValue.Equals(value))//different fire onchange
  {
    mFocusedValue = value;
    FireOnChange();
  }
  return NS_OK;
}

nsresult
nsTextControlFrame::FireOnChange()
{
  // Dispatch th1e change event
  nsCOMPtr<nsIContent> content;
  if (NS_SUCCEEDED(GetFormContent(*getter_AddRefs(content))))
  {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsInputEvent event(NS_FORM_CHANGE);

    // Have the content handle the event.
    nsWeakPtr &shell = mTextSelImpl->GetPresShell();
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(shell);
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
    nsCOMPtr<nsPresContext> context;
    if (NS_SUCCEEDED(presShell->GetPresContext(getter_AddRefs(context))) && context)
      return presShell->HandleEventWithTarget(&event, nsnull, mContent, NS_EVENT_FLAG_INIT, &status); 
  }
  return NS_OK;
}


//======
//privates

NS_IMETHODIMP
nsTextControlFrame::GetValue(nsAString& aValue, PRBool aIgnoreWrap)
{
  aValue.Truncate();  // initialize out param
  
  if (mEditor && mUseEditor) 
  {
    PRUint32 flags = nsIDocumentEncoder::OutputLFLineBreak;;

    if (PR_TRUE==IsPlainTextControl())
    {
      flags |= nsIDocumentEncoder::OutputBodyOnly;
    }

    flags |= nsIDocumentEncoder::OutputPreformatted;

    if (!aIgnoreWrap) {
      nsFormControlHelper::nsHTMLTextWrap wrapProp;
      nsresult rv = nsFormControlHelper::GetWrapPropertyEnum(mContent, wrapProp);
      if (rv != NS_CONTENT_ATTR_NOT_THERE) {
        if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Hard)
        {
          flags |= nsIDocumentEncoder::OutputWrap;
        }
      }
    }

    mEditor->OutputToString(NS_LITERAL_STRING("text/plain"), flags, aValue);
  }
  else
  {
    // Otherwise get the value from content.
    nsCOMPtr<nsIDOMHTMLInputElement> inputControl = do_QueryInterface(mContent);
    if (inputControl)
    {
      inputControl->GetValue(aValue);
    }
    else
    {
      nsCOMPtr<nsIDOMHTMLTextAreaElement> textareaControl
          = do_QueryInterface(mContent);
      if (textareaControl)
      {
        textareaControl->GetValue(aValue);
      }
    }
  }

  return NS_OK;
}


// END IMPLEMENTING NS_IFORMCONTROLFRAME

void
nsTextControlFrame::SetValue(const nsAString& aValue)
{
  if (mEditor && mUseEditor) 
  {
    nsAutoString currentValue;
    GetValue(currentValue, PR_FALSE);
    if (IsSingleLineTextControl())
    {
      RemoveNewlines(currentValue); 
    }
    // this is necessary to avoid infinite recursion
    if (!currentValue.Equals(aValue))
    {
      nsCOMPtr<nsISelection> domSel;
      nsCOMPtr<nsISelectionPrivate> selPriv;
      mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
      if (domSel)
      {
        selPriv = do_QueryInterface(domSel);
        if (selPriv)
          selPriv->StartBatchChanges();
      }
      // \r is an illegal character in the dom, but people use them,
      // so convert windows and mac platform linebreaks to \n:
      // Unfortunately aValue is declared const, so we have to copy
      // in order to do this substitution.
      currentValue.Assign(aValue);
      nsFormControlHelper::PlatformToDOMLineBreaks(currentValue);

      nsCOMPtr<nsIDOMDocument>domDoc;
      nsresult rv = mEditor->GetDocument(getter_AddRefs(domDoc));
      if (NS_FAILED(rv)) return;
      if (!domDoc) return;
      mSelCon->SelectAll();
      nsCOMPtr<nsIPlaintextEditor> htmlEditor = do_QueryInterface(mEditor);
      if (!htmlEditor) return;

      // get the flags, remove readonly and disabled, set the value,
      // restore flags
      PRUint32 flags, savedFlags;
      mEditor->GetFlags(&savedFlags);
      flags = savedFlags;
      flags &= ~(nsIPlaintextEditor::eEditorDisabledMask);
      flags &= ~(nsIPlaintextEditor::eEditorReadonlyMask);
      mEditor->SetFlags(flags);
      if (currentValue.Length() < 1)
        mEditor->DeleteSelection(nsIEditor::eNone);
      else {
        nsCOMPtr<nsIPlaintextEditor> textEditor = do_QueryInterface(mEditor);
        if (textEditor)
          textEditor->InsertText(currentValue);
      }
      mEditor->SetFlags(savedFlags);
      if (selPriv)
        selPriv->EndBatchChanges();
    }

    if (mScrollableView)
    {
      // Scroll the upper left corner of the text control's
      // content area back into view.

      mScrollableView->ScrollTo(0, 0, NS_VMREFRESH_NO_SYNC);
    }
  }
  else
  {
    // Otherwise set the value in content.
    nsCOMPtr<nsITextControlElement> textControl = do_QueryInterface(mContent);
    if (textControl)
    {
      textControl->TakeTextFrameValue(aValue);
    }
  }
}


NS_IMETHODIMP
nsTextControlFrame::SetInitialChildList(nsPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  /*nsIFrame *list = aChildList;
  while (list)
  {
    list->AddStateBits(NS_FRAME_INDEPENDENT_SELECTION);
    list = list->GetNextSibling();
  }
  */
  nsresult rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  if (mEditor)
    mEditor->PostCreate();
  //look for scroll view below this frame go along first child list
  nsIFrame* first = GetFirstChild(nsnull);

  // Mark the scroll frame as being a reflow root. This will allow
  // incremental reflows to be initiated at the scroll frame, rather
  // than descending from the root frame of the frame hierarchy.
  first->AddStateBits(NS_FRAME_REFLOW_ROOT);

//we must turn off scrollbars for singleline text controls
  if (IsSingleLineTextControl()) 
  {
    nsIScrollableFrame *scrollableFrame = nsnull;
    if (first)
      first->QueryInterface(NS_GET_IID(nsIScrollableFrame), (void **) &scrollableFrame);
    if (scrollableFrame)
      scrollableFrame->SetScrollbarVisibility(aPresContext,PR_FALSE,PR_FALSE);
  }

  //register keylistener
  nsCOMPtr<nsIDOMEventReceiver> erP;
  if (NS_SUCCEEDED(mContent->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(erP))) && erP)
  {
    // register the event listeners with the DOM event reveiver
    rv = erP->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMFocusListener *,mTextListener), NS_GET_IID(nsIDOMFocusListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register focus listener");
    // XXXbryner do we need to check for a null presshell here?
    if (!aPresContext->GetPresShell())
      return NS_ERROR_FAILURE;
  }

  while(first)
  {
    nsIView *view = first->GetView();
    if (view)
    {
      nsIScrollableView *scrollView;
      if (NS_SUCCEEDED(CallQueryInterface(view, &scrollView)))
      {
        mScrollableView = scrollView; // Note: views are not addref'd
        mTextSelImpl->SetScrollableView(scrollView);
        break;
      }
    }
    first = first->GetFirstChild(nsnull);
  }

  return rv;
}


PRInt32 
nsTextControlFrame::GetWidthInCharacters() const
{
  // see if there's a COL attribute, if so it wins
  nsCOMPtr<nsIHTMLContent> content;
  nsresult rv = mContent->QueryInterface(NS_GET_IID(nsIHTMLContent), getter_AddRefs(content));
  if (NS_SUCCEEDED(rv) && content)
  {
    nsHTMLValue resultValue;
    rv = content->GetHTMLAttribute(nsHTMLAtoms::cols, resultValue);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    {
      if (resultValue.GetUnit() == eHTMLUnit_Integer) 
      {
        return (resultValue.GetIntValue());
      }
    }
  }

  // otherwise, see if CSS has a width specified.  If so, work backwards to get the 
  // number of characters this width represents.
 
  
  // otherwise, the default is just returned.
  return DEFAULT_COLUMN_WIDTH;
}

NS_IMETHODIMP
nsTextControlFrame::GetScrollableView(nsPresContext* aPresContext,
                                      nsIScrollableView** aView)
{
  *aView = mScrollableView;
  return NS_OK;
}

PRBool
nsTextControlFrame::IsScrollable() const
{
  return !IsSingleLineTextControl();
}

NS_IMETHODIMP
nsTextControlFrame::OnContentReset()
{
  return NS_OK;
}

void
nsTextControlFrame::SetValueChanged(PRBool aValueChanged)
{
  nsCOMPtr<nsITextControlElement> elem = do_QueryInterface(mContent);
  if (elem) {
    elem->SetValueChanged(aValueChanged);
  }
}

NS_IMETHODIMP 
nsTextControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // temp fix until Bug 124990 gets fixed
  if (aPresContext->IsPaginated() && NS_IS_MOUSE_EVENT(aEvent)) {
    return NS_OK;
  }

  return nsStackFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
    
}
