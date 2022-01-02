/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorEventListener.h"

#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc.
#include "mozilla/AutoRestore.h"
#include "mozilla/ContentEvents.h"         // for InternalFocusEvent
#include "mozilla/EditorBase.h"            // for EditorBase, etc.
#include "mozilla/EventListenerManager.h"  // for EventListenerManager
#include "mozilla/EventStateManager.h"     // for EventStateManager
#include "mozilla/HTMLEditor.h"            // for HTMLEditor
#include "mozilla/IMEStateManager.h"       // for IMEStateManager
#include "mozilla/Preferences.h"           // for Preferences
#include "mozilla/PresShell.h"             // for PresShell
#include "mozilla/TextEditor.h"            // for TextEditor
#include "mozilla/TextEvents.h"            // for WidgetCompositionEvent
#include "mozilla/dom/Element.h"           // for Element
#include "mozilla/dom/Event.h"             // for Event
#include "mozilla/dom/EventTarget.h"       // for EventTarget
#include "mozilla/dom/MouseEvent.h"        // for MouseEvent
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsCaret.h"            // for nsCaret
#include "nsDebug.h"            // for NS_WARNING, etc.
#include "nsFocusManager.h"     // for nsFocusManager
#include "nsGkAtoms.h"          // for nsGkAtoms, nsGkAtoms::input
#include "nsIContent.h"         // for nsIContent
#include "nsIContentInlines.h"  // for nsINode::IsInDesignMode()
#include "nsIController.h"      // for nsIController
#include "nsID.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DragEvent.h"
#include "mozilla/dom/Document.h"  // for Document
#include "nsIFormControl.h"        // for nsIFormControl, etc.
#include "nsINode.h"               // for nsINode, etc.
#include "nsIWidget.h"             // for nsIWidget
#include "nsLiteralString.h"       // for NS_LITERAL_STRING
#include "nsPIWindowRoot.h"        // for nsPIWindowRoot
#include "nsPrintfCString.h"       // for nsPrintfCString
#include "nsRange.h"
#include "nsServiceManagerUtils.h"  // for do_GetService
#include "nsString.h"               // for nsAutoString
#include "nsQueryObject.h"          // for do_QueryObject
#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
#  include "nsContentUtils.h"   // for nsContentUtils, etc.
#  include "nsIBidiKeyboard.h"  // for nsIBidiKeyboard
#endif

#include "mozilla/dom/BrowserParent.h"

class nsPresContext;

namespace mozilla {

using namespace dom;

MOZ_CAN_RUN_SCRIPT static void DoCommandCallback(Command aCommand,
                                                 void* aData) {
  Document* doc = static_cast<Document*>(aData);
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
  if (NS_WARN_IF(NS_FAILED(
          controller->IsCommandEnabled(commandStr, &commandEnabled)))) {
    return;
  }
  if (commandEnabled) {
    controller->DoCommand(commandStr);
  }
}

EditorEventListener::EditorEventListener()
    : mEditorBase(nullptr),
      mCommitText(false),
      mInTransaction(false),
      mMouseDownOrUpConsumedByIME(false)
#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
      ,
      mHaveBidiKeyboards(false),
      mShouldSwitchTextDirection(false),
      mSwitchToRTL(false)
#endif
{
}

EditorEventListener::~EditorEventListener() {
  if (mEditorBase) {
    NS_WARNING("We've not been uninstalled yet");
    Disconnect();
  }
}

nsresult EditorEventListener::Connect(EditorBase* aEditorBase) {
  if (NS_WARN_IF(!aEditorBase)) {
    return NS_ERROR_INVALID_ARG;
  }

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
    NS_WARNING("EditorEventListener::InstallToEditor() failed");
    Disconnect();
  }
  return rv;
}

nsresult EditorEventListener::InstallToEditor() {
  MOZ_ASSERT(mEditorBase, "The caller must set mEditorBase");

  EventTarget* eventTarget = mEditorBase->GetDOMEventTarget();
  if (NS_WARN_IF(!eventTarget)) {
    return NS_ERROR_FAILURE;
  }

  // register the event listeners with the listener manager
  EventListenerManager* eventListenerManager =
      eventTarget->GetOrCreateListenerManager();
  if (NS_WARN_IF(!eventListenerManager)) {
    return NS_ERROR_FAILURE;
  }

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  eventListenerManager->AddEventListenerByType(
      this, u"keydown"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->AddEventListenerByType(
      this, u"keyup"_ns, TrustedEventsAtSystemGroupBubble());
#endif
  eventListenerManager->AddEventListenerByType(
      this, u"keypress"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->AddEventListenerByType(
      this, u"dragover"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->AddEventListenerByType(
      this, u"dragleave"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->AddEventListenerByType(
      this, u"drop"_ns, TrustedEventsAtSystemGroupBubble());
  // XXX We should add the mouse event listeners as system event group.
  //     E.g., web applications cannot prevent middle mouse paste by
  //     preventDefault() of click event at bubble phase.
  //     However, if we do so, all click handlers in any frames and frontend
  //     code need to check if it's editable.  It makes easier create new bugs.
  eventListenerManager->AddEventListenerByType(this, u"mousedown"_ns,
                                               TrustedEventsAtCapture());
  eventListenerManager->AddEventListenerByType(this, u"mouseup"_ns,
                                               TrustedEventsAtCapture());
  eventListenerManager->AddEventListenerByType(this, u"click"_ns,
                                               TrustedEventsAtCapture());
  eventListenerManager->AddEventListenerByType(
      this, u"auxclick"_ns, TrustedEventsAtSystemGroupCapture());
  // Focus event doesn't bubble so adding the listener to capturing phase as
  // system event group.
  eventListenerManager->AddEventListenerByType(
      this, u"blur"_ns, TrustedEventsAtSystemGroupCapture());
  eventListenerManager->AddEventListenerByType(
      this, u"focus"_ns, TrustedEventsAtSystemGroupCapture());
  eventListenerManager->AddEventListenerByType(
      this, u"text"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->AddEventListenerByType(
      this, u"compositionstart"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->AddEventListenerByType(
      this, u"compositionend"_ns, TrustedEventsAtSystemGroupBubble());

  return NS_OK;
}

void EditorEventListener::Disconnect() {
  if (DetachedFromEditor()) {
    return;
  }
  UninstallFromEditor();

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    nsIContent* focusedContent = fm->GetFocusedElement();
    mozilla::dom::Element* root = mEditorBase->GetRoot();
    if (focusedContent && root &&
        focusedContent->IsInclusiveDescendantOf(root)) {
      // Reset the Selection ancestor limiter and SelectionController state
      // that EditorBase::InitializeSelection set up.
      DebugOnly<nsresult> rvIgnored = mEditorBase->FinalizeSelection();
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::FinalizeSelection() failed, but ignored");
    }
  }

  mEditorBase = nullptr;
}

void EditorEventListener::UninstallFromEditor() {
  CleanupDragDropCaret();

  EventTarget* eventTarget = mEditorBase->GetDOMEventTarget();
  if (NS_WARN_IF(!eventTarget)) {
    return;
  }

  EventListenerManager* eventListenerManager =
      eventTarget->GetOrCreateListenerManager();
  if (NS_WARN_IF(!eventListenerManager)) {
    return;
  }

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  eventListenerManager->RemoveEventListenerByType(
      this, u"keydown"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->RemoveEventListenerByType(
      this, u"keyup"_ns, TrustedEventsAtSystemGroupBubble());
#endif
  eventListenerManager->RemoveEventListenerByType(
      this, u"keypress"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->RemoveEventListenerByType(
      this, u"dragover"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->RemoveEventListenerByType(
      this, u"dragleave"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->RemoveEventListenerByType(
      this, u"drop"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->RemoveEventListenerByType(this, u"mousedown"_ns,
                                                  TrustedEventsAtCapture());
  eventListenerManager->RemoveEventListenerByType(this, u"mouseup"_ns,
                                                  TrustedEventsAtCapture());
  eventListenerManager->RemoveEventListenerByType(this, u"click"_ns,
                                                  TrustedEventsAtCapture());
  eventListenerManager->RemoveEventListenerByType(
      this, u"auxclick"_ns, TrustedEventsAtSystemGroupCapture());
  eventListenerManager->RemoveEventListenerByType(
      this, u"blur"_ns, TrustedEventsAtSystemGroupCapture());
  eventListenerManager->RemoveEventListenerByType(
      this, u"focus"_ns, TrustedEventsAtSystemGroupCapture());
  eventListenerManager->RemoveEventListenerByType(
      this, u"text"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->RemoveEventListenerByType(
      this, u"compositionstart"_ns, TrustedEventsAtSystemGroupBubble());
  eventListenerManager->RemoveEventListenerByType(
      this, u"compositionend"_ns, TrustedEventsAtSystemGroupBubble());
}

PresShell* EditorEventListener::GetPresShell() const {
  MOZ_ASSERT(!DetachedFromEditor());
  return mEditorBase->GetPresShell();
}

nsPresContext* EditorEventListener::GetPresContext() const {
  PresShell* presShell = GetPresShell();
  return presShell ? presShell->GetPresContext() : nullptr;
}

bool EditorEventListener::EditorHasFocus() {
  MOZ_ASSERT(!DetachedFromEditor());
  nsCOMPtr<nsIContent> focusedContent = mEditorBase->GetFocusedContent();
  if (!focusedContent) {
    return false;
  }
  return !!focusedContent->GetComposedDoc();
}

NS_IMPL_ISUPPORTS(EditorEventListener, nsIDOMEventListener)

bool EditorEventListener::DetachedFromEditor() const { return !mEditorBase; }

bool EditorEventListener::DetachedFromEditorOrDefaultPrevented(
    WidgetEvent* aWidgetEvent) const {
  return NS_WARN_IF(!aWidgetEvent) || DetachedFromEditor() ||
         aWidgetEvent->DefaultPrevented();
}

bool EditorEventListener::EnsureCommitComposition() {
  MOZ_ASSERT(!DetachedFromEditor());
  RefPtr<EditorBase> editorBase(mEditorBase);
  editorBase->CommitComposition();
  return !DetachedFromEditor();
}

NS_IMETHODIMP EditorEventListener::HandleEvent(Event* aEvent) {
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
    // dragover and drop
    case eDragOver:
    case eDrop: {
      // aEvent should be grabbed by the caller since this is
      // nsIDOMEventListener method.  However, our clang plugin cannot check it
      // if we use Event::As*Event().  So, we need to grab it by ourselves.
      RefPtr<DragEvent> dragEvent = aEvent->AsDragEvent();
      nsresult rv = DragOverOrDrop(dragEvent);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::DragOverOrDrop() failed");
      return rv;
    }
    // DragLeave
    case eDragLeave: {
      RefPtr<DragEvent> dragEvent = aEvent->AsDragEvent();
      nsresult rv = DragLeave(dragEvent);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::DragLeave() failed");
      return rv;
    }
#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
    // keydown
    case eKeyDown: {
      nsresult rv = KeyDown(internalEvent->AsKeyboardEvent());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::KeyDown() failed");
      return rv;
    }
    // keyup
    case eKeyUp: {
      nsresult rv = KeyUp(internalEvent->AsKeyboardEvent());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::KeyUp() failed");
      return rv;
    }
#endif  // #ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
    // keypress
    case eKeyPress: {
      nsresult rv = KeyPress(internalEvent->AsKeyboardEvent());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::KeyPress() failed");
      return rv;
    }
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
      RefPtr<MouseEvent> mouseEvent = aEvent->AsMouseEvent();
      if (NS_WARN_IF(!mouseEvent)) {
        return NS_OK;
      }
      nsresult rv = MouseDown(mouseEvent);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::MouseDown() failed");
      return rv;
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
      RefPtr<MouseEvent> mouseEvent = aEvent->AsMouseEvent();
      if (NS_WARN_IF(!mouseEvent)) {
        return NS_OK;
      }
      nsresult rv = MouseUp(mouseEvent);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::MouseUp() failed");
      return rv;
    }
    // click
    case eMouseClick: {
      WidgetMouseEvent* widgetMouseEvent = internalEvent->AsMouseEvent();
      // Don't handle non-primary click events
      if (widgetMouseEvent->mButton != MouseButton::ePrimary) {
        return NS_OK;
      }
      [[fallthrough]];
    }
    // auxclick
    case eMouseAuxClick: {
      WidgetMouseEvent* widgetMouseEvent = internalEvent->AsMouseEvent();
      if (NS_WARN_IF(!widgetMouseEvent)) {
        return NS_OK;
      }
      // If the preceding mousedown event or mouseup event was consumed,
      // editor shouldn't handle this click event.
      if (mMouseDownOrUpConsumedByIME) {
        mMouseDownOrUpConsumedByIME = false;
        widgetMouseEvent->PreventDefault();
        return NS_OK;
      }
      nsresult rv = MouseClick(widgetMouseEvent);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::MouseClick() failed");
      return rv;
    }
    // focus
    case eFocus: {
      nsresult rv = Focus(internalEvent->AsFocusEvent());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::Focus() failed");
      return rv;
    }
    // blur
    case eBlur: {
      nsresult rv = Blur(internalEvent->AsFocusEvent());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::Blur() failed");
      return rv;
    }
    // text
    case eCompositionChange: {
      nsresult rv =
          HandleChangeComposition(internalEvent->AsCompositionEvent());
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "EditorEventListener::HandleChangeComposition() failed");
      return rv;
    }
    // compositionstart
    case eCompositionStart: {
      nsresult rv = HandleStartComposition(internalEvent->AsCompositionEvent());
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "EditorEventListener::HandleStartComposition() failed");
      return rv;
    }
    // compositionend
    case eCompositionEnd: {
      HandleEndComposition(internalEvent->AsCompositionEvent());
      return NS_OK;
    }
    default:
      break;
  }

#ifdef DEBUG
  nsAutoString eventType;
  aEvent->GetType(eventType);
  nsPrintfCString assertMessage(
      "Editor doesn't handle \"%s\" event "
      "because its internal event doesn't have proper message",
      NS_ConvertUTF16toUTF8(eventType).get());
  NS_ASSERTION(false, assertMessage.get());
#endif

  return NS_OK;
}

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH

// This function is borrowed from Chromium's ImeInput::IsCtrlShiftPressed
bool IsCtrlShiftPressed(const WidgetKeyboardEvent* aKeyboardEvent,
                        bool& isRTL) {
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

nsresult EditorEventListener::KeyUp(const WidgetKeyboardEvent* aKeyboardEvent) {
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
      mShouldSwitchTextDirection && editorBase->IsInPlaintextMode()) {
    editorBase->SwitchTextDirectionTo(mSwitchToRTL
                                          ? EditorBase::TextDirection::eRTL
                                          : EditorBase::TextDirection::eLTR);
    mShouldSwitchTextDirection = false;
  }
  return NS_OK;
}

nsresult EditorEventListener::KeyDown(
    const WidgetKeyboardEvent* aKeyboardEvent) {
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

#endif  // #ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH

nsresult EditorEventListener::KeyPress(WidgetKeyboardEvent* aKeyboardEvent) {
  if (NS_WARN_IF(!aKeyboardEvent)) {
    return NS_OK;
  }

  RefPtr<EditorBase> editorBase(mEditorBase);
  if (!editorBase->IsAcceptableInputEvent(aKeyboardEvent) ||
      DetachedFromEditorOrDefaultPrevented(aKeyboardEvent)) {
    return NS_OK;
  }

  // The exposed root of our editor may have been hidden or destroyed by a
  // preceding event listener.  However, the destruction has not occurred yet if
  // pending notifications have not been flushed yet.  Therefore, before
  // handling user input, we need to get the latest state and if it's now
  // destroyed with the flushing, we should just ignore this event instead of
  // returning error since this is just a event listener.
  RefPtr<Document> document = editorBase->GetDocument();
  if (!document) {
    return NS_OK;
  }
  document->FlushPendingNotifications(FlushType::Layout);
  if (editorBase->Destroyed() || DetachedFromEditor()) {
    return NS_OK;
  }

  nsresult rv = editorBase->HandleKeyPressEvent(aKeyboardEvent);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::HandleKeyPressEvent() failed");
    return rv;
  }
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
    nsPresContext* presContext = GetPresContext();
    if (NS_WARN_IF(!presContext)) {
      return NS_OK;
    }
    widget = presContext->GetNearestWidget();
    if (NS_WARN_IF(!widget)) {
      return NS_OK;
    }
  }

  RefPtr<Document> doc = editorBase->GetDocument();

  // WidgetKeyboardEvent::ExecuteEditCommands() requires non-nullptr mWidget.
  // If the event is created by chrome script, it is nullptr but we need to
  // execute native key bindings.  Therefore, we need to set widget to
  // WidgetEvent::mWidget temporarily.
  AutoRestore<nsCOMPtr<nsIWidget>> saveWidget(aKeyboardEvent->mWidget);
  aKeyboardEvent->mWidget = widget;
  if (aKeyboardEvent->ExecuteEditCommands(
          nsIWidget::NativeKeyBindingsForRichTextEditor, DoCommandCallback,
          doc)) {
    aKeyboardEvent->PreventDefault();
  }
  return NS_OK;
}

nsresult EditorEventListener::MouseClick(WidgetMouseEvent* aMouseClickEvent) {
  if (NS_WARN_IF(!aMouseClickEvent) || DetachedFromEditor()) {
    return NS_OK;
  }
  // nothing to do if editor isn't editable or clicked on out of the editor.
  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  if (editorBase->IsReadonly() ||
      !editorBase->IsAcceptableInputEvent(aMouseClickEvent)) {
    return NS_OK;
  }

  // Notifies clicking on editor to IMEStateManager even when the event was
  // consumed.
  if (EditorHasFocus()) {
    RefPtr<nsPresContext> presContext = GetPresContext();
    if (presContext) {
      nsCOMPtr<nsIContent> focusedContent = mEditorBase->GetFocusedContent();
      IMEStateManager::OnClickInEditor(presContext, focusedContent,
                                       aMouseClickEvent);
      if (DetachedFromEditor()) {
        return NS_OK;
      }
    }
  }

  if (DetachedFromEditorOrDefaultPrevented(aMouseClickEvent)) {
    // We're done if 'preventdefault' is true (see for example bug 70698).
    return NS_OK;
  }

  // If we got a mouse down inside the editing area, we should force the
  // IME to commit before we change the cursor position.
  if (!EnsureCommitComposition()) {
    return NS_OK;
  }

  // XXX The following code is hack for our buggy "click" and "auxclick"
  //     implementation.  "auxclick" event was added recently, however,
  //     any non-primary button click event handlers in our UI still keep
  //     listening to "click" events.  Additionally, "auxclick" event is
  //     fired after "click" events and even if we do this in the system event
  //     group, middle click opens new tab before us.  Therefore, we need to
  //     handle middle click at capturing phase of the default group even
  //     though this makes web apps cannot prevent middle click paste with
  //     calling preventDefault() of "click" nor "auxclick".

  if (aMouseClickEvent->mButton != MouseButton::eMiddle ||
      !WidgetMouseEvent::IsMiddleClickPasteEnabled()) {
    return NS_OK;
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return NS_OK;
  }
  nsPresContext* presContext = GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return NS_OK;
  }
  MOZ_ASSERT(!aMouseClickEvent->DefaultPrevented());
  nsEventStatus status = nsEventStatus_eIgnore;
  RefPtr<EventStateManager> esm = presContext->EventStateManager();
  DebugOnly<nsresult> rvIgnored = esm->HandleMiddleClickPaste(
      presShell, aMouseClickEvent, &status, editorBase);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EventStateManager::HandleMiddleClickPaste() failed, but ignored");
  if (status == nsEventStatus_eConsumeNoDefault) {
    // We no longer need to StopImmediatePropagation here since
    // ClickHandlerChild.jsm checks for and ignores editables, so won't
    // re-handle the event
    aMouseClickEvent->PreventDefault();
  }
  return NS_OK;
}

bool EditorEventListener::NotifyIMEOfMouseButtonEvent(
    WidgetMouseEvent* aMouseEvent) {
  MOZ_ASSERT(aMouseEvent);

  if (!EditorHasFocus()) {
    return false;
  }

  RefPtr<nsPresContext> presContext = GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return false;
  }
  nsCOMPtr<nsIContent> focusedContent = mEditorBase->GetFocusedContent();
  return IMEStateManager::OnMouseButtonEventInEditor(
      presContext, focusedContent, aMouseEvent);
}

nsresult EditorEventListener::MouseDown(MouseEvent* aMouseEvent) {
  // FYI: We don't need to check if it's already consumed here because
  //      we need to commit composition at mouse button operation.
  // FYI: This may be called by HTMLEditorEventListener::MouseDown() even
  //      when the event is not acceptable for committing composition.
  if (DetachedFromEditor()) {
    return NS_OK;
  }
  Unused << EnsureCommitComposition();
  return NS_OK;
}

/**
 * Drag event implementation
 */

void EditorEventListener::RefuseToDropAndHideCaret(DragEvent* aDragEvent) {
  MOZ_ASSERT(aDragEvent->WidgetEventPtr()->mFlags.mInSystemGroup);

  aDragEvent->PreventDefault();
  aDragEvent->StopImmediatePropagation();
  DataTransfer* dataTransfer = aDragEvent->GetDataTransfer();
  if (dataTransfer) {
    dataTransfer->SetDropEffectInt(nsIDragService::DRAGDROP_ACTION_NONE);
  }
  if (mCaret) {
    mCaret->SetVisible(false);
  }
}

nsresult EditorEventListener::DragOverOrDrop(DragEvent* aDragEvent) {
  MOZ_ASSERT(aDragEvent);
  MOZ_ASSERT(aDragEvent->WidgetEventPtr()->mMessage == eDrop ||
             aDragEvent->WidgetEventPtr()->mMessage == eDragOver);

  if (aDragEvent->WidgetEventPtr()->mMessage == eDrop) {
    CleanupDragDropCaret();
    MOZ_ASSERT(!mCaret);
  } else {
    InitializeDragDropCaret();
    MOZ_ASSERT(mCaret);
  }

  if (DetachedFromEditorOrDefaultPrevented(aDragEvent->WidgetEventPtr())) {
    return NS_OK;
  }

  int32_t dropOffset = -1;
  nsCOMPtr<nsIContent> dropParentContent =
      aDragEvent->GetRangeParentContentAndOffset(&dropOffset);
  if (NS_WARN_IF(!dropParentContent) || NS_WARN_IF(dropOffset < 0)) {
    return NS_ERROR_FAILURE;
  }
  if (DetachedFromEditor()) {
    RefuseToDropAndHideCaret(aDragEvent);
    return NS_OK;
  }

  bool notEditable =
      !dropParentContent->IsEditable() || mEditorBase->IsReadonly();

  // First of all, hide caret if we won't insert the drop data into the editor
  // obviously.
  if (mCaret && (IsFileControlTextBox() || notEditable)) {
    mCaret->SetVisible(false);
  }

  // If we're a native anonymous <input> element in <input type="file">,
  // we don't need to handle the drop.
  if (IsFileControlTextBox()) {
    return NS_OK;
  }

  // If the drop target isn't ediable, the drop should be handled by the
  // element.
  if (notEditable) {
    // If we're a text control element which is readonly or disabled,
    // we should refuse to drop.
    if (mEditorBase->IsTextEditor()) {
      RefuseToDropAndHideCaret(aDragEvent);
      return NS_OK;
    }
    // Otherwise, we shouldn't handle the drop.
    return NS_OK;
  }

  // If the drag event does not have any data which we can handle, we should
  // refuse to drop even if some parents can handle it because user may be
  // trying to drop it on us, not our parent.  For example, users don't want
  // to drop HTML data to parent contenteditable element if they drop it on
  // a child <input> element.
  if (!DragEventHasSupportingData(aDragEvent)) {
    RefuseToDropAndHideCaret(aDragEvent);
    return NS_OK;
  }

  // If we don't accept the data drop at the point, for example, while dragging
  // selection, it's not allowed dropping on its selection ranges. In this
  // case, any parents shouldn't handle the drop instead of us, for example,
  // dropping text shouldn't be treated as URL and load new page.
  if (!CanInsertAtDropPosition(aDragEvent)) {
    RefuseToDropAndHideCaret(aDragEvent);
    return NS_OK;
  }

  aDragEvent->PreventDefault();
  aDragEvent->StopImmediatePropagation();

  if (aDragEvent->WidgetEventPtr()->mMessage == eDrop) {
    RefPtr<EditorBase> editorBase = mEditorBase;
    nsresult rv = editorBase->HandleDropEvent(aDragEvent);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::HandleDropEvent() failed");
    return rv;
  }

  MOZ_ASSERT(aDragEvent->WidgetEventPtr()->mMessage == eDragOver);

  // If we handle the dragged item, we need to adjust drop effect here
  // because once DataTransfer is retrieved, DragEvent has initialized it
  // with nsContentUtils::SetDataTransferInEvent() but it does not check
  // whether the content is movable or not.
  DataTransfer* dataTransfer = aDragEvent->GetDataTransfer();
  if (dataTransfer &&
      dataTransfer->DropEffectInt() == nsIDragService::DRAGDROP_ACTION_MOVE) {
    nsCOMPtr<nsINode> dragSource = dataTransfer->GetMozSourceNode();
    if (dragSource && !dragSource->IsEditable()) {
      // In this case, we shouldn't allow "move" because the drag source
      // isn't editable.
      dataTransfer->SetDropEffectInt(
          nsContentUtils::FilterDropEffect(nsIDragService::DRAGDROP_ACTION_COPY,
                                           dataTransfer->EffectAllowedInt()));
    }
  }

  if (!mCaret) {
    return NS_OK;
  }

  mCaret->SetVisible(true);
  mCaret->SetCaretPosition(dropParentContent, dropOffset);

  return NS_OK;
}

void EditorEventListener::InitializeDragDropCaret() {
  if (mCaret) {
    return;
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return;
  }

  mCaret = new nsCaret();
  DebugOnly<nsresult> rvIgnored = mCaret->Init(presShell);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "nsCaret::Init() failed, but ignored");
  mCaret->SetCaretReadOnly(true);
  // This is to avoid the requirement that the Selection is Collapsed which
  // it can't be when dragging a selection in the same shell.
  // See nsCaret::IsVisible().
  mCaret->SetVisibilityDuringSelection(true);

  presShell->SetCaret(mCaret);
}

void EditorEventListener::CleanupDragDropCaret() {
  if (!mCaret) {
    return;
  }

  mCaret->SetVisible(false);  // hide it, so that it turns off its timer

  RefPtr<PresShell> presShell = GetPresShell();
  if (presShell) {
    presShell->RestoreCaret();
  }

  mCaret->Terminate();
  mCaret = nullptr;
}

nsresult EditorEventListener::DragLeave(DragEvent* aDragEvent) {
  // XXX If aDragEvent was created by chrome script, its defaultPrevented
  //     may be true, though.  We shouldn't handle such event but we don't
  //     have a way to distinguish if coming event is created by chrome script.
  NS_WARNING_ASSERTION(!aDragEvent->WidgetEventPtr()->DefaultPrevented(),
                       "eDragLeave shouldn't be cancelable");
  if (NS_WARN_IF(!aDragEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  CleanupDragDropCaret();

  return NS_OK;
}

bool EditorEventListener::DragEventHasSupportingData(
    DragEvent* aDragEvent) const {
  MOZ_ASSERT(
      !DetachedFromEditorOrDefaultPrevented(aDragEvent->WidgetEventPtr()));
  MOZ_ASSERT(aDragEvent->GetDataTransfer());

  // Plaintext editors only support dropping text. Otherwise, HTML and files
  // can be dropped as well.
  DataTransfer* dataTransfer = aDragEvent->GetDataTransfer();
  if (!dataTransfer) {
    NS_WARNING("No data transfer returned");
    return false;
  }
  return dataTransfer->HasType(NS_LITERAL_STRING_FROM_CSTRING(kTextMime)) ||
         dataTransfer->HasType(
             NS_LITERAL_STRING_FROM_CSTRING(kMozTextInternal)) ||
         (!mEditorBase->IsInPlaintextMode() &&
          (dataTransfer->HasType(NS_LITERAL_STRING_FROM_CSTRING(kHTMLMime)) ||
           dataTransfer->HasType(NS_LITERAL_STRING_FROM_CSTRING(kFileMime))));
}

bool EditorEventListener::CanInsertAtDropPosition(DragEvent* aDragEvent) {
  MOZ_ASSERT(
      !DetachedFromEditorOrDefaultPrevented(aDragEvent->WidgetEventPtr()));
  MOZ_ASSERT(!mEditorBase->IsReadonly());
  MOZ_ASSERT(DragEventHasSupportingData(aDragEvent));

  DataTransfer* dataTransfer = aDragEvent->GetDataTransfer();
  if (NS_WARN_IF(!dataTransfer)) {
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

  RefPtr<Document> targetDocument = mEditorBase->GetDocument();
  if (NS_WARN_IF(!targetDocument)) {
    return false;
  }

  RefPtr<Document> sourceDocument = sourceNode->OwnerDoc();

  // If the source and the dest are not same document, allow to drop it always.
  if (targetDocument != sourceDocument) {
    return true;
  }

  // If the source node is a remote browser, treat this as coming from a
  // different document and allow the drop.
  if (BrowserParent::GetFrom(nsIContent::FromNode(sourceNode))) {
    return true;
  }

  RefPtr<Selection> selection = mEditorBase->GetSelection();
  if (!selection) {
    return false;
  }

  // If selection is collapsed, allow to drop it always.
  if (selection->IsCollapsed()) {
    return true;
  }

  int32_t dropOffset = -1;
  nsCOMPtr<nsIContent> dropParentContent =
      aDragEvent->GetRangeParentContentAndOffset(&dropOffset);
  if (!dropParentContent || NS_WARN_IF(dropOffset < 0) ||
      NS_WARN_IF(DetachedFromEditor())) {
    return false;
  }

  return !EditorUtils::IsPointInSelection(*selection, *dropParentContent,
                                          dropOffset);
}

nsresult EditorEventListener::HandleStartComposition(
    WidgetCompositionEvent* aCompositionStartEvent) {
  if (NS_WARN_IF(!aCompositionStartEvent)) {
    return NS_ERROR_FAILURE;
  }
  if (DetachedFromEditor()) {
    return NS_OK;
  }
  RefPtr<EditorBase> editorBase(mEditorBase);
  if (!editorBase->IsAcceptableInputEvent(aCompositionStartEvent)) {
    return NS_OK;
  }
  // Although, "compositionstart" should be cancelable, but currently,
  // eCompositionStart event coming from widget is not cancelable.
  MOZ_ASSERT(!aCompositionStartEvent->DefaultPrevented(),
             "eCompositionStart shouldn't be cancelable");
  nsresult rv = editorBase->OnCompositionStart(*aCompositionStartEvent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::OnCompositionStart() failed");
  return rv;
}

nsresult EditorEventListener::HandleChangeComposition(
    WidgetCompositionEvent* aCompositionChangeEvent) {
  if (NS_WARN_IF(!aCompositionChangeEvent)) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(!aCompositionChangeEvent->DefaultPrevented(),
             "eCompositionChange event shouldn't be cancelable");
  if (DetachedFromEditor()) {
    return NS_OK;
  }
  RefPtr<EditorBase> editorBase(mEditorBase);
  if (!editorBase->IsAcceptableInputEvent(aCompositionChangeEvent)) {
    return NS_OK;
  }

  // if we are readonly, then do nothing.
  if (editorBase->IsReadonly()) {
    return NS_OK;
  }

  nsresult rv = editorBase->OnCompositionChange(*aCompositionChangeEvent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::OnCompositionChange() failed");
  return rv;
}

void EditorEventListener::HandleEndComposition(
    WidgetCompositionEvent* aCompositionEndEvent) {
  if (NS_WARN_IF(!aCompositionEndEvent) || DetachedFromEditor()) {
    return;
  }
  RefPtr<EditorBase> editorBase(mEditorBase);
  if (!editorBase->IsAcceptableInputEvent(aCompositionEndEvent)) {
    return;
  }
  MOZ_ASSERT(!aCompositionEndEvent->DefaultPrevented(),
             "eCompositionEnd shouldn't be cancelable");

  editorBase->OnCompositionEnd(*aCompositionEndEvent);
}

nsresult EditorEventListener::Focus(InternalFocusEvent* aFocusEvent) {
  if (NS_WARN_IF(!aFocusEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  RefPtr<EditorBase> editorBase(mEditorBase);

  // Spell check a textarea the first time that it is focused.
  SpellCheckIfNeeded();
  if (DetachedFromEditor()) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> eventTargetNode =
      nsINode::FromEventTargetOrNull(aFocusEvent->GetOriginalDOMEventTarget());
  if (NS_WARN_IF(!eventTargetNode)) {
    return NS_ERROR_UNEXPECTED;
  }

  // If the target is a document node but it's not editable, we should ignore
  // it because actual focused element's event is going to come.
  if (eventTargetNode->IsDocument() && !eventTargetNode->IsInDesignMode()) {
    return NS_OK;
  }

  if (eventTargetNode->IsContent()) {
    nsIContent* content =
        eventTargetNode->AsContent()->FindFirstNonChromeOnlyAccessContent();
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
      nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
      if (NS_WARN_IF(!focusManager)) {
        return NS_OK;
      }

      nsIContent* focusedContent = focusManager->GetFocusedElement();
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

  editorBase->OnFocus(*eventTargetNode);
  if (DetachedFromEditorOrDefaultPrevented(aFocusEvent)) {
    return NS_OK;
  }

  RefPtr<nsPresContext> presContext = GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return NS_OK;
  }
  nsCOMPtr<nsIContent> focusedContent = editorBase->GetFocusedContent();
  IMEStateManager::OnFocusInEditor(presContext, focusedContent, *editorBase);

  return NS_OK;
}

nsresult EditorEventListener::Blur(InternalFocusEvent* aBlurEvent) {
  if (NS_WARN_IF(!aBlurEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  // check if something else is focused. If another element is focused, then
  // we should not change the selection.
  nsFocusManager* focusManager = nsFocusManager::GetFocusManager();
  if (NS_WARN_IF(!focusManager)) {
    return NS_OK;
  }

  Element* focusedElement = focusManager->GetFocusedElement();
  if (!focusedElement) {
    // If it's in the designMode, and blur occurs, the target must be the
    // window.  If a blur event is fired and the target is an element, it
    // must be delayed blur event at initializing the `HTMLEditor`.
    // TODO: Add automated tests for checking the case that the target node
    //       is in a shadow DOM tree whose host is in design mode.
    if (mEditorBase->IsHTMLEditor() &&
        mEditorBase->AsHTMLEditor()->IsInDesignMode()) {
      if (Element::FromEventTargetOrNull(aBlurEvent->mTarget)) {
        return NS_OK;
      }
    }
    RefPtr<EditorBase> editorBase(mEditorBase);
    DebugOnly<nsresult> rvIgnored = editorBase->FinalizeSelection();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "EditorBase::FinalizeSelection() failed, but ignored");
  }
  return NS_OK;
}

void EditorEventListener::SpellCheckIfNeeded() {
  MOZ_ASSERT(!DetachedFromEditor());

  // If the spell check skip flag is still enabled from creation time,
  // disable it because focused editors are allowed to spell check.
  RefPtr<EditorBase> editorBase(mEditorBase);
  if (!editorBase->ShouldSkipSpellCheck()) {
    return;
  }
  DebugOnly<nsresult> rvIgnored =
      editorBase->RemoveFlags(nsIEditor::eEditorSkipSpellCheck);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "EditorBase::RemoveFlags() failed, but ignored");
}

bool EditorEventListener::IsFileControlTextBox() {
  MOZ_ASSERT(!DetachedFromEditor());

  RefPtr<EditorBase> editorBase(mEditorBase);
  Element* rootElement = editorBase->GetRoot();
  if (!rootElement || !rootElement->ChromeOnlyAccess()) {
    return false;
  }
  nsIContent* parent = rootElement->FindFirstNonChromeOnlyAccessContent();
  if (!parent || !parent->IsHTMLElement(nsGkAtoms::input)) {
    return false;
  }
  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(parent);
  return formControl->ControlType() == FormControlType::InputFile;
}

bool EditorEventListener::ShouldHandleNativeKeyBindings(
    WidgetKeyboardEvent* aKeyboardEvent) {
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
  if (NS_WARN_IF(!targetContent)) {
    return false;
  }

  RefPtr<HTMLEditor> htmlEditor = HTMLEditor::GetFrom(mEditorBase);
  if (!htmlEditor) {
    return false;
  }

  if (htmlEditor->IsInDesignMode()) {
    // Don't need to perform any checks in designMode documents.
    return true;
  }

  nsIContent* editingHost = htmlEditor->GetActiveEditingHost();
  if (!editingHost) {
    return false;
  }

  return targetContent->IsInclusiveDescendantOf(editingHost);
}

}  // namespace mozilla
