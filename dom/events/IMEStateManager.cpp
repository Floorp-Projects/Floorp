/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlog.h"

#include "mozilla/IMEStateManager.h"

#include "mozilla/Attributes.h"
#include "mozilla/EventStates.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/HTMLFormElement.h"

#include "HTMLInputElement.h"
#include "IMEContentObserver.h"

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMMouseEvent.h"
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

#ifdef PR_LOGGING
/**
 * When a method is called, log its arguments and/or related static variables
 * with PR_LOG_ALWAYS.  However, if it puts too many logs like
 * OnDestroyPresContext(), should long only when the method actually does
 * something. In this case, the log should start with "ISM: <method name>".
 *
 * When a method quits due to unexpected situation, log the reason with
 * PR_LOG_ERROR.  In this case, the log should start with
 * "ISM:   <method name>(), FAILED".  The indent makes the log look easier.
 *
 * When a method does something only in some situations and it may be important
 * for debug, log the information with PR_LOG_DEBUG.  In this case, the log
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
GetEventClassIDName(EventClassID aEventClassID)
{
  switch (aEventClassID) {
    case eCompositionEventClass:
      return "eCompositionEventClass";
    case eTextEventClass:
      return "eTextEventClass";
    default:
      return "unacceptable event struct type";
  }
}

static const char*
GetEventMessageName(uint32_t aMessage)
{
  switch (aMessage) {
    case NS_COMPOSITION_START:
      return "NS_COMPOSITION_START";
    case NS_COMPOSITION_END:
      return "NS_COMPOSITION_END";
    case NS_COMPOSITION_UPDATE:
      return "NS_COMPOSITION_UPDATE";
    case NS_TEXT_TEXT:
      return "NS_TEXT_TEXT";
    default:
      return "unacceptable event message";
  }
}

static const char*
GetNotifyIMEMessageName(IMEMessage aMessage)
{
  switch (aMessage) {
    case NOTIFY_IME_OF_CURSOR_POS_CHANGED:
      return "NOTIFY_IME_OF_CURSOR_POS_CHANGED";
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
    case REQUEST_TO_COMMIT_COMPOSITION:
      return "REQUEST_TO_COMMIT_COMPOSITION";
    case REQUEST_TO_CANCEL_COMPOSITION:
      return "REQUEST_TO_CANCEL_COMPOSITION";
    default:
      return "unacceptable IME notification message";
  }
}
#endif // #ifdef PR_LOGGING

nsIContent* IMEStateManager::sContent = nullptr;
nsPresContext* IMEStateManager::sPresContext = nullptr;
bool IMEStateManager::sInstalledMenuKeyboardListener = false;
bool IMEStateManager::sIsTestingIME = false;
bool IMEStateManager::sIsGettingNewIMEState = false;

// sActiveIMEContentObserver points to the currently active IMEContentObserver.
// sActiveIMEContentObserver is null if there is no focused editor.
IMEContentObserver* IMEStateManager::sActiveIMEContentObserver = nullptr;
TextCompositionArray* IMEStateManager::sTextCompositions = nullptr;

// static
void
IMEStateManager::Init()
{
#ifdef PR_LOGGING
  if (!sISMLog) {
    sISMLog = PR_NewLogModule("IMEStateManager");
  }
#endif
}

// static
void
IMEStateManager::Shutdown()
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::Shutdown(), "
     "sTextCompositions=0x%p, sTextCompositions->Length()=%u",
     sTextCompositions, sTextCompositions ? sTextCompositions->Length() : 0));

  MOZ_ASSERT(!sTextCompositions || !sTextCompositions->Length());
  delete sTextCompositions;
  sTextCompositions = nullptr;
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
      PR_LOG(sISMLog, PR_LOG_DEBUG,
        ("ISM:   IMEStateManager::OnDestroyPresContext(), "
         "removing TextComposition instance from the array (index=%u)", i));
      // there should be only one composition per presContext object.
      sTextCompositions->ElementAt(i)->Destroy();
      sTextCompositions->RemoveElementAt(i);
#if defined DEBUG || PR_LOGGING
      if (sTextCompositions->IndexOf(aPresContext) !=
            TextCompositionArray::NoIndex) {
        PR_LOG(sISMLog, PR_LOG_ERROR,
          ("ISM:   IMEStateManager::OnDestroyPresContext(), FAILED to remove "
           "TextComposition instance from the array"));
        MOZ_CRASH("Failed to remove TextComposition instance from the array");
      }
#endif // #if defined DEBUG || PR_LOGGING
    }
  }

  if (aPresContext != sPresContext) {
    return NS_OK;
  }

  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::OnDestroyPresContext(aPresContext=0x%p), "
     "sPresContext=0x%p, sContent=0x%p, sTextCompositions=0x%p",
     aPresContext, sPresContext, sContent, sTextCompositions));

  DestroyIMEContentObserver();

  nsCOMPtr<nsIWidget> widget = sPresContext->GetRootWidget();
  if (widget) {
    IMEState newState = GetNewIMEState(sPresContext, nullptr);
    InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                              InputContextAction::LOST_FOCUS);
    SetIMEState(newState, nullptr, widget, action);
  }
  NS_IF_RELEASE(sContent);
  sPresContext = nullptr;
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
      PR_LOG(sISMLog, PR_LOG_DEBUG,
        ("ISM:   IMEStateManager::OnRemoveContent(), "
         "composition is in the content"));

      // Try resetting the native IME state.  Be aware, typically, this method
      // is called during the content being removed.  Then, the native
      // composition events which are caused by following APIs are ignored due
      // to unsafe to run script (in PresShell::HandleEvent()).
      nsCOMPtr<nsIWidget> widget = aPresContext->GetRootWidget();
      if (widget) {
        nsresult rv =
          compositionInContent->NotifyIME(REQUEST_TO_CANCEL_COMPOSITION);
        if (NS_FAILED(rv)) {
          compositionInContent->NotifyIME(REQUEST_TO_COMMIT_COMPOSITION);
        }
        // By calling the APIs, the composition may have been finished normally.
        compositionInContent =
          sTextCompositions->GetCompositionFor(
                               compositionInContent->GetPresContext(),
                               compositionInContent->GetEventTargetNode());
      }
    }

    // If the compositionInContent is still available, we should finish the
    // composition just on the content forcibly.
    if (compositionInContent) {
      PR_LOG(sISMLog, PR_LOG_DEBUG,
        ("ISM:   IMEStateManager::OnRemoveContent(), "
         "composition in the content still alive, committing it forcibly..."));
      compositionInContent->SynthesizeCommit(true);
    }
  }

  if (!sPresContext || !sContent ||
      !nsContentUtils::ContentIsDescendantOf(sContent, aContent)) {
    return NS_OK;
  }

  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::OnRemoveContent(aPresContext=0x%p, "
     "aContent=0x%p), sPresContext=0x%p, sContent=0x%p, sTextCompositions=0x%p",
     aPresContext, aContent, sPresContext, sContent, sTextCompositions));

  DestroyIMEContentObserver();

  // Current IME transaction should commit
  nsCOMPtr<nsIWidget> widget = sPresContext->GetRootWidget();
  if (widget) {
    IMEState newState = GetNewIMEState(sPresContext, nullptr);
    InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                              InputContextAction::LOST_FOCUS);
    SetIMEState(newState, nullptr, widget, action);
  }

  NS_IF_RELEASE(sContent);
  sPresContext = nullptr;

  return NS_OK;
}

// static
nsresult
IMEStateManager::OnChangeFocus(nsPresContext* aPresContext,
                               nsIContent* aContent,
                               InputContextAction::Cause aCause)
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
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
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::OnChangeFocusInternal(aPresContext=0x%p, "
     "aContent=0x%p, aAction={ mCause=%s, mFocusChange=%s }), "
     "sPresContext=0x%p, sContent=0x%p, sActiveIMEContentObserver=0x%p",
     aPresContext, aContent, GetActionCauseName(aAction.mCause),
     GetActionFocusChangeName(aAction.mFocusChange),
     sPresContext, sContent, sActiveIMEContentObserver));

  bool focusActuallyChanging =
    (sContent != aContent || sPresContext != aPresContext);

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
    PR_LOG(sISMLog, PR_LOG_DEBUG,
      ("ISM:   IMEStateManager::OnChangeFocusInternal(), "
       "no nsPresContext is being activated"));
    return NS_OK;
  }

  nsCOMPtr<nsIWidget> widget =
    (sPresContext == aPresContext) ? oldWidget.get() :
                                     aPresContext->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    PR_LOG(sISMLog, PR_LOG_ERROR,
      ("ISM:   IMEStateManager::OnChangeFocusInternal(), FAILED due to "
       "no widget to manage its IME state"));
    return NS_OK;
  }

  IMEState newState = GetNewIMEState(aPresContext, aContent);
  if (!focusActuallyChanging) {
    // actual focus isn't changing, but if IME enabled state is changing,
    // we should do it.
    InputContext context = widget->GetInputContext();
    if (context.mIMEState.mEnabled == newState.mEnabled) {
      PR_LOG(sISMLog, PR_LOG_DEBUG,
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
      gotFocus ? InputContextAction::GOT_FOCUS : InputContextAction::LOST_FOCUS;
  }

  // Update IME state for new focus widget
  SetIMEState(newState, aContent, widget, aAction);

  sPresContext = aPresContext;
  if (sContent != aContent) {
    NS_IF_RELEASE(sContent);
    NS_IF_ADDREF(sContent = aContent);
  }

  // Don't call CreateIMEContentObserver() here, it should be called from
  // focus event handler of editor.

  return NS_OK;
}

// static
void
IMEStateManager::OnInstalledMenuKeyboardListener(bool aInstalling)
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
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
void
IMEStateManager::OnClickInEditor(nsPresContext* aPresContext,
                                 nsIContent* aContent,
                                 nsIDOMMouseEvent* aMouseEvent)
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::OnClickInEditor(aPresContext=0x%p, aContent=0x%p, "
     "aMouseEvent=0x%p), sPresContext=0x%p, sContent=0x%p",
     aPresContext, aContent, aMouseEvent, sPresContext, sContent));

  if (sPresContext != aPresContext || sContent != aContent) {
    PR_LOG(sISMLog, PR_LOG_DEBUG,
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
    PR_LOG(sISMLog, PR_LOG_DEBUG,
      ("ISM:   IMEStateManager::OnClickInEditor(), "
       "the mouse event isn't a trusted event"));
    return; // ignore untrusted event.
  }

  int16_t button;
  rv = aMouseEvent->GetButton(&button);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (button != 0) {
    PR_LOG(sISMLog, PR_LOG_DEBUG,
      ("ISM:   IMEStateManager::OnClickInEditor(), "
       "the mouse event isn't a left mouse button event"));
    return; // not a left click event.
  }

  int32_t clickCount;
  rv = aMouseEvent->GetDetail(&clickCount);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (clickCount != 1) {
    PR_LOG(sISMLog, PR_LOG_DEBUG,
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
                                 nsIContent* aContent)
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::OnFocusInEditor(aPresContext=0x%p, aContent=0x%p), "
     "sPresContext=0x%p, sContent=0x%p, sActiveIMEContentObserver=0x%p",
     aPresContext, aContent, sPresContext, sContent,
     sActiveIMEContentObserver));

  if (sPresContext != aPresContext || sContent != aContent) {
    PR_LOG(sISMLog, PR_LOG_DEBUG,
      ("ISM:   IMEStateManager::OnFocusInEditor(), "
       "an editor not managed by ISM gets focus"));
    return;
  }

  // If the IMEContentObserver instance isn't managing the editor actually,
  // we need to recreate the instance.
  if (sActiveIMEContentObserver) {
    if (sActiveIMEContentObserver->IsManaging(aPresContext, aContent)) {
      PR_LOG(sISMLog, PR_LOG_DEBUG,
        ("ISM:   IMEStateManager::OnFocusInEditor(), "
         "the editor is already being managed by sActiveIMEContentObserver"));
      return;
    }
    DestroyIMEContentObserver();
  }

  CreateIMEContentObserver();
}

// static
void
IMEStateManager::UpdateIMEState(const IMEState& aNewIMEState,
                                nsIContent* aContent)
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::UpdateIMEState(aNewIMEState={ mEnabled=%s, "
     "mOpen=%s }, aContent=0x%p), "
     "sPresContext=0x%p, sContent=0x%p, sActiveIMEContentObserver=0x%p, "
     "sIsGettingNewIMEState=%s",
     GetIMEStateEnabledName(aNewIMEState.mEnabled),
     GetIMEStateSetOpenName(aNewIMEState.mOpen), aContent,
     sPresContext, sContent, sActiveIMEContentObserver,
     GetBoolName(sIsGettingNewIMEState)));

  if (sIsGettingNewIMEState) {
    PR_LOG(sISMLog, PR_LOG_DEBUG,
      ("ISM:   IMEStateManager::UpdateIMEState(), "
       "does nothing because of called while getting new IME state"));
    return;
  }

  if (NS_WARN_IF(!sPresContext)) {
    PR_LOG(sISMLog, PR_LOG_ERROR,
      ("ISM:   IMEStateManager::UpdateIMEState(), FAILED due to "
       "no managing nsPresContext"));
    return;
  }
  nsCOMPtr<nsIWidget> widget = sPresContext->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    PR_LOG(sISMLog, PR_LOG_ERROR,
      ("ISM:   IMEStateManager::UpdateIMEState(), FAILED due to "
       "no widget for the managing nsPresContext"));
    return;
  }

  // If the IMEContentObserver instance isn't managing the editor's current
  // editable root content, the editor frame might be reframed.  We should
  // recreate the instance at that time.
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
    CreateIMEContentObserver();
  }
}

// static
IMEState
IMEStateManager::GetNewIMEState(nsPresContext* aPresContext,
                                nsIContent*    aContent)
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::GetNewIMEState(aPresContext=0x%p, aContent=0x%p), "
     "sInstalledMenuKeyboardListener=%s",
     aPresContext, aContent, GetBoolName(sInstalledMenuKeyboardListener)));

  // On Printing or Print Preview, we don't need IME.
  if (aPresContext->Type() == nsPresContext::eContext_PrintPreview ||
      aPresContext->Type() == nsPresContext::eContext_Print) {
    PR_LOG(sISMLog, PR_LOG_DEBUG,
      ("ISM:   IMEStateManager::GetNewIMEState() returns DISABLED because "
       "the nsPresContext is for print or print preview"));
    return IMEState(IMEState::DISABLED);
  }

  if (sInstalledMenuKeyboardListener) {
    PR_LOG(sISMLog, PR_LOG_DEBUG,
      ("ISM:   IMEStateManager::GetNewIMEState() returns DISABLED because "
       "menu keyboard listener was installed"));
    return IMEState(IMEState::DISABLED);
  }

  if (!aContent) {
    // Even if there are no focused content, the focused document might be
    // editable, such case is design mode.
    nsIDocument* doc = aPresContext->Document();
    if (doc && doc->HasFlag(NODE_IS_EDITABLE)) {
      PR_LOG(sISMLog, PR_LOG_DEBUG,
        ("ISM:   IMEStateManager::GetNewIMEState() returns ENABLED because "
         "design mode editor has focus"));
      return IMEState(IMEState::ENABLED);
    }
    PR_LOG(sISMLog, PR_LOG_DEBUG,
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
  PR_LOG(sISMLog, PR_LOG_DEBUG,
    ("ISM:   IMEStateManager::GetNewIMEState() returns { mEnabled=%s, "
     "mOpen=%s }",
     GetIMEStateEnabledName(newIMEState.mEnabled),
     GetIMEStateSetOpenName(newIMEState.mOpen)));
  return newIMEState;
}

// Helper class, used for IME enabled state change notification
class IMEEnabledStateChangedEvent : public nsRunnable {
public:
  IMEEnabledStateChangedEvent(uint32_t aState)
    : mState(aState)
  {
  }

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (observerService) {
      PR_LOG(sISMLog, PR_LOG_ALWAYS,
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

// static
void
IMEStateManager::SetIMEState(const IMEState& aState,
                             nsIContent* aContent,
                             nsIWidget* aWidget,
                             InputContextAction aAction)
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::SetIMEState(aState={ mEnabled=%s, mOpen=%s }, "
     "aContent=0x%p, aWidget=0x%p, aAction={ mCause=%s, mFocusChange=%s })",
     GetIMEStateEnabledName(aState.mEnabled),
     GetIMEStateSetOpenName(aState.mOpen), aContent, aWidget,
     GetActionCauseName(aAction.mCause),
     GetActionFocusChangeName(aAction.mFocusChange)));

  NS_ENSURE_TRUE_VOID(aWidget);

  InputContext oldContext = aWidget->GetInputContext();

  InputContext context;
  context.mIMEState = aState;

  if (aContent && aContent->GetNameSpaceID() == kNameSpaceID_XHTML &&
      (aContent->Tag() == nsGkAtoms::input ||
       aContent->Tag() == nsGkAtoms::textarea)) {
    if (aContent->Tag() != nsGkAtoms::textarea) {
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
        inputContent->Tag() == nsGkAtoms::input) {
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
        } else if (formElement && formElement->Tag() == nsGkAtoms::form &&
                   formElement->IsHTML() &&
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
      XRE_GetProcessType() != GeckoProcessType_Content) {
    aAction.mCause = InputContextAction::CAUSE_UNKNOWN_CHROME;
  }


  PR_LOG(sISMLog, PR_LOG_DEBUG,
    ("ISM:   IMEStateManager::SetIMEState(), "
     "calling nsIWidget::SetInputContext(context={ mIMEState={ mEnabled=%s, "
     "mOpen=%s }, mHTMLInputType=\"%s\", mHTMLInputInputmode=\"%s\", "
     "mActionHint=\"%s\" }, aAction={ mCause=%s, mAction=%s })",
     GetIMEStateEnabledName(context.mIMEState.mEnabled),
     GetIMEStateSetOpenName(context.mIMEState.mOpen),
     NS_ConvertUTF16toUTF8(context.mHTMLInputType).get(),
     NS_ConvertUTF16toUTF8(context.mHTMLInputInputmode).get(),
     NS_ConvertUTF16toUTF8(context.mActionHint).get(),
     GetActionCauseName(aAction.mCause),
     GetActionFocusChangeName(aAction.mFocusChange)));

  aWidget->SetInputContext(context, aAction);
  if (oldContext.mIMEState.mEnabled == context.mIMEState.mEnabled) {
    return;
  }

  nsContentUtils::AddScriptRunner(
    new IMEEnabledStateChangedEvent(context.mIMEState.mEnabled));
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
IMEStateManager::DispatchCompositionEvent(nsINode* aEventTargetNode,
                                          nsPresContext* aPresContext,
                                          WidgetEvent* aEvent,
                                          nsEventStatus* aStatus,
                                          EventDispatchingCallback* aCallBack)
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::DispatchCompositionEvent(aNode=0x%p, "
     "aPresContext=0x%p, aEvent={ mClass=%s, message=%s, "
     " mFlags={ mIsTrusted=%s, mPropagationStopped=%s } })",
     aEventTargetNode, aPresContext,
     GetEventClassIDName(aEvent->mClass),
     GetEventMessageName(aEvent->message),
     GetBoolName(aEvent->mFlags.mIsTrusted),
     GetBoolName(aEvent->mFlags.mPropagationStopped)));

  MOZ_ASSERT(aEvent->mClass == eCompositionEventClass ||
             aEvent->mClass == eTextEventClass);
  if (!aEvent->mFlags.mIsTrusted || aEvent->mFlags.mPropagationStopped) {
    return;
  }

  EnsureTextCompositionArray();

  WidgetGUIEvent* GUIEvent = aEvent->AsGUIEvent();

  nsRefPtr<TextComposition> composition =
    sTextCompositions->GetCompositionFor(GUIEvent->widget);
  if (!composition) {
    PR_LOG(sISMLog, PR_LOG_DEBUG,
      ("ISM:   IMEStateManager::DispatchCompositionEvent(), "
       "adding new TextComposition to the array"));
    MOZ_ASSERT(GUIEvent->message == NS_COMPOSITION_START);
    composition = new TextComposition(aPresContext, aEventTargetNode, GUIEvent);
    sTextCompositions->AppendElement(composition);
  }
#ifdef DEBUG
  else {
    MOZ_ASSERT(GUIEvent->message != NS_COMPOSITION_START);
  }
#endif // #ifdef DEBUG

  // Dispatch the event on composing target.
  composition->DispatchEvent(GUIEvent, aStatus, aCallBack);

  // WARNING: the |composition| might have been destroyed already.

  // Remove the ended composition from the array.
  if (aEvent->message == NS_COMPOSITION_END) {
    TextCompositionArray::index_type i =
      sTextCompositions->IndexOf(GUIEvent->widget);
    if (i != TextCompositionArray::NoIndex) {
      PR_LOG(sISMLog, PR_LOG_DEBUG,
        ("ISM:   IMEStateManager::DispatchCompositionEvent(), "
         "removing TextComposition from the array since NS_COMPOSTION_END "
         "was dispatched"));
      sTextCompositions->ElementAt(i)->Destroy();
      sTextCompositions->RemoveElementAt(i);
    }
  }
}

// static
nsresult
IMEStateManager::NotifyIME(IMEMessage aMessage,
                           nsIWidget* aWidget)
{
  nsRefPtr<TextComposition> composition;
  if (aWidget && sTextCompositions) {
    composition = sTextCompositions->GetCompositionFor(aWidget);
  }

  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::NotifyIME(aMessage=%s, aWidget=0x%p), "
     "composition=0x%p, composition->IsSynthesizedForTests()=%s",
     GetNotifyIMEMessageName(aMessage), aWidget, composition.get(),
     GetBoolName(composition ? composition->IsSynthesizedForTests() : false)));

  if (NS_WARN_IF(!aWidget)) {
    PR_LOG(sISMLog, PR_LOG_ERROR,
      ("ISM:   IMEStateManager::NotifyIME(), FAILED due to no widget"));
    return NS_ERROR_INVALID_ARG;
  }

  if (!composition || !composition->IsSynthesizedForTests()) {
    switch (aMessage) {
      case NOTIFY_IME_OF_CURSOR_POS_CHANGED:
        return aWidget->NotifyIME(IMENotification(aMessage));
      case REQUEST_TO_COMMIT_COMPOSITION:
      case REQUEST_TO_CANCEL_COMPOSITION:
      case NOTIFY_IME_OF_COMPOSITION_UPDATE:
        return composition ?
          aWidget->NotifyIME(IMENotification(aMessage)) : NS_OK;
      default:
        MOZ_CRASH("Unsupported notification");
    }
    MOZ_CRASH(
      "Failed to handle the notification for non-synthesized composition");
  }

  // If the composition is synthesized events for automated tests, we should
  // dispatch composition events for emulating the native composition behavior.
  // NOTE: The dispatched events are discarded if it's not safe to run script.
  switch (aMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION: {
      nsCOMPtr<nsIWidget> widget(aWidget);
      nsEventStatus status = nsEventStatus_eIgnore;
      if (!composition->LastData().IsEmpty()) {
        WidgetTextEvent textEvent(true, NS_TEXT_TEXT, widget);
        textEvent.theText = composition->LastData();
        textEvent.mFlags.mIsSynthesizedForTests = true;
        widget->DispatchEvent(&textEvent, status);
        if (widget->Destroyed()) {
          return NS_OK;
        }
      }

      status = nsEventStatus_eIgnore;
      WidgetCompositionEvent endEvent(true, NS_COMPOSITION_END, widget);
      endEvent.data = composition->LastData();
      endEvent.mFlags.mIsSynthesizedForTests = true;
      widget->DispatchEvent(&endEvent, status);

      return NS_OK;
    }
    case REQUEST_TO_CANCEL_COMPOSITION: {
      nsCOMPtr<nsIWidget> widget(aWidget);
      nsEventStatus status = nsEventStatus_eIgnore;
      if (!composition->LastData().IsEmpty()) {
        WidgetCompositionEvent updateEvent(true, NS_COMPOSITION_UPDATE, widget);
        updateEvent.data = composition->LastData();
        updateEvent.mFlags.mIsSynthesizedForTests = true;
        widget->DispatchEvent(&updateEvent, status);
        if (widget->Destroyed()) {
          return NS_OK;
        }

        status = nsEventStatus_eIgnore;
        WidgetTextEvent textEvent(true, NS_TEXT_TEXT, widget);
        textEvent.theText = composition->LastData();
        textEvent.mFlags.mIsSynthesizedForTests = true;
        widget->DispatchEvent(&textEvent, status);
        if (widget->Destroyed()) {
          return NS_OK;
        }
      }

      status = nsEventStatus_eIgnore;
      WidgetCompositionEvent endEvent(true, NS_COMPOSITION_END, widget);
      endEvent.data = composition->LastData();
      endEvent.mFlags.mIsSynthesizedForTests = true;
      widget->DispatchEvent(&endEvent, status);

      return NS_OK;
    }
    default:
      return NS_OK;
  }
}

// static
nsresult
IMEStateManager::NotifyIME(IMEMessage aMessage,
                           nsPresContext* aPresContext)
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::NotifyIME(aMessage=%s, aPresContext=0x%p)",
     GetNotifyIMEMessageName(aMessage), aPresContext));

  NS_ENSURE_TRUE(aPresContext, NS_ERROR_INVALID_ARG);

  nsIWidget* widget = aPresContext->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    PR_LOG(sISMLog, PR_LOG_ERROR,
      ("ISM:   IMEStateManager::NotifyIME(), FAILED due to no widget for the "
       "nsPresContext"));
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NotifyIME(aMessage, widget);
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
IMEStateManager::IsEditableIMEState(nsIWidget* aWidget)
{
  switch (aWidget->GetInputContext().mIMEState.mEnabled) {
    case IMEState::ENABLED:
    case IMEState::PASSWORD:
      return true;
    case IMEState::PLUGIN:
    case IMEState::DISABLED:
      return false;
    default:
      MOZ_CRASH("Unknown IME enable state");
  }
}

// static
void
IMEStateManager::DestroyIMEContentObserver()
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::DestroyIMEContentObserver(), "
     "sActiveIMEContentObserver=0x%p",
     sActiveIMEContentObserver));

  if (!sActiveIMEContentObserver) {
    PR_LOG(sISMLog, PR_LOG_DEBUG,
      ("ISM:   IMEStateManager::DestroyIMEContentObserver() does nothing"));
    return;
  }

  PR_LOG(sISMLog, PR_LOG_DEBUG,
    ("ISM:   IMEStateManager::DestroyIMEContentObserver(), destroying "
     "the active IMEContentObserver..."));
  nsRefPtr<IMEContentObserver> tsm;
  tsm.swap(sActiveIMEContentObserver);
  tsm->Destroy();
}

// static
void
IMEStateManager::CreateIMEContentObserver()
{
  PR_LOG(sISMLog, PR_LOG_ALWAYS,
    ("ISM: IMEStateManager::CreateIMEContentObserver(), "
     "sPresContext=0x%p, sContent=0x%p, sActiveIMEContentObserver=0x%p, "
     "sActiveIMEContentObserver->IsManaging(sPresContext, sContent)=%s",
     sPresContext, sContent, sActiveIMEContentObserver,
     GetBoolName(sActiveIMEContentObserver ?
       sActiveIMEContentObserver->IsManaging(sPresContext, sContent) : false)));

  if (NS_WARN_IF(sActiveIMEContentObserver)) {
    PR_LOG(sISMLog, PR_LOG_ERROR,
      ("ISM:   IMEStateManager::CreateIMEContentObserver(), FAILED due to "
       "there is already an active IMEContentObserver"));
    MOZ_ASSERT(sActiveIMEContentObserver->IsManaging(sPresContext, sContent));
    return;
  }

  nsCOMPtr<nsIWidget> widget = sPresContext->GetRootWidget();
  if (!widget) {
    PR_LOG(sISMLog, PR_LOG_ERROR,
      ("ISM:   IMEStateManager::CreateIMEContentObserver(), FAILED due to "
       "there is a root widget for the nsPresContext"));
    return; // Sometimes, there are no widgets.
  }

  // If it's not text ediable, we don't need to create IMEContentObserver.
  if (!IsEditableIMEState(widget)) {
    PR_LOG(sISMLog, PR_LOG_DEBUG,
      ("ISM:   IMEStateManager::CreateIMEContentObserver() doesn't create "
       "IMEContentObserver because of non-editable IME state"));
    return;
  }

  static bool sInitializeIsTestingIME = true;
  if (sInitializeIsTestingIME) {
    Preferences::AddBoolVarCache(&sIsTestingIME, "test.IME", false);
    sInitializeIsTestingIME = false;
  }

  PR_LOG(sISMLog, PR_LOG_DEBUG,
    ("ISM:   IMEStateManager::CreateIMEContentObserver() is creating an "
     "IMEContentObserver instance..."));
  sActiveIMEContentObserver = new IMEContentObserver();
  NS_ADDREF(sActiveIMEContentObserver);

  // IMEContentObserver::Init() might create another IMEContentObserver
  // instance.  So, sActiveIMEContentObserver would be replaced with new one.
  // We should hold the current instance here.
  nsRefPtr<IMEContentObserver> kungFuDeathGrip(sActiveIMEContentObserver);
  sActiveIMEContentObserver->Init(widget, sPresContext, sContent);
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
IMEStateManager::GetTextCompositionFor(WidgetGUIEvent* aEvent)
{
  MOZ_ASSERT(aEvent->AsCompositionEvent() || aEvent->AsTextEvent() ||
             aEvent->AsKeyboardEvent(),
             "aEvent has to be WidgetCompositionEvent, WidgetTextEvent or "
             "WidgetKeyboardEvent");
  return GetTextCompositionFor(aEvent->widget);
}

} // namespace mozilla
