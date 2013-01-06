/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMEStateManager.h"
#include "nsCOMPtr.h"
#include "nsViewManager.h"
#include "nsIPresShell.h"
#include "nsISupports.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIEditorDocShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIDOMWindow.h"
#include "nsIDOMMouseEvent.h"
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
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsHTMLFormElement.h"
#include "mozilla/Attributes.h"
#include "nsEventDispatcher.h"
#include "TextComposition.h"
#include "mozilla/Preferences.h"
#include "nsAsyncDOMEvent.h"

using namespace mozilla;
using namespace mozilla::widget;

// nsTextStateManager notifies widget of any text and selection changes
//  in the currently focused editor
// sTextStateObserver points to the currently active nsTextStateManager
// sTextStateObserver is null if there is no focused editor

class nsTextStateManager MOZ_FINAL : public nsISelectionListener,
                                     public nsStubMutationObserver
{
public:
  nsTextStateManager()
    : mObserving(false)
    {
    }

  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONLISTENER
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  void     Init(nsIWidget* aWidget,
                nsPresContext* aPresContext,
                nsIContent* aContent);
  void     Destroy(void);
  bool     IsManaging(nsPresContext* aPresContext, nsIContent* aContent);

  nsCOMPtr<nsIWidget>            mWidget;
  nsCOMPtr<nsISelection>         mSel;
  nsCOMPtr<nsIContent>           mRootContent;
  nsCOMPtr<nsINode>              mEditableNode;

private:
  void NotifyContentAdded(nsINode* aContainer, int32_t aStart, int32_t aEnd);
  void ObserveEditableNode();

  bool mObserving;
  uint32_t mPreAttrChangeLength;
};

/******************************************************************/
/* nsIMEStateManager                                              */
/******************************************************************/

nsIContent*    nsIMEStateManager::sContent      = nullptr;
nsPresContext* nsIMEStateManager::sPresContext  = nullptr;
bool           nsIMEStateManager::sInstalledMenuKeyboardListener = false;
bool           nsIMEStateManager::sInSecureInputMode = false;
bool           nsIMEStateManager::sIsTestingIME = false;

nsTextStateManager* nsIMEStateManager::sTextStateObserver = nullptr;
TextCompositionArray* nsIMEStateManager::sTextCompositions = nullptr;

void
nsIMEStateManager::Shutdown()
{
  MOZ_ASSERT(!sTextCompositions || !sTextCompositions->Length());
  delete sTextCompositions;
  sTextCompositions = nullptr;
}

nsresult
nsIMEStateManager::OnDestroyPresContext(nsPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);

  // First, if there is a composition in the aPresContext, clean up it.
  if (sTextCompositions) {
    TextCompositionArray::index_type i =
      sTextCompositions->IndexOf(aPresContext);
    if (i != TextCompositionArray::NoIndex) {
      // there should be only one composition per presContext object.
      sTextCompositions->RemoveElementAt(i);
      MOZ_ASSERT(sTextCompositions->IndexOf(aPresContext) ==
                   TextCompositionArray::NoIndex);
    }
  }

  if (aPresContext != sPresContext)
    return NS_OK;

  DestroyTextStateManager();

  nsCOMPtr<nsIWidget> widget = sPresContext->GetNearestWidget();
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

nsresult
nsIMEStateManager::OnRemoveContent(nsPresContext* aPresContext,
                                   nsIContent* aContent)
{
  NS_ENSURE_ARG_POINTER(aPresContext);

  // First, if there is a composition in the aContent, clean up it.
  if (sTextCompositions) {
    TextComposition* compositionInContent =
      sTextCompositions->GetCompositionInContent(aPresContext, aContent);

    if (compositionInContent) {
      // Store the composition before accessing the native IME.
      TextComposition storedComposition = *compositionInContent;
      // Try resetting the native IME state.  Be aware, typically, this method
      // is called during the content being removed.  Then, the native
      // composition events which are caused by following APIs are ignored due
      // to unsafe to run script (in PresShell::HandleEvent()).
      nsCOMPtr<nsIWidget> widget = aPresContext->GetNearestWidget();
      if (widget) {
        nsresult rv =
          storedComposition.NotifyIME(REQUEST_TO_CANCEL_COMPOSITION);
        if (NS_FAILED(rv)) {
          storedComposition.NotifyIME(REQUEST_TO_COMMIT_COMPOSITION);
        }
        // By calling the APIs, the composition may have been finished normally.
        compositionInContent =
          sTextCompositions->GetCompositionFor(
                               storedComposition.GetPresContext(),
                               storedComposition.GetEventTargetNode());
      }
    }

    // If the compositionInContent is still available, we should finish the
    // composition just on the content forcibly.
    if (compositionInContent) {
      compositionInContent->SynthesizeCommit(true);
    }
  }

  if (!sPresContext || !sContent ||
      !nsContentUtils::ContentIsDescendantOf(sContent, aContent)) {
    return NS_OK;
  }

  DestroyTextStateManager();

  // Current IME transaction should commit
  nsCOMPtr<nsIWidget> widget = sPresContext->GetNearestWidget();
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

nsresult
nsIMEStateManager::OnChangeFocus(nsPresContext* aPresContext,
                                 nsIContent* aContent,
                                 InputContextAction::Cause aCause)
{
  InputContextAction action(aCause);
  return OnChangeFocusInternal(aPresContext, aContent, action);
}

nsresult
nsIMEStateManager::OnChangeFocusInternal(nsPresContext* aPresContext,
                                         nsIContent* aContent,
                                         InputContextAction aAction)
{
  bool focusActuallyChanging =
    (sContent != aContent || sPresContext != aPresContext);

  nsCOMPtr<nsIWidget> oldWidget =
    sPresContext ? sPresContext->GetNearestWidget() : nullptr;
  if (oldWidget && focusActuallyChanging) {
    // If we're deactivating, we shouldn't commit composition forcibly because
    // the user may want to continue the composition.
    if (aPresContext) {
      NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, oldWidget);
    }
  }

  if (sTextStateObserver &&
      !sTextStateObserver->IsManaging(aPresContext, aContent)) {
    DestroyTextStateManager();
  }

  if (!aPresContext) {
    return NS_OK;
  }

  nsCOMPtr<nsIWidget> widget =
    (sPresContext == aPresContext) ? oldWidget.get() :
                                     aPresContext->GetNearestWidget();
  if (!widget) {
    return NS_OK;
  }

  // Handle secure input mode for password field input.
  bool contentIsPassword = false;
  if (aContent && aContent->GetNameSpaceID() == kNameSpaceID_XHTML) {
    if (aContent->Tag() == nsGkAtoms::input) {
      nsAutoString type;
      aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);
      contentIsPassword = type.LowerCaseEqualsLiteral("password");
    }
  }
  if (sInSecureInputMode) {
    if (!contentIsPassword) {
      if (NS_SUCCEEDED(widget->EndSecureKeyboardInput())) {
        sInSecureInputMode = false;
      }
    }
  } else {
    if (contentIsPassword) {
      if (NS_SUCCEEDED(widget->BeginSecureKeyboardInput())) {
        sInSecureInputMode = true;
      }
    }
  }

  IMEState newState = GetNewIMEState(aPresContext, aContent);
  if (!focusActuallyChanging) {
    // actual focus isn't changing, but if IME enabled state is changing,
    // we should do it.
    InputContext context = widget->GetInputContext();
    if (context.mIMEState.mEnabled == newState.mEnabled) {
      // the enabled state isn't changing.
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

  // Don't call CreateTextStateManager() here, it should be called from
  // focus event handler of editor.

  return NS_OK;
}

void
nsIMEStateManager::OnInstalledMenuKeyboardListener(bool aInstalling)
{
  sInstalledMenuKeyboardListener = aInstalling;

  InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
    aInstalling ? InputContextAction::MENU_GOT_PSEUDO_FOCUS :
                  InputContextAction::MENU_LOST_PSEUDO_FOCUS);
  OnChangeFocusInternal(sPresContext, sContent, action);
}

void
nsIMEStateManager::OnClickInEditor(nsPresContext* aPresContext,
                                   nsIContent* aContent,
                                   nsIDOMMouseEvent* aMouseEvent)
{
  if (sPresContext != aPresContext || sContent != aContent) {
    return;
  }

  nsCOMPtr<nsIWidget> widget = aPresContext->GetNearestWidget();
  NS_ENSURE_TRUE_VOID(widget);

  bool isTrusted;
  nsresult rv = aMouseEvent->GetIsTrusted(&isTrusted);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!isTrusted) {
    return; // ignore untrusted event.
  }

  uint16_t button;
  rv = aMouseEvent->GetButton(&button);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (button != 0) {
    return; // not a left click event.
  }

  int32_t clickCount;
  rv = aMouseEvent->GetDetail(&clickCount);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (clickCount != 1) {
    return; // should notify only first click event.
  }

  InputContextAction action(InputContextAction::CAUSE_MOUSE,
                            InputContextAction::FOCUS_NOT_CHANGED);
  IMEState newState = GetNewIMEState(aPresContext, aContent);
  SetIMEState(newState, aContent, widget, action);
}

void
nsIMEStateManager::OnFocusInEditor(nsPresContext* aPresContext,
                                   nsIContent* aContent)
{
  if (sPresContext != aPresContext || sContent != aContent) {
    return;
  }

  // If the nsTextStateManager instance isn't managing the editor actually,
  // we need to recreate the instance.
  if (sTextStateObserver) {
    if (sTextStateObserver->IsManaging(aPresContext, aContent)) {
      return;
    }
    DestroyTextStateManager();
  }

  CreateTextStateManager();
}

void
nsIMEStateManager::UpdateIMEState(const IMEState &aNewIMEState,
                                  nsIContent* aContent)
{
  if (!sPresContext) {
    NS_WARNING("ISM doesn't know which editor has focus");
    return;
  }
  nsCOMPtr<nsIWidget> widget = sPresContext->GetNearestWidget();
  if (!widget) {
    NS_WARNING("focused widget is not found");
    return;
  }

  // If the nsTextStateManager instance isn't managing the editor's current
  // editable root content, the editor frame might be reframed.  We should
  // recreate the instance at that time.
  bool createTextStateManager =
    (!sTextStateObserver ||
     !sTextStateObserver->IsManaging(sPresContext, aContent));

  bool updateIMEState =
    (widget->GetInputContext().mIMEState.mEnabled != aNewIMEState.mEnabled);

  if (updateIMEState) {
    // commit current composition before modifying IME state.
    NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, widget);
  }

  if (createTextStateManager) {
    DestroyTextStateManager();
  }

  if (updateIMEState) {
    InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                              InputContextAction::FOCUS_NOT_CHANGED);
    SetIMEState(aNewIMEState, aContent, widget, action);
  }

  if (createTextStateManager) {
    CreateTextStateManager();
  }
}

IMEState
nsIMEStateManager::GetNewIMEState(nsPresContext* aPresContext,
                                  nsIContent*    aContent)
{
  // On Printing or Print Preview, we don't need IME.
  if (aPresContext->Type() == nsPresContext::eContext_PrintPreview ||
      aPresContext->Type() == nsPresContext::eContext_Print) {
    return IMEState(IMEState::DISABLED);
  }

  if (sInstalledMenuKeyboardListener) {
    return IMEState(IMEState::DISABLED);
  }

  if (!aContent) {
    // Even if there are no focused content, the focused document might be
    // editable, such case is design mode.
    nsIDocument* doc = aPresContext->Document();
    if (doc && doc->HasFlag(NODE_IS_EDITABLE)) {
      return IMEState(IMEState::ENABLED);
    }
    return IMEState(IMEState::DISABLED);
  }

  return aContent->GetDesiredIMEState();
}

// Helper class, used for IME enabled state change notification
class IMEEnabledStateChangedEvent : public nsRunnable {
public:
  IMEEnabledStateChangedEvent(uint32_t aState)
    : mState(aState)
  {
  }

  NS_IMETHOD Run() {
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    if (observerService) {
      nsAutoString state;
      state.AppendInt(mState);
      observerService->NotifyObservers(nullptr, "ime-enabled-state-changed", state.get());
    }
    return NS_OK;
  }

private:
  uint32_t mState;
};

void
nsIMEStateManager::SetIMEState(const IMEState &aState,
                               nsIContent* aContent,
                               nsIWidget* aWidget,
                               InputContextAction aAction)
{
  NS_ENSURE_TRUE_VOID(aWidget);

  InputContext oldContext = aWidget->GetInputContext();

  InputContext context;
  context.mIMEState = aState;

  if (aContent && aContent->GetNameSpaceID() == kNameSpaceID_XHTML &&
      (aContent->Tag() == nsGkAtoms::input ||
       aContent->Tag() == nsGkAtoms::textarea)) {
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type,
                      context.mHTMLInputType);
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::inputmode,
                      context.mHTMLInputInputmode);
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::moz_action_hint,
                      context.mActionHint);

    // if we don't have an action hint and  return won't submit the form use "next"
    if (context.mActionHint.IsEmpty() && aContent->Tag() == nsGkAtoms::input) {
      bool willSubmit = false;
      nsCOMPtr<nsIFormControl> control(do_QueryInterface(aContent));
      mozilla::dom::Element* formElement = control->GetFormElement();
      nsCOMPtr<nsIForm> form;
      if (control) {
        // is this a form and does it have a default submit element?
        if ((form = do_QueryInterface(formElement)) && form->GetDefaultSubmitElement()) {
          willSubmit = true;
        // is this an html form and does it only have a single text input element?
        } else if (formElement && formElement->Tag() == nsGkAtoms::form && formElement->IsHTML() &&
                   static_cast<nsHTMLFormElement*>(formElement)->HasSingleTextControl()) {
          willSubmit = true;
        }
      }
      context.mActionHint.Assign(willSubmit ? control->GetType() == NS_FORM_INPUT_SEARCH
                                                ? NS_LITERAL_STRING("search")
                                                : NS_LITERAL_STRING("go")
                                            : formElement
                                                ? NS_LITERAL_STRING("next")
                                                : EmptyString());
    }
  }

  // XXX I think that we should use nsContentUtils::IsCallerChrome() instead
  //     of the process type.
  if (aAction.mCause == InputContextAction::CAUSE_UNKNOWN &&
      XRE_GetProcessType() != GeckoProcessType_Content) {
    aAction.mCause = InputContextAction::CAUSE_UNKNOWN_CHROME;
  }

  aWidget->SetInputContext(context, aAction);
  if (oldContext.mIMEState.mEnabled != context.mIMEState.mEnabled) {
    nsContentUtils::AddScriptRunner(
      new IMEEnabledStateChangedEvent(context.mIMEState.mEnabled));
  }
}

void
nsIMEStateManager::EnsureTextCompositionArray()
{
  if (sTextCompositions) {
    return;
  }
  sTextCompositions = new TextCompositionArray();
}

void
nsIMEStateManager::DispatchCompositionEvent(nsINode* aEventTargetNode,
                                            nsPresContext* aPresContext,
                                            nsEvent* aEvent,
                                            nsEventStatus* aStatus,
                                            nsDispatchingCallback* aCallBack)
{
  MOZ_ASSERT(aEvent->eventStructType == NS_COMPOSITION_EVENT ||
             aEvent->eventStructType == NS_TEXT_EVENT);
  if (!aEvent->mFlags.mIsTrusted || aEvent->mFlags.mPropagationStopped) {
    return;
  }

  EnsureTextCompositionArray();

  nsGUIEvent* GUIEvent = static_cast<nsGUIEvent*>(aEvent);

  TextComposition* composition =
    sTextCompositions->GetCompositionFor(GUIEvent->widget);
  if (!composition) {
    MOZ_ASSERT(GUIEvent->message == NS_COMPOSITION_START);
    TextComposition newComposition(aPresContext, aEventTargetNode, GUIEvent);
    composition = sTextCompositions->AppendElement(newComposition);
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
      sTextCompositions->RemoveElementAt(i);
    }
  }
}

// static
nsresult
nsIMEStateManager::NotifyIME(NotificationToIME aNotification,
                             nsIWidget* aWidget)
{
  NS_ENSURE_TRUE(aWidget, NS_ERROR_INVALID_ARG);

  TextComposition* composition = nullptr;
  if (sTextCompositions) {
    composition = sTextCompositions->GetCompositionFor(aWidget);
  }
  if (!composition || !composition->IsSynthesizedForTests()) {
    switch (aNotification) {
      case NOTIFY_IME_OF_CURSOR_POS_CHANGED:
        return aWidget->ResetInputState();
      case REQUEST_TO_COMMIT_COMPOSITION:
        return composition ? aWidget->ResetInputState() : NS_OK;
      case REQUEST_TO_CANCEL_COMPOSITION:
        return composition ? aWidget->CancelIMEComposition() : NS_OK;
      default:
        MOZ_NOT_REACHED("Unsupported notification");
        return NS_ERROR_INVALID_ARG;
    }
    MOZ_NOT_REACHED(
      "Failed to handle the notification for non-synthesized composition");
  }

  // If the composition is synthesized events for automated tests, we should
  // dispatch composition events for emulating the native composition behavior.
  // NOTE: The dispatched events are discarded if it's not safe to run script.
  switch (aNotification) {
    case REQUEST_TO_COMMIT_COMPOSITION: {
      nsCOMPtr<nsIWidget> widget(aWidget);
      TextComposition backup = *composition;

      nsEventStatus status = nsEventStatus_eIgnore;
      if (!backup.GetLastData().IsEmpty()) {
        nsTextEvent textEvent(true, NS_TEXT_TEXT, widget);
        textEvent.theText = backup.GetLastData();
        textEvent.mFlags.mIsSynthesizedForTests = true;
        widget->DispatchEvent(&textEvent, status);
        if (widget->Destroyed()) {
          return NS_OK;
        }
      }

      status = nsEventStatus_eIgnore;
      nsCompositionEvent endEvent(true, NS_COMPOSITION_END, widget);
      endEvent.data = backup.GetLastData();
      endEvent.mFlags.mIsSynthesizedForTests = true;
      widget->DispatchEvent(&endEvent, status);

      return NS_OK;
    }
    case REQUEST_TO_CANCEL_COMPOSITION: {
      nsCOMPtr<nsIWidget> widget(aWidget);
      TextComposition backup = *composition;

      nsEventStatus status = nsEventStatus_eIgnore;
      if (!backup.GetLastData().IsEmpty()) {
        nsCompositionEvent updateEvent(true, NS_COMPOSITION_UPDATE, widget);
        updateEvent.data = backup.GetLastData();
        updateEvent.mFlags.mIsSynthesizedForTests = true;
        widget->DispatchEvent(&updateEvent, status);
        if (widget->Destroyed()) {
          return NS_OK;
        }

        status = nsEventStatus_eIgnore;
        nsTextEvent textEvent(true, NS_TEXT_TEXT, widget);
        textEvent.theText = backup.GetLastData();
        textEvent.mFlags.mIsSynthesizedForTests = true;
        widget->DispatchEvent(&textEvent, status);
        if (widget->Destroyed()) {
          return NS_OK;
        }
      }

      status = nsEventStatus_eIgnore;
      nsCompositionEvent endEvent(true, NS_COMPOSITION_END, widget);
      endEvent.data = backup.GetLastData();
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
nsIMEStateManager::NotifyIME(NotificationToIME aNotification,
                             nsPresContext* aPresContext)
{
  NS_ENSURE_TRUE(aPresContext, NS_ERROR_INVALID_ARG);

  nsIWidget* widget = aPresContext->GetNearestWidget();
  if (!widget) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NotifyIME(aNotification, widget);
}

void
nsTextStateManager::Init(nsIWidget* aWidget,
                         nsPresContext* aPresContext,
                         nsIContent* aContent)
{
  mWidget = aWidget;
  mEditableNode =
    nsIMEStateManager::GetRootEditableNode(aPresContext, aContent);
  if (!mEditableNode) {
    return;
  }

  nsIPresShell* presShell = aPresContext->PresShell();

  // get selection and root content
  nsCOMPtr<nsISelectionController> selCon;
  if (mEditableNode->IsNodeOfType(nsINode::eCONTENT)) {
    nsIFrame* frame =
      static_cast<nsIContent*>(mEditableNode.get())->GetPrimaryFrame();
    NS_ENSURE_TRUE_VOID(frame);

    frame->GetSelectionController(aPresContext,
                                  getter_AddRefs(selCon));
  } else {
    // mEditableNode is a document
    selCon = do_QueryInterface(presShell);
  }
  NS_ENSURE_TRUE_VOID(selCon);

  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                       getter_AddRefs(mSel));
  NS_ENSURE_TRUE_VOID(mSel);

  nsCOMPtr<nsIDOMRange> selDomRange;
  if (NS_SUCCEEDED(mSel->GetRangeAt(0, getter_AddRefs(selDomRange)))) {
    nsRange* selRange = static_cast<nsRange*>(selDomRange.get());
    NS_ENSURE_TRUE_VOID(selRange && selRange->GetStartParent());

    mRootContent = selRange->GetStartParent()->
                     GetSelectionRootContent(presShell);
  } else {
    mRootContent = mEditableNode->GetSelectionRootContent(presShell);
  }
  if (!mRootContent && mEditableNode->IsNodeOfType(nsINode::eDOCUMENT)) {
    // The document node is editable, but there are no contents, this document
    // is not editable.
    return;
  }
  NS_ENSURE_TRUE_VOID(mRootContent);

  if (nsIMEStateManager::sIsTestingIME) {
    nsIDocument* doc = aPresContext->Document();
    (new nsAsyncDOMEvent(doc, NS_LITERAL_STRING("MozIMEFocusIn"),
                         false, false))->RunDOMEventWhenSafe();
  }

  aWidget->OnIMEFocusChange(true);

  // OnIMEFocusChange(true) might cause recreating nsTextStateManager
  // instance via nsIMEStateManager::UpdateIMEState().  So, this
  // instance might already have been destroyed, check it.
  if (!mRootContent) {
    return;
  }

  if (mWidget->GetIMEUpdatePreference().mWantUpdates) {
    ObserveEditableNode();
  }
}

void
nsTextStateManager::ObserveEditableNode()
{
  MOZ_ASSERT(mSel);
  MOZ_ASSERT(mRootContent);

  // add selection change listener
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mSel));
  NS_ENSURE_TRUE_VOID(selPrivate);
  nsresult rv = selPrivate->AddSelectionListener(this);
  NS_ENSURE_SUCCESS_VOID(rv);
  rv = selPrivate->AddSelectionListener(this);
  NS_ENSURE_SUCCESS_VOID(rv);

  // add text change observer
  mRootContent->AddMutationObserver(this);

  mObserving = true;
}

void
nsTextStateManager::Destroy(void)
{
  // If CreateTextStateManager failed, mRootContent will be null,
  // and we should not call OnIMEFocusChange(false)
  if (mRootContent) {
    if (nsIMEStateManager::sIsTestingIME && mEditableNode) {
      nsIDocument* doc = mEditableNode->OwnerDoc();
      (new nsAsyncDOMEvent(doc, NS_LITERAL_STRING("MozIMEFocusOut"),
                           false, false))->RunDOMEventWhenSafe();
    }
    mWidget->OnIMEFocusChange(false);
  }
  // Even if there are some pending notification, it'll never notify the widget.
  mWidget = nullptr;
  if (mObserving && mSel) {
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mSel));
    if (selPrivate)
      selPrivate->RemoveSelectionListener(this);
  }
  mSel = nullptr;
  if (mObserving && mRootContent) {
    mRootContent->RemoveMutationObserver(this);
  }
  mRootContent = nullptr;
  mEditableNode = nullptr;
  mObserving = false;
}

bool
nsTextStateManager::IsManaging(nsPresContext* aPresContext,
                               nsIContent* aContent)
{
  if (!mSel || !mRootContent || !mEditableNode) {
    return false; // failed to initialize.
  }
  if (!mRootContent->IsInDoc()) {
    return false; // the focused editor has already been reframed.
  }
  return mEditableNode == nsIMEStateManager::GetRootEditableNode(aPresContext,
                                                                 aContent);
}

NS_IMPL_ISUPPORTS2(nsTextStateManager,
                   nsIMutationObserver,
                   nsISelectionListener)

// Helper class, used for selection change notification
class SelectionChangeEvent : public nsRunnable {
public:
  SelectionChangeEvent(nsTextStateManager *aDispatcher)
    : mDispatcher(aDispatcher)
  {
    MOZ_ASSERT(mDispatcher);
  }

  NS_IMETHOD Run() {
    if (mDispatcher->mWidget) {
      mDispatcher->mWidget->OnIMESelectionChange();
    }
    return NS_OK;
  }

private:
  nsRefPtr<nsTextStateManager> mDispatcher;
};

nsresult
nsTextStateManager::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                           nsISelection* aSel,
                                           int16_t aReason)
{
  int32_t count = 0;
  nsresult rv = aSel->GetRangeCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);
  if (count > 0 && mWidget) {
    nsContentUtils::AddScriptRunner(new SelectionChangeEvent(this));
  }
  return NS_OK;
}

// Helper class, used for text change notification
class TextChangeEvent : public nsRunnable {
public:
  TextChangeEvent(nsTextStateManager* aDispatcher,
                  uint32_t start, uint32_t oldEnd, uint32_t newEnd)
    : mDispatcher(aDispatcher)
    , mStart(start)
    , mOldEnd(oldEnd)
    , mNewEnd(newEnd)
  {
    MOZ_ASSERT(mDispatcher);
  }

  NS_IMETHOD Run() {
    if (mDispatcher->mWidget) {
      mDispatcher->mWidget->OnIMETextChange(mStart, mOldEnd, mNewEnd);
    }
    return NS_OK;
  }

private:
  nsRefPtr<nsTextStateManager> mDispatcher;
  uint32_t mStart, mOldEnd, mNewEnd;
};

void
nsTextStateManager::CharacterDataChanged(nsIDocument* aDocument,
                                         nsIContent* aContent,
                                         CharacterDataChangeInfo* aInfo)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "character data changed for non-text node");

  uint32_t offset = 0;
  // get offsets of change and fire notification
  if (NS_FAILED(nsContentEventHandler::GetFlatTextOffsetOfRange(
                    mRootContent, aContent, aInfo->mChangeStart, &offset)))
    return;

  uint32_t oldEnd = offset + aInfo->mChangeEnd - aInfo->mChangeStart;
  uint32_t newEnd = offset + aInfo->mReplaceLength;

  nsContentUtils::AddScriptRunner(
      new TextChangeEvent(this, offset, oldEnd, newEnd));
}

void
nsTextStateManager::NotifyContentAdded(nsINode* aContainer,
                                       int32_t aStartIndex,
                                       int32_t aEndIndex)
{
  uint32_t offset = 0, newOffset = 0;
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
        new TextChangeEvent(this, offset, offset, offset + newOffset));
}

void
nsTextStateManager::ContentAppended(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aFirstNewContent,
                                    int32_t aNewIndexInContainer)
{
  NotifyContentAdded(aContainer, aNewIndexInContainer,
                     aContainer->GetChildCount());
}

void
nsTextStateManager::ContentInserted(nsIDocument* aDocument,
                                     nsIContent* aContainer,
                                     nsIContent* aChild,
                                     int32_t aIndexInContainer)
{
  NotifyContentAdded(NODE_FROM(aContainer, aDocument),
                     aIndexInContainer, aIndexInContainer + 1);
}

void
nsTextStateManager::ContentRemoved(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   int32_t aIndexInContainer,
                                   nsIContent* aPreviousSibling)
{
  uint32_t offset = 0, childOffset = 1;
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
        new TextChangeEvent(this, offset, offset + childOffset, offset));
}

static nsIContent*
GetContentBR(mozilla::dom::Element *aElement) {
  if (!aElement->IsNodeOfType(nsINode::eCONTENT)) {
    return nullptr;
  }
  nsIContent *content = static_cast<nsIContent*>(aElement);
  return content->IsHTML(nsGkAtoms::br) ? content : nullptr;
}

void
nsTextStateManager::AttributeWillChange(nsIDocument* aDocument,
                                        mozilla::dom::Element* aElement,
                                        int32_t      aNameSpaceID,
                                        nsIAtom*     aAttribute,
                                        int32_t      aModType)
{
  nsIContent *content = GetContentBR(aElement);
  mPreAttrChangeLength = content ?
    nsContentEventHandler::GetNativeTextLength(content) : 0;
}

void
nsTextStateManager::AttributeChanged(nsIDocument* aDocument,
                                     mozilla::dom::Element* aElement,
                                     int32_t aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     int32_t aModType)
{
  nsIContent *content = GetContentBR(aElement);
  if (!content) {
    return;
  }
  uint32_t postAttrChangeLength =
    nsContentEventHandler::GetNativeTextLength(content);
  if (postAttrChangeLength != mPreAttrChangeLength) {
    uint32_t start;
    if (NS_SUCCEEDED(nsContentEventHandler::GetFlatTextOffsetOfRange(
        mRootContent, content, 0, &start))) {
      nsContentUtils::AddScriptRunner(new TextChangeEvent(this, start,
        start + mPreAttrChangeLength, start + postAttrChangeLength));
    }
  }
}

bool
nsIMEStateManager::IsEditable(nsINode* node)
{
  if (node->IsEditable()) {
    return true;
  }
  // |node| might be readwrite (for example, a text control)
  if (node->IsElement() && node->AsElement()->State().HasState(NS_EVENT_STATE_MOZ_READWRITE)) {
    return true;
  }
  return false;
}

nsINode*
nsIMEStateManager::GetRootEditableNode(nsPresContext* aPresContext,
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
    if (document && document->IsEditable())
      return document;
  }
  return nullptr;
}

bool
nsIMEStateManager::IsEditableIMEState(nsIWidget* aWidget)
{
  switch (aWidget->GetInputContext().mIMEState.mEnabled) {
    case widget::IMEState::ENABLED:
    case widget::IMEState::PASSWORD:
      return true;
    case widget::IMEState::PLUGIN:
    case widget::IMEState::DISABLED:
      return false;
    default:
      MOZ_NOT_REACHED("Unknown IME enable state");
      return false;
  }
}

void
nsIMEStateManager::DestroyTextStateManager()
{
  if (!sTextStateObserver) {
    return;
  }

  nsRefPtr<nsTextStateManager> tsm;
  tsm.swap(sTextStateObserver);
  tsm->Destroy();
}

void
nsIMEStateManager::CreateTextStateManager()
{
  if (sTextStateObserver) {
    NS_WARNING("text state observer has been there already");
    MOZ_ASSERT(sTextStateObserver->IsManaging(sPresContext, sContent));
    return;
  }

  nsCOMPtr<nsIWidget> widget = sPresContext->GetNearestWidget();
  if (!widget) {
    return; // Sometimes, there are no widgets.
  }

  // If it's not text ediable, we don't need to create nsTextStateManager.
  if (!IsEditableIMEState(widget)) {
    return;
  }

  static bool sInitializeIsTestingIME = true;
  if (sInitializeIsTestingIME) {
    Preferences::AddBoolVarCache(&sIsTestingIME, "test.IME", false);
    sInitializeIsTestingIME = false;
  }

  sTextStateObserver = new nsTextStateManager();
  NS_ADDREF(sTextStateObserver);

  // nsTextStateManager::Init() might create another nsTextStateManager
  // instance.  So, sTextStateObserver would be replaced with new one.
  // We should hold the current instance here.
  nsRefPtr<nsTextStateManager> kungFuDeathGrip(sTextStateObserver);
  sTextStateObserver->Init(widget, sPresContext, sContent);
}

nsresult
nsIMEStateManager::GetFocusSelectionAndRoot(nsISelection** aSel,
                                            nsIContent** aRoot)
{
  if (!sTextStateObserver || !sTextStateObserver->mEditableNode ||
      !sTextStateObserver->mSel)
    return NS_ERROR_NOT_AVAILABLE;

  NS_ASSERTION(sTextStateObserver->mSel && sTextStateObserver->mRootContent,
               "uninitialized text state observer");
  NS_ADDREF(*aSel = sTextStateObserver->mSel);
  NS_ADDREF(*aRoot = sTextStateObserver->mRootContent);
  return NS_OK;
}
