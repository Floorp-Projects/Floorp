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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Ross <blakeross@telocity.com>
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


#include "nsCOMPtr.h"
#include "nsGfxTextControlFrame2.h"
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
#include "nsIEditorController.h"
#include "nsIElementFactory.h"
#include "nsIHTMLContent.h"
#include "nsFormFrame.h"
#include "nsIEditorIMESupport.h"
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
#include "nsIPresContext.h"
#include "nsHTMLIIDs.h"
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
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h" //getframetype
//for keylistener for "return" check
#include "nsIDOMKeyListener.h" 
#include "nsIDOMKeyEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDocument.h" //observe documents to send onchangenotifications
#include "nsIStyleSheet.h"//observe documents to send onchangenotifications
#include "nsIStyleRule.h"//observe documents to send onchangenotifications
#include "nsIDOMEventListener.h"//observe documents to send onchangenotifications
#include "nsGUIEvent.h"

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

#include "nsITransactionManager.h"
#include "nsITransactionListener.h"
#include "nsIDOMText.h" //for multiline getselection


#ifdef IBMBIDI
#include "nsIBidiKeyboard.h"
#include "nsWidgetsCID.h"
#endif // IBMBIDI

#define DEFAULT_COLUMN_WIDTH 20
#define GUESS_INPUT_SIZE 150  // 10 pixels wide

#include "nsContentCID.h"
static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);

static NS_DEFINE_CID(kTextEditorCID, NS_TEXTEDITOR_CID);
static NS_DEFINE_CID(kFrameSelectionCID, NS_FRAMESELECTION_CID);


static nsresult GetElementFactoryService(nsIElementFactory **aFactory)
{
  nsresult rv(NS_OK);
  static nsWeakPtr sElementFactory = getter_AddRefs( NS_GetWeakReference(nsCOMPtr<nsIElementFactory>(do_GetService(
                     NS_ELEMENT_FACTORY_CONTRACTID_PREFIX"http://www.w3.org/1999/xhtml", &rv) )));
  if (sElementFactory)
  {
    nsCOMPtr<nsIElementFactory> fac(do_QueryReferent(sElementFactory));
    *aFactory = fac.get();
    if (!*aFactory)
      rv = NS_ERROR_FAILURE;
    NS_IF_ADDREF(*aFactory);
  }
  else
    return NS_ERROR_FAILURE;
  return rv;
}


class nsTextInputListener : public nsIDOMKeyListener,
                            public nsISelectionListener,
                            public nsIDOMFocusListener,
                            public nsIEditorObserver,
                            public nsITransactionListener,
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
  void SetFrame(nsGfxTextControlFrame2 *aFrame){mFrame = aFrame;}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN interfaces in to the keylister base interface. must be supplied to handle pure virtual interfaces
  see the nsIDOMKeyListener interface implementation for details
  */
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);
/*END interfaces from nsIDOMKeyListener*/
/*BEGIN nsISelectionListener Interface*/
  NS_IMETHOD    NotifySelectionChanged(nsIDOMDocument* aDoc, nsISelection* aSel, PRInt16 aReason);
/*END nsISelectionListener*/

/* BEGIN EditorObserver*/
  NS_IMETHOD EditAction();
/*END EditorObserver*/

  /** nsIDOMFocusListener interfaces 
    * used to propogate focus, blur, and change notifications
    * @see nsIDOMFocusListener
    */
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur (nsIDOMEvent* aEvent);
  /* END interfaces from nsIDOMFocusListener*/


  /** nsITransactionListener interfaces
    */
  
  NS_IMETHOD WillDo(nsITransactionManager *aManager, nsITransaction *aTransaction, PRBool *aInterrupt);
  NS_IMETHOD DidDo(nsITransactionManager *aManager, nsITransaction *aTransaction, nsresult aDoResult);
  NS_IMETHOD WillUndo(nsITransactionManager *aManager, nsITransaction *aTransaction, PRBool *aInterrupt);
  NS_IMETHOD DidUndo(nsITransactionManager *aManager, nsITransaction *aTransaction, nsresult aUndoResult);
  NS_IMETHOD WillRedo(nsITransactionManager *aManager, nsITransaction *aTransaction, PRBool *aInterrupt);
  NS_IMETHOD DidRedo(nsITransactionManager *aManager, nsITransaction *aTransaction, nsresult aRedoResult);
  NS_IMETHOD WillBeginBatch(nsITransactionManager *aManager, PRBool *aInterrupt);
  NS_IMETHOD DidBeginBatch(nsITransactionManager *aManager, nsresult aResult);
  NS_IMETHOD WillEndBatch(nsITransactionManager *aManager, PRBool *aInterrupt);
  NS_IMETHOD DidEndBatch(nsITransactionManager *aManager, nsresult aResult);
  NS_IMETHOD WillMerge(nsITransactionManager *aManager, nsITransaction *aTopTransaction,
                       nsITransaction *aTransactionToMerge, PRBool *aInterrupt);
  NS_IMETHOD DidMerge(nsITransactionManager *aManager, nsITransaction *aTopTransaction,
                      nsITransaction *aTransactionToMerge,
                      PRBool aDidMerge, nsresult aMergeResult);


protected:

  nsresult  UpdateTextInputCommands(const nsString& commandsToUpdate);

protected:

  nsGfxTextControlFrame2* mFrame;  // weak reference
  nsString mFocusedValue;
  
  PRPackedBool    mSelectionWasCollapsed;
  PRPackedBool    mKnowSelectionCollapsed;

  PRPackedBool    mFirstDoOfFirstUndo;
};


/*
 * nsTextEditorListener implementation
 */

NS_IMPL_ADDREF(nsTextInputListener)

NS_IMPL_RELEASE(nsTextInputListener)


nsTextInputListener::nsTextInputListener()
: mFrame(nsnull)
, mSelectionWasCollapsed(PR_TRUE)
, mKnowSelectionCollapsed(PR_FALSE)
, mFirstDoOfFirstUndo(PR_TRUE)
{
  NS_INIT_REFCNT();
}



nsTextInputListener::~nsTextInputListener() 
{
}


NS_IMPL_QUERY_INTERFACE6(nsTextInputListener,
                          nsIDOMKeyListener,
                          nsISelectionListener,
                          nsIDOMFocusListener,
                          nsIEditorObserver,
                          nsITransactionListener,
                          nsISupportsWeakReference)

nsresult
nsTextInputListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

// individual key handlers return NS_OK to indicate NOT consumed
// by default, an error is returned indicating event is consumed
// joki is fixing this interface.
nsresult
nsTextInputListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}


nsresult
nsTextInputListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}


nsresult
nsTextInputListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  if (!mFrame)
    return NS_OK;
  nsCOMPtr<nsIDOMKeyEvent>keyEvent;
  keyEvent = do_QueryInterface(aKeyEvent);
  if (!keyEvent) 
  {
    //non-key event passed to keydown.  bad things.
    return NS_OK;
  }
  
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(aKeyEvent);
  if(privateEvent) 
  {
    PRBool dispatchStopped;
    privateEvent->IsDispatchStopped(&dispatchStopped);
    if(dispatchStopped)
      return NS_OK;
  }

  mFrame->SetValueChanged(PR_TRUE);

  if (mFrame && mFrame->IsSingleLineTextControl())
  {
    PRUint32     keyCode;
    keyEvent->GetKeyCode(&keyCode);

    if (nsIDOMKeyEvent::DOM_VK_RETURN==keyCode ||
        nsIDOMKeyEvent::DOM_VK_ENTER==keyCode)
    {
      nsAutoString curValue;
      mFrame->GetText(&curValue);

      // If the text control's contents have changed, fire
      // off an onChange().

      if (!mFocusedValue.Equals(curValue))
      {
        mFocusedValue = curValue;
        mFrame->CallOnChange();
      }

      // Now try to submit the form. Be sure to check mFrame again
      // since CallOnChange() may have caused the deletion of mFrame.

      if (mFrame)
        mFrame->SubmitAttempt();
    }
  }
  return NS_OK;
}

//END KeyListener

//BEGIN NS_IDOMSELECTIONLISTENER

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
      nsCOMPtr<nsIDocument> doc;
      if (NS_SUCCEEDED(content->GetDocument(*getter_AddRefs(doc)))) 
      {
        if (doc) 
        {
          nsCOMPtr<nsIPresShell> presShell;
          doc->GetShellAt(0, getter_AddRefs(presShell));
          if (presShell) 
          {
            nsEventStatus status = nsEventStatus_eIgnore;
            nsEvent event;
            event.eventStructType = NS_EVENT;
            event.message = NS_FORM_SELECTED;

            presShell->HandleEventWithTarget(&event,mFrame,content,NS_EVENT_FLAG_INIT,&status);
          }
        }
      }
    }
  }

  // if the collapsed state did not change, don't fire notifications
  if (mKnowSelectionCollapsed && collapsed == mSelectionWasCollapsed)
    return NS_OK;
  
  mSelectionWasCollapsed = collapsed;
  mKnowSelectionCollapsed = PR_TRUE;

  return UpdateTextInputCommands(NS_ConvertASCIItoUCS2("select"));
}

//EDITOR INTERFACE

NS_IMETHODIMP nsTextInputListener::EditAction()
{
  return mFrame?mFrame->InternalContentChanged():NS_ERROR_NULL_POINTER;
}

//END NS_IDOMSELECTIONLISTENER
//focuslistener

nsresult
nsTextInputListener::Focus(nsIDOMEvent* aEvent)
{
  if (!mFrame) return NS_OK;
  nsCOMPtr<nsIEditor> editor;
  mFrame->GetEditor(getter_AddRefs(editor));
  if (editor)
  {
    editor->AddEditorObserver(this);
  }
  
  nsresult rv = mFrame->GetText(&mFocusedValue);
  if (NS_FAILED(rv)) return rv;
  
  return NS_OK;
}

nsresult
nsTextInputListener::Blur (nsIDOMEvent* aEvent)
{
  if (!mFrame)
    return NS_OK;
    
  nsAutoString blurValue;
  nsCOMPtr<nsIEditor> editor;
  mFrame->GetEditor(getter_AddRefs(editor));
  if (editor)
  {
    editor->RemoveEditorObserver(this);

  }  
  mFrame->GetText(&blurValue);
  if (!mFocusedValue.Equals(blurValue))//different fire onchange
  {
    mFocusedValue = blurValue;
    mFrame->CallOnChange();
  }
  return NS_OK;
}
//END focuslistener


NS_IMETHODIMP nsTextInputListener::WillDo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::DidDo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, nsresult aDoResult)
{
  // we only need to update if the undo count is now 1
  PRInt32 undoCount;
  aManager->GetNumberOfUndoItems(&undoCount);
  if (undoCount == 1)
  {
    if (mFirstDoOfFirstUndo)
      UpdateTextInputCommands(NS_ConvertASCIItoUCS2("undo"));

    mFirstDoOfFirstUndo = PR_FALSE;
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::WillUndo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::DidUndo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, nsresult aUndoResult)
{
  PRInt32 undoCount;
  aManager->GetNumberOfUndoItems(&undoCount);
  if (undoCount == 0)
    mFirstDoOfFirstUndo = PR_TRUE;    // reset the state for the next do

  UpdateTextInputCommands(NS_ConvertASCIItoUCS2("undo"));
  
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::WillRedo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::DidRedo(nsITransactionManager *aManager,  
  nsITransaction *aTransaction, nsresult aRedoResult)
{
  UpdateTextInputCommands(NS_ConvertASCIItoUCS2("undo"));
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::WillBeginBatch(nsITransactionManager *aManager, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::DidBeginBatch(nsITransactionManager *aManager, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::WillEndBatch(nsITransactionManager *aManager, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::DidEndBatch(nsITransactionManager *aManager, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::WillMerge(nsITransactionManager *aManager,
        nsITransaction *aTopTransaction, nsITransaction *aTransactionToMerge, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsTextInputListener::DidMerge(nsITransactionManager *aManager,
      nsITransaction *aTopTransaction, nsITransaction *aTransactionToMerge,
      PRBool aDidMerge, nsresult aMergeResult)
{
  return NS_OK;
}



nsresult nsTextInputListener::UpdateTextInputCommands(const nsString& commandsToUpdate)
{
  if (!mFrame) return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsIContent> content;
  nsresult rv = mFrame->GetContent(getter_AddRefs(content));
  if (NS_FAILED(rv)) 
    return rv;
  if (!content) 
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDocument> doc;
  rv = content->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(rv)) 
    return rv;
  if (!doc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
  rv = doc->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));
  if (NS_FAILED(rv))
    return rv;
  if (!scriptGlobalObject)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(scriptGlobalObject, &rv);
  if (NS_FAILED(rv))
    return rv;
  if (!domWindow)
    return NS_ERROR_FAILURE;

  return domWindow->UpdateCommands(commandsToUpdate);
}


//END NSTEXTINPUTLISTENER

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
  NS_IMETHOD SetDisplayNonTextSelection(PRBool toggle);
  NS_IMETHOD GetDisplayNonTextSelection(PRBool *_retval);
  NS_IMETHOD GetSelection(PRInt16 type, nsISelection **_retval);
  NS_IMETHOD ScrollSelectionIntoView(PRInt16 type, PRInt16 region);
  NS_IMETHOD RepaintSelection(PRInt16 type);
  NS_IMETHOD RepaintSelection(nsIPresContext* aPresContext, SelectionType aSelectionType);
  NS_IMETHOD SetCaretEnabled(PRBool enabled);
  NS_IMETHOD SetCaretWidth(PRInt16 twips);
  NS_IMETHOD SetCaretReadOnly(PRBool aReadOnly);
  NS_IMETHOD GetCaretEnabled(PRBool *_retval);
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
  NS_IMETHOD HandleKeyEvent(nsIPresContext* aPresContext, nsGUIEvent *aGuiEvent);
  NS_IMETHOD HandleClick(nsIContent *aNewFocus, PRUint32 aContentOffset, PRUint32 aContentEndOffset , 
                       PRBool aContinueSelection, PRBool aMultipleSelection, PRBool aHint); 
  NS_IMETHOD HandleDrag(nsIPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint);
  NS_IMETHOD HandleTableSelection(nsIContent *aParentContent, PRInt32 aContentOffset, PRInt32 aTarget, nsMouseEvent *aMouseEvent);
  NS_IMETHOD StartAutoScrollTimer(nsIPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint, PRUint32 aDelay);
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
#ifdef IBMBIDI
  NS_IMETHOD GetPrevNextBidiLevels(nsIPresContext *aPresContext,
                                   nsIContent *aNode,
                                   PRUint32 aContentOffset,
                                   nsIFrame **aPrevFrame,
                                   nsIFrame **aNextFrame,
                                   PRUint8 *aPrevLevel,
                                   PRUint8 *aNextLevel);
  NS_IMETHOD GetFrameFromLevel(nsIPresContext *aPresContext,
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
  NS_INIT_REFCNT();
  if (aSel && aShell)
  {
    mFrameSelection = aSel;//we are the owner now!
    nsCOMPtr<nsIFocusTracker> tracker = do_QueryInterface(aShell);
    mLimiter = aLimiter;
    mFrameSelection->Init(tracker, mLimiter);
    mPresShellWeak = getter_AddRefs( NS_GetWeakReference(aShell) );
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
nsTextInputSelectionImpl::SetDisplayNonTextSelection(PRBool aToggle)
{
  return NS_OK;//stub this out. not used in input
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetDisplayNonTextSelection(PRBool *aToggle)
{
  return NS_OK;//stub this out. not used in input
}

NS_IMETHODIMP
nsTextInputSelectionImpl::GetSelection(PRInt16 type, nsISelection **_retval)
{
  if (mFrameSelection)
    return mFrameSelection->GetSelection(type, _retval);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollSelectionIntoView(PRInt16 type, PRInt16 region)
{
  if (mFrameSelection)
    return mFrameSelection->ScrollSelectionIntoView(type, region);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::RepaintSelection(PRInt16 type)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
  if (presShell)
  {
    nsCOMPtr<nsIPresContext> context;
    if (NS_SUCCEEDED(presShell->GetPresContext(getter_AddRefs(context))) && context)
    {
      return mFrameSelection->RepaintSelection(context, type);
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextInputSelectionImpl::RepaintSelection(nsIPresContext* aPresContext, SelectionType aSelectionType)
{
  return RepaintSelection(aSelectionType);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::SetCaretEnabled(PRBool enabled)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak);
  if (!shell) return NS_ERROR_FAILURE;
  
  // first, tell the caret which selection to use
  nsCOMPtr<nsISelection> domSel;
  GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
  if (!domSel) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsICaret> caret;
  shell->GetCaret(getter_AddRefs(caret));
  if (!caret) return NS_OK;
  caret->SetCaretDOMSelection(domSel);

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
#if XXXXX_WORK_TO_BE_COMPLETED_XXXXX
  // XXX: this code needs to be finished or rewritten
  // expected bahavior for PageMove is to scroll AND move the caret
  nsIScrollableView *scrollableView;
  nsresult result;
  nscoord containerHeight, containerWidth, lineHeight;
  nsPoint oldLocation;

  result = GetScrollableView(&scrollableView);
  if (NS_FAILED(result))
    return result;
  if (!scrollableView)
    return NS_ERROR_NOT_INITIALIZED;

  // find out where we are; determine amount to page up/down
  result = scrollableView->GetScrollPosition(oldLocation.x, oldLocation.y);
  if (NS_FAILED(result))
    return result;

  result = scrollableView->GetLineHeight(&lineHeight);
  if (NS_FAILED(result))
    return result;

  result = scrollableView->GetContainerSize(&containerWidth, &containerHeight);
  if (NS_SUCCEEDED(result))
  {
    // scroll this amount backwards
    if (!aForward)
      containerHeight = 0 - containerHeight;

    nsPoint desiredLocation(oldLocation.x, oldLocation.y + containerHeight - lineHeight);
    result = scrollableView->ScrollTo(desiredLocation.x, desiredLocation.y, NS_VMREFRESH_NO_SYNC);

    // grab the parent / root DIV for this text widget
    nsCOMPtr<nsIContent> parentDIV;
    result = GetLimiter(getter_AddRefs(parentDIV));
    if (NS_FAILED(result))
      return result;
    if (!parentDIV)
      return NS_ERROR_UNEXPECTED;

    // now we know how much to move up/down, we need to look into the content and 
    // figure out where that coordinate is and then place the caret at that location
    nsCOMPtr<nsIContent> content;
    result = frame->GetContentAndOffsetsFromPoint(presContext, desiredLocation, getter_AddRefs(content), startOffset, endOffset, beginFrameContent);
    result = GetFrameForNodeOffset(content, PRInt32 aOffset, HINT aHint, nsIFrame **aReturnFrame, PRInt32 *aReturnOffset)
  }
  
  return result;
#else
  return ScrollPage(aForward);
#endif
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
  if (aForward)
  {
    parentDIV->ChildCount(offset);

    // Prevent the caret from being placed after the last
    // BR node in the content tree!

    if (offset > 0)
    {
      nsCOMPtr<nsIContent> child;
      result = parentDIV->ChildAt(offset - 1, *getter_AddRefs(child));
      if (NS_SUCCEEDED(result) && child)
      {
        nsCOMPtr<nsIAtom> tagName;
        result = child->GetTag(*getter_AddRefs(tagName));
        if (NS_SUCCEEDED(result) && tagName.get() == nsHTMLAtoms::br)
          --offset;
      }
    }
  }

  result = mFrameSelection->HandleClick(parentDIV, offset, offset, aExtend, PR_FALSE, aExtend);

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

  return scrollableView->ScrollByPages(aForward ? 1 : -1);
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
nsTextInputSelectionImpl::HandleKeyEvent(nsIPresContext* aPresContext, nsGUIEvent *aGuiEvent)
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
nsTextInputSelectionImpl::HandleDrag(nsIPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint)
{
  return mFrameSelection->HandleDrag(aPresContext, aFrame, aPoint);
}


NS_IMETHODIMP
nsTextInputSelectionImpl::HandleTableSelection(nsIContent *aParentContent, PRInt32 aContentOffset, PRInt32 aTarget, nsMouseEvent *aMouseEvent)
{
  // We should never have a table inside a text control frame!
  NS_ASSERTION(PR_TRUE, "Calling HandleTableSelection inside nsGfxTextControlFrame2!");
  return NS_OK;
}


NS_IMETHODIMP
nsTextInputSelectionImpl::StartAutoScrollTimer(nsIPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint, PRUint32 aDelay)
{
  return mFrameSelection->StartAutoScrollTimer(aPresContext, aFrame, aPoint, aDelay);
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
  if(mFrameSelection) 
    return mFrameSelection->SetScrollableView(aScrollableView);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsTextInputSelectionImpl::GetScrollableView(nsIScrollableView **aScrollableView)
{
  if(mFrameSelection) 
    return mFrameSelection->GetScrollableView(aScrollableView);
  return NS_ERROR_FAILURE;
}

#ifdef IBMBIDI
NS_IMETHODIMP nsTextInputSelectionImpl::GetPrevNextBidiLevels(nsIPresContext *aPresContext,
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

NS_IMETHODIMP nsTextInputSelectionImpl::GetFrameFromLevel(nsIPresContext *aPresContext,
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
NS_NewGfxTextControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxTextControlFrame2* it = new (aPresShell) nsGfxTextControlFrame2(aPresShell);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMPL_ADDREF_INHERITED(nsGfxTextControlFrame2, nsBoxFrame);
NS_IMPL_RELEASE_INHERITED(nsGfxTextControlFrame2, nsBoxFrame);
 

NS_IMETHODIMP
nsGfxTextControlFrame2::QueryInterface(const nsIID& aIID, void** aInstancePtr)
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
  if (aIID.Equals(NS_GET_IID(nsIGfxTextControlFrame2))) {
    *aInstancePtr = (void*)(nsIGfxTextControlFrame2*) this;
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*)(nsIStatefulFrame*) this;
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIScrollableViewProvider))) {
    *aInstancePtr = (void*)(nsIScrollableViewProvider*) this;
    return NS_OK;
  }

  return nsBoxFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsGfxTextControlFrame2::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLTextFieldAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

nsGfxTextControlFrame2::nsGfxTextControlFrame2(nsIPresShell* aShell):nsStackFrame(aShell)
{
  mUseEditor = PR_FALSE;
  mIsProcessing = PR_FALSE;
  mNotifyOnInput = PR_FALSE;
  mFormFrame = nsnull;
  mSuggestedWidth = NS_FORMSIZE_NOTSET;
  mSuggestedHeight = NS_FORMSIZE_NOTSET;
  mScrollableView = nsnull;
}

nsGfxTextControlFrame2::~nsGfxTextControlFrame2()
{
  //delete mTextListener;
  //delete mTextSelImpl; dont delete this since mSelCon will release it.
}

NS_IMETHODIMP
nsGfxTextControlFrame2::Destroy(nsIPresContext* aPresContext)
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
      GetTextControlFrameState(value);

      mUseEditor = PR_FALSE;

      // Next store the frame state in the control
      // (now that mUseEditor is false values get stored
      // in content).
      SetTextControlFrameState(value);
    }
    mEditor->PreDestroy();
  }
  
  // Clean up the controller
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
        nsCOMPtr<nsIEditorController> editController = do_QueryInterface(controller);
        if (editController)
        {
          editController->SetCommandRefCon(nsnull);
        }
      }
    }
  }

  mSelCon = 0;
  mEditor = 0;
  
//unregister self from content
  mTextListener->SetFrame(nsnull);
  nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  if (mFormFrame)
  {
    mFormFrame->RemoveFormControlFrame(*this);
    mFormFrame = nsnull;
    mTextListener->SetFrame(nsnull);
  }
  if (mTextListener)
  {
    nsCOMPtr<nsIDOMEventReceiver> erP = do_QueryInterface(mContent);
    if (erP)
    {
      erP->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMFocusListener  *,mTextListener), NS_GET_IID(nsIDOMFocusListener));
      erP->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMKeyListener*,mTextListener), NS_GET_IID(nsIDOMKeyListener));
    }
  }
  return nsBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP 
nsGfxTextControlFrame2::GetFrameType(nsIAtom** aType) const 
{ 
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer"); 
  *aType = nsLayoutAtoms::textInputFrame; 
  NS_ADDREF(*aType); 
  return NS_OK; 
} 

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsGfxTextControlFrame2::IsSingleLineTextControl() const
{
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT==type) || (NS_FORM_INPUT_PASSWORD==type)) {
    return PR_TRUE;
  }
  return PR_FALSE; 
}

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsGfxTextControlFrame2::IsPlainTextControl() const
{
  // need to check HTML attribute of mContent and/or CSS.
  return PR_TRUE;
}

PRBool nsGfxTextControlFrame2::IsPasswordTextControl() const
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_PASSWORD==type) {
    return PR_TRUE;
  }
  return PR_FALSE;
}


nsresult 
nsGfxTextControlFrame2::GetColRowSizeAttr(nsIFormControlFrame*  aFrame,
                                         nsIAtom *     aColSizeAttr,
                                         nsHTMLValue & aColSize,
                                         nsresult &    aColStatus,
                                         nsIAtom *     aRowSizeAttr,
                                         nsHTMLValue & aRowSize,
                                         nsresult &    aRowStatus)
{
  nsIContent* iContent = nsnull;
  aFrame->GetFormContent((nsIContent*&) iContent);
  if (!iContent) {
    return NS_ERROR_FAILURE;
  }
  nsIHTMLContent* hContent = nsnull;
  nsresult result = iContent->QueryInterface(kIHTMLContentIID, (void**)&hContent);
  if ((NS_OK != result) || !hContent) {
    NS_RELEASE(iContent);
    return NS_ERROR_FAILURE;
  }

  aColStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aColSizeAttr) {
    aColStatus = hContent->GetHTMLAttribute(aColSizeAttr, aColSize);
  }

  aRowStatus= NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aRowSizeAttr) {
    aRowStatus = hContent->GetHTMLAttribute(aRowSizeAttr, aRowSize);
  }

  NS_RELEASE(hContent);
  NS_RELEASE(iContent);
  
  return NS_OK;
}



NS_IMETHODIMP
nsGfxTextControlFrame2::ReflowStandard(nsIPresContext*          aPresContext,
                                       nsSize&                  aDesiredSize,
                                       const nsHTMLReflowState& aReflowState,
                                       nsReflowStatus&          aStatus,
                                       nsMargin&                aBorder,
                                       nsMargin&                aPadding)
{
  // get the css size and let the frame use or override it
  nsSize minSize;
  
  PRBool usingDefaultSize = PR_FALSE;
  PRInt32 ignore;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
    PRInt32 width = 0;
    if (NS_CONTENT_ATTR_HAS_VALUE != GetSizeFromContent(&width)) {
      width = GetDefaultColumnWidth();
      usingDefaultSize = PR_TRUE;
    }
    nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull,
                                  nsnull, width, 
                                  PR_FALSE, nsnull, 1);
    CalculateSizeStandard(aPresContext, aReflowState.rendContext, this,
                          textSpec, aDesiredSize, minSize, ignore, aBorder, aPadding, usingDefaultSize);
  } else {
    nsInputDimensionSpec areaSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, 
                                  nsnull, GetDefaultColumnWidth(), 
                                  PR_FALSE, nsHTMLAtoms::rows, 1);
    CalculateSizeStandard(aPresContext, aReflowState.rendContext, this, 
                          areaSpec, aDesiredSize, minSize, ignore, aBorder, aPadding, usingDefaultSize);
  }

  // CalculateSize makes calls in the nsFormControlHelper that figures
  // out the entire size of the control when in NavQuirks mode. For the
  // textarea, this means the scrollbar sizes hav already been added to
  // its overall size and do not need to be added here.
  if (NS_FORM_TEXTAREA == type) {
    float   p2t;
    aPresContext->GetPixelsToTwips(&p2t);

    nscoord scrollbarWidth  = 0;
    nscoord scrollbarHeight = 0;
    float   scale;
    nsCOMPtr<nsIDeviceContext> dx;
    aPresContext->GetDeviceContext(getter_AddRefs(dx));
    if (dx) { 
      float sbWidth;
      float sbHeight;
      dx->GetCanonicalPixelScale(scale);
      dx->GetScrollBarDimensions(sbWidth, sbHeight);
      scrollbarWidth  = PRInt32(sbWidth * scale);
      scrollbarHeight = PRInt32(sbHeight * scale);
    } else {
      scrollbarWidth  = nsFormControlFrame::GetScrollbarWidth(p2t);
      scrollbarHeight = scrollbarWidth;
    }

    aDesiredSize.height += scrollbarHeight;
    minSize.height      += scrollbarHeight;
    aDesiredSize.width  += scrollbarWidth;
    minSize.width       += scrollbarWidth;
  }
  aDesiredSize.width  += aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;
  aDesiredSize.height += aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;

  return NS_OK;

}



PRInt32
nsGfxTextControlFrame2::CalculateSizeStandard (nsIPresContext*       aPresContext, 
                                              nsIRenderingContext*  aRendContext,
                                              nsIFormControlFrame*  aFrame,
                                              nsInputDimensionSpec& aSpec, 
                                              nsSize&               aDesiredSize, 
                                              nsSize&               aMinSize, 
                                              nscoord&              aRowHeight,
                                              nsMargin&             aBorder,
                                              nsMargin&             aPadding,
                                              PRBool                aIsUsingDefSize) 
{
  nscoord charWidth   = 0; 
  aDesiredSize.width  = CSS_NOTSET;
  aDesiredSize.height = CSS_NOTSET;

  nsHTMLValue colAttr;
  nsresult    colStatus;
  nsHTMLValue rowAttr;
  nsresult    rowStatus;
  if (NS_ERROR_FAILURE == GetColRowSizeAttr(aFrame, 
                                            aSpec.mColSizeAttr, colAttr, colStatus,
                                            aSpec.mRowSizeAttr, rowAttr, rowStatus)) {
    return 0;
  }

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);

  nscoord fontHeight  = 0;
  // get leading
  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult res = nsFormControlHelper::GetFrameFontFM(aPresContext, aFrame, getter_AddRefs(fontMet));
  if (NS_SUCCEEDED(res) && fontMet) {
    aRendContext->SetFont(fontMet);
    fontMet->GetHeight(fontHeight);
    aDesiredSize.height = fontHeight;
  } else {
    aDesiredSize.height = GUESS_INPUT_SIZE; // punt
  }

  // Internal padding is necessary for better matching IE's width
  nscoord internalPadding = 0;

  // Get the Average char width to calc the min width and 
  // pref width. Pref Width is calced by multiplying the size times avecharwidth
  // 
  // XXX Currently, WIN32 is the only platform to implement the GetAveCharWidth in 
  // nsIFontMetrics. The other need to have it implemeneted (Bug 50998)
  // and then this if def removed. We are too close to RTM to implement it in all 
  // the platforms and ports.
#if defined(_WIN32) || defined(XP_OS2)
  fontMet->GetAveCharWidth(charWidth);

  // Get frame font
  const nsFont * font = nsnull;
  if (NS_SUCCEEDED(aFrame->GetFont(aPresContext, font))) {
    // To better match IE, take the size (in twips) and remove 4 pixels
    // add this on as additional padding
    internalPadding = PR_MAX(font->size - NSToCoordRound(4 * p2t), 0);
    // round to a multiple of p2t
    nscoord rest = internalPadding%NSToCoordRound(p2t);
    if( rest < NSToCoordRound(p2t) - rest) {
      internalPadding = internalPadding - rest;
    } else {
      internalPadding = internalPadding + NSToCoordRound(p2t) - rest;
    }
  }

#else
  // XP implementation of AveCharWidth
  nsAutoString aveStr; 
  aveStr.AssignWithConversion(" ABCDEFGHIJKLMNOPQRSTUVWXYabcdefghijklmnopqrstuvwxyz!@#$%^&*()_+=-0987654321~`';\":[]}{?><,./\\|");
  aRendContext->GetWidth(aveStr, charWidth);
  charWidth /= aveStr.Length();
  // Round to the nearest twip
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);  // get the rounding right
  charWidth = nscoord((float(charWidth) / float(onePixel)) + 0.5)*onePixel;
#endif
  aDesiredSize.width = charWidth;

#ifdef DEBUG_rodsXXX
  printf("Ave: %d  MA: %d  %d\n", charWidth, measAveWidth, charWidth-measAveWidth);
  printf("Ave: %d  MA: %d  %d\n", charWidth/15, measAveWidth/15, (charWidth/15)-(measAveWidth/15));
#endif

  // set the default col size back
  aMinSize.width  = aDesiredSize.width;
  aMinSize.height = aDesiredSize.height;

  // determine the width, char height, row height
  if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
    PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
    col = (col <= 0) ? 1 : col; // XXX why a default of 1 char, why hide it
    aDesiredSize.width = col * charWidth;
  } else {
    aDesiredSize.width = aSpec.mColDefaultSize * charWidth;
  }

  // Now add the extra internal padding on
  aDesiredSize.width += internalPadding;

  aRowHeight      = aDesiredSize.height;
  aMinSize.height = aDesiredSize.height;
  PRInt32 numRows = 0;

  if (NS_CONTENT_ATTR_HAS_VALUE == rowStatus) { // row attr will provide height
    PRInt32 rowAttrInt = ((rowAttr.GetUnit() == eHTMLUnit_Pixel) 
                            ? rowAttr.GetPixelValue() : rowAttr.GetIntValue());
    numRows = (rowAttrInt > 0) ? rowAttrInt : 1;
    aDesiredSize.height = aDesiredSize.height * numRows;
  } else {
    aDesiredSize.height = aDesiredSize.height * aSpec.mRowDefaultSize;
  }

  numRows = (aRowHeight > 0) ? (aDesiredSize.height / aRowHeight) : 0;
  if (numRows == 1) {
    PRInt32 type;
    GetType(&type);
    if (NS_FORM_TEXTAREA == type) {
      aDesiredSize.height += fontHeight;
    }
  }

  // if we are not using the default size 
  // then make the minimum size the size we want to be
  if (!aIsUsingDefSize) {
    aMinSize.width  = aDesiredSize.width;
    aMinSize.height = aDesiredSize.height;
  }

  return numRows;
}




PRInt32
nsGfxTextControlFrame2::CalculateSizeNavQuirks (nsIPresContext*       aPresContext, 
                                              nsIRenderingContext*  aRendContext,
                                              nsIFormControlFrame*  aFrame,
                                              nsInputDimensionSpec& aSpec, 
                                              nsSize&               aDesiredSize, 
                                              nsSize&               aMinSize, 
                                              nscoord&              aRowHeight,
                                              nsMargin&             aBorder,
                                              nsMargin&             aPadding,
                                              PRBool                aIsUsingDefSize)
{
  nscoord charWidth   = 0; 
  aDesiredSize.width  = CSS_NOTSET;
  aDesiredSize.height = CSS_NOTSET;

  // Quirks does not use rowAttr
  nsHTMLValue colAttr;
  nsresult    colStatus;
  nsHTMLValue rowAttr;
  nsresult    rowStatus;
  if (NS_ERROR_FAILURE == GetColRowSizeAttr(aFrame, 
                                            aSpec.mColSizeAttr, colAttr, colStatus,
                                            aSpec.mRowSizeAttr, rowAttr, rowStatus)) {
    return 0;
  }

  // Get the Font Metrics for the Control
  // without it we can't calculate  the size
  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult res = nsFormControlHelper::GetFrameFontFM(aPresContext, aFrame, getter_AddRefs(fontMet));
  if (NS_SUCCEEDED(res) && fontMet) {
    aRendContext->SetFont(fontMet);

    // Calculate the min size of the text control as one char
    // save the current default col size
    nscoord tmpCol        = aSpec.mColDefaultSize;
    aSpec.mColDefaultSize = 1;
    charWidth = nsFormControlHelper::CalcNavQuirkSizing(aPresContext, 
                                                        aRendContext, fontMet, 
                                                        aFrame, aSpec, aDesiredSize);
    // set the default col size back
    aMinSize.width        = aDesiredSize.width;
    aMinSize.height       = aDesiredSize.height;
    aSpec.mColDefaultSize = tmpCol;

    // Figure out the number of columns
    // and set that as the default col size
    if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
      PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
      col = (col <= 0) ? 1 : col; // XXX why a default of 1 char, why hide it
      aSpec.mColDefaultSize = col;
    }
    charWidth = nsFormControlHelper::CalcNavQuirkSizing(aPresContext, 
                                                        aRendContext, fontMet, 
                                                        aFrame, aSpec, aDesiredSize);
    aDesiredSize.height = aDesiredSize.height * aSpec.mRowDefaultSize;
  } else {
    NS_ASSERTION(fontMet, "Couldn't get Font Metrics"); 
    aDesiredSize.width = 300;  // arbitrary values
    aDesiredSize.width = 1500;
  }

  aRowHeight      = aDesiredSize.height;
  aMinSize.height = aDesiredSize.height;

  // if we are not using the default size 
  // then make the minimum size the size we want to be
  if (!aIsUsingDefSize) {
    aMinSize.width  = aDesiredSize.width;
    aMinSize.height = aDesiredSize.height;
  }

  PRInt32 numRows = (aRowHeight > 0) ? (aDesiredSize.height / aRowHeight) : 0;

  return numRows;
}

//------------------------------------------------------------------
NS_IMETHODIMP
nsGfxTextControlFrame2::ReflowNavQuirks(nsIPresContext*          aPresContext,
                                        nsSize&                  aDesiredSize,
                                        const nsHTMLReflowState& aReflowState,
                                        nsReflowStatus&          aStatus,
                                        nsMargin&                aBorder,
                                        nsMargin&                aPadding)
{
  PRBool usingDefaultSize = PR_FALSE;
  PRInt32 ignore;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
    PRInt32 width = 0;
    if (NS_CONTENT_ATTR_HAS_VALUE != GetSizeFromContent(&width)) {
      width = GetDefaultColumnWidth();
      usingDefaultSize = PR_TRUE;
    }
    nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull,
                                  nsnull, width, 
                                  PR_FALSE, nsnull, 1);
    CalculateSizeNavQuirks(aPresContext, aReflowState.rendContext, this,  
                           textSpec, aDesiredSize, mMinSize, ignore, aBorder, aPadding, usingDefaultSize);
  } else {
    nsInputDimensionSpec areaSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, 
                                  nsnull, GetDefaultColumnWidth(), 
                                  PR_FALSE, nsHTMLAtoms::rows, 1);
    CalculateSizeNavQuirks(aPresContext, aReflowState.rendContext, this,  
                           areaSpec, aDesiredSize, mMinSize, ignore, aBorder, aPadding, usingDefaultSize);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsGfxTextControlFrame2::CreateFrameFor(nsIPresContext*   aPresContext,
                                       nsIContent *      aContent,
                                       nsIFrame**        aFrame)
{
  aContent = nsnull;
  return NS_ERROR_FAILURE;
}

nsresult
nsGfxTextControlFrame2::SetInitialValue()
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
  GetTextControlFrameState(defaultValue);

  // Turn on mUseEditor so that subsequent calls will use the
  // editor.
  mUseEditor = PR_TRUE;

  // If we have a default value, insert it under the div we created
  // above, but be sure to use the editor so that '*' characters get
  // displayed for password fields, etc. SetTextControlFrameState()
  // will call the editor for us.

  if (!defaultValue.IsEmpty()) {
    PRUint32 editorFlags = 0;

    nsresult rv = mEditor->GetFlags(&editorFlags);

    if (NS_FAILED(rv))
      return rv;

    // Avoid causing reentrant painting and reflowing by telling the editor
    // that we don't want it to force immediate view refreshes or force
    // immediate reflows during any editor calls.

    rv = mEditor->SetFlags(editorFlags |
                           nsIPlaintextEditor::eEditorDisableForcedUpdatesMask |
                           nsIPlaintextEditor::eEditorDisableForcedReflowsMask);

    if (NS_FAILED(rv))
      return rv;

    // Now call SetTextControlFrameState() which will make the
    // neccessary editor calls to set the default value.
    // Make sure to turn off undo before setting the default
    // value, and turn it back on afterwards. This will make
    // sure we can't undo past the default value.

    rv = mEditor->EnableUndo(PR_FALSE);

    if (NS_FAILED(rv))
      return rv;

    SetTextControlFrameState(defaultValue);

    rv = mEditor->EnableUndo(PR_TRUE);

    if (NS_FAILED(rv))
      return rv;

    // Now restore the original editor flags.

    rv = mEditor->SetFlags(editorFlags);

    if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}

#define DIV_STRING \
  "-moz-user-focus: none;" \
  "border: 0px !important;" \
  "padding: 0px;" \
  "margin: 0px;" \
  ""

#define DIV_STRING_SINGLELINE \
  "-moz-user-focus: none;" \
  "white-space : nowrap;" \
  "overflow: auto;" \
  "border: 0px !important;" \
  "padding: 0px;" \
  "margin: 0px;" \
  ""

NS_IMETHODIMP
nsGfxTextControlFrame2::CreateAnonymousContent(nsIPresContext* aPresContext,
                                           nsISupportsArray& aChildList)
{
  // Get the PresShell

  mState |= NS_FRAME_INDEPENDENT_SELECTION;

  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));

  if (NS_FAILED(rv))
    return rv;

  if (!shell)
    return NS_ERROR_FAILURE;

  // Get the DOM document

  nsCOMPtr<nsIDocument> doc;
  rv = shell->GetDocument(getter_AddRefs(doc));
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
  nsCOMPtr<nsIElementFactory> elementFactory;
  rv = GetElementFactoryService(getter_AddRefs(elementFactory));
  if (NS_FAILED(rv))
    return rv;
  if (!elementFactory)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsINodeInfoManager> nodeInfoManager;
  rv = doc->GetNodeInfoManager(*getter_AddRefs(nodeInfoManager));

  if (NS_FAILED(rv))
    return rv;

  NS_ENSURE_TRUE(nodeInfoManager, NS_ERROR_FAILURE);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = nodeInfoManager->GetNodeInfo(nsHTMLAtoms::div, nsnull, kNameSpaceID_HTML, *getter_AddRefs(nodeInfo));

  if (NS_FAILED(rv))
    return rv;

  if (!nodeInfo)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> divContent;

  rv = elementFactory->CreateInstanceByTag(nodeInfo, getter_AddRefs(divContent));

  if (NS_FAILED(rv))
    return rv;

  if (!divContent)
    return NS_ERROR_FAILURE;


  // Set the neccessary style attributes on the text control.

  if (IsSingleLineTextControl())
    rv = divContent->SetAttr(kNameSpaceID_None,nsHTMLAtoms::style, NS_ConvertASCIItoUCS2(DIV_STRING_SINGLELINE), PR_FALSE);
  else {
    nsAutoString divStr; divStr.AssignWithConversion(DIV_STRING);
    const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
    if (disp->mOverflow == NS_STYLE_OVERFLOW_SCROLL)
      divStr += NS_LITERAL_STRING("overflow:scroll;");
    else if (disp->mOverflow == NS_STYLE_OVERFLOW_HIDDEN)
      divStr += NS_LITERAL_STRING("overflow:hidden;");
    else divStr += NS_LITERAL_STRING("overflow:auto;");
    rv = divContent->SetAttr(kNameSpaceID_None,nsHTMLAtoms::style, divStr, PR_FALSE);
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

  //all gfxtextcontrolframe2's are widgets
  editorFlags |= nsIPlaintextEditor::eEditorWidgetMask;

  // Now initialize the editor.
  //
  // NOTE: Conversion of '\n' to <BR> happens inside the
  //       editor's Init() call.

  rv = mEditor->Init(domdoc, shell, divContent, mSelCon, editorFlags);

  if (NS_FAILED(rv))
    return rv;

  // Initialize the controller for the editor

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
        nsCOMPtr<nsIEditorController> editController = do_QueryInterface(controller);
        if (editController)
        {
          editController->SetCommandRefCon(mEditor);
          found = PR_TRUE;
        }
      }
    }
    if (!found)
      rv = NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPlaintextEditor> textEditor(do_QueryInterface(mEditor));
  if (textEditor)
  {
    nsHTMLValue colAttr;
    nsresult    colStatus;
    nsHTMLValue rowAttr;
    nsresult    rowStatus;
    PRInt32 type;
    GetType(&type);
    nsInputDimensionSpec *spec;
    if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
      PRInt32 width = 0;
      if (NS_CONTENT_ATTR_HAS_VALUE != GetSizeFromContent(&width)) {
        width = GetDefaultColumnWidth();
      }
      spec = new nsInputDimensionSpec(nsnull, PR_FALSE, nsnull,
                                    nsnull, width, 
                                    PR_FALSE, nsnull, 1);
    } else {
      spec = new nsInputDimensionSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, 
                                    nsnull, GetDefaultColumnWidth(), 
                                    PR_FALSE, nsHTMLAtoms::rows, 1);
    }
    if (spec)
    {
      if (NS_FAILED(GetColRowSizeAttr(this, 
                                              spec->mColSizeAttr, colAttr, colStatus,
                                              spec->mRowSizeAttr, rowAttr, rowStatus)))
        return NS_ERROR_FAILURE;
      PRInt32 col =-1;
      if (!(colAttr.GetUnit() == eHTMLUnit_Null))
      {
        col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
        col = (col <= 0) ? 1 : col; // XXX why a default of 1 char, why hide it
      }
      textEditor->SetWrapWidth(col);
      delete spec;
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
  
  // also set up the text listener as a transaction listener on the editor
  if (mEditor && mTextListener)
  {
    nsCOMPtr<nsITransactionManager> txMgr;
    rv = mEditor->GetTransactionManager(getter_AddRefs(txMgr));
    if (NS_FAILED(rv)) return rv;
    if (!txMgr) return NS_ERROR_NULL_POINTER;

    rv = txMgr->AddListener(NS_STATIC_CAST(nsITransactionListener*, mTextListener));
    if (NS_FAILED(rv)) return rv;
  }  

  if (mContent)
  {
    rv = mEditor->GetFlags(&editorFlags);

    if (NS_FAILED(rv))
      return rv;

    PRInt32 nameSpaceID;

    rv = mContent->GetNameSpaceID(nameSpaceID);

    if (NS_FAILED(rv))
      return rv;

    nsAutoString resultValue;

    // Check if the readonly attribute is set.

    rv = mContent->GetAttr(nameSpaceID, nsHTMLAtoms::readonly, resultValue);

    if (NS_FAILED(rv))
      return rv;

    if (NS_CONTENT_ATTR_NOT_THERE != rv)
      editorFlags |= nsIPlaintextEditor::eEditorReadonlyMask;

    // Check if the disabled attribute is set.

    rv = mContent->GetAttr(nameSpaceID, nsHTMLAtoms::disabled, resultValue);

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
        mSelCon->SetCaretEnabled(PR_FALSE);

        if (editorFlags & nsIPlaintextEditor::eEditorDisabledMask)
          mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
      }

      mEditor->SetFlags(editorFlags);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::Reflow(nsIPresContext*   aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsGfxTextControlFrame2", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  SetInitialValue();

  // make sure the the form registers itself on the initial/first reflow
  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
    nsFormFrame::AddFormControlFrame(aPresContext, *NS_STATIC_CAST(nsIFrame*, this));
    mNotifyOnInput = PR_TRUE;//its ok to notify now. all has been prepared.
  }

  nsresult rv = nsStackFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  if (NS_SUCCEEDED(rv))
  { // fix for bug 40596, width:auto means the control sets it's maxElementSize.width to it's default width
    if (aDesiredSize.maxElementSize)
    {
      nsStylePosition *stylePosition;
      GetStyleData(eStyleStruct_Position,  (const nsStyleStruct *&)stylePosition);
      nsStyleUnit widthUnit = stylePosition->mWidth.GetUnit();
      if (eStyleUnit_Auto == widthUnit) {
        aDesiredSize.maxElementSize->width = aDesiredSize.width;
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::Paint(nsIPresContext*      aPresContext,
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
    rv = nsStackFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FLOATERS);
    if (NS_FAILED(rv)) return rv;
    rv = nsStackFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FOREGROUND);
  }
  DO_GLOBAL_REFLOW_COUNT_DSP("nsGfxTextControlFrame2", &aRenderingContext);
  return rv;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetFrameForPoint(nsIPresContext* aPresContext,
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
                                        NS_FRAME_PAINT_LAYER_FLOATERS, aFrame);
    if (NS_SUCCEEDED(rv))
      return NS_OK;
    rv = nsStackFrame::GetFrameForPoint(aPresContext, aPoint,
                                      NS_FRAME_PAINT_LAYER_BACKGROUND, aFrame);
  }
  return rv;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mPrefSize)) {
     aSize = mPrefSize;
     return NS_OK;
  }

  PropagateDebug(aState);

  // navquirk can only happen if we are in the HTML namespace. It does not apply in XUL.
  PRInt32 nameSpaceID;
  mContent->GetNameSpaceID(nameSpaceID);
  
  aSize.width = 0;
  aSize.height = 0;

  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed)
    return NS_OK;

  nsIPresContext* aPresContext = aState.GetPresContext();
  const nsHTMLReflowState* aReflowState = aState.GetReflowState();
  nsSize styleSize(CSS_NOTSET,CSS_NOTSET);
  nsFormControlFrame::GetStyleSize(aPresContext, *aReflowState, styleSize);

  if (!aReflowState)
    return NS_OK;

  SetInitialValue();

  if (mState & NS_FRAME_FIRST_REFLOW)
    mNotifyOnInput = PR_TRUE;//its ok to notify now. all has been prepared.

  nsCompatibility mode;
  nsFormControlHelper::GetFormCompatibilityMode(aPresContext, mode);
  PRBool navQuirksMode = eCompatibility_NavQuirks == mode && nameSpaceID == kNameSpaceID_HTML;

  nsReflowStatus aStatus;
  nsMargin border;
  border.SizeTo(0, 0, 0, 0);
  nsMargin padding;
  padding.SizeTo(0, 0, 0, 0);

  // Get the CSS border
  const nsStyleBorder* borderStyle;
  const nsStylePadding* paddingStyle;
  GetStyleData(eStyleStruct_Border,  (const nsStyleStruct *&)borderStyle);
  GetStyleData(eStyleStruct_Padding,  (const nsStyleStruct *&)paddingStyle);
  borderStyle->CalcBorderFor(this, border);
  paddingStyle->CalcPaddingFor(this, padding);

  nsresult rv;
  if (navQuirksMode) {
    rv = ReflowNavQuirks(aPresContext, aSize, *aReflowState, aStatus, border, padding);
  } else {
    rv = ReflowStandard(aPresContext, aSize, *aReflowState, aStatus, border, padding);
  }
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
nsGfxTextControlFrame2::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
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
nsGfxTextControlFrame2::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  return nsBox::GetMaxSize(aState, aSize);
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  nsSize size;
  nsresult rv = GetPrefSize(aState, size);
  aAscent = size.height;
  return rv;
}

PRIntn
nsGfxTextControlFrame2::GetSkipSides() const
{
  return 0;
}

//IMPLEMENTING NS_IFORMCONTROLFRAME
NS_IMETHODIMP
nsGfxTextControlFrame2::GetName(nsAString* aResult)
{
  nsresult rv = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    rv = mContent->QueryInterface(NS_GET_IID(nsIHTMLContent),(void**)&formControl);
    if (NS_SUCCEEDED(rv) && formControl) {
      nsHTMLValue value;
      rv = formControl->GetHTMLAttribute(nsHTMLAtoms::name, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(*aResult);
        }
      }
      NS_RELEASE(formControl);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetType(PRInt32* aType) const
{
  nsresult rv = NS_FORM_NOTOK;
  if (mContent) {
    nsIFormControl* formControl = nsnull;
    rv = mContent->QueryInterface(NS_GET_IID(nsIFormControl), (void**)&formControl);
    if ((NS_OK == rv) && formControl) {
      rv = formControl->GetType(aType);
      NS_RELEASE(formControl);
    }
  }
  return rv;
}

nsresult
nsGfxTextControlFrame2::GetSizeFromContent(PRInt32* aSize) const
{
  *aSize = -1;
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::size, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
    NS_RELEASE(content);
  }
  if (*aSize < 1) {
    // This is part of bug 46224
    // when we can get a PresContent (may be cache it) 
    // then we can check the compatibility mode
#ifdef FUTURE_ADDITIONAL_FIX_FOR_46224
    nsCompatibility mode;
    nsFormControlHelper::GetFormCompatibilityMode(aPresContext, mode);
    if (eCompatibility_NavQuirks == mode) {
      *aSize = 1;
    } else {
      *aSize = 20;
    }
#else
      *aSize = 20; // use '1' to be compatable with Nav 4.x, Use '20' to be compatable with IE
#endif
  }
  return result;
}

void    nsGfxTextControlFrame2::SetFocus(PRBool aOn , PRBool aRepaint){}

void    nsGfxTextControlFrame2::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    }
  }
}

void    nsGfxTextControlFrame2::MouseClicked(nsIPresContext* aPresContext){}

nscoord 
nsGfxTextControlFrame2::GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(0, aPixToTwip); 
}


//---------------------------------------------------------
nscoord 
nsGfxTextControlFrame2::GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return GetVerticalInsidePadding(aPresContext, aPixToTwip, aInnerWidth);
}


void 
nsGfxTextControlFrame2::SetFormFrame(nsFormFrame* aFormFrame) 
{ 
  mFormFrame = aFormFrame; 
}


NS_IMETHODIMP 
nsGfxTextControlFrame2::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  mSuggestedWidth = aWidth;
  mSuggestedHeight = aHeight;
  return NS_OK;
}

nsresult 
nsGfxTextControlFrame2::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetFont(nsIPresContext* aPresContext, 
                            const nsFont*&  aFont)
{
  return nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
}

NS_IMETHODIMP nsGfxTextControlFrame2::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue)
{
  if (!mIsProcessing)//some kind of lock.
  {
    mIsProcessing = PR_TRUE;
    
    if (nsHTMLAtoms::value == aName) 
    {
      if (mEditor) {
        mEditor->EnableUndo(PR_FALSE);    // wipe out undo info
      }
      if (mEditor && mUseEditor) {
        // If the editor exists, the control needs to be informed that the value
        // has changed.
        SetValueChanged(PR_TRUE);
      }
      SetTextControlFrameState(aValue);   // set new text value
      if (mEditor)  {
        mEditor->EnableUndo(PR_TRUE);     // fire up a new txn stack
      }
    }
    else if (nsHTMLAtoms::select == aName && mSelCon)
    {
      // select all the text
      SelectAllContents();
    }
    mIsProcessing = PR_FALSE;
  }
  return NS_OK;
}      

NS_IMETHODIMP nsGfxTextControlFrame2::GetProperty(nsIAtom* aName, nsAWritableString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If widget is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::value == aName) {
    GetTextControlFrameState(aValue);
  }
  return NS_OK;
}  



NS_IMETHODIMP
nsGfxTextControlFrame2::GetEditor(nsIEditor **aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);
  *aEditor = mEditor;
  NS_IF_ADDREF(*aEditor);
  return NS_OK;
}



NS_IMETHODIMP
nsGfxTextControlFrame2::GetTextLength(PRInt32* aTextLength)
{
  NS_ENSURE_ARG_POINTER(aTextLength);

  nsAutoString   textContents;
  GetTextControlFrameState(textContents);   // this is expensive!
  *aTextLength = textContents.Length();
  return NS_OK;
}



NS_IMETHODIMP
nsGfxTextControlFrame2::GetFirstTextNode(nsIDOMCharacterData* *aFirstTextNode)
{
  if (!mEditor)
    return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMElement> rootElement;
  mEditor->GetRootElement(getter_AddRefs(rootElement));
  *aFirstTextNode = nsnull;
  
  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootElement);
  if (!rootNode) return NS_ERROR_FAILURE;
  
  // for a text widget, the text of the document is in a single
  // text node under the body. Let's make sure that's true.
  nsCOMPtr<nsIDOMNodeList> childNodesList;
  rootNode->GetChildNodes(getter_AddRefs(childNodesList));
  if (!childNodesList)
  {
    NS_WARNING("rootNode has no text node list");
    return NS_ERROR_FAILURE;
  }

  PRUint32 numChildNodes = 0;
  childNodesList->GetLength(&numChildNodes);

  nsCOMPtr<nsIDOMNode> firstChild;
  nsresult rv = rootNode->GetFirstChild(getter_AddRefs(firstChild));
  if (NS_FAILED(rv)) return rv;
  if (!firstChild) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMCharacterData> charDataNode = do_QueryInterface(firstChild, &rv);
  if (NS_FAILED(rv)) return rv;
  
  NS_ADDREF(*aFirstTextNode = charDataNode);
  return NS_OK;
}


nsresult
nsGfxTextControlFrame2::SelectAllContents()
{
  nsresult rv;
  
  if (IsSingleLineTextControl())
  {
    rv = SetSelectionRange(0, eSelectToEnd);
  }
  else
  {
    // we have to select all
    if (!mEditor)
      return NS_ERROR_NOT_INITIALIZED;
    NS_ASSERTION(mEditor, "Should have an editor here");    
    rv = mEditor->SelectAll();
  }

  return rv;
}


nsresult
nsGfxTextControlFrame2::SetSelectionEndPoints(PRInt32 aSelStart, PRInt32 aSelEnd)
{
  NS_ASSERTION(IsSingleLineTextControl(), "Should only call this on a single line input");
  NS_ASSERTION(mEditor, "Should have an editor here");
  NS_ASSERTION(mTextSelImpl,"selection not found!");

  nsCOMPtr<nsIDOMCharacterData> firstTextNode;
  nsresult rv = GetFirstTextNode(getter_AddRefs(firstTextNode));
  if (NS_FAILED(rv) || !firstTextNode)
  {
    // probably an empty document. not an error
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMNode> firstNode = do_QueryInterface(firstTextNode, &rv);
  if (!firstNode) return rv;
  
  // constrain the selection to this node
  PRUint32 nodeLengthU;
  firstTextNode->GetLength(&nodeLengthU);
  PRInt32 nodeLength = (PRInt32)nodeLengthU;
    
  nsCOMPtr<nsISelection> selection;
  mTextSelImpl->GetSelection(nsISelectionController::SELECTION_NORMAL,getter_AddRefs(selection));  
  if (!selection) return NS_ERROR_FAILURE;

  // are we setting both start and end?
  if (aSelStart != eIgnoreSelect && aSelEnd != eIgnoreSelect)
  {
    if (aSelStart == eSelectToEnd || aSelStart > nodeLength)
      aSelStart = nodeLength;
    if (aSelStart < 0)
      aSelStart = 0;

    if (aSelEnd == eSelectToEnd || aSelEnd > nodeLength)
      aSelEnd = nodeLength;
    if (aSelEnd < 0)
      aSelEnd = 0;

    // remove existing ranges
    selection->RemoveAllRanges();  

    nsCOMPtr<nsIDOMRange> selectionRange(do_CreateInstance(kRangeCID,&rv));
    if (NS_FAILED(rv)) 
      return rv;
    
    selectionRange->SetStart(firstTextNode, aSelStart);
    selectionRange->SetEnd(firstTextNode, aSelEnd);
    
    selection->AddRange(selectionRange);
  }
  else    // we're setting either start or end but not both
  {
    // does a range exist?
    nsCOMPtr<nsIDOMRange> firstRange;
    selection->GetRangeAt(0, getter_AddRefs(firstRange));
    PRBool mustAdd = PR_FALSE;
    PRInt32 selStart = 0, selEnd = 0;

    if (firstRange)
    {
     firstRange->GetStartOffset(&selStart);
     firstRange->GetEndOffset(&selEnd);
    }
    else
    {
      // no range. Make a new one.
      firstRange = do_CreateInstance(kRangeCID,&rv);
      if (NS_FAILED(rv)) 
        return rv;
      mustAdd = PR_TRUE;
    }
    
    if (aSelStart == eSelectToEnd)
      selStart = nodeLength;
    else if (aSelStart != eIgnoreSelect)
      selStart = aSelStart;

    if (aSelEnd == eSelectToEnd)
      selEnd = nodeLength;
    else if (aSelEnd != eIgnoreSelect)
      selEnd = aSelEnd;
    
    // swap them
    if (selEnd < selStart)
    {
      PRInt32 temp = selStart;
      selStart = selEnd;
      selEnd = temp;
    }
    
    firstRange->SetStart(firstTextNode, selStart);
    firstRange->SetEnd(firstTextNode, selEnd);
    if (mustAdd)  
      selection->AddRange(firstRange);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::SetSelectionRange(PRInt32 aSelStart, PRInt32 aSelEnd)
{
  if (!IsSingleLineTextControl()) return NS_ERROR_NOT_IMPLEMENTED;
  
  // make sure we have an editor
  if (!mEditor) 
    return NS_ERROR_NOT_INITIALIZED;
  
  return SetSelectionEndPoints(aSelStart, aSelEnd);
}


NS_IMETHODIMP
nsGfxTextControlFrame2::SetSelectionStart(PRInt32 aSelectionStart)
{
  if (!IsSingleLineTextControl()) return NS_ERROR_NOT_IMPLEMENTED;

  // make sure we have an editor
  if (!mEditor) 
    return NS_ERROR_NOT_INITIALIZED;
  
  return SetSelectionEndPoints(aSelectionStart, eIgnoreSelect);
}

NS_IMETHODIMP
nsGfxTextControlFrame2::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  if (!IsSingleLineTextControl()) return NS_ERROR_NOT_IMPLEMENTED;

  // make sure we have an editor
  if (!mEditor) 
    return NS_ERROR_NOT_INITIALIZED;
  
  return SetSelectionEndPoints(eIgnoreSelect, aSelectionEnd);
}


NS_IMETHODIMP
nsGfxTextControlFrame2::GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd)
{
    NS_ENSURE_ARG_POINTER((aSelectionStart && aSelectionEnd));

    // make sure we have an editor
    if (!mEditor) 
      return NS_ERROR_NOT_INITIALIZED;

    *aSelectionStart = 0;
    *aSelectionEnd = 0;

    nsCOMPtr<nsISelection> selection;
    mTextSelImpl->GetSelection(nsISelectionController::SELECTION_NORMAL,getter_AddRefs(selection));  
    if (!selection) return NS_ERROR_FAILURE;

    // we should have only zero or one range
    PRInt32 numRanges = 0;
    selection->GetRangeCount(&numRanges);
    if (numRanges > 1)
    {
      NS_ASSERTION(0, "Found more than on range in GetSelectionRange");
    }
  
    if (numRanges != 0)
    {
      nsCOMPtr<nsIDOMRange> firstRange;
      selection->GetRangeAt(0, getter_AddRefs(firstRange));
      if (!firstRange) 
        return NS_ERROR_FAILURE;

      if (IsSingleLineTextControl())
      {
        firstRange->GetStartOffset(aSelectionStart);
        firstRange->GetEndOffset(aSelectionEnd);
      }
      else//multiline
      {
        //mContent = parent. iterate over each child. 
        //when text nodes are reached add text length. 
        //if you find range-startoffset,startnode then mark aSelecitonStart
        nsresult rv = NS_ERROR_FAILURE;
        nsCOMPtr<nsIDOMNode> contentNode;
        nsCOMPtr<nsIDOMNode> curNode;
        contentNode = do_QueryInterface(mContent);
        if (!contentNode || NS_FAILED(rv = contentNode->GetFirstChild(getter_AddRefs(curNode))) || !curNode)
          return rv;
        nsCOMPtr<nsIDOMNode> startParent;
        nsCOMPtr<nsIDOMNode> endParent;
        PRInt32 startOffset;
        PRInt32 endOffset;

        firstRange->GetStartContainer(getter_AddRefs(startParent));
        firstRange->GetStartOffset(&startOffset);
        firstRange->GetEndContainer(getter_AddRefs(endParent));
        firstRange->GetEndOffset(&endOffset);

        PRInt32 currentTextOffset = 0;
        
        while(curNode)
        {
          nsCOMPtr<nsIDOMText> domText;
          domText = do_QueryInterface(curNode);
          if (contentNode == startParent)
          {
            if (domText)
              *aSelectionStart = currentTextOffset + startOffset;
            else
              *aSelectionStart = currentTextOffset;
          }
          if (curNode == endParent)
          {
            if (domText)
              *aSelectionEnd = currentTextOffset + endOffset;
            else
              *aSelectionEnd = currentTextOffset;
            break;
          }
          if (domText)
          {
            PRUint32 length;
            if (NS_SUCCEEDED(domText->GetLength(&length)))
              currentTextOffset += length;
          }
          else
            ++currentTextOffset;
        }
        if (!curNode) //something went very wrong...
        {
          *aSelectionEnd = *aSelectionStart;//couldnt find the end
        }
      }
    }
    return NS_OK;
}


NS_IMETHODIMP
nsGfxTextControlFrame2::GetSelectionContr(nsISelectionController **aSelCon)
{
  NS_ENSURE_ARG_POINTER(aSelCon);
  NS_IF_ADDREF(*aSelCon = mSelCon);
  return NS_OK;
}


/////END INTERFACE IMPLEMENTATIONS

////NSIFRAME
NS_IMETHODIMP
nsGfxTextControlFrame2::AttributeChanged(nsIPresContext* aPresContext,
                                        nsIContent*     aChild,
                                        PRInt32         aNameSpaceID,
                                        nsIAtom*        aAttribute,
                                        PRInt32         aModType, 
                                        PRInt32         aHint)
{
  if (!mEditor || !mSelCon) {return NS_ERROR_NOT_INITIALIZED;}
  nsresult rv = NS_OK;

  if (nsHTMLAtoms::value == aAttribute) 
  {
    // XXX If this should happen when value= attribute is set, shouldn't it
    // happen when .value is set too?
    if (aHint != NS_STYLE_HINT_REFLOW)
      nsFormFrame::StyleChangeReflow(aPresContext, this);
  } 
  else if (nsHTMLAtoms::maxlength == aAttribute) 
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
      if (mSelCon)
        mSelCon->SetCaretEnabled(PR_FALSE);
    }
    else 
    { // unset readonly
      flags &= ~(nsIPlaintextEditor::eEditorReadonlyMask);
      if (mSelCon && !(flags & nsIPlaintextEditor::eEditorDisabledMask))
        mSelCon->SetCaretEnabled(PR_TRUE);
    }    
    mEditor->SetFlags(flags);
  }
  else if (mEditor && nsHTMLAtoms::disabled == aAttribute) 
  {
    nsCOMPtr<nsIPresShell> shell;
    rv = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(rv))
      return rv;
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
        mSelCon->SetCaretEnabled(PR_FALSE);
        mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
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
  else if ((nsHTMLAtoms::size == aAttribute ||
            nsHTMLAtoms::rows == aAttribute ||
            nsHTMLAtoms::cols == aAttribute) && aHint != NS_STYLE_HINT_REFLOW) {
    // XXX Bug 34573 & 50280
    // The following code should be all we need for these two bugs (it does work for bug 50280)
    // This doesn't wrong entirely for rows/cols, the borders don't get painted
    // to fix that I have added a REFLOW hint in nsHTMLTextAreaElement::GetMappedAttributeImpact
    // but it appears there are some problems when you hold down the return key
    mPrefSize.width  = -1;
    mPrefSize.height = -1;
    nsFormFrame::StyleChangeReflow(aPresContext, this);
  }
  // Allow the base class to handle common attributes supported
  // by all form elements... 
  else {
    rv = nsBoxFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aModType, aHint);
  }

  return rv;
}


NS_IMETHODIMP
nsGfxTextControlFrame2::GetText(nsString* aText)
{
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
    GetTextControlFrameState(*aText);
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



///END NSIFRAME OVERLOADS
/////BEGIN PROTECTED METHODS

void nsGfxTextControlFrame2::RemoveNewlines(nsString &aString)
{
  // strip CR/LF and null
  static const char badChars[] = {10, 13, 0};
  aString.StripChars(badChars);
}


NS_IMETHODIMP
nsGfxTextControlFrame2::GetMaxLength(PRInt32* aSize)
{
  *aSize = -1;
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    rv = content->GetHTMLAttribute(nsHTMLAtoms::maxlength, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
    NS_RELEASE(content);
  }
  return rv;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::DoesAttributeExist(nsIAtom *aAtt)
{
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) 
  {
    nsHTMLValue value;
    rv = content->GetHTMLAttribute(aAtt, value);
    NS_RELEASE(content);
  }
  return rv;
}

void
nsGfxTextControlFrame2::SubmitAttempt()
{
  // Submit the form
  PRInt32 type;
  GetType(&type);
  if (mFormFrame && mTextSelImpl && NS_FORM_TEXTAREA != type) {
    nsWeakPtr &shell = mTextSelImpl->GetPresShell();
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(shell);
    if (!presShell) return;
    {
      nsCOMPtr<nsIPresContext> context;
      if (NS_SUCCEEDED(presShell->GetPresContext(getter_AddRefs(context))) && context)
      {
        // We got here because somebody press <return> instead a text input
        // Bug 99920 - Finds the first submit button and how many text inputs there
        // if there is only one text input or password then submission can take place
        // or it can take place if it finds at least one submit button
        //
        // Here we change the submitter frame to the submit button it found (if it found one)
        // so the button gets included as part of the "post data"
        nsIFrame* originFrame = this;
        PRInt32 inputTxtCnt;
        nsIFrame* submitBtn = mFormFrame->GetFirstSubmitButtonAndTxtCnt(inputTxtCnt);
        if (submitBtn != nsnull) {
          originFrame = submitBtn;
        }
        if (inputTxtCnt == 1 || submitBtn != nsnull) {
          // do Submit & Frame processing of event
          nsFormControlHelper::DoManualSubmitOrReset(context, presShell, mFormFrame, 
                                                     originFrame, PR_TRUE, PR_FALSE); 
        }
      }
    }
  }
}

// this is where we propogate a content changed event
NS_IMETHODIMP
nsGfxTextControlFrame2::InternalContentChanged()
{
  NS_PRECONDITION(mContent, "illegal to call unless we map to a content node");

  if (!mContent) { return NS_ERROR_NULL_POINTER; }

  if (PR_FALSE==mNotifyOnInput) { 
    return NS_OK; // if notification is turned off, just return ok
  } 
  
  // Dispatch the change event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  event.widget = nsnull;
  event.message = NS_FORM_INPUT;
  event.flags = NS_EVENT_FLAG_INIT;

  // Have the content handle the event, propogating it according to normal DOM rules.
  nsWeakPtr &shell = mTextSelImpl->GetPresShell();
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(shell);
  if (!presShell) 
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPresContext> context;
  if (NS_SUCCEEDED(presShell->GetPresContext(getter_AddRefs(context))) && context)
    return mContent->HandleDOMEvent(context, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsGfxTextControlFrame2::CallOnChange()
{
  // Dispatch th1e change event
  nsCOMPtr<nsIContent> content;
  if (NS_SUCCEEDED(GetFormContent(*getter_AddRefs(content))))
  {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsInputEvent event;
    event.eventStructType = NS_INPUT_EVENT;
    event.widget = nsnull;
    event.message = NS_FORM_CHANGE;
    event.flags = NS_EVENT_FLAG_INIT;
    event.isShift = PR_FALSE;
    event.isControl = PR_FALSE;
    event.isAlt = PR_FALSE;
    event.isMeta = PR_FALSE;

    // Have the content handle the event.
    nsWeakPtr &shell = mTextSelImpl->GetPresShell();
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(shell);
    if (!presShell)
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsIPresContext> context;
    if (NS_SUCCEEDED(presShell->GetPresContext(getter_AddRefs(context))) && context)
      return mContent->HandleDOMEvent(context, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
  }
  return NS_OK;
}


//======
//privates

void nsGfxTextControlFrame2::GetTextControlFrameState(nsAWritableString& aValue)
{
  aValue.Truncate();  // initialize out param
  
  if (mEditor && mUseEditor) 
  {
    PRUint32 flags = nsIDocumentEncoder::OutputLFLineBreak;;

    if (PR_TRUE==IsPlainTextControl())
    {
      flags |= nsIDocumentEncoder::OutputBodyOnly;
    }

    nsFormControlHelper::nsHTMLTextWrap wrapProp;
    nsresult rv = nsFormControlHelper::GetWrapPropertyEnum(mContent, wrapProp);
    flags |= nsIDocumentEncoder::OutputPreformatted;
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    {
      if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Hard)
      {
        flags |= nsIDocumentEncoder::OutputWrap;
      }
    }

    mEditor->OutputToString(aValue, NS_LITERAL_STRING("text/plain"), flags);
  }
  else
  {
    // Otherwise get the value from content.
    nsCOMPtr<nsITextControlElement> control = do_QueryInterface(mContent);
    if (control)
    {
      control->GetValueInternal(aValue);
    }
  }
}


// END IMPLEMENTING NS_IFORMCONTROLFRAME

void
nsGfxTextControlFrame2::SetTextControlFrameState(const nsAReadableString& aValue)
{
  if (mEditor && mUseEditor) 
  {
    nsAutoString currentValue;
    GetTextControlFrameState(currentValue);
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
    // Otherwise get the value from content.
    nsCOMPtr<nsITextControlElement> control = do_QueryInterface(mContent);
    if (control)
    {
      control->SetValueInternal(aValue);
    }
  }
}


NS_IMETHODIMP
nsGfxTextControlFrame2::SetInitialChildList(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  /*nsIFrame *list = aChildList;
  nsFrameState  frameState;
  while (list)
  {
    list->GetFrameState(&frameState);
    frameState |= NS_FRAME_INDEPENDENT_SELECTION;
    list->SetFrameState(frameState);
    list->GetNextSibling(&list);
  }
  */
  nsresult rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  if (mEditor)
    mEditor->PostCreate();
  //look for scroll view below this frame go along first child list
  nsIFrame *first;
  FirstChild(aPresContext,nsnull, &first);

//we must turn off scrollbars for singleline text controls
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) 
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
    rv = erP->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMKeyListener *,mTextListener), NS_GET_IID(nsIDOMKeyListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register key listener");
    rv = erP->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMFocusListener *,mTextListener), NS_GET_IID(nsIDOMFocusListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register focus listener");
    nsCOMPtr<nsIPresShell> shell;
    nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(rv))
      return rv;
    if (!shell)
      return NS_ERROR_FAILURE;
  }

  while(first)
  {
    nsIScrollableView *scrollView;
    nsIView *view;
    first->GetView(aPresContext,&view);
    if (view)
    {
      view->QueryInterface(NS_GET_IID(nsIScrollableView),(void **)&scrollView);
      if (scrollView)
      {
        mScrollableView = scrollView; // Note: views are not addref'd
        mTextSelImpl->SetScrollableView(scrollView);
        break;
      }
    }
    first->FirstChild(aPresContext,nsnull, &first);
  }

  return rv;
}


PRInt32 
nsGfxTextControlFrame2::GetWidthInCharacters() const
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

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsGfxTextControlFrame2::SaveState(nsIPresContext* aPresContext, nsIPresState** aState)
{
  return nsFormControlHelper::SaveContentState(this, aPresContext, aState);
}

NS_IMETHODIMP
nsGfxTextControlFrame2::RestoreState(nsIPresContext* aPresContext, nsIPresState* aState)
{
  return nsFormControlHelper::RestoreContentState(this, aPresContext, aState);
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetScrollableView(nsIScrollableView** aView)
{
  nsresult rv = NS_OK;
  *aView = mScrollableView;
  if (mScrollableView && !IsScrollable()) {
    nsIView *view = 0;
    nsIScrollableView* scrollableView = 0;
    rv = mScrollableView->QueryInterface(NS_GET_IID(nsIView), (void**)&view);
    while (view) {
      rv = view->QueryInterface(NS_GET_IID(nsIScrollableView), (void **)&scrollableView);
      if (NS_SUCCEEDED(rv) && scrollableView)
        *aView = scrollableView;
      view->GetParent(view);
    }
  }
  return rv;
}

PRBool
nsGfxTextControlFrame2::IsScrollable() const
{
  return !IsSingleLineTextControl();
}

NS_IMETHODIMP
nsGfxTextControlFrame2::OnContentReset()
{
  return NS_OK;
}

void
nsGfxTextControlFrame2::SetValueChanged(PRBool aValueChanged)
{
  nsCOMPtr<nsITextControlElement> elem = do_QueryInterface(mContent);
  if (elem) {
    elem->SetValueChanged(aValueChanged);
  }
}
