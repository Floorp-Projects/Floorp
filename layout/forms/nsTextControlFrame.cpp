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
#include "nsFrameSelection.h"
#include "nsIPlaintextEditor.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsIDocumentEncoder.h"
#include "nsICaret.h"
#include "nsISelectionListener.h"
#include "nsISelectionPrivate.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIControllerContext.h"
#include "nsGenericHTMLElement.h"
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

#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#ifdef USE_QI_IN_SUPPRESS_EVENT_HANDLERS
#include "nsIPrintContext.h"
#include "nsIPrintPreviewContext.h"
#endif // USE_QI_IN_SUPPRESS_EVENT_HANDLERS
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsISupportsArray.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIPresShell.h"
#include "nsIComponentManager.h"

#include "nsBoxLayoutState.h"
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
#include "nsIDOMNSEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIEventStateManager.h"

#include "nsIDOMFocusListener.h" //onchange events
#include "nsIDOMCharacterData.h" //for selection setting helper func
#include "nsIDOMNodeList.h" //for selection setting helper func
#include "nsIDOMRange.h" //for selection setting helper func
#include "nsPIDOMWindow.h" //needed for notify selection changed to update the menus ect.
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
#include "nsIDOMKeyListener.h"
#include "nsIDOMEventGroup.h"
#include "nsIDOM3EventTarget.h"
#include "nsINativeKeyBindings.h"
#include "nsIJSContextStack.h"

#define DEFAULT_COLUMN_WIDTH 20

#include "nsContentCID.h"
static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);

static NS_DEFINE_CID(kTextEditorCID, NS_TEXTEDITOR_CID);
static NS_DEFINE_CID(kFrameSelectionCID, NS_FRAMESELECTION_CID);

static const PRInt32 DEFAULT_COLS = 20;
static const PRInt32 DEFAULT_ROWS = 1;
static const PRInt32 DEFAULT_ROWS_TEXTAREA = 2;
static const PRInt32 DEFAULT_UNDO_CAP = 1000;

static nsINativeKeyBindings *sNativeInputBindings = nsnull;
static nsINativeKeyBindings *sNativeTextAreaBindings = nsnull;

static void
PlatformToDOMLineBreaks(nsString &aString)
{
  // Windows linebreaks: Map CRLF to LF:
  aString.ReplaceSubstring(NS_LITERAL_STRING("\r\n").get(),
                           NS_LITERAL_STRING("\n").get());

  // Mac linebreaks: Map any remaining CR to LF:
  aString.ReplaceSubstring(NS_LITERAL_STRING("\r").get(),
                           NS_LITERAL_STRING("\n").get());
}

// wrap can be one of these three values.  
typedef enum {
  eHTMLTextWrap_Off     = 1,    // "off"
  eHTMLTextWrap_Hard    = 2,    // "hard"
  eHTMLTextWrap_Soft    = 3     // the default
} nsHTMLTextWrap;

static PRBool 
GetWrapPropertyEnum(nsIContent* aContent, nsHTMLTextWrap& aWrapProp)
{
  // soft is the default; "physical" defaults to soft as well because all other
  // browsers treat it that way and there is no real reason to maintain physical
  // and virtual as separate entities if no one else does.  Only hard and off
  // do anything different.
  aWrapProp = eHTMLTextWrap_Soft; // the default
  
  nsAutoString wrap;
  if (aContent->IsNodeOfType(nsINode::eHTML)) {
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

class nsTextInputListener : public nsISelectionListener,
                            public nsIDOMFocusListener,
                            public nsIDOMKeyListener,
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

  // nsIDOMKeyListener
  NS_IMETHOD KeyDown(nsIDOMEvent *aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent *aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent *aKeyEvent);

  NS_DECL_NSIEDITOROBSERVER

protected:

  nsresult  UpdateTextInputCommands(const nsAString& commandsToUpdate);

  NS_HIDDEN_(nsINativeKeyBindings*) GetKeyBindings();

protected:

  nsTextControlFrame* mFrame;  // weak reference
  
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
};


/*
 * nsTextEditorListener implementation
 */

nsTextInputListener::nsTextInputListener()
: mFrame(nsnull)
, mSelectionWasCollapsed(PR_TRUE)
, mHadUndoItems(PR_FALSE)
, mHadRedoItems(PR_FALSE)
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
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFocusListener)
NS_INTERFACE_MAP_END

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
    nsIContent* content = mFrame->GetContent();
    if (content) 
    {
      nsCOMPtr<nsIDocument> doc = content->GetDocument();
      if (doc) 
      {
        nsIPresShell *presShell = doc->GetShellAt(0);
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

  return NS_OK;
}

// END nsIFocusListener

// BEGIN nsIDOMKeyListener

static void
DoCommandCallback(const char *aCommand, void *aData)
{
  nsTextControlFrame *frame = NS_STATIC_CAST(nsTextControlFrame*, aData);
  nsIContent *content = frame->GetContent();

  nsCOMPtr<nsIControllers> controllers;
  nsCOMPtr<nsIDOMNSHTMLInputElement> input = do_QueryInterface(content);
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

static PRBool
DOMEventToNativeKeyEvent(nsIDOMEvent      *aDOMEvent,
                         nsNativeKeyEvent *aNativeEvent,
                         PRBool            aGetCharCode)
{
  nsCOMPtr<nsIDOMNSUIEvent> uievent = do_QueryInterface(aDOMEvent);
  PRBool defaultPrevented;
  uievent->GetPreventDefault(&defaultPrevented);
  if (defaultPrevented)
    return PR_FALSE;

  nsCOMPtr<nsIDOMNSEvent> nsevent = do_QueryInterface(aDOMEvent);
  PRBool trusted = PR_FALSE;
  nsevent->GetIsTrusted(&trusted);
  if (!trusted)
    return PR_FALSE;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aDOMEvent);

  if (aGetCharCode) {
      keyEvent->GetCharCode(&aNativeEvent->charCode);
  } else {
      aNativeEvent->charCode = 0;
  }
  keyEvent->GetKeyCode(&aNativeEvent->keyCode);
  keyEvent->GetAltKey(&aNativeEvent->altKey);
  keyEvent->GetCtrlKey(&aNativeEvent->ctrlKey);
  keyEvent->GetShiftKey(&aNativeEvent->shiftKey);
  keyEvent->GetMetaKey(&aNativeEvent->metaKey);

  return PR_TRUE;
}

NS_IMETHODIMP
nsTextInputListener::KeyDown(nsIDOMEvent *aKeyEvent)
{
  nsNativeKeyEvent nativeEvent;
  nsINativeKeyBindings *bindings = GetKeyBindings();
  if (bindings &&
      DOMEventToNativeKeyEvent(aKeyEvent, &nativeEvent, PR_FALSE)) {
    if (bindings->KeyDown(nativeEvent, DoCommandCallback, mFrame)) {
      aKeyEvent->PreventDefault();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTextInputListener::KeyPress(nsIDOMEvent *aKeyEvent)
{
  nsNativeKeyEvent nativeEvent;
  nsINativeKeyBindings *bindings = GetKeyBindings();
  if (bindings &&
      DOMEventToNativeKeyEvent(aKeyEvent, &nativeEvent, PR_TRUE)) {
    if (bindings->KeyPress(nativeEvent, DoCommandCallback, mFrame)) {
      aKeyEvent->PreventDefault();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTextInputListener::KeyUp(nsIDOMEvent *aKeyEvent)
{
  nsNativeKeyEvent nativeEvent;
  nsINativeKeyBindings *bindings = GetKeyBindings();
  if (bindings &&
      DOMEventToNativeKeyEvent(aKeyEvent, &nativeEvent, PR_FALSE)) {
    if (bindings->KeyUp(nativeEvent, DoCommandCallback, mFrame)) {
      aKeyEvent->PreventDefault();
    }
  }

  return NS_OK;
}
// END nsIDOMKeyListener

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
  NS_ENSURE_STATE(mFrame);

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
  if (mFrame->IsTextArea()) {
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

class nsTextInputSelectionImpl : public nsSupportsWeakReference
                               , public nsISelectionController
{
public:
  NS_DECL_ISUPPORTS

  nsTextInputSelectionImpl(nsFrameSelection *aSel, nsIPresShell *aShell, nsIContent *aLimiter);
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
  NS_IMETHOD SetCaretReadOnly(PRBool aReadOnly);
  NS_IMETHOD GetCaretEnabled(PRBool *_retval);
  NS_IMETHOD SetCaretVisibilityDuringSelection(PRBool aVisibility);
  NS_IMETHOD CharacterMove(PRBool aForward, PRBool aExtend);
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
  nsCOMPtr<nsIContent>        mLimiter;
  nsWeakPtr mPresShellWeak;
};

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS2(nsTextInputSelectionImpl, nsISelectionController, nsISupportsWeakReference)


// BEGIN nsTextInputSelectionImpl

nsTextInputSelectionImpl::nsTextInputSelectionImpl(nsFrameSelection *aSel, nsIPresShell *aShell, nsIContent *aLimiter)
{
  if (aSel && aShell)
  {
    mFrameSelection = aSel;//we are the owner now!
    mLimiter = aLimiter;
    mFrameSelection->Init(aShell, mLimiter);
    mPresShellWeak = do_GetWeakReference(aShell);
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
nsTextInputSelectionImpl::ScrollSelectionIntoView(PRInt16 aType, PRInt16 aRegion, PRBool aIsSynchronous)
{
  if (mFrameSelection) {
    nsresult rv = mFrameSelection->ScrollSelectionIntoView(aType, aRegion, aIsSynchronous);

    nsIScrollableView* scrollableView = mFrameSelection->GetScrollableView();
    if (!scrollableView) {
      return rv;
    }
    nsIView* view = nsnull;
    scrollableView->GetScrolledView(view);
    if (!view) {
      return rv;
    }
    const nsRect portRect = scrollableView->View()->GetBounds();
    const nsRect viewRect = view->GetBounds();
    if (viewRect.XMost() < portRect.width) {
      return scrollableView->ScrollTo(PR_MAX(viewRect.width - portRect.width, 0), -viewRect.y, 0);
    }

    return rv;
  }
  return NS_ERROR_NULL_POINTER;
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
    nsCOMPtr<nsICaret> caret;
    if (NS_SUCCEEDED(shell->GetCaret(getter_AddRefs(caret))))
    {
      nsISelection* domSel = mFrameSelection->
        GetSelection(nsISelectionController::SELECTION_NORMAL);
      if (domSel)
        return caret->SetCaretReadOnly(aReadOnly);
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
    if (NS_SUCCEEDED(shell->GetCaret(getter_AddRefs(caret))))
    {
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
    nsCOMPtr<nsICaret> caret;
    if (NS_SUCCEEDED(shell->GetCaret(getter_AddRefs(caret))))
    {
      nsISelection* domSel = mFrameSelection->
        GetSelection(nsISelectionController::SELECTION_NORMAL);
      if (domSel)
        return caret->SetVisibilityDuringSelection(aVisibility);
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

  if (mPresShellWeak)
  {
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
    if (!presShell)
      return NS_ERROR_NULL_POINTER;

    //get the scroll view
    nsIScrollableView *scrollableView = mFrameSelection->GetScrollableView();
    if (scrollableView)
      mFrameSelection->CommonPageMove(aForward, aExtend, scrollableView);
  }
  return ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::CompleteScroll(PRBool aForward)
{
  nsIScrollableView *scrollableView = mFrameSelection->GetScrollableView();

  if (!scrollableView)
    return NS_ERROR_NOT_INITIALIZED;

  return scrollableView->ScrollByWhole(!aForward); //TRUE = top, aForward TRUE=bottom
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
  nsIScrollableView *scrollableView = mFrameSelection->GetScrollableView();
  if (!scrollableView)
    return NS_ERROR_NOT_INITIALIZED;

  return scrollableView->ScrollByPages(0, aForward ? 1 : -1);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollLine(PRBool aForward)
{
  nsIScrollableView *scrollableView = mFrameSelection->GetScrollableView();
  if (!scrollableView)
    return NS_ERROR_NOT_INITIALIZED;

  // will we have bug #7354 because we aren't forcing an update here?
  return scrollableView->ScrollByLines(0, aForward ? 1 : -1);
}

NS_IMETHODIMP
nsTextInputSelectionImpl::ScrollHorizontal(PRBool aLeft)
{
  nsIScrollableView *scrollableView = mFrameSelection->GetScrollableView();
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

nsIFrame*
NS_NewTextControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTextControlFrame(aPresShell, aContext);
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

nsTextControlFrame::nsTextControlFrame(nsIPresShell* aShell, nsStyleContext* aContext)
  : nsStackFrame(aShell, aContext)
  , mUseEditor(PR_FALSE)
  , mIsProcessing(PR_FALSE)
  , mNotifyOnInput(PR_TRUE)
  , mDidPreDestroy(PR_FALSE)
  , mFireChangeEventState(PR_FALSE)
  , mTextListener(nsnull)
  , mScrollableView(nsnull)
#ifdef DEBUG
  , mCreateFrameForCalled(PR_FALSE)
#endif
{
}

nsTextControlFrame::~nsTextControlFrame()
{
  NS_IF_RELEASE(mTextListener);
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
nsTextControlFrame::PreDestroy()
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

  if (!SuppressEventHandlers(GetPresContext()))
  {
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsIDOMNSHTMLInputElement> inputElement = do_QueryInterface(mContent);
    if (inputElement)
      inputElement->GetControllers(getter_AddRefs(controllers));
    else
    {
      nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textAreaElement = do_QueryInterface(mContent);
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

  mEditor = nsnull;
  mSelCon = nsnull;
  mFrameSel = nsnull;

//unregister self from content
  mTextListener->SetFrame(nsnull);
  nsFormControlFrame::RegUnRegAccessKey(NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  if (mTextListener)
  {
    nsCOMPtr<nsIDOMEventReceiver> erP = do_QueryInterface(mContent);
    if (erP)
    {
      erP->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMFocusListener  *,mTextListener), NS_GET_IID(nsIDOMFocusListener));
    }

    nsCOMPtr<nsIDOMEventGroup> systemGroup;
    erP->GetSystemEventGroup(getter_AddRefs(systemGroup));
    nsCOMPtr<nsIDOM3EventTarget> dom3Targ = do_QueryInterface(mContent);
    if (dom3Targ) {
      // cast because of ambiguous base
      nsIDOMEventListener *listener = NS_STATIC_CAST(nsIDOMKeyListener*,
                                                     mTextListener);

      dom3Targ->RemoveGroupedEventListener(NS_LITERAL_STRING("keydown"),
                                           listener, PR_FALSE, systemGroup);
      dom3Targ->RemoveGroupedEventListener(NS_LITERAL_STRING("keypress"),
                                           listener, PR_FALSE, systemGroup);
      dom3Targ->RemoveGroupedEventListener(NS_LITERAL_STRING("keyup"),
                                           listener, PR_FALSE, systemGroup);
    }
  }

  mDidPreDestroy = PR_TRUE; 
}

void
nsTextControlFrame::Destroy()
{
  if (!mDidPreDestroy) {
    PreDestroy();
  }
  nsBoxFrame::Destroy();
}

void 
nsTextControlFrame::RemovedAsPrimaryFrame()
{
  if (!mDidPreDestroy) {
    PreDestroy();
  }
  else NS_ASSERTION(PR_FALSE, "RemovedAsPrimaryFrame called after PreDestroy");
}

nsIAtom*
nsTextControlFrame::GetType() const 
{ 
  return nsGkAtoms::textInputFrame;
} 

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsTextControlFrame::IsSingleLineTextControl() const
{
  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(mContent);
  if (formControl) {
    PRInt32 type = formControl->GetType();
    return (type == NS_FORM_INPUT_TEXT) || (type == NS_FORM_INPUT_PASSWORD);
  }
  return PR_FALSE;
}

PRBool nsTextControlFrame::IsTextArea() const
{
  return mContent && mContent->Tag() == nsGkAtoms::textarea;
}

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsTextControlFrame::IsPlainTextControl() const
{
  // need to check HTML attribute of mContent and/or CSS.
  return PR_TRUE;
}

PRBool nsTextControlFrame::IsPasswordTextControl() const
{
  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(mContent);
  return formControl && formControl->GetType() == NS_FORM_INPUT_PASSWORD;
}


PRInt32
nsTextControlFrame::GetCols()
{
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);
  NS_ASSERTION(content, "Content is not HTML content!");

  if (IsTextArea()) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::cols);
    if (attr) {
      PRInt32 cols = attr->Type() == nsAttrValue::eInteger ?
                     attr->GetIntegerValue() : 0;
      // XXX why a default of 1 char, why hide it
      return (cols <= 0) ? 1 : cols;
    }
  } else {
    // Else we know (assume) it is an input with size attr
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::size);
    if (attr && attr->Type() == nsAttrValue::eInteger) {
      PRInt32 cols = attr->GetIntegerValue();
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
    nsGenericHTMLElement *content =
      nsGenericHTMLElement::FromContent(mContent);
    NS_ASSERTION(content, "Content is not HTML content!");

    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::rows);
    if (attr && attr->Type() == nsAttrValue::eInteger) {
      PRInt32 rows = attr->GetIntegerValue();
      return (rows <= 0) ? DEFAULT_ROWS_TEXTAREA : rows;
    }
    return DEFAULT_ROWS_TEXTAREA;
  }

  return DEFAULT_ROWS;
}


nsresult
nsTextControlFrame::CalcIntrinsicSize(nsIRenderingContext* aRenderingContext,
                                      nsSize&              aIntrinsicSize)
{
  // Get leading and the Average/MaxAdvance char width 
  nscoord lineHeight  = 0;
  nscoord charWidth   = 0;
  nscoord charMaxAdvance  = 0;

  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult rv =
    nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet));
  NS_ENSURE_SUCCESS(rv, rv);
  aRenderingContext->SetFont(fontMet);

  nsPresContext* presContext = GetPresContext();
  lineHeight = nsHTMLReflowState::CalcLineHeight(presContext,
                                                 aRenderingContext,
                                                 this);
  fontMet->GetAveCharWidth(charWidth);
  fontMet->GetMaxAdvance(charMaxAdvance);

  // Set the width equal to the width in characters
  PRInt32 cols = GetCols();
  aIntrinsicSize.width = cols * charWidth;

  // To better match IE, take the maximum character width(in twips) and remove
  // 4 pixels add this on as additional padding(internalPadding). But only do
  // this if charMaxAdvance != charWidth; if they are equal, this is almost
  // certainly a fixed-width font.
  if (charWidth != charMaxAdvance) {
    float p2t;
    p2t = presContext->PixelsToTwips();
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
    aIntrinsicSize.width += internalPadding;
  } else {
    // This is to account for the anonymous <br> having a 1 twip width
    // in Full Standards mode, see BRFrame::Reflow and bug 228752.
    if (presContext->CompatibilityMode() == eCompatibility_FullStandards) {
      aIntrinsicSize.width += 1;
    }
  }

  // Increment width with cols * letter-spacing.
  {
    const nsStyleCoord& lsCoord = GetStyleText()->mLetterSpacing;
    if (eStyleUnit_Coord == lsCoord.GetUnit()) {
      nscoord letterSpacing = lsCoord.GetCoordValue();
      if (letterSpacing != 0) {
        aIntrinsicSize.width += cols * letterSpacing;
      }
    }
  }

  // Set the height equal to total number of rows (times the height of each
  // line, of course)
  aIntrinsicSize.height = lineHeight * GetRows();

  // Add in the size of the scrollbars for textarea
  if (IsTextArea()) {
    nsIFrame* first = GetFirstChild(nsnull);

    nsIScrollableFrame *scrollableFrame;
    CallQueryInterface(first, &scrollableFrame);
    NS_ASSERTION(scrollableFrame, "Child must be scrollable");

    nsBoxLayoutState bls(GetPresContext(), aRenderingContext);
    nsMargin scrollbarSizes = scrollableFrame->GetDesiredScrollbarSizes(&bls);

    aIntrinsicSize.width  += scrollbarSizes.LeftRight();
    
    aIntrinsicSize.height += scrollbarSizes.TopBottom();;
  }

  return NS_OK;
}

void nsTextControlFrame::PostCreateFrames() {
  InitEditor();
}

NS_IMETHODIMP
nsTextControlFrame::CreateFrameFor(nsPresContext*   aPresContext,
                                   nsIContent*      aContent,
                                   nsIFrame**       aFrame)
{
#ifdef DEBUG
  NS_ASSERTION(!mCreateFrameForCalled, "CreateFrameFor called more than once!");
  mCreateFrameForCalled = PR_TRUE;
#endif
  
  // Note, we must set aFrame to nsnull.
  *aFrame = nsnull;

  nsIPresShell *shell = aPresContext->GetPresShell();
  if (!shell)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMDocument> domdoc = do_QueryInterface(shell->GetDocument());
  if (!domdoc)
    return NS_ERROR_FAILURE;

  // Don't create any frames here, but just setup the editor.
  // This way DOM Ranges (which editor uses) work properly since the anonymous
  // content is bound to tree after CreateAnonymousContent but before this
  // method.
  nsresult rv = NS_OK;
  mEditor = do_CreateInstance(kTextEditorCID, &rv);
  if (NS_FAILED(rv))
    return rv;
  if (!mEditor) 
    return NS_ERROR_OUT_OF_MEMORY;

  // Create selection

  mFrameSel = do_CreateInstance(kFrameSelectionCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  // Create a SelectionController

  mSelCon = NS_STATIC_CAST(nsISelectionController*,
              new nsTextInputSelectionImpl(mFrameSel, shell, aContent));
  if (!mSelCon)
    return NS_ERROR_OUT_OF_MEMORY;
  mTextListener = new nsTextInputListener();
  if (!mTextListener)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mTextListener);

  mTextListener->SetFrame(this);
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

  rv = mEditor->Init(domdoc, shell, aContent, mSelCon, editorFlags);

  if (NS_FAILED(rv))
    return rv;

  // Initialize the controller for the editor

  if (!SuppressEventHandlers(aPresContext)) {
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsIDOMNSHTMLInputElement> inputElement =
      do_QueryInterface(mContent);
    if (inputElement) {
      rv = inputElement->GetControllers(getter_AddRefs(controllers));
    } else {
      nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textAreaElement =
        do_QueryInterface(mContent);

      if (!textAreaElement)
        return NS_ERROR_FAILURE;

      rv = textAreaElement->GetControllers(getter_AddRefs(controllers));
    }

    if (NS_FAILED(rv))
      return rv;

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
      nsHTMLTextWrap wrapProp;
      ::GetWrapPropertyEnum(mContent, wrapProp);
      if (wrapProp == eHTMLTextWrap_Off) {
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
    if (GetMaxLength(&maxLength)) { 
      textEditor->SetMaxTextLength(maxLength);
    }
  }
    
  // Get the caret and make it a selection listener.

  nsCOMPtr<nsISelection> domSelection;
  if (NS_SUCCEEDED(mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                         getter_AddRefs(domSelection))) &&
      domSelection) {
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(domSelection));
    nsCOMPtr<nsICaret> caret;
    nsCOMPtr<nsISelectionListener> listener;
    if (NS_SUCCEEDED(shell->GetCaret(getter_AddRefs(caret))) && caret) {
      listener = do_QueryInterface(caret);
      if (listener) {
        selPriv->AddSelectionListener(listener);
      }
    }

    selPriv->AddSelectionListener(NS_STATIC_CAST(nsISelectionListener*,
                                                 mTextListener));
  }
  
  if (mContent) {
    rv = mEditor->GetFlags(&editorFlags);

    if (NS_FAILED(rv))
      return rv;

    // Check if the readonly attribute is set.

    if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::readonly))
      editorFlags |= nsIPlaintextEditor::eEditorReadonlyMask;

    // Check if the disabled attribute is set.

    if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) 
      editorFlags |= nsIPlaintextEditor::eEditorDisabledMask;

    // Disable the selection if necessary.

    if (editorFlags & nsIPlaintextEditor::eEditorDisabledMask)
      mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);

    mEditor->SetFlags(editorFlags);
  }
  return NS_OK;
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

    // Now call SetValue() which will make the necessary editor calls to set
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

  nsCOMPtr<nsITransactionManager> transMgr;
  mEditor->GetTransactionManager(getter_AddRefs(transMgr));
  NS_ENSURE_TRUE(transMgr, NS_ERROR_FAILURE);

  transMgr->SetMaxTransactionCount(DEFAULT_UNDO_CAP);

  if (IsPasswordTextControl()) {
    // Disable undo for password textfields.  Note that we want to do this at
    // the very end of InitEditor, so the calls to EnableUndo when setting the
    // default value don't screw us up.
    // Since changing the control type does a reframe, we don't have to worry
    // about dynamic type changes here.
    mEditor->EnableUndo(PR_FALSE);
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

  nsIDocument *doc = shell->GetDocument();
  if (!doc)
    return NS_ERROR_FAILURE;

  // Now create a DIV and add it to the anonymous content child list.
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::div, nsnull,
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

  // Set the necessary style attributes on the text control.

  rv = divContent->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                           NS_LITERAL_STRING("anonymous-div"), PR_FALSE);

  if (!IsSingleLineTextControl()) {
    // We can't just inherit the overflow because setting visible overflow will
    // crash when the number of lines exceeds the height of the textarea and
    // setting -moz-hidden-unscrollable overflow (NS_STYLE_OVERFLOW_CLIP)
    // doesn't paint the caret for some reason.
    const nsStyleDisplay* disp = GetStyleDisplay();
    if (disp->mOverflowX != NS_STYLE_OVERFLOW_VISIBLE &&
        disp->mOverflowX != NS_STYLE_OVERFLOW_CLIP) {
      rv = divContent->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
                               NS_LITERAL_STRING("overflow: inherit;"),
                               PR_FALSE);
    }
  }

  if (NS_FAILED(rv))
    return rv;

  // rv = divContent->SetAttr(kNameSpaceID_None,nsGkAtoms::debug, NS_LITERAL_STRING("true"), PR_FALSE);
  return aChildList.AppendElement(divContent);
}

nscoord
nsTextControlFrame::GetMinWidth(nsIRenderingContext* aRenderingContext)
{
  // Note: to fix bug 40596 while still working correctly in general, we want
  // to return our preferred width as our min width if our style width is auto.
  // Otherwise, we're ok with shrinking as small as needed.
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  if (GetStylePosition()->mWidth.GetUnit() == eStyleUnit_Auto) {
    result = GetPrefWidth(aRenderingContext);
  } else {
    result = 0;
  }

  return result;
}

// We inherit our GetPrefWidth from nsBoxFrame

NS_IMETHODIMP
nsTextControlFrame::Reflow(nsPresContext*   aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTextControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // make sure the the form registers itself on the initial/first reflow
  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, PR_TRUE);
  }

  return nsStackFrame::Reflow(aPresContext, aDesiredSize, aReflowState,
                              aStatus);
}

nsSize
nsTextControlFrame::GetPrefSize(nsBoxLayoutState& aState)
{
  if (!DoesNeedRecalc(mPrefSize))
     return mPrefSize;

#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize pref(0,0);

  nsresult rv = CalcIntrinsicSize(aState.GetRenderingContext(), pref);
  NS_ENSURE_SUCCESS(rv, pref);
  AddBorderAndPadding(pref);
  AddInset(pref);

  mPrefSize = pref;

#ifdef DEBUG_rods
  {
    nsMargin borderPadding(0,0,0,0);
    GetBorderAndPadding(borderPadding);
    nsSize size(169, 24);
    nsSize actual(pref.width/15, 
                  pref.height/15);
    printf("nsGfxText(field) %d,%d  %d,%d  %d,%d\n", 
           size.width, size.height, actual.width, actual.height, actual.width-size.width, actual.height-size.height);  // text field
  }
#endif

  return pref;
}

nsSize
nsTextControlFrame::GetMinSize(nsBoxLayoutState& aState)
{
  // XXXbz why?  Why not the nsBoxFrame sizes?
  return nsBox::GetMinSize(aState);
}

nsSize
nsTextControlFrame::GetMaxSize(nsBoxLayoutState& aState)
{
  // XXXbz why?  Why not the nsBoxFrame sizes?
  return nsBox::GetMaxSize(aState);
}

NS_IMETHODIMP
nsTextControlFrame::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  // First calculate the ascent of the text inside
  nsresult rv = nsStackFrame::GetAscent(aState, aAscent);
  NS_ENSURE_SUCCESS(rv, rv);
    
  // Now adjust the ascent for our borders and padding
  nsMargin borderPadding;
  GetBorderAndPadding(borderPadding);
  aAscent += borderPadding.top;
  
  return NS_OK;
}

NS_IMETHODIMP
nsTextControlFrame::IsCollapsed(nsBoxLayoutState& aBoxLayoutState,
                                PRBool& aCollapsed)
{
  // We're never collapsed in the box sense.
  aCollapsed = PR_FALSE;
  return NS_OK;
}

PRBool
nsTextControlFrame::IsLeaf() const
{
  return PR_TRUE;
}

PRBool
nsTextControlFrame::IsFrameOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eReplaced | eReplacedContainsBlock));
}

static PRBool
IsFocusedContent(nsPresContext* aPresContext, nsIContent* aContent)
{
  nsCOMPtr<nsIContent> focusedContent;
  aPresContext->EventStateManager()->
    GetFocusedContent(getter_AddRefs(focusedContent));
  return focusedContent == aContent;
}

//IMPLEMENTING NS_IFORMCONTROLFRAME
void nsTextControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
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

nsresult nsTextControlFrame::SetFormProperty(nsIAtom* aName, const nsAString& aValue)
{
  if (!mIsProcessing)//some kind of lock.
  {
    mIsProcessing = PR_TRUE;
    
    if (nsGkAtoms::value == aName) 
    {
      if (mEditor && mUseEditor) {
        // If the editor exists, the control needs to be informed that the value
        // has changed.
        SetValueChanged(PR_TRUE);
      }
      SetValue(aValue);   // set new text value
    }
    else if (nsGkAtoms::select == aName)
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

nsresult
nsTextControlFrame::GetFormProperty(nsIAtom* aName, nsAString& aValue) const
{
  // Return the value of the property from the widget it is not null.
  // If widget is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsGkAtoms::value == aName) {
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
  mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));  
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
      if (child->Tag() == nsGkAtoms::br)
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
               "Invalid node offset!");

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
  nsresult rv = mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));  
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

/////END INTERFACE IMPLEMENTATIONS

////NSIFRAME
NS_IMETHODIMP
nsTextControlFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType)
{
  if (!mEditor || !mSelCon) 
    return NS_ERROR_NOT_INITIALIZED;
  nsresult rv = NS_OK;

  if (nsGkAtoms::maxlength == aAttribute) 
  {
    PRInt32 maxLength;
    PRBool maxDefined = GetMaxLength(&maxLength);
    
    nsCOMPtr<nsIPlaintextEditor> textEditor = do_QueryInterface(mEditor);
    if (textEditor)
    {
      if (maxDefined) 
      {  // set the maxLength attribute
          textEditor->SetMaxTextLength(maxLength);
        // if maxLength>docLength, we need to truncate the doc content
      }
      else { // unset the maxLength attribute
          textEditor->SetMaxTextLength(-1);
      }
    }
    rv = NS_OK; // don't propagate the error
  } 
  else if (nsGkAtoms::readonly == aAttribute) 
  {
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (AttributeExists(nsGkAtoms::readonly))
    { // set readonly
      flags |= nsIPlaintextEditor::eEditorReadonlyMask;
      if (IsFocusedContent(GetPresContext(), mContent))
        mSelCon->SetCaretEnabled(PR_FALSE);
    }
    else 
    { // unset readonly
      flags &= ~(nsIPlaintextEditor::eEditorReadonlyMask);
      if (!(flags & nsIPlaintextEditor::eEditorDisabledMask) &&
          IsFocusedContent(GetPresContext(), mContent))
        mSelCon->SetCaretEnabled(PR_TRUE);
    }    
    mEditor->SetFlags(flags);
  }
  else if (mEditor && nsGkAtoms::disabled == aAttribute) 
  {
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (AttributeExists(nsGkAtoms::disabled))
    { // set disabled
      flags |= nsIPlaintextEditor::eEditorDisabledMask;
      mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
      if (IsFocusedContent(GetPresContext(), mContent))
        mSelCon->SetCaretEnabled(PR_FALSE);
    }
    else 
    { // unset disabled
      flags &= ~(nsIPlaintextEditor::eEditorDisabledMask);
      mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);
    }    
    mEditor->SetFlags(flags);
  }
  // Allow the base class to handle common attributes supported
  // by all form elements... 
  else {
    rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
  }

  return rv;
}


NS_IMETHODIMP
nsTextControlFrame::GetText(nsString* aText)
{
  nsresult rv = NS_OK;
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


PRBool
nsTextControlFrame::GetMaxLength(PRInt32* aSize)
{
  *aSize = -1;

  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);
  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::maxlength);
    if (attr && attr->Type() == nsAttrValue::eInteger) {
      *aSize = attr->GetIntegerValue();

      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

// this is where we propagate a content changed event
void
nsTextControlFrame::FireOnInput()
{
  if (!mNotifyOnInput)
    return; // if notification is turned off, do nothing
  
  // Dispatch the "input" event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsUIEvent event(PR_TRUE, NS_FORM_INPUT, 0);

  // Have the content handle the event, propagating it according to normal
  // DOM rules.
  GetPresContext()->PresShell()->HandleEventWithTarget(&event, nsnull, mContent,
                                                       &status); 
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
  if (!mFocusedValue.Equals(value))
  {
    mFocusedValue = value;
    // Dispatch the change event
    nsEventStatus status = nsEventStatus_eIgnore;
    nsInputEvent event(PR_TRUE, NS_FORM_CHANGE, nsnull);

    GetPresContext()->PresShell()->HandleEventWithTarget(&event, nsnull,
                                                         mContent, &status);
  }
  return NS_OK;
}

//======
//privates

NS_IMETHODIMP
nsTextControlFrame::GetValue(nsAString& aValue, PRBool aIgnoreWrap) const
{
  aValue.Truncate();  // initialize out param
  nsresult rv = NS_OK;
  
  if (mEditor && mUseEditor) 
  {
    PRUint32 flags = (nsIDocumentEncoder::OutputLFLineBreak |
                      nsIDocumentEncoder::OutputPreformatted |
                      nsIDocumentEncoder::OutputPersistNBSP);

    if (PR_TRUE==IsPlainTextControl())
    {
      flags |= nsIDocumentEncoder::OutputBodyOnly;
    }

    if (!aIgnoreWrap) {
      nsHTMLTextWrap wrapProp;
      if (::GetWrapPropertyEnum(mContent, wrapProp) &&
          wrapProp == eHTMLTextWrap_Hard) {
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
    nsCOMPtr<nsIJSContextStack> stack =
      do_GetService("@mozilla.org/js/xpc/ContextStack;1");
    PRBool pushed = stack && NS_SUCCEEDED(stack->Push(nsnull));
      
    rv = mEditor->OutputToString(NS_LITERAL_STRING("text/plain"), flags,
                                 aValue);

    if (pushed) {
      JSContext* cx;
      stack->Pop(&cx);
      NS_ASSERTION(!cx, "Unexpected JSContext popped!");
    }
  }
  else
  {
    // Otherwise get the value from content.
    nsCOMPtr<nsIDOMHTMLInputElement> inputControl = do_QueryInterface(mContent);
    if (inputControl)
    {
      rv = inputControl->GetValue(aValue);
    }
    else
    {
      nsCOMPtr<nsIDOMHTMLTextAreaElement> textareaControl
          = do_QueryInterface(mContent);
      if (textareaControl)
      {
        rv = textareaControl->GetValue(aValue);
      }
    }
  }

  return rv;
}


// END IMPLEMENTING NS_IFORMCONTROLFRAME

void
nsTextControlFrame::SetValue(const nsAString& aValue)
{
  // XXX this method should actually propagate errors!  It'd make debugging it
  // so much easier...
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
      // \r is an illegal character in the dom, but people use them,
      // so convert windows and mac platform linebreaks to \n:
      // Unfortunately aValue is declared const, so we have to copy
      // in order to do this substitution.
      currentValue.Assign(aValue);
      ::PlatformToDOMLineBreaks(currentValue);

      nsCOMPtr<nsIDOMDocument>domDoc;
      nsresult rv = mEditor->GetDocument(getter_AddRefs(domDoc));
      if (NS_FAILED(rv)) return;
      if (!domDoc) return;

      // Time to mess with our security context... See comments in GetValue()
      // for why this is needed.  Note that we have to do this up here, because
      // otherwise SelectAll() will fail.
      nsCOMPtr<nsIJSContextStack> stack =
        do_GetService("@mozilla.org/js/xpc/ContextStack;1");
      PRBool pushed = stack && NS_SUCCEEDED(stack->Push(nsnull));

      nsCOMPtr<nsISelection> domSel;
      nsCOMPtr<nsISelectionPrivate> selPriv;
      mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
      if (domSel)
      {
        selPriv = do_QueryInterface(domSel);
        if (selPriv)
          selPriv->StartBatchChanges();
      }

      mSelCon->SelectAll();
      nsCOMPtr<nsIPlaintextEditor> htmlEditor = do_QueryInterface(mEditor);
      if (!htmlEditor) {
        NS_WARNING("Somehow not a plaintext editor?");
        if (pushed) {
          JSContext* cx;
          stack->Pop(&cx);
          NS_ASSERTION(!cx, "Unexpected JSContext popped!");
        }
        return;
      }

      // Since this code does not handle user-generated changes to the text,
      // make sure we don't fire oninput when the editor notifies us.
      // (mNotifyOnInput must be reset before we return).

      // To protect against a reentrant call to SetValue, we check whether
      // another SetValue is already happening for this frame.  If it is,
      // we must wait until we unwind to re-enable oninput events.
      PRBool outerTransaction = mNotifyOnInput;
      if (outerTransaction)
        mNotifyOnInput = PR_FALSE;

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

      if (pushed) {
        JSContext* cx;
        stack->Pop(&cx);
        NS_ASSERTION(!cx, "Unexpected JSContext popped!");
      }

      if (outerTransaction)
        mNotifyOnInput = PR_TRUE;

      // This method isn't used for user-generated changes, except for calls
      // from nsFileControlFrame which sets mFireChangeEventState==true and
      // restores it afterwards (ie. we want onchange events for those changes).
      if (!mFireChangeEventState) {
        // Reset mFocusedValue so the onchange event doesn't fire incorrectly.
        InitFocusedValue();
      }
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
nsTextControlFrame::SetInitialChildList(nsIAtom*        aListName,
                                        nsIFrame*       aChildList)
{
  nsresult rv = nsBoxFrame::SetInitialChildList(aListName, aChildList);
  if (mEditor)
    mEditor->PostCreate();
  //look for scroll view below this frame go along first child list
  nsIFrame* first = GetFirstChild(nsnull);

  // Mark the scroll frame as being a reflow root. This will allow
  // incremental reflows to be initiated at the scroll frame, rather
  // than descending from the root frame of the frame hierarchy.
  first->AddStateBits(NS_FRAME_REFLOW_ROOT);

  nsIScrollableFrame *scrollableFrame = nsnull;
  CallQueryInterface(first, &scrollableFrame);
  NS_ASSERTION(scrollableFrame, "Child must be scrollable");

  // we must turn off scrollbars for singleline text controls
  // XXX FIXME this should be removed,
  // nsGfxScrollFrameInner::CreateAnonymousContent handles this
  if (IsSingleLineTextControl()) 
  {
    if (scrollableFrame)
      scrollableFrame->SetScrollbarVisibility(PR_FALSE, PR_FALSE);
  }

  //register focus and key listeners
  nsCOMPtr<nsIDOMEventReceiver> erP = do_QueryInterface(mContent);
  if (erP) {
    // register the event listeners with the DOM event receiver
    rv = erP->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMFocusListener *,mTextListener), NS_GET_IID(nsIDOMFocusListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register focus listener");
    // XXXbryner do we need to check for a null presshell here?
    if (!GetPresContext()->GetPresShell())
      return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  erP->GetSystemEventGroup(getter_AddRefs(systemGroup));
  nsCOMPtr<nsIDOM3EventTarget> dom3Targ = do_QueryInterface(mContent);
  if (dom3Targ) {
    // cast because of ambiguous base
    nsIDOMEventListener *listener = NS_STATIC_CAST(nsIDOMKeyListener*,
                                                   mTextListener);

    dom3Targ->AddGroupedEventListener(NS_LITERAL_STRING("keydown"),
                                      listener, PR_FALSE, systemGroup);
    dom3Targ->AddGroupedEventListener(NS_LITERAL_STRING("keypress"),
                                      listener, PR_FALSE, systemGroup);
    dom3Targ->AddGroupedEventListener(NS_LITERAL_STRING("keyup"),
                                      listener, PR_FALSE, systemGroup);
  }

  if (scrollableFrame) {
    mScrollableView = scrollableFrame->GetScrollableView();
    mFrameSel->SetScrollableView(mScrollableView);
  }

  return rv;
}

nsIScrollableView* nsTextControlFrame::GetScrollableView()
{
  return mScrollableView;
}

PRBool
nsTextControlFrame::IsScrollable() const
{
  return !IsSingleLineTextControl();
}

void
nsTextControlFrame::SetValueChanged(PRBool aValueChanged)
{
  nsCOMPtr<nsITextControlElement> elem = do_QueryInterface(mContent);
  if (elem) {
    elem->SetValueChanged(aValueChanged);
  }
}

/* static */ void
nsTextControlFrame::ShutDown()
{
  NS_IF_RELEASE(sNativeTextAreaBindings);
  NS_IF_RELEASE(sNativeInputBindings);
}
