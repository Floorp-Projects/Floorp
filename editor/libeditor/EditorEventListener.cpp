/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorEventListener.h"

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc.
#include "mozilla/AutoRestore.h"
#include "mozilla/ContentEvents.h"      // for InternalFocusEvent
#include "mozilla/EditorBase.h"         // for EditorBase, etc.
#include "mozilla/EventListenerManager.h" // for EventListenerManager
#include "mozilla/IMEStateManager.h"    // for IMEStateManager
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/TextEditor.h"         // for TextEditor
#include "mozilla/TextEvents.h"         // for WidgetCompositionEvent
#include "mozilla/dom/Element.h"        // for Element
#include "mozilla/dom/Event.h"          // for Event
#include "mozilla/dom/EventTarget.h"    // for EventTarget
#include "mozilla/dom/MouseEvent.h"     // for MouseEvent
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsCaret.h"                    // for nsCaret
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc.
#include "nsFocusManager.h"             // for nsFocusManager
#include "nsGkAtoms.h"                  // for nsGkAtoms, nsGkAtoms::input
#include "nsIClipboard.h"               // for nsIClipboard, etc.
#include "nsIContent.h"                 // for nsIContent
#include "nsIController.h"              // for nsIController
#include "nsID.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DragEvent.h"
#include "nsIDocument.h"                // for nsIDocument
#include "nsIFocusManager.h"            // for nsIFocusManager
#include "nsIFormControl.h"             // for nsIFormControl, etc.
#include "nsINode.h"                    // for nsINode, ::NODE_IS_EDITABLE, etc.
#include "nsIPlaintextEditor.h"         // for nsIPlaintextEditor, etc.
#include "nsIPresShell.h"               // for nsIPresShell
#include "nsISelectionController.h"     // for nsISelectionController, etc.
#include "nsITransferable.h"            // for kFileMime, kHTMLMime, etc.
#include "nsIWidget.h"                  // for nsIWidget
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsPIWindowRoot.h"             // for nsPIWindowRoot
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsRange.h"
#include "nsServiceManagerUtils.h"      // for do_GetService
#include "nsString.h"                   // for nsAutoString
#include "nsQueryObject.h"              // for do_QueryObject
#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
#include "nsContentUtils.h"             // for nsContentUtils, etc.
#include "nsIBidiKeyboard.h"            // for nsIBidiKeyboard
#endif

#include "mozilla/dom/TabParent.h"

class nsPresContext;

namespace mozilla {

using namespace dom;

static void
DoCommandCallback(Command aCommand, void* aData)
{
  nsIDocument* doc = static_cast<nsIDocument*>(aData);
  nsPIDOMWindowOuter* win = doc->GetWindow();
  if (!win) {
    return;
  }
  nsCOMPtr<nsPIWindowRoot> root = win->GetTopWindowRoot();
  if (!root) {
    return;
  }

  const char* commandStr = WidgetKeyboardEvent::GetCommandStr(aCommand);

  nsCOMPtr<nsIController> controller;
  root->GetControllerForCommand(commandStr, false /* for any window */,
                                getter_AddRefs(controller));
  if (!controller) {
    return;
  }

  bool commandEnabled;
  nsresult rv = controller->IsCommandEnabled(commandStr, &commandEnabled);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (commandEnabled) {
    controller->DoCommand(commandStr);
  }
}

EditorEventListener::EditorEventListener()
  : mEditorBase(nullptr)
  , mCommitText(false)
  , mInTransaction(false)
  , mMouseDownOrUpConsumedByIME(false)
#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  , mHaveBidiKeyboards(false)
  , mShouldSwitchTextDirection(false)
  , mSwitchToRTL(false)
#endif
{
}

EditorEventListener::~EditorEventListener()
{
  if (mEditorBase) {
    NS_WARNING("We're not uninstalled");
    Disconnect();
  }
}

nsresult
EditorEventListener::Connect(EditorBase* aEditorBase)
{
  NS_ENSURE_ARG(aEditorBase);

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  nsIBidiKeyboard* bidiKeyboard = nsContentUtils::GetBidiKeyboard();
  if (bidiKeyboard) {
    bool haveBidiKeyboards = false;
    bidiKeyboard->GetHaveBidiKeyboards(&haveBidiKeyboards);
    mHaveBidiKeyboards = haveBidiKeyboards;
  }
#endif

  mEditorBase = aEditorBase;

  nsresult rv = InstallToEditor();
  if (NS_FAILED(rv)) {
    Disconnect();
  }
  return rv;
}

nsresult
EditorEventListener::InstallToEditor()
{
  MOZ_ASSERT(mEditorBase, "The caller must set mEditorBase");

  EventTarget* piTarget = mEditorBase->GetDOMEventTarget();
  NS_ENSURE_TRUE(piTarget, NS_ERROR_FAILURE);

  // register the event listeners with the listener manager
  EventListenerManager* elmP = piTarget->GetOrCreateListenerManager();
  NS_ENSURE_STATE(elmP);

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("keydown"),
                               TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("keyup"),
                               TrustedEventsAtSystemGroupBubble());
#endif
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("keypress"),
                               TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("dragenter"),
                               TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("dragover"),
                               TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("dragexit"),
                               TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("drop"),
                               TrustedEventsAtSystemGroupBubble());
  // XXX We should add the mouse event listeners as system event group.
  //     E.g., web applications cannot prevent middle mouse paste by
  //     preventDefault() of click event at bubble phase.
  //     However, if we do so, all click handlers in any frames and frontend
  //     code need to check if it's editable.  It makes easier create new bugs.
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("mousedown"),
                               TrustedEventsAtCapture());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("mouseup"),
                               TrustedEventsAtCapture());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("click"),
                               TrustedEventsAtCapture());
  // Focus event doesn't bubble so adding the listener to capturing phase.
  // XXX Should we listen focus/blur events of system group too? Or should
  //     editor notified focus/blur of the element from nsFocusManager
  //     directly?  Because if the event propagation is stopped by JS,
  //     editor cannot initialize selection as expected.
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("blur"),
                               TrustedEventsAtCapture());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("focus"),
                               TrustedEventsAtCapture());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("text"),
                               TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("compositionstart"),
                               TrustedEventsAtSystemGroupBubble());
  elmP->AddEventListenerByType(this,
                               NS_LITERAL_STRING("compositionend"),
                               TrustedEventsAtSystemGroupBubble());

  return NS_OK;
}

void
EditorEventListener::Disconnect()
{
  if (DetachedFromEditor()) {
    return;
  }
  UninstallFromEditor();

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    nsIContent* focusedContent = fm->GetFocusedElement();
    mozilla::dom::Element* root = mEditorBase->GetRoot();
    if (focusedContent && root &&
        nsContentUtils::ContentIsDescendantOf(focusedContent, root)) {
      // Reset the Selection ancestor limiter and SelectionController state
      // that EditorBase::InitializeSelection set up.
      mEditorBase->FinalizeSelection();
    }
  }

  mEditorBase = nullptr;
}

void
EditorEventListener::UninstallFromEditor()
{
  nsCOMPtr<EventTarget> piTarget = mEditorBase->GetDOMEventTarget();
  if (!piTarget) {
    return;
  }

  EventListenerManager* elmP = piTarget->GetOrCreateListenerManager();
  if (!elmP) {
    return;
  }

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("keydown"),
                                  TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("keyup"),
                                  TrustedEventsAtSystemGroupBubble());
#endif
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("keypress"),
                                  TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("dragenter"),
                                  TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("dragover"),
                                  TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("dragexit"),
                                  TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("drop"),
                                  TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("mousedown"),
                                  TrustedEventsAtCapture());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("mouseup"),
                                  TrustedEventsAtCapture());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("click"),
                                  TrustedEventsAtCapture());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("blur"),
                                  TrustedEventsAtCapture());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("focus"),
                                  TrustedEventsAtCapture());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("text"),
                                  TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("compositionstart"),
                                  TrustedEventsAtSystemGroupBubble());
  elmP->RemoveEventListenerByType(this,
                                  NS_LITERAL_STRING("compositionend"),
                                  TrustedEventsAtSystemGroupBubble());
}

nsIPresShell*
EditorEventListener::GetPresShell() const
{
  MOZ_ASSERT(!DetachedFromEditor());
  return mEditorBase->GetPresShell();
}

nsPresContext*
EditorEventListener::GetPresContext() const
{
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  return presShell ? presShell->GetPresContext() : nullptr;
}

nsIContent*
EditorEventListener::GetFocusedRootContent()
{
  MOZ_ASSERT(!DetachedFromEditor());
  nsCOMPtr<nsIContent> focusedContent = mEditorBase->GetFocusedContent();
  if (!focusedContent) {
    return nullptr;
  }

  nsIDocument* composedDoc = focusedContent->GetComposedDoc();
  NS_ENSURE_TRUE(composedDoc, nullptr);

  if (composedDoc->HasFlag(NODE_IS_EDITABLE)) {
    return nullptr;
  }

  return focusedContent;
}

bool
EditorEventListener::EditorHasFocus()
{
  MOZ_ASSERT(!DetachedFromEditor());
  nsCOMPtr<nsIContent> focusedContent = mEditorBase->GetFocusedContent();
  if (!focusedContent) {
    return false;
  }
  nsIDocument* composedDoc = focusedContent->GetComposedDoc();
  return !!composedDoc;
}

NS_IMPL_ISUPPORTS(EditorEventListener, nsIDOMEventListener)

bool
EditorEventListener::DetachedFromEditor() const
{
  return !mEditorBase;
}

bool
EditorEventListener::DetachedFromEditorOrDefaultPrevented(
                       WidgetEvent* aWidgetEvent) const
{
  return NS_WARN_IF(!aWidgetEvent) || DetachedFromEditor() ||
         aWidgetEvent->DefaultPrevented();
}

bool
EditorEventListener::EnsureCommitCompoisition()
{
  MOZ_ASSERT(!DetachedFromEditor());
  RefPtr<EditorBase> editorBase(mEditorBase);
  editorBase->CommitComposition();
  return !DetachedFromEditor();
}

NS_IMETHODIMP
EditorEventListener::HandleEvent(Event* aEvent)
{
  // Let's handle each event with the message of the internal event of the
  // coming event.  If the DOM event was created with improper interface,
  // e.g., keydown event is created with |new MouseEvent("keydown", {});|,
  // its message is always 0.  Therefore, we can ban such strange event easy.
  // However, we need to handle strange "focus" and "blur" event.  See the
  // following code of this switch statement.
  // NOTE: Each event handler may require specific event interface.  Before
  //       calling it, this queries the specific interface.  If it would fail,
  //       each event handler would just ignore the event.  So, in this method,
  //       you don't need to check if the QI succeeded before each call.
  WidgetEvent* internalEvent = aEvent->WidgetEventPtr();
  switch (internalEvent->mMessage) {
    // dragenter
    case eDragEnter: {
      return DragEnter(aEvent->AsDragEvent());
    }
    // dragover
    case eDragOver: {
      return DragOver(aEvent->AsDragEvent());
    }
    // dragexit
    case eDragExit: {
      return DragExit(aEvent->AsDragEvent());
    }
    // drop
    case eDrop: {
      return Drop(aEvent->AsDragEvent());
    }
#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
    // keydown
    case eKeyDown: {
      return KeyDown(internalEvent->AsKeyboardEvent());
    }
    // keyup
    case eKeyUp:
      return KeyUp(internalEvent->AsKeyboardEvent());
#endif // #ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
    // keypress
    case eKeyPress:
      return KeyPress(internalEvent->AsKeyboardEvent());
    // mousedown
    case eMouseDown: {
      // EditorEventListener may receive (1) all mousedown, mouseup and click
      // events, (2) only mousedown event or (3) only mouseup event.
      // mMouseDownOrUpConsumedByIME is used only for ignoring click event if
      // preceding mousedown and/or mouseup event is consumed by IME.
      // Therefore, even if case #2 or case #3 occurs,
      // mMouseDownOrUpConsumedByIME is true here.  Therefore, we should always
      // overwrite it here.
      mMouseDownOrUpConsumedByIME =
        NotifyIMEOfMouseButtonEvent(internalEvent->AsMouseEvent());
      if (mMouseDownOrUpConsumedByIME) {
        return NS_OK;
      }
      MouseEvent* mouseEvent = aEvent->AsMouseEvent();
      return NS_WARN_IF(!mouseEvent) ? NS_OK : MouseDown(mouseEvent);
    }
    // mouseup
    case eMouseUp: {
      // See above comment in the eMouseDown case, first.
      // This code assumes that case #1 is occuring.  However, if case #3 may
      // occurs after case #2 and the mousedown is consumed,
      // mMouseDownOrUpConsumedByIME is true even though EditorEventListener
      // has not received the preceding mousedown event of this mouseup event.
      // So, mMouseDownOrUpConsumedByIME may be invalid here.  However,
      // this is not a matter because mMouseDownOrUpConsumedByIME is referred
      // only by eMouseClick case but click event is fired only in case #1.
      // So, before a click event is fired, mMouseDownOrUpConsumedByIME is
      // always initialized in the eMouseDown case if it's referred.
      if (NotifyIMEOfMouseButtonEvent(internalEvent->AsMouseEvent())) {
        mMouseDownOrUpConsumedByIME = true;
      }
      if (mMouseDownOrUpConsumedByIME) {
        return NS_OK;
      }
      MouseEvent* mouseEvent = aEvent->AsMouseEvent();
      return NS_WARN_IF(!mouseEvent) ? NS_OK : MouseUp(mouseEvent);
    }
    // click
    case eMouseClick: {
      MouseEvent* mouseEvent = aEvent->AsMouseEvent();
      NS_ENSURE_TRUE(mouseEvent, NS_OK);
      // If the preceding mousedown event or mouseup event was consumed,
      // editor shouldn't handle this click event.
      if (mMouseDownOrUpConsumedByIME) {
        mMouseDownOrUpConsumedByIME = false;
        mouseEvent->PreventDefault();
        return NS_OK;
      }
      return MouseClick(mouseEvent);
    }
    // focus
    case eFocus:
      return Focus(internalEvent->AsFocusEvent());
    // blur
    case eBlur:
      return Blur(internalEvent->AsFocusEvent());
    // text
    case eCompositionChange:
      return HandleChangeComposition(internalEvent->AsCompositionEvent());
    // compositionstart
    case eCompositionStart:
      return HandleStartComposition(internalEvent->AsCompositionEvent());
    // compositionend
    case eCompositionEnd:
      HandleEndComposition(internalEvent->AsCompositionEvent());
      return NS_OK;
    default:
      break;
  }

#ifdef DEBUG
  nsAutoString eventType;
  aEvent->GetType(eventType);
  nsPrintfCString assertMessage("Editor doesn't handle \"%s\" event "
    "because its internal event doesn't have proper message",
    NS_ConvertUTF16toUTF8(eventType).get());
  NS_ASSERTION(false, assertMessage.get());
#endif

  return NS_OK;
}

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH

// This function is borrowed from Chromium's ImeInput::IsCtrlShiftPressed
bool IsCtrlShiftPressed(const WidgetKeyboardEvent* aKeyboardEvent, bool& isRTL)
{
  MOZ_ASSERT(aKeyboardEvent);
  // To check if a user is pressing only a control key and a right-shift key
  // (or a left-shift key), we use the steps below:
  // 1. Check if a user is pressing a control key and a right-shift key (or
  //    a left-shift key).
  // 2. If the condition 1 is true, we should check if there are any other
  //    keys pressed at the same time.
  //    To ignore the keys checked in 1, we set their status to 0 before
  //    checking the key status.

  if (!aKeyboardEvent->IsControl()) {
    return false;
  }

  switch (aKeyboardEvent->mLocation) {
    case eKeyLocationRight:
      isRTL = true;
      break;
    case eKeyLocationLeft:
      isRTL = false;
      break;
    default:
      return false;
  }

  // Scan the key status to find pressed keys. We should abandon changing the
  // text direction when there are other pressed keys.
  if (aKeyboardEvent->IsAlt() || aKeyboardEvent->IsOS()) {
    return false;
  }

  return true;
}

// This logic is mostly borrowed from Chromium's
// RenderWidgetHostViewWin::OnKeyEvent.

nsresult
EditorEventListener::KeyUp(const WidgetKeyboardEvent* aKeyboardEvent)
{
  if (NS_WARN_IF(!aKeyboardEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  if (!mHaveBidiKeyboards) {
    return NS_OK;
  }

  // XXX Why doesn't this method check if it's consumed?
  RefPtr<EditorBase> editorBase(mEditorBase);
  if ((aKeyboardEvent->mKeyCode == NS_VK_SHIFT ||
       aKeyboardEvent->mKeyCode == NS_VK_CONTROL) &&
      mShouldSwitchTextDirection && editorBase->IsPlaintextEditor()) {
    editorBase->SwitchTextDirectionTo(
                  mSwitchToRTL ? EditorBase::TextDirection::eRTL :
                                 EditorBase::TextDirection::eLTR);
    mShouldSwitchTextDirection = false;
  }
  return NS_OK;
}

nsresult
EditorEventListener::KeyDown(const WidgetKeyboardEvent* aKeyboardEvent)
{
  if (NS_WARN_IF(!aKeyboardEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  if (!mHaveBidiKeyboards) {
    return NS_OK;
  }

  // XXX Why isn't this method check if it's consumed?
  if (aKeyboardEvent->mKeyCode == NS_VK_SHIFT) {
    bool switchToRTL;
    if (IsCtrlShiftPressed(aKeyboardEvent, switchToRTL)) {
      mShouldSwitchTextDirection = true;
      mSwitchToRTL = switchToRTL;
    }
  } else if (aKeyboardEvent->mKeyCode != NS_VK_CONTROL) {
    // In case the user presses any other key besides Ctrl and Shift
    mShouldSwitchTextDirection = false;
  }
  return NS_OK;
}

#endif // #ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH

nsresult
EditorEventListener::KeyPress(WidgetKeyboardEvent* aKeyboardEvent)
{
  if (NS_WARN_IF(!aKeyboardEvent)) {
    return NS_OK;
  }

  RefPtr<EditorBase> editorBase(mEditorBase);
  if (!editorBase->IsAcceptableInputEvent(aKeyboardEvent) ||
      DetachedFromEditorOrDefaultPrevented(aKeyboardEvent)) {
    return NS_OK;
  }

  nsresult rv = editorBase->HandleKeyPressEvent(aKeyboardEvent);
  NS_ENSURE_SUCCESS(rv, rv);
  if (DetachedFromEditorOrDefaultPrevented(aKeyboardEvent)) {
    return NS_OK;
  }

  if (!ShouldHandleNativeKeyBindings(aKeyboardEvent)) {
    return NS_OK;
  }

  // Now, ask the native key bindings to handle the event.
  nsIWidget* widget = aKeyboardEvent->mWidget;
  // If the event is created by chrome script, the widget is always nullptr.
  if (!widget) {
    nsCOMPtr<nsIPresShell> ps = GetPresShell();
    nsPresContext* pc = ps ? ps->GetPresContext() : nullptr;
    widget = pc ? pc->GetNearestWidget() : nullptr;
    NS_ENSURE_TRUE(widget, NS_OK);
  }

  nsCOMPtr<nsIDocument> doc = editorBase->GetDocument();

  // WidgetKeyboardEvent::ExecuteEditCommands() requires non-nullptr mWidget.
  // If the event is created by chrome script, it is nullptr but we need to
  // execute native key bindings.  Therefore, we need to set widget to
  // WidgetEvent::mWidget temporarily.
  AutoRestore<nsCOMPtr<nsIWidget>> saveWidget(aKeyboardEvent->mWidget);
  aKeyboardEvent->mWidget = widget;
  if (aKeyboardEvent->ExecuteEditCommands(
                        nsIWidget::NativeKeyBindingsForRichTextEditor,
                        DoCommandCallback, doc)) {
    aKeyboardEvent->PreventDefault();
  }
  return NS_OK;
}

nsresult
EditorEventListener::MouseClick(MouseEvent* aMouseEvent)
{
  if (NS_WARN_IF(!aMouseEvent) || DetachedFromEditor()) {
    return NS_OK;
  }
  // nothing to do if editor isn't editable or clicked on out of the editor.
  RefPtr<EditorBase> editorBase(mEditorBase);
  WidgetMouseEvent* clickEvent =
    aMouseEvent->WidgetEventPtr()->AsMouseEvent();
  if (editorBase->IsReadonly() || editorBase->IsDisabled() ||
      !editorBase->IsAcceptableInputEvent(clickEvent)) {
    return NS_OK;
  }

  // Notifies clicking on editor to IMEStateManager even when the event was
  // consumed.
  if (EditorHasFocus()) {
    nsPresContext* presContext = GetPresContext();
    if (presContext) {
      IMEStateManager::OnClickInEditor(presContext, GetFocusedRootContent(),
                                       clickEvent);
      if (DetachedFromEditor()) {
        return NS_OK;
      }
    }
  }

  if (DetachedFromEditorOrDefaultPrevented(clickEvent)) {
    // We're done if 'preventdefault' is true (see for example bug 70698).
    return NS_OK;
  }

  // If we got a mouse down inside the editing area, we should force the
  // IME to commit before we change the cursor position
  if (!EnsureCommitCompoisition()) {
    return NS_OK;
  }

  if (clickEvent->button == 1) {
    return HandleMiddleClickPaste(aMouseEvent);
  }
  return NS_OK;
}

nsresult
EditorEventListener::HandleMiddleClickPaste(MouseEvent* aMouseEvent)
{
  MOZ_ASSERT(aMouseEvent);

  WidgetMouseEvent* clickEvent =
    aMouseEvent->WidgetEventPtr()->AsMouseEvent();
  MOZ_ASSERT(!DetachedFromEditorOrDefaultPrevented(clickEvent));

  if (!Preferences::GetBool("middlemouse.paste", false)) {
    // Middle click paste isn't enabled.
    return NS_OK;
  }

  // Set the selection to the point under the mouse cursor:
  nsCOMPtr<nsINode> parent = aMouseEvent->GetRangeParent();
  int32_t offset = aMouseEvent->RangeOffset();

  RefPtr<TextEditor> textEditor = mEditorBase->AsTextEditor();
  MOZ_ASSERT(textEditor);

  RefPtr<Selection> selection = textEditor->GetSelection();
  if (selection) {
    selection->Collapse(parent, offset);
  }

  nsresult rv;
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

  // If the ctrl key is pressed, we'll do paste as quotation.
  // Would've used the alt key, but the kde wmgr treats alt-middle specially.
  if (clickEvent->IsControl()) {
    textEditor->PasteAsQuotation(clipboard);
  } else {
    textEditor->Paste(clipboard);
  }

  // Prevent the event from propagating up to be possibly handled
  // again by the containing window:
  clickEvent->StopPropagation();
  clickEvent->PreventDefault();

  // We processed the event, whether drop/paste succeeded or not
  return NS_OK;
}

bool
EditorEventListener::NotifyIMEOfMouseButtonEvent(
                       WidgetMouseEvent* aMouseEvent)
{
  MOZ_ASSERT(aMouseEvent);

  if (!EditorHasFocus()) {
    return false;
  }

  nsPresContext* presContext = GetPresContext();
  NS_ENSURE_TRUE(presContext, false);
  return IMEStateManager::OnMouseButtonEventInEditor(presContext,
                                                     GetFocusedRootContent(),
                                                     aMouseEvent);
}

nsresult
EditorEventListener::MouseDown(MouseEvent* aMouseEvent)
{
  // FYI: We don't need to check if it's already consumed here because
  //      we need to commit composition at mouse button operation.
  // FYI: This may be called by HTMLEditorEventListener::MouseDown() even
  //      when the event is not acceptable for committing composition.
  if (DetachedFromEditor()) {
    return NS_OK;
  }
  Unused << EnsureCommitCompoisition();
  return NS_OK;
}

/**
 * Drag event implementation
 */

nsresult
EditorEventListener::DragEnter(DragEvent* aDragEvent)
{
  if (NS_WARN_IF(!aDragEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_OK);

  if (!mCaret) {
    mCaret = new nsCaret();
    mCaret->Init(presShell);
    mCaret->SetCaretReadOnly(true);
    // This is to avoid the requirement that the Selection is Collapsed which
    // it can't be when dragging a selection in the same shell.
    // See nsCaret::IsVisible().
    mCaret->SetVisibilityDuringSelection(true);
  }

  presShell->SetCaret(mCaret);

  return DragOver(aDragEvent);
}

nsresult
EditorEventListener::DragOver(DragEvent* aDragEvent)
{
  if (NS_WARN_IF(!aDragEvent) ||
      DetachedFromEditorOrDefaultPrevented(
        aDragEvent->WidgetEventPtr())) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> parent = aDragEvent->GetRangeParent();
  nsCOMPtr<nsIContent> dropParent = do_QueryInterface(parent);
  NS_ENSURE_TRUE(dropParent, NS_ERROR_FAILURE);

  if (dropParent->IsEditable() && CanDrop(aDragEvent)) {
    aDragEvent->PreventDefault(); // consumed

    if (!mCaret) {
      return NS_OK;
    }

    int32_t offset = aDragEvent->RangeOffset();

    mCaret->SetVisible(true);
    mCaret->SetCaretPosition(dropParent, offset);

    return NS_OK;
  }

  if (!IsFileControlTextBox()) {
    // This is needed when dropping on an input, to prevent the editor for
    // the editable parent from receiving the event.
    aDragEvent->StopPropagation();
  }

  if (mCaret) {
    mCaret->SetVisible(false);
  }
  return NS_OK;
}

void
EditorEventListener::CleanupDragDropCaret()
{
  if (!mCaret) {
    return;
  }

  mCaret->SetVisible(false);    // hide it, so that it turns off its timer

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (presShell) {
    presShell->RestoreCaret();
  }

  mCaret->Terminate();
  mCaret = nullptr;
}

nsresult
EditorEventListener::DragExit(DragEvent* aDragEvent)
{
  // XXX If aDragEvent was created by chrome script, its defaultPrevented
  //     may be true, though.  We shouldn't handle such event but we don't
  //     have a way to distinguish if coming event is created by chrome script.
  NS_WARNING_ASSERTION(
    !aDragEvent->WidgetEventPtr()->DefaultPrevented(),
    "eDragExit shouldn't be cancelable");
  if (NS_WARN_IF(!aDragEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  CleanupDragDropCaret();

  return NS_OK;
}

nsresult
EditorEventListener::Drop(DragEvent* aDragEvent)
{
  CleanupDragDropCaret();

  if (NS_WARN_IF(!aDragEvent) ||
      DetachedFromEditorOrDefaultPrevented(
        aDragEvent->WidgetEventPtr())) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> parent = aDragEvent->GetRangeParent();
  nsCOMPtr<nsIContent> dropParent = do_QueryInterface(parent);
  NS_ENSURE_TRUE(dropParent, NS_ERROR_FAILURE);

  if (!dropParent->IsEditable() || !CanDrop(aDragEvent)) {
    // was it because we're read-only?
    RefPtr<EditorBase> editorBase(mEditorBase);
    if ((editorBase->IsReadonly() || editorBase->IsDisabled()) &&
        !IsFileControlTextBox()) {
      // it was decided to "eat" the event as this is the "least surprise"
      // since someone else handling it might be unintentional and the
      // user could probably re-drag to be not over the disabled/readonly
      // editfields if that is what is desired.
      aDragEvent->StopPropagation();
    }
    return NS_OK;
  }

  aDragEvent->StopPropagation();
  aDragEvent->PreventDefault();
  RefPtr<TextEditor> textEditor = mEditorBase->AsTextEditor();
  return textEditor->OnDrop(aDragEvent);
}

bool
EditorEventListener::CanDrop(DragEvent* aEvent)
{
  MOZ_ASSERT(!DetachedFromEditorOrDefaultPrevented(
                aEvent->WidgetEventPtr()));

  // if the target doc is read-only, we can't drop
  RefPtr<EditorBase> editorBase(mEditorBase);
  if (editorBase->IsReadonly() || editorBase->IsDisabled()) {
    return false;
  }

  RefPtr<DataTransfer> dataTransfer = aEvent->GetDataTransfer();
  NS_ENSURE_TRUE(dataTransfer, false);

  nsTArray<nsString> types;
  dataTransfer->GetTypes(types, CallerType::System);

  // Plaintext editors only support dropping text. Otherwise, HTML and files
  // can be dropped as well.
  if (!types.Contains(NS_LITERAL_STRING(kTextMime)) &&
      !types.Contains(NS_LITERAL_STRING(kMozTextInternal)) &&
      (editorBase->IsPlaintextEditor() ||
       (!types.Contains(NS_LITERAL_STRING(kHTMLMime)) &&
        !types.Contains(NS_LITERAL_STRING(kFileMime))))) {
    return false;
  }

  // If there is no source node, this is probably an external drag and the
  // drop is allowed. The later checks rely on checking if the drag target
  // is the same as the drag source.
  nsCOMPtr<nsINode> sourceNode = dataTransfer->GetMozSourceNode();
  if (!sourceNode) {
    return true;
  }

  // There is a source node, so compare the source documents and this document.
  // Disallow drops on the same document.

  nsCOMPtr<nsIDocument> domdoc = editorBase->GetDocument();
  NS_ENSURE_TRUE(domdoc, false);

  nsCOMPtr<nsIDocument> sourceDoc = sourceNode->OwnerDoc();

  // If the source and the dest are not same document, allow to drop it always.
  if (domdoc != sourceDoc) {
    return true;
  }

  // If the source node is a remote browser, treat this as coming from a
  // different document and allow the drop.
  nsCOMPtr<nsIContent> sourceContent = do_QueryInterface(sourceNode);
  TabParent* tp = TabParent::GetFrom(sourceContent);
  if (tp) {
    return true;
  }

  RefPtr<Selection> selection = editorBase->GetSelection();
  if (!selection) {
    return false;
  }

  // If selection is collapsed, allow to drop it always.
  if (selection->IsCollapsed()) {
    return true;
  }

  nsCOMPtr<nsINode> parent = aEvent->GetRangeParent();
  if (!parent) {
    return false;
  }

  int32_t offset = aEvent->RangeOffset();

  uint32_t rangeCount = selection->RangeCount();
  for (uint32_t i = 0; i < rangeCount; i++) {
    RefPtr<nsRange> range = selection->GetRangeAt(i);
    if (!range) {
      // Don't bail yet, iterate through them all
      continue;
    }

    IgnoredErrorResult rv;
    bool inRange = range->IsPointInRange(*parent, offset, rv);
    if (!rv.Failed() && inRange) {
      // Okay, now you can bail, we are over the orginal selection
      return false;
    }
  }
  return true;
}

nsresult
EditorEventListener::HandleStartComposition(
                       WidgetCompositionEvent* aCompositionStartEvent)
{
  if (NS_WARN_IF(!aCompositionStartEvent)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<EditorBase> editorBase(mEditorBase);
  if (DetachedFromEditor() ||
      !editorBase->IsAcceptableInputEvent(aCompositionStartEvent)) {
    return NS_OK;
  }
  // Although, "compositionstart" should be cancelable, but currently,
  // eCompositionStart event coming from widget is not cancelable.
  MOZ_ASSERT(!aCompositionStartEvent->DefaultPrevented(),
             "eCompositionStart shouldn't be cancelable");
  TextEditor* textEditor = editorBase->AsTextEditor();
  return textEditor->OnCompositionStart(*aCompositionStartEvent);
}

nsresult
EditorEventListener::HandleChangeComposition(
                       WidgetCompositionEvent* aCompositionChangeEvent)
{
  if (NS_WARN_IF(!aCompositionChangeEvent)) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(!aCompositionChangeEvent->DefaultPrevented(),
             "eCompositionChange event shouldn't be cancelable");
  RefPtr<EditorBase> editorBase(mEditorBase);
  if (DetachedFromEditor() ||
      !editorBase->IsAcceptableInputEvent(aCompositionChangeEvent)) {
    return NS_OK;
  }

  // if we are readonly or disabled, then do nothing.
  if (editorBase->IsReadonly() || editorBase->IsDisabled()) {
    return NS_OK;
  }

  TextEditor* textEditor = editorBase->AsTextEditor();
  return textEditor->OnCompositionChange(*aCompositionChangeEvent);
}

void
EditorEventListener::HandleEndComposition(
                       WidgetCompositionEvent* aCompositionEndEvent)
{
  if (NS_WARN_IF(!aCompositionEndEvent)) {
    return;
  }
  RefPtr<EditorBase> editorBase(mEditorBase);
  if (DetachedFromEditor() ||
      !editorBase->IsAcceptableInputEvent(aCompositionEndEvent)) {
    return;
  }
  MOZ_ASSERT(!aCompositionEndEvent->DefaultPrevented(),
             "eCompositionEnd shouldn't be cancelable");

  TextEditor* textEditor = editorBase->AsTextEditor();
  textEditor->OnCompositionEnd(*aCompositionEndEvent);
}

nsresult
EditorEventListener::Focus(InternalFocusEvent* aFocusEvent)
{
  if (NS_WARN_IF(!aFocusEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  // Don't turn on selection and caret when the editor is disabled.
  RefPtr<EditorBase> editorBase(mEditorBase);
  if (editorBase->IsDisabled()) {
    return NS_OK;
  }

  // Spell check a textarea the first time that it is focused.
  SpellCheckIfNeeded();
  if (!editorBase) {
    // In e10s, this can cause us to flush notifications, which can destroy
    // the node we're about to focus.
    return NS_OK;
  }

  EventTarget* target = aFocusEvent->GetOriginalDOMEventTarget();
  nsCOMPtr<nsINode> node = do_QueryInterface(target);
  NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

  // If the target is a document node but it's not editable, we should ignore
  // it because actual focused element's event is going to come.
  if (node->IsDocument() && !node->HasFlag(NODE_IS_EDITABLE)) {
    return NS_OK;
  }

  if (node->IsContent()) {
    nsIContent* content =
      node->AsContent()->FindFirstNonChromeOnlyAccessContent();
    // XXX If the focus event target is a form control in contenteditable
    // element, perhaps, the parent HTML editor should do nothing by this
    // handler.  However, FindSelectionRoot() returns the root element of the
    // contenteditable editor.  So, the editableRoot value is invalid for
    // the plain text editor, and it will be set to the wrong limiter of
    // the selection.  However, fortunately, actual bugs are not found yet.
    nsCOMPtr<nsIContent> editableRoot = editorBase->FindSelectionRoot(content);

    // make sure that the element is really focused in case an earlier
    // listener in the chain changed the focus.
    if (editableRoot) {
      nsFocusManager* fm = nsFocusManager::GetFocusManager();
      NS_ENSURE_TRUE(fm, NS_OK);

      nsIContent* focusedContent = fm->GetFocusedElement();
      if (!focusedContent) {
        return NS_OK;
      }

      nsCOMPtr<nsIContent> originalTargetAsContent =
        do_QueryInterface(aFocusEvent->GetOriginalDOMEventTarget());

      if (!SameCOMIdentity(
            focusedContent->FindFirstNonChromeOnlyAccessContent(),
            originalTargetAsContent->FindFirstNonChromeOnlyAccessContent())) {
        return NS_OK;
      }
    }
  }

  editorBase->OnFocus(target);
  if (DetachedFromEditorOrDefaultPrevented(aFocusEvent)) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_OK);
  nsCOMPtr<nsIContent> focusedContent = editorBase->GetFocusedContentForIME();
  IMEStateManager::OnFocusInEditor(ps->GetPresContext(), focusedContent,
                                   *editorBase);

  return NS_OK;
}

nsresult
EditorEventListener::Blur(InternalFocusEvent* aBlurEvent)
{
  if (NS_WARN_IF(!aBlurEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  // check if something else is focused. If another element is focused, then
  // we should not change the selection.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, NS_OK);

  Element* focusedElement = fm->GetFocusedElement();
  if (!focusedElement) {
    RefPtr<EditorBase> editorBase(mEditorBase);
    editorBase->FinalizeSelection();
  }
  return NS_OK;
}

void
EditorEventListener::SpellCheckIfNeeded()
{
  MOZ_ASSERT(!DetachedFromEditor());

  // If the spell check skip flag is still enabled from creation time,
  // disable it because focused editors are allowed to spell check.
  RefPtr<EditorBase> editorBase(mEditorBase);
  if(editorBase->ShouldSkipSpellCheck()) {
    editorBase->RemoveFlags(nsIPlaintextEditor::eEditorSkipSpellCheck);
  }
}

bool
EditorEventListener::IsFileControlTextBox()
{
  MOZ_ASSERT(!DetachedFromEditor());

  RefPtr<EditorBase> editorBase(mEditorBase);
  Element* root = editorBase->GetRoot();
  if (!root || !root->ChromeOnlyAccess()) {
    return false;
  }
  nsIContent* parent = root->FindFirstNonChromeOnlyAccessContent();
  if (!parent || !parent->IsHTMLElement(nsGkAtoms::input)) {
    return false;
  }
  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(parent);
  return formControl->ControlType() == NS_FORM_INPUT_FILE;
}

bool
EditorEventListener::ShouldHandleNativeKeyBindings(
                       WidgetKeyboardEvent* aKeyboardEvent)
{
  MOZ_ASSERT(!DetachedFromEditor());

  // Only return true if the target of the event is a desendant of the active
  // editing host in order to match the similar decision made in
  // nsXBLWindowKeyHandler.
  // Note that IsAcceptableInputEvent doesn't check for the active editing
  // host for keyboard events, otherwise this check would have been
  // unnecessary.  IsAcceptableInputEvent currently makes a similar check for
  // mouse events.

  nsCOMPtr<nsIContent> targetContent =
    do_QueryInterface(aKeyboardEvent->GetDOMEventTarget());
  if (!targetContent) {
    return false;
  }

  RefPtr<EditorBase> editorBase(mEditorBase);
  HTMLEditor* htmlEditor = editorBase->AsHTMLEditor();
  if (!htmlEditor) {
    return false;
  }

  nsCOMPtr<nsIDocument> doc = editorBase->GetDocument();
  if (doc->HasFlag(NODE_IS_EDITABLE)) {
    // Don't need to perform any checks in designMode documents.
    return true;
  }

  nsIContent* editingHost = htmlEditor->GetActiveEditingHost();
  if (!editingHost) {
    return false;
  }

  return nsContentUtils::ContentIsDescendantOf(targetContent, editingHost);
}

} // namespace mozilla
