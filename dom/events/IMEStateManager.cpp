/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "mozilla/IMEStateManager.h"

#include "mozilla/Attributes.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/unused.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/TabParent.h"

#include "HTMLInputElement.h"
#include "IMEContentObserver.h"

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMMouseEvent.h"
#include "nsIEditor.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsINode.h"
#include "nsIObserverService.h"
#include "nsIPresShell.h"
#include "nsISelection.h"
#include "nsISupports.h"
#include "nsPresContext.h"

namespace mozilla {

using namespace dom;
using namespace widget;

/**
 * When a method is called, log its arguments and/or related static variables
 * with LogLevel::Info.  However, if it puts too many logs like
 * OnDestroyPresContext(), should long only when the method actually does
 * something. In this case, the log should start with "ISM: <method name>".
 *
 * When a method quits due to unexpected situation, log the reason with
 * LogLevel::Error.  In this case, the log should start with
 * "ISM:   <method name>(), FAILED".  The indent makes the log look easier.
 *
 * When a method does something only in some situations and it may be important
 * for debug, log the information with LogLevel::Debug.  In this case, the log
 * should start with "ISM:   <method name>(),".
 */
PRLogModuleInfo* sISMLog = nullptr;

static const char*
GetBoolName(bool aBool)
{
  return aBool ? "true" : "false";
}

static const char*
GetActionCauseName(InputContextAction::Cause aCause)
{
  switch (aCause) {
    case InputContextAction::CAUSE_UNKNOWN:
      return "CAUSE_UNKNOWN";
    case InputContextAction::CAUSE_UNKNOWN_CHROME:
      return "CAUSE_UNKNOWN_CHROME";
    case InputContextAction::CAUSE_KEY:
      return "CAUSE_KEY";
    case InputContextAction::CAUSE_MOUSE:
      return "CAUSE_MOUSE";
    default:
      return "illegal value";
  }
}

static const char*
GetActionFocusChangeName(InputContextAction::FocusChange aFocusChange)
{
  switch (aFocusChange) {
    case InputContextAction::FOCUS_NOT_CHANGED:
      return "FOCUS_NOT_CHANGED";
    case InputContextAction::GOT_FOCUS:
      return "GOT_FOCUS";
    case InputContextAction::LOST_FOCUS:
      return "LOST_FOCUS";
    case InputContextAction::MENU_GOT_PSEUDO_FOCUS:
      return "MENU_GOT_PSEUDO_FOCUS";
    case InputContextAction::MENU_LOST_PSEUDO_FOCUS:
      return "MENU_LOST_PSEUDO_FOCUS";
    default:
      return "illegal value";
  }
}

static const char*
GetIMEStateEnabledName(IMEState::Enabled aEnabled)
{
  switch (aEnabled) {
    case IMEState::DISABLED:
      return "DISABLED";
    case IMEState::ENABLED:
      return "ENABLED";
    case IMEState::PASSWORD:
      return "PASSWORD";
    case IMEState::PLUGIN:
      return "PLUGIN";
    default:
      return "illegal value";
  }
}

static const char*
GetIMEStateSetOpenName(IMEState::Open aOpen)
{
  switch (aOpen) {
    case IMEState::DONT_CHANGE_OPEN_STATE:
      return "DONT_CHANGE_OPEN_STATE";
    case IMEState::OPEN:
      return "OPEN";
    case IMEState::CLOSED:
      return "CLOSED";
    default:
      return "illegal value";
  }
}

static const char*
GetEventMessageName(EventMessage aMessage)
{
  switch (aMessage) {
    case eCompositionStart:
      return "eCompositionStart";
    case eCompositionEnd:
      return "eCompositionEnd";
    case eCompositionUpdate:
      return "eCompositionUpdate";
    case eCompositionChange:
      return "eCompositionChange";
    case eCompositionCommitAsIs:
      return "eCompositionCommitAsIs";
    case eCompositionCommit:
      return "eCompositionCommit";
    case eSetSelection:
      return "eSetSelection";
    default:
      return "unacceptable event message";
  }
}

static const char*
GetNotifyIMEMessageName(IMEMessage aMessage)
{
  switch (aMessage) {
    case NOTIFY_IME_OF_FOCUS:
      return "NOTIFY_IME_OF_FOCUS";
    case NOTIFY_IME_OF_BLUR:
      return "NOTIFY_IME_OF_BLUR";
    case NOTIFY_IME_OF_SELECTION_CHANGE:
      return "NOTIFY_IME_OF_SELECTION_CHANGE";
    case NOTIFY_IME_OF_TEXT_CHANGE:
      return "NOTIFY_IME_OF_TEXT_CHANGE";
    case NOTIFY_IME_OF_COMPOSITION_UPDATE:
      return "NOTIFY_IME_OF_COMPOSITION_UPDATE";
    case NOTIFY_IME_OF_POSITION_CHANGE:
      return "NOTIFY_IME_OF_POSITION_CHANGE";
    case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
      return "NOTIFY_IME_OF_MOUSE_BUTTON_EVENT";
    case REQUEST_TO_COMMIT_COMPOSITION:
      return "REQUEST_TO_COMMIT_COMPOSITION";
    case REQUEST_TO_CANCEL_COMPOSITION:
      return "REQUEST_TO_CANCEL_COMPOSITION";
    default:
      return "unacceptable IME notification message";
  }
}

StaticRefPtr<nsIContent> IMEStateManager::sContent;
nsPresContext* IMEStateManager::sPresContext = nullptr;
StaticRefPtr<nsIWidget> IMEStateManager::sFocusedIMEWidget;
StaticRefPtr<TabParent> IMEStateManager::sActiveTabParent;
StaticRefPtr<IMEContentObserver> IMEStateManager::sActiveIMEContentObserver;
TextCompositionArray* IMEStateManager::sTextCompositions = nullptr;
bool IMEStateManager::sInstalledMenuKeyboardListener = false;
bool IMEStateManager::sIsGettingNewIMEState = false;
bool IMEStateManager::sCheckForIMEUnawareWebApps = false;
bool IMEStateManager::sRemoteHasFocus = false;

// static
void
IMEStateManager::Init()
{
  if (!sISMLog) {
    sISMLog = PR_NewLogModule("IMEStateManager");
  }

  Preferences::AddBoolVarCache(
    &sCheckForIMEUnawareWebApps,
    "intl.ime.hack.on_ime_unaware_apps.fire_key_events_for_composition",
    false);
}

// static
void
IMEStateManager::Shutdown()
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::Shutdown(), "
     "sTextCompositions=0x%p, sTextCompositions->Length()=%u",
     sTextCompositions, sTextCompositions ? sTextCompositions->Length() : 0));

  MOZ_ASSERT(!sTextCompositions || !sTextCompositions->Length());
  delete sTextCompositions;
  sTextCompositions = nullptr;
}

// static
void
IMEStateManager::OnTabParentDestroying(TabParent* aTabParent)
{
  if (sActiveTabParent != aTabParent) {
    return;
  }
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnTabParentDestroying(aTabParent=0x%p), "
     "The active TabParent is being destroyed", aTabParent));

  // The active remote process might have crashed.
  sActiveTabParent = nullptr;

  // TODO: Need to cancel composition without TextComposition and make
  //       disable IME.
}

// static
void
IMEStateManager::StopIMEStateManagement()
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::StopIMEStateManagement()"));

  // NOTE: Don't set input context from here since this has already lost
  //       the rights to change input context.

  if (sTextCompositions && sPresContext) {
    NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, sPresContext);
  }
  sPresContext = nullptr;
  sContent = nullptr;
  sActiveTabParent = nullptr;
  DestroyIMEContentObserver();
}

// static
nsresult
IMEStateManager::OnDestroyPresContext(nsPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);

  // First, if there is a composition in the aPresContext, clean up it.
  if (sTextCompositions) {
    TextCompositionArray::index_type i =
      sTextCompositions->IndexOf(aPresContext);
    if (i != TextCompositionArray::NoIndex) {
      MOZ_LOG(sISMLog, LogLevel::Debug,
        ("ISM:   IMEStateManager::OnDestroyPresContext(), "
         "removing TextComposition instance from the array (index=%u)", i));
      // there should be only one composition per presContext object.
      sTextCompositions->ElementAt(i)->Destroy();
      sTextCompositions->RemoveElementAt(i);
      if (sTextCompositions->IndexOf(aPresContext) !=
            TextCompositionArray::NoIndex) {
        MOZ_LOG(sISMLog, LogLevel::Error,
          ("ISM:   IMEStateManager::OnDestroyPresContext(), FAILED to remove "
           "TextComposition instance from the array"));
        MOZ_CRASH("Failed to remove TextComposition instance from the array");
      }
    }
  }

  if (aPresContext != sPresContext) {
    return NS_OK;
  }

  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnDestroyPresContext(aPresContext=0x%p), "
     "sPresContext=0x%p, sContent=0x%p, sTextCompositions=0x%p",
     aPresContext, sPresContext, sContent.get(), sTextCompositions));

  DestroyIMEContentObserver();

  nsCOMPtr<nsIWidget> widget = sPresContext->GetRootWidget();
  if (widget) {
    IMEState newState = GetNewIMEState(sPresContext, nullptr);
    InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                              InputContextAction::LOST_FOCUS);
    SetIMEState(newState, nullptr, widget, action);
  }
  sContent = nullptr;
  sPresContext = nullptr;
  sActiveTabParent = nullptr;
  return NS_OK;
}

// static
nsresult
IMEStateManager::OnRemoveContent(nsPresContext* aPresContext,
                                 nsIContent* aContent)
{
  NS_ENSURE_ARG_POINTER(aPresContext);

  // First, if there is a composition in the aContent, clean up it.
  if (sTextCompositions) {
    nsRefPtr<TextComposition> compositionInContent =
      sTextCompositions->GetCompositionInContent(aPresContext, aContent);

    if (compositionInContent) {
      MOZ_LOG(sISMLog, LogLevel::Debug,
        ("ISM:   IMEStateManager::OnRemoveContent(), "
         "composition is in the content"));

      // Try resetting the native IME state.  Be aware, typically, this method
      // is called during the content being removed.  Then, the native
      // composition events which are caused by following APIs are ignored due
      // to unsafe to run script (in PresShell::HandleEvent()).
      nsCOMPtr<nsIWidget> widget = aPresContext->GetRootWidget();
      MOZ_ASSERT(widget, "Why is there no widget?");
      nsresult rv =
        compositionInContent->NotifyIME(REQUEST_TO_CANCEL_COMPOSITION);
      if (NS_FAILED(rv)) {
        compositionInContent->NotifyIME(REQUEST_TO_COMMIT_COMPOSITION);
      }
    }
  }

  if (!sPresContext || !sContent ||
      !nsContentUtils::ContentIsDescendantOf(sContent, aContent)) {
    return NS_OK;
  }

  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnRemoveContent(aPresContext=0x%p, "
     "aContent=0x%p), sPresContext=0x%p, sContent=0x%p, sTextCompositions=0x%p",
     aPresContext, aContent, sPresContext, sContent.get(), sTextCompositions));

  DestroyIMEContentObserver();

  // Current IME transaction should commit
  nsCOMPtr<nsIWidget> widget = sPresContext->GetRootWidget();
  if (widget) {
    IMEState newState = GetNewIMEState(sPresContext, nullptr);
    InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                              InputContextAction::LOST_FOCUS);
    SetIMEState(newState, nullptr, widget, action);
  }

  sContent = nullptr;
  sPresContext = nullptr;
  sActiveTabParent = nullptr;

  return NS_OK;
}

// static
nsresult
IMEStateManager::OnChangeFocus(nsPresContext* aPresContext,
                               nsIContent* aContent,
                               InputContextAction::Cause aCause)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnChangeFocus(aPresContext=0x%p, "
     "aContent=0x%p, aCause=%s)",
     aPresContext, aContent, GetActionCauseName(aCause)));

  InputContextAction action(aCause);
  return OnChangeFocusInternal(aPresContext, aContent, action);
}

// static
nsresult
IMEStateManager::OnChangeFocusInternal(nsPresContext* aPresContext,
                                       nsIContent* aContent,
                                       InputContextAction aAction)
{
  nsRefPtr<TabParent> newTabParent = TabParent::GetFrom(aContent);

  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnChangeFocusInternal(aPresContext=0x%p, "
     "aContent=0x%p (TabParent=0x%p), aAction={ mCause=%s, mFocusChange=%s }), "
     "sPresContext=0x%p, sContent=0x%p, sActiveTabParent=0x%p, "
     "sActiveIMEContentObserver=0x%p, sInstalledMenuKeyboardListener=%s",
     aPresContext, aContent, newTabParent.get(),
     GetActionCauseName(aAction.mCause),
     GetActionFocusChangeName(aAction.mFocusChange),
     sPresContext, sContent.get(), sActiveTabParent.get(),
     sActiveIMEContentObserver.get(),
     GetBoolName(sInstalledMenuKeyboardListener)));

  bool focusActuallyChanging =
    (sContent != aContent || sPresContext != aPresContext ||
     sActiveTabParent != newTabParent);

  nsCOMPtr<nsIWidget> oldWidget =
    sPresContext ? sPresContext->GetRootWidget() : nullptr;
  if (oldWidget && focusActuallyChanging) {
    // If we're deactivating, we shouldn't commit composition forcibly because
    // the user may want to continue the composition.
    if (aPresContext) {
      NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, oldWidget);
    }
  }

  if (sActiveIMEContentObserver &&
      (aPresContext || !sActiveIMEContentObserver->KeepAliveDuringDeactive()) &&
      !sActiveIMEContentObserver->IsManaging(aPresContext, aContent)) {
    DestroyIMEContentObserver();
  }

  if (!aPresContext) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnChangeFocusInternal(), "
       "no nsPresContext is being activated"));
    return NS_OK;
  }

  nsIContentParent* currentContentParent =
    sActiveTabParent ? sActiveTabParent->Manager() : nullptr;
  nsIContentParent* newContentParent =
    newTabParent ? newTabParent->Manager() : nullptr;
  if (sActiveTabParent && currentContentParent != newContentParent) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnChangeFocusInternal(), notifying previous "
       "focused child process of parent process or another child process "
       "getting focus"));
    unused << sActiveTabParent->SendStopIMEStateManagement();
  }

  nsCOMPtr<nsIWidget> widget =
    (sPresContext == aPresContext) ? oldWidget.get() :
                                     aPresContext->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
      ("ISM:   IMEStateManager::OnChangeFocusInternal(), FAILED due to "
       "no widget to manage its IME state"));
    return NS_OK;
  }

  // If a child process has focus, we should disable IME state until the child
  // process actually gets focus because if user types keys before that they
  // are handled by IME.
  IMEState newState =
    newTabParent ? IMEState(IMEState::DISABLED) :
                   GetNewIMEState(aPresContext, aContent);
  bool setIMEState = true;

  if (newTabParent) {
    if (aAction.mFocusChange == InputContextAction::MENU_GOT_PSEUDO_FOCUS ||
        aAction.mFocusChange == InputContextAction::MENU_LOST_PSEUDO_FOCUS) {
      // XXX When menu keyboard listener is being uninstalled, IME state needs
      //     to be restored by the child process asynchronously.  Therefore,
      //     some key events which are fired immediately after closing menu
      //     may not be handled by IME.
      unused << newTabParent->
        SendMenuKeyboardListenerInstalled(sInstalledMenuKeyboardListener);
      setIMEState = sInstalledMenuKeyboardListener;
    } else if (focusActuallyChanging) {
      InputContext context = widget->GetInputContext();
      if (context.mIMEState.mEnabled == IMEState::DISABLED) {
        setIMEState = false;
        MOZ_LOG(sISMLog, LogLevel::Debug,
          ("ISM:   IMEStateManager::OnChangeFocusInternal(), doesn't set IME "
           "state because focused element (or document) is in a child process "
           "and the IME state is already disabled"));
      } else {
        MOZ_LOG(sISMLog, LogLevel::Debug,
          ("ISM:   IMEStateManager::OnChangeFocusInternal(), will disable IME "
           "until new focused element (or document) in the child process "
           "will get focus actually"));
      }
    } else {
      // When focus is NOT changed actually, we shouldn't set IME state since
      // that means that the window is being activated and the child process
      // may have composition.  Then, we shouldn't commit the composition with
      // making IME state disabled.
      setIMEState = false; 
      MOZ_LOG(sISMLog, LogLevel::Debug,
        ("ISM:   IMEStateManager::OnChangeFocusInternal(), doesn't set IME "
         "state because focused element (or document) is already in the child "
         "process"));
    }
  }

  if (setIMEState) {
    if (!focusActuallyChanging) {
      // actual focus isn't changing, but if IME enabled state is changing,
      // we should do it.
      InputContext context = widget->GetInputContext();
      if (context.mIMEState.mEnabled == newState.mEnabled) {
        MOZ_LOG(sISMLog, LogLevel::Debug,
          ("ISM:   IMEStateManager::OnChangeFocusInternal(), "
           "neither focus nor IME state is changing"));
        return NS_OK;
      }
      aAction.mFocusChange = InputContextAction::FOCUS_NOT_CHANGED;

      // Even if focus isn't changing actually, we should commit current
      // composition here since the IME state is changing.
      if (sPresContext && oldWidget && !focusActuallyChanging) {
        NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, oldWidget);
      }
    } else if (aAction.mFocusChange == InputContextAction::FOCUS_NOT_CHANGED) {
      // If aContent isn't null or aContent is null but editable, somebody gets
      // focus.
      bool gotFocus = aContent || (newState.mEnabled == IMEState::ENABLED);
      aAction.mFocusChange =
        gotFocus ? InputContextAction::GOT_FOCUS :
                   InputContextAction::LOST_FOCUS;
    }

    // Update IME state for new focus widget
    SetIMEState(newState, aContent, widget, aAction);
  }

  sActiveTabParent = newTabParent;
  sPresContext = aPresContext;
  sContent = aContent;

  // Don't call CreateIMEContentObserver() here, it should be called from
  // focus event handler of editor.

  return NS_OK;
}

// static
void
IMEStateManager::OnInstalledMenuKeyboardListener(bool aInstalling)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnInstalledMenuKeyboardListener(aInstalling=%s), "
     "sInstalledMenuKeyboardListener=%s",
     GetBoolName(aInstalling), GetBoolName(sInstalledMenuKeyboardListener)));

  sInstalledMenuKeyboardListener = aInstalling;

  InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
    aInstalling ? InputContextAction::MENU_GOT_PSEUDO_FOCUS :
                  InputContextAction::MENU_LOST_PSEUDO_FOCUS);
  OnChangeFocusInternal(sPresContext, sContent, action);
}

// static
bool
IMEStateManager::OnMouseButtonEventInEditor(nsPresContext* aPresContext,
                                            nsIContent* aContent,
                                            nsIDOMMouseEvent* aMouseEvent)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnMouseButtonEventInEditor(aPresContext=0x%p, "
     "aContent=0x%p, aMouseEvent=0x%p), sPresContext=0x%p, sContent=0x%p",
     aPresContext, aContent, aMouseEvent, sPresContext, sContent.get()));

  if (sPresContext != aPresContext || sContent != aContent) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnMouseButtonEventInEditor(), "
       "the mouse event isn't fired on the editor managed by ISM"));
    return false;
  }

  if (!sActiveIMEContentObserver) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnMouseButtonEventInEditor(), "
       "there is no active IMEContentObserver"));
    return false;
  }

  if (!sActiveIMEContentObserver->IsManaging(aPresContext, aContent)) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnMouseButtonEventInEditor(), "
       "the active IMEContentObserver isn't managing the editor"));
    return false;
  }

  WidgetMouseEvent* internalEvent =
    aMouseEvent->GetInternalNSEvent()->AsMouseEvent();
  if (NS_WARN_IF(!internalEvent)) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnMouseButtonEventInEditor(), "
       "the internal event of aMouseEvent isn't WidgetMouseEvent"));
    return false;
  }

  bool consumed =
    sActiveIMEContentObserver->OnMouseButtonEvent(aPresContext, internalEvent);

  if (MOZ_LOG_TEST(sISMLog, LogLevel::Info)) {
    nsAutoString eventType;
    aMouseEvent->GetType(eventType);
    MOZ_LOG(sISMLog, LogLevel::Info,
      ("ISM:   IMEStateManager::OnMouseButtonEventInEditor(), "
       "mouse event (type=%s, button=%d) is %s",
       NS_ConvertUTF16toUTF8(eventType).get(), internalEvent->button,
       consumed ? "consumed" : "not consumed"));
  }

  return consumed;
}

// static
void
IMEStateManager::OnClickInEditor(nsPresContext* aPresContext,
                                 nsIContent* aContent,
                                 nsIDOMMouseEvent* aMouseEvent)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnClickInEditor(aPresContext=0x%p, aContent=0x%p, "
     "aMouseEvent=0x%p), sPresContext=0x%p, sContent=0x%p",
     aPresContext, aContent, aMouseEvent, sPresContext, sContent.get()));

  if (sPresContext != aPresContext || sContent != aContent) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnClickInEditor(), "
       "the mouse event isn't fired on the editor managed by ISM"));
    return;
  }

  nsCOMPtr<nsIWidget> widget = aPresContext->GetRootWidget();
  NS_ENSURE_TRUE_VOID(widget);

  bool isTrusted;
  nsresult rv = aMouseEvent->GetIsTrusted(&isTrusted);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!isTrusted) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnClickInEditor(), "
       "the mouse event isn't a trusted event"));
    return; // ignore untrusted event.
  }

  int16_t button;
  rv = aMouseEvent->GetButton(&button);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (button != 0) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnClickInEditor(), "
       "the mouse event isn't a left mouse button event"));
    return; // not a left click event.
  }

  int32_t clickCount;
  rv = aMouseEvent->GetDetail(&clickCount);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (clickCount != 1) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnClickInEditor(), "
       "the mouse event isn't a single click event"));
    return; // should notify only first click event.
  }

  InputContextAction action(InputContextAction::CAUSE_MOUSE,
                            InputContextAction::FOCUS_NOT_CHANGED);
  IMEState newState = GetNewIMEState(aPresContext, aContent);
  SetIMEState(newState, aContent, widget, action);
}

// static
void
IMEStateManager::OnFocusInEditor(nsPresContext* aPresContext,
                                 nsIContent* aContent,
                                 nsIEditor* aEditor)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnFocusInEditor(aPresContext=0x%p, aContent=0x%p, "
     "aEditor=0x%p), sPresContext=0x%p, sContent=0x%p, "
     "sActiveIMEContentObserver=0x%p",
     aPresContext, aContent, aEditor, sPresContext, sContent.get(),
     sActiveIMEContentObserver.get()));

  if (sPresContext != aPresContext || sContent != aContent) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::OnFocusInEditor(), "
       "an editor not managed by ISM gets focus"));
    return;
  }

  // If the IMEContentObserver instance isn't managing the editor actually,
  // we need to recreate the instance.
  if (sActiveIMEContentObserver) {
    if (sActiveIMEContentObserver->IsManaging(aPresContext, aContent)) {
      MOZ_LOG(sISMLog, LogLevel::Debug,
        ("ISM:   IMEStateManager::OnFocusInEditor(), "
         "the editor is already being managed by sActiveIMEContentObserver"));
      return;
    }
    DestroyIMEContentObserver();
  }

  CreateIMEContentObserver(aEditor);
}

// static
void
IMEStateManager::OnEditorInitialized(nsIEditor* aEditor)
{
  if (!sActiveIMEContentObserver ||
      sActiveIMEContentObserver->GetEditor() != aEditor) {
    return;
  }

  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnEditorInitialized(aEditor=0x%p)",
     aEditor));

  sActiveIMEContentObserver->UnsuppressNotifyingIME();
}

// static
void
IMEStateManager::OnEditorDestroying(nsIEditor* aEditor)
{
  if (!sActiveIMEContentObserver ||
      sActiveIMEContentObserver->GetEditor() != aEditor) {
    return;
  }

  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnEditorDestroying(aEditor=0x%p)",
     aEditor));

  // The IMEContentObserver shouldn't notify IME of anything until reframing
  // is finished.
  sActiveIMEContentObserver->SuppressNotifyingIME();
}

// static
void
IMEStateManager::UpdateIMEState(const IMEState& aNewIMEState,
                                nsIContent* aContent,
                                nsIEditor* aEditor)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::UpdateIMEState(aNewIMEState={ mEnabled=%s, "
     "mOpen=%s }, aContent=0x%p, aEditor=0x%p), "
     "sPresContext=0x%p, sContent=0x%p, sActiveIMEContentObserver=0x%p, "
     "sIsGettingNewIMEState=%s",
     GetIMEStateEnabledName(aNewIMEState.mEnabled),
     GetIMEStateSetOpenName(aNewIMEState.mOpen), aContent, aEditor,
     sPresContext, sContent.get(), sActiveIMEContentObserver.get(),
     GetBoolName(sIsGettingNewIMEState)));

  if (sIsGettingNewIMEState) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::UpdateIMEState(), "
       "does nothing because of called while getting new IME state"));
    return;
  }

  if (NS_WARN_IF(!sPresContext)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
      ("ISM:   IMEStateManager::UpdateIMEState(), FAILED due to "
       "no managing nsPresContext"));
    return;
  }
  nsCOMPtr<nsIWidget> widget = sPresContext->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
      ("ISM:   IMEStateManager::UpdateIMEState(), FAILED due to "
       "no widget for the managing nsPresContext"));
    return;
  }

  // Even if there is active IMEContentObserver, it may not be observing the
  // editor with current editable root content due to reframed.  In such case,
  // We should try to reinitialize the IMEContentObserver.
  if (sActiveIMEContentObserver && IsIMEObserverNeeded(aNewIMEState)) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::UpdateIMEState(), try to reinitialize the "
       "active IMEContentObserver"));
    if (!sActiveIMEContentObserver->MaybeReinitialize(widget, sPresContext,
                                                      aContent, aEditor)) {
      MOZ_LOG(sISMLog, LogLevel::Error,
        ("ISM:   IMEStateManager::UpdateIMEState(), failed to reinitialize the "
         "active IMEContentObserver"));
    }
  }

  // If there is no active IMEContentObserver or it isn't observing the
  // editor correctly, we should recreate it.
  bool createTextStateManager =
    (!sActiveIMEContentObserver ||
     !sActiveIMEContentObserver->IsManaging(sPresContext, aContent));

  bool updateIMEState =
    (widget->GetInputContext().mIMEState.mEnabled != aNewIMEState.mEnabled);

  if (updateIMEState) {
    // commit current composition before modifying IME state.
    NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, widget);
  }

  if (createTextStateManager) {
    DestroyIMEContentObserver();
  }

  if (updateIMEState) {
    InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                              InputContextAction::FOCUS_NOT_CHANGED);
    SetIMEState(aNewIMEState, aContent, widget, action);
  }

  if (createTextStateManager) {
    CreateIMEContentObserver(aEditor);
  }
}

// static
IMEState
IMEStateManager::GetNewIMEState(nsPresContext* aPresContext,
                                nsIContent*    aContent)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::GetNewIMEState(aPresContext=0x%p, aContent=0x%p), "
     "sInstalledMenuKeyboardListener=%s",
     aPresContext, aContent, GetBoolName(sInstalledMenuKeyboardListener)));

  // On Printing or Print Preview, we don't need IME.
  if (aPresContext->Type() == nsPresContext::eContext_PrintPreview ||
      aPresContext->Type() == nsPresContext::eContext_Print) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::GetNewIMEState() returns DISABLED because "
       "the nsPresContext is for print or print preview"));
    return IMEState(IMEState::DISABLED);
  }

  if (sInstalledMenuKeyboardListener) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::GetNewIMEState() returns DISABLED because "
       "menu keyboard listener was installed"));
    return IMEState(IMEState::DISABLED);
  }

  if (!aContent) {
    // Even if there are no focused content, the focused document might be
    // editable, such case is design mode.
    nsIDocument* doc = aPresContext->Document();
    if (doc && doc->HasFlag(NODE_IS_EDITABLE)) {
      MOZ_LOG(sISMLog, LogLevel::Debug,
        ("ISM:   IMEStateManager::GetNewIMEState() returns ENABLED because "
         "design mode editor has focus"));
      return IMEState(IMEState::ENABLED);
    }
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::GetNewIMEState() returns DISABLED because "
       "no content has focus"));
    return IMEState(IMEState::DISABLED);
  }

  // nsIContent::GetDesiredIMEState() may cause a call of UpdateIMEState()
  // from nsEditor::PostCreate() because GetDesiredIMEState() needs to retrieve
  // an editor instance for the element if it's editable element.
  // For avoiding such nested IME state updates, we should set
  // sIsGettingNewIMEState here and UpdateIMEState() should check it.
  GettingNewIMEStateBlocker blocker;

  IMEState newIMEState = aContent->GetDesiredIMEState();
  MOZ_LOG(sISMLog, LogLevel::Debug,
    ("ISM:   IMEStateManager::GetNewIMEState() returns { mEnabled=%s, "
     "mOpen=%s }",
     GetIMEStateEnabledName(newIMEState.mEnabled),
     GetIMEStateSetOpenName(newIMEState.mOpen)));
  return newIMEState;
}

// Helper class, used for IME enabled state change notification
class IMEEnabledStateChangedEvent : public nsRunnable {
public:
  explicit IMEEnabledStateChangedEvent(uint32_t aState)
    : mState(aState)
  {
  }

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (observerService) {
      MOZ_LOG(sISMLog, LogLevel::Info,
        ("ISM: IMEEnabledStateChangedEvent::Run(), notifies observers of "
         "\"ime-enabled-state-changed\""));
      nsAutoString state;
      state.AppendInt(mState);
      observerService->NotifyObservers(nullptr, "ime-enabled-state-changed",
                                       state.get());
    }
    return NS_OK;
  }

private:
  uint32_t mState;
};

static bool
MayBeIMEUnawareWebApp(nsINode* aNode)
{
  bool haveKeyEventsListener = false;

  while (aNode) {
    EventListenerManager* const mgr = aNode->GetExistingListenerManager();
    if (mgr) {
      if (mgr->MayHaveInputOrCompositionEventListener()) {
        return false;
      }
      haveKeyEventsListener |= mgr->MayHaveKeyEventListener();
    }
    aNode = aNode->GetParentNode();
  }

  return haveKeyEventsListener;
}

// static
void
IMEStateManager::SetInputContextForChildProcess(
                   TabParent* aTabParent,
                   const InputContext& aInputContext,
                   const InputContextAction& aAction)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::SetInputContextForChildProcess(aTabParent=0x%p, "
     "aInputContext={ mIMEState={ mEnabled=%s, mOpen=%s }, "
     "mHTMLInputType=\"%s\", mHTMLInputInputmode=\"%s\", mActionHint=\"%s\" }, "
     "aAction={ mCause=%s, mAction=%s }, aTabParent=0x%p), sPresContext=0x%p, "
     "sActiveTabParent=0x%p",
     aTabParent, GetIMEStateEnabledName(aInputContext.mIMEState.mEnabled),
     GetIMEStateSetOpenName(aInputContext.mIMEState.mOpen),
     NS_ConvertUTF16toUTF8(aInputContext.mHTMLInputType).get(),
     NS_ConvertUTF16toUTF8(aInputContext.mHTMLInputInputmode).get(),
     NS_ConvertUTF16toUTF8(aInputContext.mActionHint).get(),
     GetActionCauseName(aAction.mCause),
     GetActionFocusChangeName(aAction.mFocusChange),
     sPresContext, sActiveTabParent.get()));

  if (aTabParent != sActiveTabParent) {
    MOZ_LOG(sISMLog, LogLevel::Error,
      ("ISM:    IMEStateManager::SetInputContextForChildProcess(), FAILED, "
       "because non-focused tab parent tries to set input context"));
    return;
  }

  if (NS_WARN_IF(!sPresContext)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
      ("ISM:    IMEStateManager::SetInputContextForChildProcess(), FAILED, "
       "due to no focused presContext"));
    return;
  }

  nsCOMPtr<nsIWidget> widget = sPresContext->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
      ("ISM:    IMEStateManager::SetInputContextForChildProcess(), FAILED, "
       "due to no widget in the focused presContext"));
    return;
  }

  MOZ_ASSERT(aInputContext.mOrigin == InputContext::ORIGIN_CONTENT);

  SetInputContext(widget, aInputContext, aAction);
}

// static
void
IMEStateManager::SetIMEState(const IMEState& aState,
                             nsIContent* aContent,
                             nsIWidget* aWidget,
                             InputContextAction aAction)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::SetIMEState(aState={ mEnabled=%s, mOpen=%s }, "
     "aContent=0x%p (TabParent=0x%p), aWidget=0x%p, aAction={ mCause=%s, "
     "mFocusChange=%s })",
     GetIMEStateEnabledName(aState.mEnabled),
     GetIMEStateSetOpenName(aState.mOpen), aContent,
     TabParent::GetFrom(aContent), aWidget,
     GetActionCauseName(aAction.mCause),
     GetActionFocusChangeName(aAction.mFocusChange)));

  NS_ENSURE_TRUE_VOID(aWidget);

  InputContext oldContext = aWidget->GetInputContext();

  InputContext context;
  context.mIMEState = aState;
  context.mMayBeIMEUnaware = context.mIMEState.IsEditable() &&
    sCheckForIMEUnawareWebApps && MayBeIMEUnawareWebApp(aContent);

  if (aContent &&
      aContent->IsAnyOfHTMLElements(nsGkAtoms::input, nsGkAtoms::textarea)) {
    if (!aContent->IsHTMLElement(nsGkAtoms::textarea)) {
      // <input type=number> has an anonymous <input type=text> descendant
      // that gets focus whenever anyone tries to focus the number control. We
      // need to check if aContent is one of those anonymous text controls and,
      // if so, use the number control instead:
      nsIContent* content = aContent;
      HTMLInputElement* inputElement =
        HTMLInputElement::FromContentOrNull(aContent);
      if (inputElement) {
        HTMLInputElement* ownerNumberControl =
          inputElement->GetOwnerNumberControl();
        if (ownerNumberControl) {
          content = ownerNumberControl; // an <input type=number>
        }
      }
      content->GetAttr(kNameSpaceID_None, nsGkAtoms::type,
                       context.mHTMLInputType);
    } else {
      context.mHTMLInputType.Assign(nsGkAtoms::textarea->GetUTF16String());
    }

    if (Preferences::GetBool("dom.forms.inputmode", false)) {
      aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::inputmode,
                        context.mHTMLInputInputmode);
    }

    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::moz_action_hint,
                      context.mActionHint);

    // Get the input content corresponding to the focused node,
    // which may be an anonymous child of the input content.
    nsIContent* inputContent = aContent->FindFirstNonChromeOnlyAccessContent();

    // If we don't have an action hint and
    // return won't submit the form, use "next".
    if (context.mActionHint.IsEmpty() &&
        inputContent->IsHTMLElement(nsGkAtoms::input)) {
      bool willSubmit = false;
      nsCOMPtr<nsIFormControl> control(do_QueryInterface(inputContent));
      mozilla::dom::Element* formElement = control->GetFormElement();
      nsCOMPtr<nsIForm> form;
      if (control) {
        // is this a form and does it have a default submit element?
        if ((form = do_QueryInterface(formElement)) &&
            form->GetDefaultSubmitElement()) {
          willSubmit = true;
        // is this an html form and does it only have a single text input element?
        } else if (formElement && formElement->IsHTMLElement(nsGkAtoms::form) &&
                   !static_cast<dom::HTMLFormElement*>(formElement)->
                     ImplicitSubmissionIsDisabled()) {
          willSubmit = true;
        }
      }
      context.mActionHint.Assign(
        willSubmit ? (control->GetType() == NS_FORM_INPUT_SEARCH ?
                       NS_LITERAL_STRING("search") : NS_LITERAL_STRING("go")) :
                     (formElement ?
                       NS_LITERAL_STRING("next") : EmptyString()));
    }
  }

  // XXX I think that we should use nsContentUtils::IsCallerChrome() instead
  //     of the process type.
  if (aAction.mCause == InputContextAction::CAUSE_UNKNOWN &&
      !XRE_IsContentProcess()) {
    aAction.mCause = InputContextAction::CAUSE_UNKNOWN_CHROME;
  }

  SetInputContext(aWidget, context, aAction);
}

// static
void
IMEStateManager::SetInputContext(nsIWidget* aWidget,
                                 const InputContext& aInputContext,
                                 const InputContextAction& aAction)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::SetInputContext(aWidget=0x%p, aInputContext={ "
     "mIMEState={ mEnabled=%s, mOpen=%s }, mHTMLInputType=\"%s\", "
     "mHTMLInputInputmode=\"%s\", mActionHint=\"%s\" }, "
     "aAction={ mCause=%s, mAction=%s }), sActiveTabParent=0x%p",
     aWidget,
     GetIMEStateEnabledName(aInputContext.mIMEState.mEnabled),
     GetIMEStateSetOpenName(aInputContext.mIMEState.mOpen),
     NS_ConvertUTF16toUTF8(aInputContext.mHTMLInputType).get(),
     NS_ConvertUTF16toUTF8(aInputContext.mHTMLInputInputmode).get(),
     NS_ConvertUTF16toUTF8(aInputContext.mActionHint).get(),
     GetActionCauseName(aAction.mCause),
     GetActionFocusChangeName(aAction.mFocusChange),
     sActiveTabParent.get()));

  MOZ_RELEASE_ASSERT(aWidget);

  InputContext oldContext = aWidget->GetInputContext();

  aWidget->SetInputContext(aInputContext, aAction);
  if (oldContext.mIMEState.mEnabled == aInputContext.mIMEState.mEnabled) {
    return;
  }

  nsContentUtils::AddScriptRunner(
    new IMEEnabledStateChangedEvent(aInputContext.mIMEState.mEnabled));
}

// static
void
IMEStateManager::EnsureTextCompositionArray()
{
  if (sTextCompositions) {
    return;
  }
  sTextCompositions = new TextCompositionArray();
}

// static
void
IMEStateManager::DispatchCompositionEvent(
                   nsINode* aEventTargetNode,
                   nsPresContext* aPresContext,
                   WidgetCompositionEvent* aCompositionEvent,
                   nsEventStatus* aStatus,
                   EventDispatchingCallback* aCallBack,
                   bool aIsSynthesized)
{
  nsRefPtr<TabParent> tabParent =
    aEventTargetNode->IsContent() ?
      TabParent::GetFrom(aEventTargetNode->AsContent()) : nullptr;

  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::DispatchCompositionEvent(aNode=0x%p, "
     "aPresContext=0x%p, aCompositionEvent={ message=%s, "
     "mFlags={ mIsTrusted=%s, mPropagationStopped=%s } }, "
     "aIsSynthesized=%s), tabParent=%p",
     aEventTargetNode, aPresContext,
     GetEventMessageName(aCompositionEvent->mMessage),
     GetBoolName(aCompositionEvent->mFlags.mIsTrusted),
     GetBoolName(aCompositionEvent->mFlags.mPropagationStopped),
     GetBoolName(aIsSynthesized), tabParent.get()));

  if (!aCompositionEvent->mFlags.mIsTrusted ||
      aCompositionEvent->mFlags.mPropagationStopped) {
    return;
  }

  MOZ_ASSERT(aCompositionEvent->mMessage != eCompositionUpdate,
             "compositionupdate event shouldn't be dispatched manually");

  EnsureTextCompositionArray();

  nsRefPtr<TextComposition> composition =
    sTextCompositions->GetCompositionFor(aCompositionEvent->widget);
  if (!composition) {
    // If synthesized event comes after delayed native composition events
    // for request of commit or cancel, we should ignore it.
    if (NS_WARN_IF(aIsSynthesized)) {
      return;
    }
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::DispatchCompositionEvent(), "
       "adding new TextComposition to the array"));
    MOZ_ASSERT(aCompositionEvent->mMessage == eCompositionStart);
    composition =
      new TextComposition(aPresContext, aEventTargetNode, tabParent,
                          aCompositionEvent);
    sTextCompositions->AppendElement(composition);
  }
#ifdef DEBUG
  else {
    MOZ_ASSERT(aCompositionEvent->mMessage != eCompositionStart);
  }
#endif // #ifdef DEBUG

  // Dispatch the event on composing target.
  composition->DispatchCompositionEvent(aCompositionEvent, aStatus, aCallBack,
                                        aIsSynthesized);

  // WARNING: the |composition| might have been destroyed already.

  // Remove the ended composition from the array.
  // NOTE: When TextComposition is synthesizing compositionend event for
  //       emulating a commit, the instance shouldn't be removed from the array
  //       because IME may perform it later.  Then, we need to ignore the
  //       following commit events in TextComposition::DispatchEvent().
  //       However, if commit or cancel for a request is performed synchronously
  //       during not safe to dispatch events, PresShell must have discarded
  //       compositionend event.  Then, the synthesized compositionend event is
  //       the last event for the composition.  In this case, we need to
  //       destroy the TextComposition with synthesized compositionend event.
  if ((!aIsSynthesized ||
       composition->WasNativeCompositionEndEventDiscarded()) &&
      aCompositionEvent->CausesDOMCompositionEndEvent()) {
    TextCompositionArray::index_type i =
      sTextCompositions->IndexOf(aCompositionEvent->widget);
    if (i != TextCompositionArray::NoIndex) {
      MOZ_LOG(sISMLog, LogLevel::Debug,
        ("ISM:   IMEStateManager::DispatchCompositionEvent(), "
         "removing TextComposition from the array since NS_COMPOSTION_END "
         "was dispatched"));
      sTextCompositions->ElementAt(i)->Destroy();
      sTextCompositions->RemoveElementAt(i);
    }
  }
}

// static
nsIContent*
IMEStateManager::GetRootContent(nsPresContext* aPresContext)
{
  nsIDocument* doc = aPresContext->Document();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }
  return doc->GetRootElement();
}

// static
void
IMEStateManager::HandleSelectionEvent(nsPresContext* aPresContext,
                                      nsIContent* aEventTargetContent,
                                      WidgetSelectionEvent* aSelectionEvent)
{
  nsIContent* eventTargetContent =
    aEventTargetContent ? aEventTargetContent :
                          GetRootContent(aPresContext);
  nsRefPtr<TabParent> tabParent =
    eventTargetContent ? TabParent::GetFrom(eventTargetContent) : nullptr;

  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::HandleSelectionEvent(aPresContext=0x%p, "
     "aEventTargetContent=0x%p, aSelectionEvent={ mMessage=%s, "
     "mFlags={ mIsTrusted=%s } }), tabParent=%p",
     aPresContext, aEventTargetContent,
     GetEventMessageName(aSelectionEvent->mMessage),
     GetBoolName(aSelectionEvent->mFlags.mIsTrusted),
     tabParent.get()));

  if (!aSelectionEvent->mFlags.mIsTrusted) {
    return;
  }

  nsRefPtr<TextComposition> composition = sTextCompositions ?
    sTextCompositions->GetCompositionFor(aSelectionEvent->widget) : nullptr;
  if (composition) {
    // When there is a composition, TextComposition should guarantee that the
    // selection event will be handled in same target as composition events.
    composition->HandleSelectionEvent(aSelectionEvent);
  } else {
    // When there is no composition, the selection event should be handled
    // in the aPresContext or tabParent.
    TextComposition::HandleSelectionEvent(aPresContext, tabParent,
                                          aSelectionEvent);
  }
}

// static
void
IMEStateManager::OnCompositionEventDiscarded(
                   WidgetCompositionEvent* aCompositionEvent)
{
  // Note that this method is never called for synthesized events for emulating
  // commit or cancel composition.

  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::OnCompositionEventDiscarded(aCompositionEvent={ "
     "mMessage=%s, mFlags={ mIsTrusted=%s } })",
     GetEventMessageName(aCompositionEvent->mMessage),
     GetBoolName(aCompositionEvent->mFlags.mIsTrusted)));

  if (!aCompositionEvent->mFlags.mIsTrusted) {
    return;
  }

  // Ignore compositionstart for now because sTextCompositions may not have
  // been created yet.
  if (aCompositionEvent->mMessage == eCompositionStart) {
    return;
  }

  nsRefPtr<TextComposition> composition =
    sTextCompositions->GetCompositionFor(aCompositionEvent->widget);
  if (!composition) {
    // If the PresShell has been being destroyed during composition,
    // a TextComposition instance for the composition was already removed from
    // the array and destroyed in OnDestroyPresContext().  Therefore, we may
    // fail to retrieve a TextComposition instance here.
    MOZ_LOG(sISMLog, LogLevel::Info,
      ("ISM:   IMEStateManager::OnCompositionEventDiscarded(), "
       "TextComposition instance for the widget has already gone"));
    return;
  }
  composition->OnCompositionEventDiscarded(aCompositionEvent);
}

// static
nsresult
IMEStateManager::NotifyIME(IMEMessage aMessage,
                           nsIWidget* aWidget,
                           bool aOriginIsRemote)
{
  return IMEStateManager::NotifyIME(IMENotification(aMessage), aWidget,
                                    aOriginIsRemote);
}

// static
nsresult
IMEStateManager::NotifyIME(const IMENotification& aNotification,
                           nsIWidget* aWidget,
                           bool aOriginIsRemote)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::NotifyIME(aNotification={ mMessage=%s }, "
     "aWidget=0x%p, aOriginIsRemote=%s), sFocusedIMEWidget=0x%p, "
     "sRemoteHasFocus=%s",
     GetNotifyIMEMessageName(aNotification.mMessage), aWidget,
     GetBoolName(aOriginIsRemote), sFocusedIMEWidget.get(),
     GetBoolName(sRemoteHasFocus)));

  if (NS_WARN_IF(!aWidget)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
      ("ISM:   IMEStateManager::NotifyIME(), FAILED due to no widget"));
    return NS_ERROR_INVALID_ARG;
  }

  switch (aNotification.mMessage) {
    case NOTIFY_IME_OF_FOCUS:
      if (sFocusedIMEWidget) {
        if (NS_WARN_IF(!sRemoteHasFocus && !aOriginIsRemote)) {
          MOZ_LOG(sISMLog, LogLevel::Error,
            ("ISM:   IMEStateManager::NotifyIME(), although, this process is "
             "getting IME focus but there was focused IME widget"));
        } else {
          MOZ_LOG(sISMLog, LogLevel::Info,
            ("ISM:   IMEStateManager::NotifyIME(), tries to notify IME of "
             "blur first because remote process's blur notification hasn't "
             "been received yet..."));
        }
        nsCOMPtr<nsIWidget> focusedIMEWidget(sFocusedIMEWidget);
        sFocusedIMEWidget = nullptr;
        sRemoteHasFocus = false;
        focusedIMEWidget->NotifyIME(IMENotification(NOTIFY_IME_OF_BLUR));
      }
      sRemoteHasFocus = aOriginIsRemote;
      sFocusedIMEWidget = aWidget;
      return aWidget->NotifyIME(aNotification);
    case NOTIFY_IME_OF_BLUR: {
      if (!sRemoteHasFocus && aOriginIsRemote) {
        MOZ_LOG(sISMLog, LogLevel::Info,
          ("ISM:   IMEStateManager::NotifyIME(), received blur notification "
           "after another one has focus, nothing to do..."));
        return NS_OK;
      }
      if (NS_WARN_IF(sRemoteHasFocus && !aOriginIsRemote)) {
        MOZ_LOG(sISMLog, LogLevel::Error,
          ("ISM:   IMEStateManager::NotifyIME(), FAILED, received blur "
           "notification from this process but the remote has focus"));
        return NS_OK;
      }
      if (!sFocusedIMEWidget && aOriginIsRemote) {
        MOZ_LOG(sISMLog, LogLevel::Info,
          ("ISM:   IMEStateManager::NotifyIME(), received blur notification "
           "but the remote has already lost focus"));
        return NS_OK;
      }
      if (NS_WARN_IF(!sFocusedIMEWidget)) {
        MOZ_LOG(sISMLog, LogLevel::Error,
          ("ISM:   IMEStateManager::NotifyIME(), FAILED, received blur "
           "notification but there is no focused IME widget"));
        return NS_OK;
      }
      if (NS_WARN_IF(sFocusedIMEWidget != aWidget)) {
        MOZ_LOG(sISMLog, LogLevel::Error,
          ("ISM:   IMEStateManager::NotifyIME(), FAILED, received blur "
           "notification but there is no focused IME widget"));
        return NS_OK;
      }
      nsCOMPtr<nsIWidget> focusedIMEWidget(sFocusedIMEWidget);
      sFocusedIMEWidget = nullptr;
      sRemoteHasFocus = false;
      return focusedIMEWidget->NotifyIME(IMENotification(NOTIFY_IME_OF_BLUR));
    }
    case NOTIFY_IME_OF_SELECTION_CHANGE:
    case NOTIFY_IME_OF_TEXT_CHANGE:
    case NOTIFY_IME_OF_POSITION_CHANGE:
    case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
      if (!sRemoteHasFocus && aOriginIsRemote) {
        MOZ_LOG(sISMLog, LogLevel::Info,
          ("ISM:   IMEStateManager::NotifyIME(), received content change "
           "notification from the remote but it's already lost focus"));
        return NS_OK;
      }
      if (NS_WARN_IF(sRemoteHasFocus && !aOriginIsRemote)) {
        MOZ_LOG(sISMLog, LogLevel::Error,
          ("ISM:   IMEStateManager::NotifyIME(), FAILED, received content "
           "change notification from this process but the remote has already "
           "gotten focus"));
        return NS_OK;
      }
      if (!sFocusedIMEWidget) {
        MOZ_LOG(sISMLog, LogLevel::Info,
          ("ISM:   IMEStateManager::NotifyIME(), received content change "
           "notification but there is no focused IME widget"));
        return NS_OK;
      }
      if (NS_WARN_IF(sFocusedIMEWidget != aWidget)) {
        MOZ_LOG(sISMLog, LogLevel::Error,
          ("ISM:   IMEStateManager::NotifyIME(), FAILED, received content "
           "change notification for IME which has already lost focus, so, "
           "nothing to do..."));
        return NS_OK;
      }
      return aWidget->NotifyIME(aNotification);
    default:
      // Other notifications should be sent only when there is composition.
      // So, we need to handle the others below.
      break;
  }

  nsRefPtr<TextComposition> composition;
  if (sTextCompositions) {
    composition = sTextCompositions->GetCompositionFor(aWidget);
  }

  bool isSynthesizedForTests =
    composition && composition->IsSynthesizedForTests();

  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM:   IMEStateManager::NotifyIME(), composition=0x%p, "
     "composition->IsSynthesizedForTests()=%s",
     composition.get(), GetBoolName(isSynthesizedForTests)));

  switch (aNotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      return composition ?
        composition->RequestToCommit(aWidget, false) : NS_OK;
    case REQUEST_TO_CANCEL_COMPOSITION:
      return composition ?
        composition->RequestToCommit(aWidget, true) : NS_OK;
    case NOTIFY_IME_OF_COMPOSITION_UPDATE:
      if (!aOriginIsRemote && (!composition || isSynthesizedForTests)) {
        MOZ_LOG(sISMLog, LogLevel::Info,
          ("ISM:   IMEStateManager::NotifyIME(), FAILED, received content "
           "change notification from this process but there is no compostion"));
        return NS_OK;
      }
      if (!sRemoteHasFocus && aOriginIsRemote) {
        MOZ_LOG(sISMLog, LogLevel::Info,
          ("ISM:   IMEStateManager::NotifyIME(), received content change "
           "notification from the remote but it's already lost focus"));
        return NS_OK;
      }
      return aWidget->NotifyIME(aNotification);
    default:
      MOZ_CRASH("Unsupported notification");
  }
  MOZ_CRASH(
    "Failed to handle the notification for non-synthesized composition");
  return NS_ERROR_FAILURE;
}

// static
nsresult
IMEStateManager::NotifyIME(IMEMessage aMessage,
                           nsPresContext* aPresContext,
                           bool aOriginIsRemote)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::NotifyIME(aMessage=%s, aPresContext=0x%p, "
     "aOriginIsRemote=%s)",
     GetNotifyIMEMessageName(aMessage), aPresContext,
     GetBoolName(aOriginIsRemote)));

  NS_ENSURE_TRUE(aPresContext, NS_ERROR_INVALID_ARG);

  nsIWidget* widget = aPresContext->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
      ("ISM:   IMEStateManager::NotifyIME(), FAILED due to no widget for the "
       "nsPresContext"));
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NotifyIME(aMessage, widget, aOriginIsRemote);
}

// static
bool
IMEStateManager::IsEditable(nsINode* node)
{
  if (node->IsEditable()) {
    return true;
  }
  // |node| might be readwrite (for example, a text control)
  if (node->IsElement() &&
      node->AsElement()->State().HasState(NS_EVENT_STATE_MOZ_READWRITE)) {
    return true;
  }
  return false;
}

// static
nsINode*
IMEStateManager::GetRootEditableNode(nsPresContext* aPresContext,
                                     nsIContent* aContent)
{
  if (aContent) {
    nsINode* root = nullptr;
    nsINode* node = aContent;
    while (node && IsEditable(node)) {
      // If the node has independent selection like <input type="text"> or
      // <textarea>, the node should be the root editable node for aContent.
      // FYI: <select> element also has independent selection but IsEditable()
      //      returns false.
      // XXX: If somebody adds new editable element which has independent
      //      selection but doesn't own editor, we'll need more checks here.
      if (node->IsContent() &&
          node->AsContent()->HasIndependentSelection()) {
        return node;
      }
      root = node;
      node = node->GetParentNode();
    }
    return root;
  }
  if (aPresContext) {
    nsIDocument* document = aPresContext->Document();
    if (document && document->IsEditable()) {
      return document;
    }
  }
  return nullptr;
}

// static
bool
IMEStateManager::IsIMEObserverNeeded(const IMEState& aState)
{
  return aState.IsEditable();
}

// static
void
IMEStateManager::DestroyIMEContentObserver()
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::DestroyIMEContentObserver(), "
     "sActiveIMEContentObserver=0x%p",
     sActiveIMEContentObserver.get()));

  if (!sActiveIMEContentObserver) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::DestroyIMEContentObserver() does nothing"));
    return;
  }

  MOZ_LOG(sISMLog, LogLevel::Debug,
    ("ISM:   IMEStateManager::DestroyIMEContentObserver(), destroying "
     "the active IMEContentObserver..."));
  nsRefPtr<IMEContentObserver> tsm = sActiveIMEContentObserver.get();
  sActiveIMEContentObserver = nullptr;
  tsm->Destroy();
}

// static
void
IMEStateManager::CreateIMEContentObserver(nsIEditor* aEditor)
{
  MOZ_LOG(sISMLog, LogLevel::Info,
    ("ISM: IMEStateManager::CreateIMEContentObserver(aEditor=0x%p), "
     "sPresContext=0x%p, sContent=0x%p, sActiveIMEContentObserver=0x%p, "
     "sActiveIMEContentObserver->IsManaging(sPresContext, sContent)=%s",
     aEditor, sPresContext, sContent.get(), sActiveIMEContentObserver.get(),
     GetBoolName(sActiveIMEContentObserver ?
       sActiveIMEContentObserver->IsManaging(sPresContext, sContent) : false)));

  if (NS_WARN_IF(sActiveIMEContentObserver)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
      ("ISM:   IMEStateManager::CreateIMEContentObserver(), FAILED due to "
       "there is already an active IMEContentObserver"));
    MOZ_ASSERT(sActiveIMEContentObserver->IsManaging(sPresContext, sContent));
    return;
  }

  nsCOMPtr<nsIWidget> widget = sPresContext->GetRootWidget();
  if (!widget) {
    MOZ_LOG(sISMLog, LogLevel::Error,
      ("ISM:   IMEStateManager::CreateIMEContentObserver(), FAILED due to "
       "there is a root widget for the nsPresContext"));
    return; // Sometimes, there are no widgets.
  }

  // If it's not text editable, we don't need to create IMEContentObserver.
  if (!IsIMEObserverNeeded(widget->GetInputContext().mIMEState)) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
      ("ISM:   IMEStateManager::CreateIMEContentObserver() doesn't create "
       "IMEContentObserver because of non-editable IME state"));
    return;
  }

  MOZ_LOG(sISMLog, LogLevel::Debug,
    ("ISM:   IMEStateManager::CreateIMEContentObserver() is creating an "
     "IMEContentObserver instance..."));
  sActiveIMEContentObserver = new IMEContentObserver();

  // IMEContentObserver::Init() might create another IMEContentObserver
  // instance.  So, sActiveIMEContentObserver would be replaced with new one.
  // We should hold the current instance here.
  nsRefPtr<IMEContentObserver> kungFuDeathGrip(sActiveIMEContentObserver);
  sActiveIMEContentObserver->Init(widget, sPresContext, sContent, aEditor);
}

// static
nsresult
IMEStateManager::GetFocusSelectionAndRoot(nsISelection** aSelection,
                                          nsIContent** aRootContent)
{
  if (!sActiveIMEContentObserver) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return sActiveIMEContentObserver->GetSelectionAndRoot(aSelection,
                                                        aRootContent);
}

// static
already_AddRefed<TextComposition>
IMEStateManager::GetTextCompositionFor(nsIWidget* aWidget)
{
  if (!sTextCompositions) {
    return nullptr;
  }
  nsRefPtr<TextComposition> textComposition =
    sTextCompositions->GetCompositionFor(aWidget);
  return textComposition.forget();
}

// static
already_AddRefed<TextComposition>
IMEStateManager::GetTextCompositionFor(WidgetGUIEvent* aGUIEvent)
{
  MOZ_ASSERT(aGUIEvent->AsCompositionEvent() || aGUIEvent->AsKeyboardEvent(),
    "aGUIEvent has to be WidgetCompositionEvent or WidgetKeyboardEvent");
  return GetTextCompositionFor(aGUIEvent->widget);
}

} // namespace mozilla
