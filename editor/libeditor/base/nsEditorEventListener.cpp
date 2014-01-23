/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/dom/Element.h"        // for Element
#include "mozilla/dom/EventTarget.h"    // for EventTarget
#include "nsAString.h"
#include "nsCaret.h"                    // for nsCaret
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsEditor.h"                   // for nsEditor, etc
#include "nsEditorEventListener.h"
#include "nsEventListenerManager.h"     // for nsEventListenerManager
#include "nsFocusManager.h"             // for nsFocusManager
#include "nsGkAtoms.h"                  // for nsGkAtoms, nsGkAtoms::input
#include "nsIClipboard.h"               // for nsIClipboard, etc
#include "nsIContent.h"                 // for nsIContent
#include "nsIController.h"              // for nsIController
#include "nsID.h"
#include "nsIDOMDOMStringList.h"        // for nsIDOMDOMStringList
#include "nsIDOMDataTransfer.h"         // for nsIDOMDataTransfer
#include "nsIDOMDocument.h"             // for nsIDOMDocument
#include "nsIDOMDragEvent.h"            // for nsIDOMDragEvent
#include "nsIDOMElement.h"              // for nsIDOMElement
#include "nsIDOMEvent.h"                // for nsIDOMEvent
#include "nsIDOMEventTarget.h"          // for nsIDOMEventTarget
#include "nsIDOMKeyEvent.h"             // for nsIDOMKeyEvent
#include "nsIDOMMouseEvent.h"           // for nsIDOMMouseEvent
#include "nsIDOMNode.h"                 // for nsIDOMNode
#include "nsIDOMRange.h"                // for nsIDOMRange
#include "nsIDocument.h"                // for nsIDocument
#include "nsIEditor.h"                  // for nsEditor::GetSelection, etc
#include "nsIEditorIMESupport.h"
#include "nsIEditorMailSupport.h"       // for nsIEditorMailSupport
#include "nsIFocusManager.h"            // for nsIFocusManager
#include "nsIFormControl.h"             // for nsIFormControl, etc
#include "nsIHTMLEditor.h"              // for nsIHTMLEditor
#include "nsIMEStateManager.h"          // for nsIMEStateManager
#include "nsINativeKeyBindings.h"       // for nsINativeKeyBindings
#include "nsINode.h"                    // for nsINode, ::NODE_IS_EDITABLE, etc
#include "nsIPlaintextEditor.h"         // for nsIPlaintextEditor, etc
#include "nsIPresShell.h"               // for nsIPresShell
#include "nsIPrivateTextEvent.h"        // for nsIPrivateTextEvent
#include "nsIPrivateTextRange.h"        // for nsIPrivateTextRangeList
#include "nsISelection.h"               // for nsISelection
#include "nsISelectionController.h"     // for nsISelectionController, etc
#include "nsISelectionPrivate.h"        // for nsISelectionPrivate
#include "nsITransferable.h"            // for kFileMime, kHTMLMime, etc
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsPIWindowRoot.h"             // for nsPIWindowRoot
#include "nsServiceManagerUtils.h"      // for do_GetService
#include "nsString.h"                   // for nsAutoString
#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
#include "nsContentUtils.h"             // for nsContentUtils, etc
#include "nsIBidiKeyboard.h"            // for nsIBidiKeyboard
#endif

class nsPresContext;

using namespace mozilla;
using mozilla::dom::EventTarget;

static nsINativeKeyBindings *sNativeEditorBindings = nullptr;

static nsINativeKeyBindings*
GetEditorKeyBindings()
{
  static bool noBindings = false;
  if (!sNativeEditorBindings && !noBindings) {
    CallGetService(NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "editor",
                   &sNativeEditorBindings);

    if (!sNativeEditorBindings) {
      noBindings = true;
    }
  }

  return sNativeEditorBindings;
}

static void
DoCommandCallback(const char *aCommand, void *aData)
{
  nsIDocument* doc = static_cast<nsIDocument*>(aData);
  nsPIDOMWindow* win = doc->GetWindow();
  if (!win) {
    return;
  }
  nsCOMPtr<nsPIWindowRoot> root = win->GetTopWindowRoot();
  if (!root) {
    return;
  }

  nsCOMPtr<nsIController> controller;
  root->GetControllerForCommand(aCommand, getter_AddRefs(controller));
  if (!controller) {
    return;
  }

  bool commandEnabled;
  nsresult rv = controller->IsCommandEnabled(aCommand, &commandEnabled);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (commandEnabled) {
    controller->DoCommand(aCommand);
  }
}

nsEditorEventListener::nsEditorEventListener() :
  mEditor(nullptr), mCommitText(false),
  mInTransaction(false)
#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  , mHaveBidiKeyboards(false)
  , mShouldSwitchTextDirection(false)
  , mSwitchToRTL(false)
#endif
{
}

nsEditorEventListener::~nsEditorEventListener() 
{
  if (mEditor) {
    NS_WARNING("We're not uninstalled");
    Disconnect();
  }
}

/* static */ void
nsEditorEventListener::ShutDown()
{
  NS_IF_RELEASE(sNativeEditorBindings);
}

nsresult
nsEditorEventListener::Connect(nsEditor* aEditor)
{
  NS_ENSURE_ARG(aEditor);

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  nsIBidiKeyboard* bidiKeyboard = nsContentUtils::GetBidiKeyboard();
  if (bidiKeyboard) {
    bool haveBidiKeyboards = false;
    bidiKeyboard->GetHaveBidiKeyboards(&haveBidiKeyboards);
    mHaveBidiKeyboards = haveBidiKeyboards;
  }
#endif

  mEditor = aEditor;

  nsresult rv = InstallToEditor();
  if (NS_FAILED(rv)) {
    Disconnect();
  }
  return rv;
}

nsresult
nsEditorEventListener::InstallToEditor()
{
  NS_PRECONDITION(mEditor, "The caller must set mEditor");

  nsCOMPtr<EventTarget> piTarget = mEditor->GetDOMEventTarget();
  NS_ENSURE_TRUE(piTarget, NS_ERROR_FAILURE);

  // register the event listeners with the listener manager
  nsEventListenerManager* elmP = piTarget->GetOrCreateListenerManager();
  NS_ENSURE_STATE(elmP);

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("keydown"),
                               dom::TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("keyup"),
                               dom::TrustedEventsAtSystemGroupBubble());
#endif
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("keypress"),
                               dom::TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("dragenter"),
                               dom::TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("dragover"),
                               dom::TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("dragexit"),
                               dom::TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("drop"),
                               dom::TrustedEventsAtSystemGroupBubble());
  // XXX We should add the mouse event listeners as system event group.
  //     E.g., web applications cannot prevent middle mouse paste by
  //     preventDefault() of click event at bubble phase.
  //     However, if we do so, all click handlers in any frames and frontend
  //     code need to check if it's editable.  It makes easier create new bugs.
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("mousedown"),
                               dom::TrustedEventsAtCapture());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("mouseup"),
                               dom::TrustedEventsAtCapture());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("click"),
                               dom::TrustedEventsAtCapture());
// Focus event doesn't bubble so adding the listener to capturing phase.
// Make sure this works after bug 235441 gets fixed.
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("blur"),
                               dom::TrustedEventsAtCapture());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("focus"),
                               dom::TrustedEventsAtCapture());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("text"),
                               dom::TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("compositionstart"),
                               dom::TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("compositionend"),
                               dom::TrustedEventsAtSystemGroupBubble());

  return NS_OK;
}

void
nsEditorEventListener::Disconnect()
{
  if (!mEditor) {
    return;
  }
  UninstallFromEditor();
  mEditor = nullptr;
}

void
nsEditorEventListener::UninstallFromEditor()
{
  nsCOMPtr<EventTarget> piTarget = mEditor->GetDOMEventTarget();
  if (!piTarget) {
    return;
  }

  nsEventListenerManager* elmP = piTarget->GetOrCreateListenerManager();
  if (!elmP) {
    return;
  }

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("keydown"),
                                  dom::TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("keyup"),
                                  dom::TrustedEventsAtSystemGroupBubble());
#endif
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("keypress"),
                                  dom::TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("dragenter"),
                                  dom::TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("dragover"),
                                  dom::TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("dragexit"),
                                  dom::TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("drop"),
                                  dom::TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("mousedown"),
                                  dom::TrustedEventsAtCapture());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("mouseup"),
                                  dom::TrustedEventsAtCapture());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("click"),
                                  dom::TrustedEventsAtCapture());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("blur"),
                                  dom::TrustedEventsAtCapture());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("focus"),
                                  dom::TrustedEventsAtCapture());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("text"),
                                  dom::TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("compositionstart"),
                                  dom::TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("compositionend"),
                                  dom::TrustedEventsAtSystemGroupBubble());
}

already_AddRefed<nsIPresShell>
nsEditorEventListener::GetPresShell()
{
  NS_PRECONDITION(mEditor,
    "The caller must check whether this is connected to an editor");
  return mEditor->GetPresShell();
}

/**
 *  nsISupports implementation
 */

NS_IMPL_ISUPPORTS1(nsEditorEventListener, nsIDOMEventListener)

/**
 *  nsIDOMEventListener implementation
 */

NS_IMETHODIMP
nsEditorEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIEditor> kungFuDeathGrip = mEditor;

  nsAutoString eventType;
  aEvent->GetType(eventType);

  nsCOMPtr<nsIDOMDragEvent> dragEvent = do_QueryInterface(aEvent);
  if (dragEvent) {
    if (eventType.EqualsLiteral("dragenter"))
      return DragEnter(dragEvent);
    if (eventType.EqualsLiteral("dragover"))
      return DragOver(dragEvent);
    if (eventType.EqualsLiteral("dragexit"))
      return DragExit(dragEvent);
    if (eventType.EqualsLiteral("drop"))
      return Drop(dragEvent);
  }

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  if (eventType.EqualsLiteral("keydown"))
    return KeyDown(aEvent);
  if (eventType.EqualsLiteral("keyup"))
    return KeyUp(aEvent);
#endif
  if (eventType.EqualsLiteral("keypress"))
    return KeyPress(aEvent);
  if (eventType.EqualsLiteral("mousedown"))
    return MouseDown(aEvent);
  if (eventType.EqualsLiteral("mouseup"))
    return MouseUp(aEvent);
  if (eventType.EqualsLiteral("click"))
    return MouseClick(aEvent);
  if (eventType.EqualsLiteral("focus"))
    return Focus(aEvent);
  if (eventType.EqualsLiteral("blur"))
    return Blur(aEvent);
  if (eventType.EqualsLiteral("text"))
    return HandleText(aEvent);
  if (eventType.EqualsLiteral("compositionstart"))
    return HandleStartComposition(aEvent);
  if (eventType.EqualsLiteral("compositionend")) {
    HandleEndComposition(aEvent);
    return NS_OK;
  }

  return NS_OK;
}

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
#include <windows.h>

namespace {

// This function is borrowed from Chromium's ImeInput::IsCtrlShiftPressed
bool IsCtrlShiftPressed(bool& isRTL)
{
  BYTE keystate[256];
  if (!::GetKeyboardState(keystate)) {
    return false;
  }

  // To check if a user is pressing only a control key and a right-shift key
  // (or a left-shift key), we use the steps below:
  // 1. Check if a user is pressing a control key and a right-shift key (or
  //    a left-shift key).
  // 2. If the condition 1 is true, we should check if there are any other
  //    keys pressed at the same time.
  //    To ignore the keys checked in 1, we set their status to 0 before
  //    checking the key status.
  const int kKeyDownMask = 0x80;
  if ((keystate[VK_CONTROL] & kKeyDownMask) == 0)
    return false;

  if (keystate[VK_RSHIFT] & kKeyDownMask) {
    keystate[VK_RSHIFT] = 0;
    isRTL = true;
  } else if (keystate[VK_LSHIFT] & kKeyDownMask) {
    keystate[VK_LSHIFT] = 0;
    isRTL = false;
  } else {
    return false;
  }

  // Scan the key status to find pressed keys. We should abandon changing the
  // text direction when there are other pressed keys.
  // This code is executed only when a user is pressing a control key and a
  // right-shift key (or a left-shift key), i.e. we should ignore the status of
  // the keys: VK_SHIFT, VK_CONTROL, VK_RCONTROL, and VK_LCONTROL.
  // So, we reset their status to 0 and ignore them.
  keystate[VK_SHIFT] = 0;
  keystate[VK_CONTROL] = 0;
  keystate[VK_RCONTROL] = 0;
  keystate[VK_LCONTROL] = 0;
  for (int i = 0; i <= VK_PACKET; ++i) {
    if (keystate[i] & kKeyDownMask)
      return false;
  }
  return true;
}

}

// This logic is mostly borrowed from Chromium's
// RenderWidgetHostViewWin::OnKeyEvent.

NS_IMETHODIMP
nsEditorEventListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  if (mHaveBidiKeyboards) {
    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
    if (!keyEvent) {
      // non-key event passed to keyup.  bad things.
      return NS_OK;
    }

    uint32_t keyCode = 0;
    keyEvent->GetKeyCode(&keyCode);
    if (keyCode == nsIDOMKeyEvent::DOM_VK_SHIFT ||
        keyCode == nsIDOMKeyEvent::DOM_VK_CONTROL) {
      if (mShouldSwitchTextDirection && mEditor->IsPlaintextEditor()) {
        mEditor->SwitchTextDirectionTo(mSwitchToRTL ?
          nsIPlaintextEditor::eEditorRightToLeft :
          nsIPlaintextEditor::eEditorLeftToRight);
        mShouldSwitchTextDirection = false;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditorEventListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  if (mHaveBidiKeyboards) {
    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
    if (!keyEvent) {
      // non-key event passed to keydown.  bad things.
      return NS_OK;
    }

    uint32_t keyCode = 0;
    keyEvent->GetKeyCode(&keyCode);
    if (keyCode == nsIDOMKeyEvent::DOM_VK_SHIFT) {
      bool switchToRTL;
      if (IsCtrlShiftPressed(switchToRTL)) {
        mShouldSwitchTextDirection = true;
        mSwitchToRTL = switchToRTL;
      }
    } else if (keyCode != nsIDOMKeyEvent::DOM_VK_CONTROL) {
      // In case the user presses any other key besides Ctrl and Shift
      mShouldSwitchTextDirection = false;
    }
  }

  return NS_OK;
}
#endif

NS_IMETHODIMP
nsEditorEventListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);

  if (!mEditor->IsAcceptableInputEvent(aKeyEvent)) {
    return NS_OK;
  }

  // DOM event handling happens in two passes, the client pass and the system
  // pass.  We do all of our processing in the system pass, to allow client
  // handlers the opportunity to cancel events and prevent typing in the editor.
  // If the client pass cancelled the event, defaultPrevented will be true
  // below.

  bool defaultPrevented;
  aKeyEvent->GetDefaultPrevented(&defaultPrevented);
  if (defaultPrevented) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (!keyEvent) {
    //non-key event passed to keypress.  bad things.
    return NS_OK;
  }

  nsresult rv = mEditor->HandleKeyPressEvent(keyEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  aKeyEvent->GetDefaultPrevented(&defaultPrevented);
  if (defaultPrevented) {
    return NS_OK;
  }

  if (GetEditorKeyBindings() && ShouldHandleNativeKeyBindings(aKeyEvent)) {
    // Now, ask the native key bindings to handle the event.
    // XXX Note that we're not passing the keydown/keyup events to the native
    // key bindings, which should be OK since those events are only handled on
    // Windows for now, where we don't have native key bindings.
    WidgetKeyboardEvent* keyEvent =
      aKeyEvent->GetInternalNSEvent()->AsKeyboardEvent();
    MOZ_ASSERT(keyEvent,
               "DOM key event's internal event must be WidgetKeyboardEvent");
    nsCOMPtr<nsIDocument> doc = mEditor->GetDocument();
    bool handled = sNativeEditorBindings->KeyPress(*keyEvent,
                                                   DoCommandCallback,
                                                   doc);
    if (handled) {
      aKeyEvent->PreventDefault();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditorEventListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  NS_ENSURE_TRUE(mouseEvent, NS_OK);

  // nothing to do if editor isn't editable or clicked on out of the editor.
  if (mEditor->IsReadonly() || mEditor->IsDisabled() ||
      !mEditor->IsAcceptableInputEvent(aMouseEvent)) {
    return NS_OK;
  }

  // Notifies clicking on editor to IMEStateManager even when the event was
  // consumed.
  nsCOMPtr<nsIContent> focusedContent = mEditor->GetFocusedContent();
  if (focusedContent) {
    nsIDocument* currentDoc = focusedContent->GetCurrentDoc();
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    nsPresContext* presContext =
      presShell ? presShell->GetPresContext() : nullptr;
    if (presContext && currentDoc) {
      nsIMEStateManager::OnClickInEditor(presContext,
        currentDoc->HasFlag(NODE_IS_EDITABLE) ? nullptr : focusedContent,
        mouseEvent);
    }
  }

  bool preventDefault;
  nsresult rv = aMouseEvent->GetDefaultPrevented(&preventDefault);
  if (NS_FAILED(rv) || preventDefault) {
    // We're done if 'preventdefault' is true (see for example bug 70698).
    return rv;
  }

  // If we got a mouse down inside the editing area, we should force the 
  // IME to commit before we change the cursor position
  mEditor->ForceCompositionEnd();

  uint16_t button = (uint16_t)-1;
  mouseEvent->GetButton(&button);
  // middle-mouse click (paste);
  if (button == 1)
  {
    if (Preferences::GetBool("middlemouse.paste", false))
    {
      // Set the selection to the point under the mouse cursor:
      nsCOMPtr<nsIDOMNode> parent;
      if (NS_FAILED(mouseEvent->GetRangeParent(getter_AddRefs(parent))))
        return NS_ERROR_NULL_POINTER;
      int32_t offset = 0;
      if (NS_FAILED(mouseEvent->GetRangeOffset(&offset)))
        return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsISelection> selection;
      if (NS_SUCCEEDED(mEditor->GetSelection(getter_AddRefs(selection))))
        (void)selection->Collapse(parent, offset);

      // If the ctrl key is pressed, we'll do paste as quotation.
      // Would've used the alt key, but the kde wmgr treats alt-middle specially. 
      bool ctrlKey = false;
      mouseEvent->GetCtrlKey(&ctrlKey);

      nsCOMPtr<nsIEditorMailSupport> mailEditor;
      if (ctrlKey)
        mailEditor = do_QueryObject(mEditor);

      int32_t clipboard = nsIClipboard::kGlobalClipboard;
      nsCOMPtr<nsIClipboard> clipboardService =
        do_GetService("@mozilla.org/widget/clipboard;1", &rv);
      if (NS_SUCCEEDED(rv)) {
        bool selectionSupported;
        rv = clipboardService->SupportsSelectionClipboard(&selectionSupported);
        if (NS_SUCCEEDED(rv) && selectionSupported) {
          clipboard = nsIClipboard::kSelectionClipboard;
        }
      }

      if (mailEditor)
        mailEditor->PasteAsQuotation(clipboard);
      else
        mEditor->Paste(clipboard);

      // Prevent the event from propagating up to be possibly handled
      // again by the containing window:
      mouseEvent->StopPropagation();
      mouseEvent->PreventDefault();

      // We processed the event, whether drop/paste succeeded or not
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEditorEventListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);
  mEditor->ForceCompositionEnd();
  return NS_OK;
}

NS_IMETHODIMP
nsEditorEventListener::HandleText(nsIDOMEvent* aTextEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);

  if (!mEditor->IsAcceptableInputEvent(aTextEvent)) {
    return NS_OK;
  }

  nsCOMPtr<nsIPrivateTextEvent> textEvent = do_QueryInterface(aTextEvent);
  if (!textEvent) {
     //non-ui event passed in.  bad things.
     return NS_OK;
  }

  nsAutoString                      composedText;
  nsCOMPtr<nsIPrivateTextRangeList> textRangeList;

  textEvent->GetText(composedText);
  textRangeList = textEvent->GetInputRange();

  // if we are readonly or disabled, then do nothing.
  if (mEditor->IsReadonly() || mEditor->IsDisabled()) {
    return NS_OK;
  }

  return mEditor->UpdateIMEComposition(composedText, textRangeList);
}

/**
 * Drag event implementation
 */

nsresult
nsEditorEventListener::DragEnter(nsIDOMDragEvent* aDragEvent)
{
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_OK);

  if (!mCaret) {
    mCaret = new nsCaret();
    mCaret->Init(presShell);
    mCaret->SetCaretReadOnly(true);
  }

  presShell->SetCaret(mCaret);

  return DragOver(aDragEvent);
}

nsresult
nsEditorEventListener::DragOver(nsIDOMDragEvent* aDragEvent)
{
  nsCOMPtr<nsIDOMNode> parent;
  bool defaultPrevented;
  aDragEvent->GetDefaultPrevented(&defaultPrevented);
  if (defaultPrevented) {
    return NS_OK;
  }

  aDragEvent->GetRangeParent(getter_AddRefs(parent));
  nsCOMPtr<nsIContent> dropParent = do_QueryInterface(parent);
  NS_ENSURE_TRUE(dropParent, NS_ERROR_FAILURE);

  if (dropParent->IsEditable() && CanDrop(aDragEvent)) {
    aDragEvent->PreventDefault(); // consumed

    if (mCaret) {
      int32_t offset = 0;
      nsresult rv = aDragEvent->GetRangeOffset(&offset);
      NS_ENSURE_SUCCESS(rv, rv);

      // to avoid flicker, we could track the node and offset to see if we moved
      if (mCaret)
        mCaret->EraseCaret();
      
      //mCaret->SetCaretVisible(true);   // make sure it's visible
      mCaret->DrawAtPosition(parent, offset);
    }
  }
  else
  {
    if (!IsFileControlTextBox()) {
      // This is needed when dropping on an input, to prevent the editor for
      // the editable parent from receiving the event.
      aDragEvent->StopPropagation();
    }

    if (mCaret)
    {
      mCaret->EraseCaret();
    }
  }

  return NS_OK;
}

void
nsEditorEventListener::CleanupDragDropCaret()
{
  if (mCaret)
  {
    mCaret->EraseCaret();
    mCaret->SetCaretVisible(false);    // hide it, so that it turns off its timer

    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    if (presShell)
    {
      presShell->RestoreCaret();
    }

    mCaret->Terminate();
    mCaret = nullptr;
  }
}

nsresult
nsEditorEventListener::DragExit(nsIDOMDragEvent* aDragEvent)
{
  CleanupDragDropCaret();

  return NS_OK;
}

nsresult
nsEditorEventListener::Drop(nsIDOMDragEvent* aMouseEvent)
{
  CleanupDragDropCaret();

  bool defaultPrevented;
  aMouseEvent->GetDefaultPrevented(&defaultPrevented);
  if (defaultPrevented) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> parent;
  aMouseEvent->GetRangeParent(getter_AddRefs(parent));
  nsCOMPtr<nsIContent> dropParent = do_QueryInterface(parent);
  NS_ENSURE_TRUE(dropParent, NS_ERROR_FAILURE);

  if (!dropParent->IsEditable() || !CanDrop(aMouseEvent)) {
    // was it because we're read-only?
    if ((mEditor->IsReadonly() || mEditor->IsDisabled()) &&
        !IsFileControlTextBox()) {
      // it was decided to "eat" the event as this is the "least surprise"
      // since someone else handling it might be unintentional and the 
      // user could probably re-drag to be not over the disabled/readonly 
      // editfields if that is what is desired.
      return aMouseEvent->StopPropagation();
    }
    return NS_OK;
  }

  aMouseEvent->StopPropagation();
  aMouseEvent->PreventDefault();
  return mEditor->InsertFromDrop(aMouseEvent);
}

bool
nsEditorEventListener::CanDrop(nsIDOMDragEvent* aEvent)
{
  // if the target doc is read-only, we can't drop
  if (mEditor->IsReadonly() || mEditor->IsDisabled()) {
    return false;
  }

  nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
  aEvent->GetDataTransfer(getter_AddRefs(dataTransfer));
  NS_ENSURE_TRUE(dataTransfer, false);

  nsCOMPtr<nsIDOMDOMStringList> types;
  dataTransfer->GetTypes(getter_AddRefs(types));
  NS_ENSURE_TRUE(types, false);

  // Plaintext editors only support dropping text. Otherwise, HTML and files
  // can be dropped as well.
  bool typeSupported;
  types->Contains(NS_LITERAL_STRING(kTextMime), &typeSupported);
  if (!typeSupported) {
    types->Contains(NS_LITERAL_STRING(kMozTextInternal), &typeSupported);
    if (!typeSupported && !mEditor->IsPlaintextEditor()) {
      types->Contains(NS_LITERAL_STRING(kHTMLMime), &typeSupported);
      if (!typeSupported) {
        types->Contains(NS_LITERAL_STRING(kFileMime), &typeSupported);
      }
    }
  }

  NS_ENSURE_TRUE(typeSupported, false);

  // If there is no source node, this is probably an external drag and the
  // drop is allowed. The later checks rely on checking if the drag target
  // is the same as the drag source.
  nsCOMPtr<nsIDOMNode> sourceNode;
  dataTransfer->GetMozSourceNode(getter_AddRefs(sourceNode));
  if (!sourceNode)
    return true;

  // There is a source node, so compare the source documents and this document.
  // Disallow drops on the same document.

  nsCOMPtr<nsIDOMDocument> domdoc = mEditor->GetDOMDocument();
  NS_ENSURE_TRUE(domdoc, false);

  nsCOMPtr<nsIDOMDocument> sourceDoc;
  nsresult rv = sourceNode->GetOwnerDocument(getter_AddRefs(sourceDoc));
  NS_ENSURE_SUCCESS(rv, false);
  if (domdoc == sourceDoc)      // source and dest are the same document; disallow drops within the selection
  {
    nsCOMPtr<nsISelection> selection;
    rv = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(rv) || !selection)
      return false;
    
    // Don't bother if collapsed - can always drop
    if (!selection->Collapsed()) {
      nsCOMPtr<nsIDOMNode> parent;
      rv = aEvent->GetRangeParent(getter_AddRefs(parent));
      if (NS_FAILED(rv) || !parent) return false;

      int32_t offset = 0;
      rv = aEvent->GetRangeOffset(&offset);
      NS_ENSURE_SUCCESS(rv, false);

      int32_t rangeCount;
      rv = selection->GetRangeCount(&rangeCount);
      NS_ENSURE_SUCCESS(rv, false);

      for (int32_t i = 0; i < rangeCount; i++)
      {
        nsCOMPtr<nsIDOMRange> range;
        rv = selection->GetRangeAt(i, getter_AddRefs(range));
        if (NS_FAILED(rv) || !range) 
          continue; //don't bail yet, iterate through them all

        bool inRange = true;
        (void)range->IsPointInRange(parent, offset, &inRange);
        if (inRange)
          return false;  //okay, now you can bail, we are over the orginal selection
      }
    }
  }
  
  return true;
}

NS_IMETHODIMP
nsEditorEventListener::HandleStartComposition(nsIDOMEvent* aCompositionEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);
  if (!mEditor->IsAcceptableInputEvent(aCompositionEvent)) {
    return NS_OK;
  }
  return mEditor->BeginIMEComposition();
}

void
nsEditorEventListener::HandleEndComposition(nsIDOMEvent* aCompositionEvent)
{
  MOZ_ASSERT(mEditor);
  if (!mEditor->IsAcceptableInputEvent(aCompositionEvent)) {
    return;
  }

  mEditor->EndIMEComposition();
}

NS_IMETHODIMP
nsEditorEventListener::Focus(nsIDOMEvent* aEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG(aEvent);

  // Don't turn on selection and caret when the editor is disabled.
  if (mEditor->IsDisabled()) {
    return NS_OK;
  }

  // Spell check a textarea the first time that it is focused.
  SpellCheckIfNeeded();

  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));
  nsCOMPtr<nsINode> node = do_QueryInterface(target);
  NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

  // If the target is a document node but it's not editable, we should ignore
  // it because actual focused element's event is going to come.
  if (node->IsNodeOfType(nsINode::eDOCUMENT) &&
      !node->HasFlag(NODE_IS_EDITABLE)) {
    return NS_OK;
  }

  if (node->IsNodeOfType(nsINode::eCONTENT)) {
    // XXX If the focus event target is a form control in contenteditable
    // element, perhaps, the parent HTML editor should do nothing by this
    // handler.  However, FindSelectionRoot() returns the root element of the
    // contenteditable editor.  So, the editableRoot value is invalid for
    // the plain text editor, and it will be set to the wrong limiter of
    // the selection.  However, fortunately, actual bugs are not found yet.
    nsCOMPtr<nsIContent> editableRoot = mEditor->FindSelectionRoot(node);

    // make sure that the element is really focused in case an earlier
    // listener in the chain changed the focus.
    if (editableRoot) {
      nsIFocusManager* fm = nsFocusManager::GetFocusManager();
      NS_ENSURE_TRUE(fm, NS_OK);

      nsCOMPtr<nsIDOMElement> element;
      fm->GetFocusedElement(getter_AddRefs(element));
      if (!SameCOMIdentity(element, target))
        return NS_OK;
    }
  }

  mEditor->OnFocus(target);

  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_OK);
  nsCOMPtr<nsIContent> focusedContent = mEditor->GetFocusedContentForIME();
  nsIMEStateManager::OnFocusInEditor(ps->GetPresContext(), focusedContent);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorEventListener::Blur(nsIDOMEvent* aEvent)
{
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_ARG(aEvent);

  // check if something else is focused. If another element is focused, then
  // we should not change the selection.
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, NS_OK);

  nsCOMPtr<nsIDOMElement> element;
  fm->GetFocusedElement(getter_AddRefs(element));
  if (element)
    return NS_OK;

  mEditor->FinalizeSelection();
  return NS_OK;
}

void
nsEditorEventListener::SpellCheckIfNeeded() {
  // If the spell check skip flag is still enabled from creation time,
  // disable it because focused editors are allowed to spell check.
  uint32_t currentFlags = 0;
  mEditor->GetFlags(&currentFlags);
  if(currentFlags & nsIPlaintextEditor::eEditorSkipSpellCheck)
  {
    currentFlags ^= nsIPlaintextEditor::eEditorSkipSpellCheck;
    mEditor->SetFlags(currentFlags);
  }
}

bool
nsEditorEventListener::IsFileControlTextBox()
{
  dom::Element* root = mEditor->GetRoot();
  if (root && root->ChromeOnlyAccess()) {
    nsIContent* parent = root->FindFirstNonChromeOnlyAccessContent();
    if (parent && parent->IsHTML(nsGkAtoms::input)) {
      nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(parent);
      MOZ_ASSERT(formControl);
      return formControl->GetType() == NS_FORM_INPUT_FILE;
    }
  }
  return false;
}

bool
nsEditorEventListener::ShouldHandleNativeKeyBindings(nsIDOMEvent* aKeyEvent)
{
  // Only return true if the target of the event is a desendant of the active
  // editing host in order to match the similar decision made in
  // nsXBLWindowKeyHandler.
  // Note that IsAcceptableInputEvent doesn't check for the active editing
  // host for keyboard events, otherwise this check would have been
  // unnecessary.  IsAcceptableInputEvent currently makes a similar check for
  // mouse events.

  nsCOMPtr<nsIDOMEventTarget> target;
  aKeyEvent->GetTarget(getter_AddRefs(target));
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);
  if (!targetContent) {
    return false;
  }

  nsCOMPtr<nsIHTMLEditor> htmlEditor =
    do_QueryInterface(static_cast<nsIEditor*>(mEditor));
  if (!htmlEditor) {
    return false;
  }

  nsIContent* editingHost = htmlEditor->GetActiveEditingHost();
  if (!editingHost) {
    return false;
  }

  return nsContentUtils::ContentIsDescendantOf(targetContent, editingHost);
}

