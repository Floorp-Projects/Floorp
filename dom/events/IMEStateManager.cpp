/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "mozilla/IMEStateManager.h"

#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_intl.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/ToString.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/UserActivation.h"

#include "HTMLInputElement.h"
#include "IMEContentObserver.h"

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsINode.h"
#include "nsISupports.h"
#include "nsPresContext.h"

namespace mozilla {

using namespace dom;
using namespace widget;

/**
 * When a method is called, log its arguments and/or related static variables
 * with LogLevel::Info.  However, if it puts too many logs like
 * OnDestroyPresContext(), should long only when the method actually does
 * something. In this case, the log should start with "<method name>".
 *
 * When a method quits due to unexpected situation, log the reason with
 * LogLevel::Error.  In this case, the log should start with
 * "<method name>(), FAILED".  The indent makes the log look easier.
 *
 * When a method does something only in some situations and it may be important
 * for debug, log the information with LogLevel::Debug.  In this case, the log
 * should start with "  <method name>(),".
 */
LazyLogModule sISMLog("IMEStateManager");

static const char* GetBoolName(bool aBool) { return aBool ? "true" : "false"; }

StaticRefPtr<nsIContent> IMEStateManager::sContent;
StaticRefPtr<nsPresContext> IMEStateManager::sPresContext;
nsIWidget* IMEStateManager::sWidget = nullptr;
nsIWidget* IMEStateManager::sFocusedIMEWidget = nullptr;
StaticRefPtr<BrowserParent> IMEStateManager::sFocusedIMEBrowserParent;
nsIWidget* IMEStateManager::sActiveInputContextWidget = nullptr;
StaticRefPtr<IMEContentObserver> IMEStateManager::sActiveIMEContentObserver;
TextCompositionArray* IMEStateManager::sTextCompositions = nullptr;
InputContext::Origin IMEStateManager::sOrigin = InputContext::ORIGIN_MAIN;
InputContext IMEStateManager::sActiveChildInputContext;
bool IMEStateManager::sInstalledMenuKeyboardListener = false;
bool IMEStateManager::sIsGettingNewIMEState = false;
bool IMEStateManager::sCleaningUpForStoppingIMEStateManagement = false;
bool IMEStateManager::sIsActive = false;
Maybe<IMEStateManager::PendingFocusedBrowserSwitchingData>
    IMEStateManager::sPendingFocusedBrowserSwitchingData;

// static
void IMEStateManager::Init() {
  sOrigin = XRE_IsParentProcess() ? InputContext::ORIGIN_MAIN
                                  : InputContext::ORIGIN_CONTENT;
  ResetActiveChildInputContext();
}

// static
void IMEStateManager::Shutdown() {
  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("Shutdown(), sTextCompositions=0x%p, sTextCompositions->Length()=%zu, "
       "sPendingFocusedBrowserSwitchingData.isSome()=%s",
       sTextCompositions, sTextCompositions ? sTextCompositions->Length() : 0,
       GetBoolName(sPendingFocusedBrowserSwitchingData.isSome())));

  sPendingFocusedBrowserSwitchingData.reset();
  MOZ_ASSERT(!sTextCompositions || !sTextCompositions->Length());
  delete sTextCompositions;
  sTextCompositions = nullptr;
  // All string instances in the global space need to be empty after XPCOM
  // shutdown.
  sActiveChildInputContext.ShutDown();
}

// static
void IMEStateManager::OnFocusMovedBetweenBrowsers(BrowserParent* aBlur,
                                                  BrowserParent* aFocus) {
  MOZ_ASSERT(aBlur != aFocus);
  MOZ_ASSERT(XRE_IsParentProcess());

  if (sPendingFocusedBrowserSwitchingData.isSome()) {
    MOZ_ASSERT(aBlur ==
               sPendingFocusedBrowserSwitchingData.ref().mBrowserParentFocused);
    // If focus is not changed between browsers actually, we need to do
    // nothing here.  Let's cancel handling what this method does.
    if (sPendingFocusedBrowserSwitchingData.ref().mBrowserParentBlurred ==
        aFocus) {
      sPendingFocusedBrowserSwitchingData.reset();
      MOZ_LOG(sISMLog, LogLevel::Info,
              ("  OnFocusMovedBetweenBrowsers(), canceled all pending focus "
               "moves between browsers"));
      return;
    }
    aBlur = sPendingFocusedBrowserSwitchingData.ref().mBrowserParentBlurred;
    sPendingFocusedBrowserSwitchingData.ref().mBrowserParentFocused = aFocus;
    MOZ_ASSERT(aBlur != aFocus);
  }

  // If application was inactive, but is now activated, and the last focused
  // this is called by BrowserParent::UnsetTopLevelWebFocusAll() from
  // nsFocusManager::WindowRaised().  If a content has focus in a remote
  // process and it has composition, it may get focus back later and the
  // composition shouldn't be commited now.  Therefore, we should put off to
  // handle this until getting another call of this method or a call of
  //`OnFocusChangeInternal()`.
  if (aBlur && !aFocus && !sIsActive && sWidget && sTextCompositions &&
      sTextCompositions->GetCompositionFor(sWidget)) {
    if (sPendingFocusedBrowserSwitchingData.isNothing()) {
      sPendingFocusedBrowserSwitchingData.emplace(aBlur, aFocus);
    }
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnFocusMovedBetweenBrowsers(), put off to handle it until "
             "next OnFocusChangeInternal() call"));
    return;
  }
  sPendingFocusedBrowserSwitchingData.reset();

  nsCOMPtr<nsIWidget> oldWidget = sWidget;
  nsCOMPtr<nsIWidget> newWidget =
      aFocus ? aFocus->GetTextInputHandlingWidget() : nullptr;
  // In the chrome-process case, we'll get sWidget from a PresShell later.
  sWidget = newWidget;
  if (oldWidget && sTextCompositions) {
    RefPtr<TextComposition> composition =
        sTextCompositions->GetCompositionFor(oldWidget);
    if (composition) {
      MOZ_LOG(
          sISMLog, LogLevel::Debug,
          ("  OnFocusMovedBetweenBrowsers(), requesting to commit "
           "composition to "
           "the (previous) focused widget (would request=%s)",
           GetBoolName(
               !oldWidget->IMENotificationRequestsRef().WantDuringDeactive())));
      NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, oldWidget,
                composition->GetBrowserParent());
    }
  }

  // The manager check is to avoid telling the content process to stop
  // IME state management after focus has already moved there between
  // two same-process-hosted out-of-process iframes.
  if (aBlur && (!aFocus || (aBlur->Manager() != aFocus->Manager()))) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnFocusMovedBetweenBrowsers(), notifying previous "
             "focused child process of parent process or another child process "
             "getting focus"));
    aBlur->StopIMEStateManagement();
  }

  if (sActiveIMEContentObserver) {
    DestroyIMEContentObserver();
  }

  if (sFocusedIMEWidget) {
    // sFocusedIMEBrowserParent can be null, if IME focus hasn't been
    // taken before BrowserParent blur.
    // aBlur can be null when keyboard focus moves not actually
    // between tabs but an open menu is involved.
    MOZ_ASSERT(!sFocusedIMEBrowserParent || !aBlur ||
               (sFocusedIMEBrowserParent == aBlur));
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnFocusMovedBetweenBrowsers(), notifying IME of blur"));
    NotifyIME(NOTIFY_IME_OF_BLUR, sFocusedIMEWidget, sFocusedIMEBrowserParent);

    MOZ_ASSERT(!sFocusedIMEBrowserParent);
    MOZ_ASSERT(!sFocusedIMEWidget);

  } else {
    MOZ_ASSERT(!sFocusedIMEBrowserParent);
  }

  // We deliberely don't null out sContent or sPresContext here. When
  // focus is in remote content, as far as layout in the chrome process
  // is concerned, the corresponding content is the top-level XUL
  // browser. Changes among out-of-process iframes don't change that,
  // so dropping the pointer to the XUL browser upon such a change
  // would break IME handling.
}

// static
void IMEStateManager::WidgetDestroyed(nsIWidget* aWidget) {
  if (sWidget == aWidget) {
    sWidget = nullptr;
  }
  if (sFocusedIMEWidget == aWidget) {
    if (sFocusedIMEBrowserParent) {
      OnFocusMovedBetweenBrowsers(sFocusedIMEBrowserParent, nullptr);
      MOZ_ASSERT(!sFocusedIMEBrowserParent);
    }
    sFocusedIMEWidget = nullptr;
  }
  if (sActiveInputContextWidget == aWidget) {
    sActiveInputContextWidget = nullptr;
  }
}

// static
void IMEStateManager::WidgetOnQuit(nsIWidget* aWidget) {
  if (sFocusedIMEWidget == aWidget) {
    // Try to do it the normal way first.
    IMEStateManager::WidgetDestroyed(aWidget);
  }
  // And then in case the normal way didn't work:
  nsCOMPtr<nsIWidget> quittingWidget(aWidget);
  quittingWidget->NotifyIME(IMENotification(NOTIFY_IME_OF_BLUR));
}

// static
void IMEStateManager::StopIMEStateManagement() {
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_LOG(sISMLog, LogLevel::Info, ("StopIMEStateManagement()"));

  // NOTE: Don't set input context from here since this has already lost
  //       the rights to change input context.

  // The requestee of this method in the main process must destroy its
  // active IMEContentObserver for making existing composition end and
  // make it be possible to start new composition in new focused process.
  // Therefore, we shouldn't notify the main process of any changes which
  // occurred after here.
  AutoRestore<bool> restoreStoppingIMEStateManagementState(
      sCleaningUpForStoppingIMEStateManagement);
  sCleaningUpForStoppingIMEStateManagement = true;

  if (sTextCompositions && sPresContext) {
    NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, sPresContext, nullptr);
  }
  sActiveInputContextWidget = nullptr;
  sPresContext = nullptr;
  sContent = nullptr;
  sIsActive = false;
  DestroyIMEContentObserver();
}

// static
void IMEStateManager::MaybeStartOffsetUpdatedInChild(nsIWidget* aWidget,
                                                     uint32_t aStartOffset) {
  if (NS_WARN_IF(!sTextCompositions)) {
    MOZ_LOG(sISMLog, LogLevel::Warning,
            ("MaybeStartOffsetUpdatedInChild(aWidget=0x%p, aStartOffset=%u), "
             "called when there is no composition",
             aWidget, aStartOffset));
    return;
  }

  RefPtr<TextComposition> composition = GetTextCompositionFor(aWidget);
  if (NS_WARN_IF(!composition)) {
    MOZ_LOG(sISMLog, LogLevel::Warning,
            ("MaybeStartOffsetUpdatedInChild(aWidget=0x%p, aStartOffset=%u), "
             "called when there is no composition",
             aWidget, aStartOffset));
    return;
  }

  if (composition->NativeOffsetOfStartComposition() == aStartOffset) {
    return;
  }

  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("MaybeStartOffsetUpdatedInChild(aWidget=0x%p, aStartOffset=%u), "
       "old offset=%u",
       aWidget, aStartOffset, composition->NativeOffsetOfStartComposition()));
  composition->OnStartOffsetUpdatedInChild(aStartOffset);
}

// static
nsresult IMEStateManager::OnDestroyPresContext(nsPresContext* aPresContext) {
  NS_ENSURE_ARG_POINTER(aPresContext);

  // First, if there is a composition in the aPresContext, clean up it.
  if (sTextCompositions) {
    TextCompositionArray::index_type i =
        sTextCompositions->IndexOf(aPresContext);
    if (i != TextCompositionArray::NoIndex) {
      MOZ_LOG(sISMLog, LogLevel::Debug,
              ("  OnDestroyPresContext(), "
               "removing TextComposition instance from the array (index=%zu)",
               i));
      // there should be only one composition per presContext object.
      sTextCompositions->ElementAt(i)->Destroy();
      sTextCompositions->RemoveElementAt(i);
      if (sTextCompositions->IndexOf(aPresContext) !=
          TextCompositionArray::NoIndex) {
        MOZ_LOG(sISMLog, LogLevel::Error,
                ("  OnDestroyPresContext(), FAILED to remove "
                 "TextComposition instance from the array"));
        MOZ_CRASH("Failed to remove TextComposition instance from the array");
      }
    }
  }

  if (aPresContext != sPresContext) {
    return NS_OK;
  }

  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("OnDestroyPresContext(aPresContext=0x%p), "
       "sPresContext=0x%p, sContent=0x%p, sTextCompositions=0x%p",
       aPresContext, sPresContext.get(), sContent.get(), sTextCompositions));

  DestroyIMEContentObserver();

  if (sWidget) {
    IMEState newState = GetNewIMEState(sPresContext, nullptr);
    InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                              InputContextAction::LOST_FOCUS);
    InputContext::Origin origin =
        BrowserParent::GetFocused() ? InputContext::ORIGIN_CONTENT : sOrigin;
    SetIMEState(newState, nullptr, nullptr, sWidget, action, origin);
  }
  sWidget = nullptr;
  sContent = nullptr;
  sPresContext = nullptr;
  return NS_OK;
}

// static
nsresult IMEStateManager::OnRemoveContent(nsPresContext* aPresContext,
                                          nsIContent* aContent) {
  NS_ENSURE_ARG_POINTER(aPresContext);

  // First, if there is a composition in the aContent, clean up it.
  if (sTextCompositions) {
    RefPtr<TextComposition> compositionInContent =
        sTextCompositions->GetCompositionInContent(aPresContext, aContent);

    if (compositionInContent) {
      MOZ_LOG(sISMLog, LogLevel::Debug,
              ("  OnRemoveContent(), "
               "composition is in the content"));

      // Try resetting the native IME state.  Be aware, typically, this method
      // is called during the content being removed.  Then, the native
      // composition events which are caused by following APIs are ignored due
      // to unsafe to run script (in PresShell::HandleEvent()).
      nsresult rv =
          compositionInContent->NotifyIME(REQUEST_TO_CANCEL_COMPOSITION);
      if (NS_FAILED(rv)) {
        compositionInContent->NotifyIME(REQUEST_TO_COMMIT_COMPOSITION);
      }
    }
  }

  if (!sPresContext || !sContent ||
      !sContent->IsInclusiveDescendantOf(aContent)) {
    return NS_OK;
  }

  MOZ_LOG(sISMLog, LogLevel::Info,
          ("OnRemoveContent(aPresContext=0x%p, aContent=0x%p), "
           "sPresContext=0x%p, sContent=0x%p, sTextCompositions=0x%p",
           aPresContext, aContent, sPresContext.get(), sContent.get(),
           sTextCompositions));

  DestroyIMEContentObserver();

  // Current IME transaction should commit
  if (sWidget) {
    IMEState newState = GetNewIMEState(sPresContext, nullptr);
    InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                              InputContextAction::LOST_FOCUS);
    InputContext::Origin origin =
        BrowserParent::GetFocused() ? InputContext::ORIGIN_CONTENT : sOrigin;
    SetIMEState(newState, aPresContext, nullptr, sWidget, action, origin);
  }

  sWidget = nullptr;
  sContent = nullptr;
  sPresContext = nullptr;

  return NS_OK;
}

// static
bool IMEStateManager::CanHandleWith(nsPresContext* aPresContext) {
  return aPresContext && aPresContext->GetPresShell() &&
         !aPresContext->PresShell()->IsDestroying();
}

// static
nsresult IMEStateManager::OnChangeFocus(nsPresContext* aPresContext,
                                        nsIContent* aContent,
                                        InputContextAction::Cause aCause) {
  MOZ_LOG(sISMLog, LogLevel::Info,
          ("OnChangeFocus(aPresContext=0x%p, aContent=0x%p, aCause=%s)",
           aPresContext, aContent, ToString(aCause).c_str()));

  InputContextAction action(aCause);
  return OnChangeFocusInternal(aPresContext, aContent, action);
}

// static
nsresult IMEStateManager::OnChangeFocusInternal(nsPresContext* aPresContext,
                                                nsIContent* aContent,
                                                InputContextAction aAction) {
  bool remoteHasFocus = EventStateManager::IsRemoteTarget(aContent);
  // If we've handled focused content, we were inactive but now active,
  // a remote process has focus, and setting focus to same content in the main
  // process, it means that we're restoring focus without changing DOM focus
  // both in the main process and the remote process.
  const bool restoringContextForRemoteContent =
      XRE_IsParentProcess() && remoteHasFocus && !sIsActive && aPresContext &&
      sPresContext && sContent && sPresContext.get() == aPresContext &&
      sContent.get() == aContent &&
      aAction.mFocusChange != InputContextAction::MENU_GOT_PSEUDO_FOCUS;

  MOZ_LOG(sISMLog, LogLevel::Info,
          ("OnChangeFocusInternal(aPresContext=0x%p (available: %s), "
           "aContent=0x%p (remote: %s), aAction={ mCause=%s, "
           "mFocusChange=%s }), "
           "sPresContext=0x%p (available: %s), sContent=0x%p, "
           "sWidget=0x%p (available: %s), BrowserParent::GetFocused()=0x%p, "
           "sActiveIMEContentObserver=0x%p, sInstalledMenuKeyboardListener=%s, "
           "sIsActive=%s, restoringContextForRemoteContent=%s",
           aPresContext, GetBoolName(CanHandleWith(aPresContext)), aContent,
           GetBoolName(remoteHasFocus), ToString(aAction.mCause).c_str(),
           ToString(aAction.mFocusChange).c_str(), sPresContext.get(),
           GetBoolName(CanHandleWith(sPresContext)), sContent.get(), sWidget,
           GetBoolName(sWidget && !sWidget->Destroyed()),
           BrowserParent::GetFocused(), sActiveIMEContentObserver.get(),
           GetBoolName(sInstalledMenuKeyboardListener), GetBoolName(sIsActive),
           GetBoolName(restoringContextForRemoteContent)));

  sIsActive = !!aPresContext;
  if (sPendingFocusedBrowserSwitchingData.isSome()) {
    MOZ_ASSERT(XRE_IsParentProcess());
    nsCOMPtr<nsIContent> currentContent = sContent.get();
    RefPtr<nsPresContext> currentPresContext = sPresContext.get();
    RefPtr<BrowserParent> browserParentBlurred =
        sPendingFocusedBrowserSwitchingData.ref().mBrowserParentBlurred;
    RefPtr<BrowserParent> browserParentFocused =
        sPendingFocusedBrowserSwitchingData.ref().mBrowserParentFocused;
    OnFocusMovedBetweenBrowsers(browserParentBlurred, browserParentFocused);
    // If another call of this method happens during the
    // OnFocusMovedBetweenBrowsers call, we shouldn't take back focus to
    // the old one.
    if (currentContent != sContent.get() ||
        currentPresContext != sPresContext.get()) {
      MOZ_LOG(sISMLog, LogLevel::Debug,
              ("  OnChangeFocusInternal(aPresContext=0x%p, aContent=0x%p) "
               "stoped handling it because the focused content was changed to "
               "sPresContext=0x%p, sContent=0x%p by another call",
               aPresContext, aContent, sPresContext.get(), sContent.get()));
      return NS_OK;
    }
  }

  // If new aPresShell has been destroyed, this should handle the focus change
  // as nobody is getting focus.
  if (NS_WARN_IF(aPresContext && !CanHandleWith(aPresContext))) {
    MOZ_LOG(sISMLog, LogLevel::Warning,
            ("  OnChangeFocusInternal(), called with destroyed PresShell, "
             "handling this call as nobody getting focus"));
    aPresContext = nullptr;
    aContent = nullptr;
  }

  nsCOMPtr<nsIWidget> oldWidget = sWidget;
  nsCOMPtr<nsIWidget> newWidget =
      aPresContext ? aPresContext->GetTextInputHandlingWidget() : nullptr;
  bool focusActuallyChanging =
      (sContent != aContent || sPresContext != aPresContext ||
       oldWidget != newWidget ||
       (remoteHasFocus && !restoringContextForRemoteContent &&
        (aAction.mFocusChange != InputContextAction::MENU_GOT_PSEUDO_FOCUS)));

  // If old widget has composition, we may need to commit composition since
  // a native IME context is shared on all editors on some widgets or all
  // widgets (it depends on platforms).
  if (oldWidget && focusActuallyChanging && sTextCompositions) {
    RefPtr<TextComposition> composition =
        sTextCompositions->GetCompositionFor(oldWidget);
    if (composition) {
      // However, don't commit the composition if we're being inactivated
      // but the composition should be kept even during deactive.
      // Note that oldWidget and sFocusedIMEWidget may be different here (in
      // such case, sFocusedIMEWidget is perhaps nullptr).  For example, IME
      // may receive only blur notification but still has composition.
      // We need to clean up only the oldWidget's composition state here.
      if (aPresContext ||
          !oldWidget->IMENotificationRequestsRef().WantDuringDeactive()) {
        MOZ_LOG(
            sISMLog, LogLevel::Info,
            ("  OnChangeFocusInternal(), requesting to commit composition to "
             "the (previous) focused widget"));
        NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, oldWidget,
                  composition->GetBrowserParent());
      }
    }
  }

  if (sActiveIMEContentObserver) {
    MOZ_ASSERT(!remoteHasFocus || XRE_IsContentProcess(),
               "IMEContentObserver should have been destroyed by "
               "OnFocusMovedBetweenBrowsers.");
    if (!aPresContext) {
      if (!sActiveIMEContentObserver->KeepAliveDuringDeactive()) {
        DestroyIMEContentObserver();
      }
    }
    // Otherwise, i.e., new focused content is in this process, let's check
    // whether the new focused content is already being managed by the
    // active IME content observer.
    else if (!sActiveIMEContentObserver->IsManaging(aPresContext, aContent)) {
      DestroyIMEContentObserver();
    }
  }

  if (!aPresContext) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnChangeFocusInternal(), no nsPresContext is being activated"));
    return NS_OK;
  }

  if (NS_WARN_IF(!newWidget)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  OnChangeFocusInternal(), FAILED due to no widget to manage its "
             "IME state"));
    return NS_OK;
  }

  // Update the cached widget since root view of the presContext may be
  // changed to different view.
  sWidget = newWidget;

  // If a child process has focus, we should disable IME state until the child
  // process actually gets focus because if user types keys before that they
  // are handled by IME.
  IMEState newState = remoteHasFocus ? IMEState(IMEEnabled::Disabled)
                                     : GetNewIMEState(aPresContext, aContent);
  bool setIMEState = true;

  if (remoteHasFocus && XRE_IsParentProcess()) {
    if (aAction.mFocusChange == InputContextAction::MENU_GOT_PSEUDO_FOCUS) {
      // If menu keyboard listener is installed, we need to disable IME now.
      setIMEState = true;
    } else if (aAction.mFocusChange ==
               InputContextAction::MENU_LOST_PSEUDO_FOCUS) {
      // If menu keyboard listener is uninstalled, we need to restore
      // input context which was set by the remote process.  However, if
      // the remote process hasn't been set input context yet, we need to
      // wait next SetInputContextForChildProcess() call.
      if (HasActiveChildSetInputContext()) {
        setIMEState = true;
        newState = sActiveChildInputContext.mIMEState;
      } else {
        setIMEState = false;
      }
    } else if (focusActuallyChanging) {
      InputContext context = newWidget->GetInputContext();
      if (context.mIMEState.mEnabled == IMEEnabled::Disabled &&
          context.mOrigin == InputContext::ORIGIN_CONTENT) {
        setIMEState = false;
        MOZ_LOG(sISMLog, LogLevel::Debug,
                ("  OnChangeFocusInternal(), doesn't set IME state because "
                 "focused element (or document) is in a child process and the "
                 "IME state is already disabled by a remote process"));
      } else {
        // When new remote process gets focus, we should forget input context
        // coming from old focused remote process.
        ResetActiveChildInputContext();
        MOZ_LOG(sISMLog, LogLevel::Debug,
                ("  OnChangeFocusInternal(), will disable IME until new "
                 "focused element (or document) in the child process will get "
                 "focus actually"));
      }
    } else if (newWidget->GetInputContext().mOrigin !=
               InputContext::ORIGIN_MAIN) {
      // When focus is NOT changed actually, we shouldn't set IME state if
      // current input context was set by a remote process since that means
      // that the window is being activated and the child process may have
      // composition.  Then, we shouldn't commit the composition with making
      // IME state disabled.
      setIMEState = false;
      MOZ_LOG(
          sISMLog, LogLevel::Debug,
          ("  OnChangeFocusInternal(), doesn't set IME state because focused "
           "element (or document) is already in the child process"));
    }
  } else {
    // When this process gets focus, we should forget input context coming
    // from remote process.
    ResetActiveChildInputContext();
  }

  if (setIMEState) {
    if (!focusActuallyChanging) {
      // actual focus isn't changing, but if IME enabled state is changing,
      // we should do it.
      InputContext context = newWidget->GetInputContext();
      if (context.mIMEState.mEnabled == newState.mEnabled) {
        MOZ_LOG(sISMLog, LogLevel::Debug,
                ("  OnChangeFocusInternal(), neither focus nor IME state is "
                 "changing"));
        return NS_OK;
      }
      aAction.mFocusChange = InputContextAction::FOCUS_NOT_CHANGED;

      // Even if focus isn't changing actually, we should commit current
      // composition here since the IME state is changing.
      if (sPresContext && oldWidget && !focusActuallyChanging) {
        NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, oldWidget,
                  sFocusedIMEBrowserParent);
      }
    } else if (aAction.mFocusChange == InputContextAction::FOCUS_NOT_CHANGED) {
      // If aContent isn't null or aContent is null but editable, somebody gets
      // focus.
      bool gotFocus = aContent || (newState.mEnabled == IMEEnabled::Enabled);
      aAction.mFocusChange = gotFocus ? InputContextAction::GOT_FOCUS
                                      : InputContextAction::LOST_FOCUS;
    }

    if (remoteHasFocus && HasActiveChildSetInputContext() &&
        aAction.mFocusChange == InputContextAction::MENU_LOST_PSEUDO_FOCUS) {
      // Restore the input context in the active remote process when
      // menu keyboard listener is uninstalled and active remote tab has
      // focus.
      SetInputContext(newWidget, sActiveChildInputContext, aAction);
    } else {
      // Update IME state for new focus widget
      SetIMEState(newState, aPresContext, aContent, newWidget, aAction,
                  remoteHasFocus ? InputContext::ORIGIN_CONTENT : sOrigin);
    }
  }

  sPresContext = aPresContext;
  sContent = aContent;

  // Don't call CreateIMEContentObserver() here  because it will be called from
  // the focus event handler of focused editor.

  return NS_OK;
}

// static
void IMEStateManager::OnInstalledMenuKeyboardListener(bool aInstalling) {
  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("OnInstalledMenuKeyboardListener(aInstalling=%s), "
       "sInstalledMenuKeyboardListener=%s, BrowserParent::GetFocused()=0x%p, "
       "sActiveChildInputContext=%s",
       GetBoolName(aInstalling), GetBoolName(sInstalledMenuKeyboardListener),
       BrowserParent::GetFocused(),
       ToString(sActiveChildInputContext).c_str()));

  sInstalledMenuKeyboardListener = aInstalling;

  InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                            aInstalling
                                ? InputContextAction::MENU_GOT_PSEUDO_FOCUS
                                : InputContextAction::MENU_LOST_PSEUDO_FOCUS);
  OnChangeFocusInternal(sPresContext, sContent, action);
}

// static
bool IMEStateManager::OnMouseButtonEventInEditor(
    nsPresContext* aPresContext, nsIContent* aContent,
    WidgetMouseEvent* aMouseEvent) {
  MOZ_LOG(sISMLog, LogLevel::Info,
          ("OnMouseButtonEventInEditor(aPresContext=0x%p, "
           "aContent=0x%p, aMouseEvent=0x%p), sPresContext=0x%p, sContent=0x%p",
           aPresContext, aContent, aMouseEvent, sPresContext.get(),
           sContent.get()));

  if (NS_WARN_IF(!aMouseEvent)) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnMouseButtonEventInEditor(), aMouseEvent is nullptr"));
    return false;
  }

  if (sPresContext != aPresContext || sContent != aContent) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnMouseButtonEventInEditor(), "
             "the mouse event isn't fired on the editor managed by ISM"));
    return false;
  }

  if (!sActiveIMEContentObserver) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnMouseButtonEventInEditor(), "
             "there is no active IMEContentObserver"));
    return false;
  }

  if (!sActiveIMEContentObserver->IsManaging(aPresContext, aContent)) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnMouseButtonEventInEditor(), "
             "the active IMEContentObserver isn't managing the editor"));
    return false;
  }

  RefPtr<IMEContentObserver> observer = sActiveIMEContentObserver;
  bool consumed = observer->OnMouseButtonEvent(aPresContext, aMouseEvent);

  if (MOZ_LOG_TEST(sISMLog, LogLevel::Info)) {
    nsAutoString eventType;
    MOZ_LOG(sISMLog, LogLevel::Info,
            ("  OnMouseButtonEventInEditor(), "
             "mouse event (mMessage=%s, mButton=%d) is %s",
             ToChar(aMouseEvent->mMessage), aMouseEvent->mButton,
             consumed ? "consumed" : "not consumed"));
  }

  return consumed;
}

// static
void IMEStateManager::OnClickInEditor(nsPresContext* aPresContext,
                                      nsIContent* aContent,
                                      const WidgetMouseEvent* aMouseEvent) {
  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("OnClickInEditor(aPresContext=0x%p, aContent=0x%p, aMouseEvent=0x%p), "
       "sPresContext=0x%p, sContent=0x%p, sWidget=0x%p (available: %s)",
       aPresContext, aContent, aMouseEvent, sPresContext.get(), sContent.get(),
       sWidget, GetBoolName(sWidget && !sWidget->Destroyed())));

  if (NS_WARN_IF(!aMouseEvent)) {
    return;
  }

  if (sPresContext != aPresContext || sContent != aContent ||
      NS_WARN_IF(!sPresContext) || NS_WARN_IF(!sWidget) ||
      NS_WARN_IF(sWidget->Destroyed())) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnClickInEditor(), "
             "the mouse event isn't fired on the editor managed by ISM"));
    return;
  }

  nsCOMPtr<nsIWidget> widget(sWidget);

  MOZ_ASSERT(!sPresContext->GetTextInputHandlingWidget() ||
             sPresContext->GetTextInputHandlingWidget() == widget);

  if (!aMouseEvent->IsTrusted()) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnClickInEditor(), "
             "the mouse event isn't a trusted event"));
    return;  // ignore untrusted event.
  }

  if (aMouseEvent->mButton) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnClickInEditor(), "
             "the mouse event isn't a left mouse button event"));
    return;  // not a left click event.
  }

  if (aMouseEvent->mClickCount != 1) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnClickInEditor(), "
             "the mouse event isn't a single click event"));
    return;  // should notify only first click event.
  }

  InputContextAction::Cause cause =
      aMouseEvent->mInputSource == MouseEvent_Binding::MOZ_SOURCE_TOUCH
          ? InputContextAction::CAUSE_TOUCH
          : InputContextAction::CAUSE_MOUSE;

  InputContextAction action(cause, InputContextAction::FOCUS_NOT_CHANGED);
  IMEState newState = GetNewIMEState(aPresContext, aContent);
  SetIMEState(newState, aPresContext, aContent, widget, action, sOrigin);
}

// static
void IMEStateManager::OnFocusInEditor(nsPresContext* aPresContext,
                                      nsIContent* aContent,
                                      EditorBase& aEditorBase) {
  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("OnFocusInEditor(aPresContext=0x%p, aContent=0x%p, aEditorBase=0x%p), "
       "sPresContext=0x%p, sContent=0x%p, sActiveIMEContentObserver=0x%p",
       aPresContext, aContent, &aEditorBase, sPresContext.get(), sContent.get(),
       sActiveIMEContentObserver.get()));

  if (sPresContext != aPresContext || sContent != aContent) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnFocusInEditor(), "
             "an editor not managed by ISM gets focus"));
    return;
  }

  // If the IMEContentObserver instance isn't managing the editor actually,
  // we need to recreate the instance.
  if (sActiveIMEContentObserver) {
    if (sActiveIMEContentObserver->IsManaging(aPresContext, aContent)) {
      MOZ_LOG(
          sISMLog, LogLevel::Debug,
          ("  OnFocusInEditor(), "
           "the editor is already being managed by sActiveIMEContentObserver"));
      return;
    }
    DestroyIMEContentObserver();
  }

  CreateIMEContentObserver(aEditorBase);

  // Let's flush the focus notification now.
  if (sActiveIMEContentObserver) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnFocusInEditor(), new IMEContentObserver is "
             "created, trying to flush pending notifications..."));
    sActiveIMEContentObserver->TryToFlushPendingNotifications(false);
  }
}

// static
void IMEStateManager::OnEditorInitialized(EditorBase& aEditorBase) {
  if (!sActiveIMEContentObserver ||
      !sActiveIMEContentObserver->WasInitializedWith(aEditorBase)) {
    return;
  }

  MOZ_LOG(sISMLog, LogLevel::Info,
          ("OnEditorInitialized(aEditorBase=0x%p)", &aEditorBase));

  sActiveIMEContentObserver->UnsuppressNotifyingIME();
}

// static
void IMEStateManager::OnEditorDestroying(EditorBase& aEditorBase) {
  if (!sActiveIMEContentObserver ||
      !sActiveIMEContentObserver->WasInitializedWith(aEditorBase)) {
    return;
  }

  MOZ_LOG(sISMLog, LogLevel::Info,
          ("OnEditorDestroying(aEditorBase=0x%p)", &aEditorBase));

  // The IMEContentObserver shouldn't notify IME of anything until reframing
  // is finished.
  sActiveIMEContentObserver->SuppressNotifyingIME();
}

void IMEStateManager::OnReFocus(nsPresContext* aPresContext,
                                nsIContent& aContent) {
  MOZ_LOG(sISMLog, LogLevel::Info,
          ("OnReFocus(aPresContext=0x%p, aContent=0x%p), "
           "sActiveIMEContentObserver=0x%p, sContent=0x%p",
           aPresContext, &aContent, sActiveIMEContentObserver.get(),
           sContent.get()));

  if (NS_WARN_IF(!sWidget) || NS_WARN_IF(sWidget->Destroyed())) {
    return;
  }

  if (!sActiveIMEContentObserver ||
      !sActiveIMEContentObserver->IsManaging(aPresContext, &aContent)) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  OnReFocus(), there is no valid IMEContentObserver, so we don't "
             "manage this. Ignore this"));
    return;
  }

  MOZ_ASSERT(&aContent == sContent.get());

  if (!UserActivation::IsHandlingUserInput() ||
      UserActivation::IsHandlingKeyboardInput()) {
    return;
  }

  nsCOMPtr<nsIWidget> widget(sWidget);

  // Don't update IME state during composition.
  if (sTextCompositions) {
    if (TextComposition* composition =
            sTextCompositions->GetCompositionFor(widget)) {
      if (composition->IsComposing()) {
        return;
      }
    }
  }

  InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                            InputContextAction::FOCUS_NOT_CHANGED);
  IMEState newState = GetNewIMEState(aPresContext, &aContent);
  SetIMEState(newState, aPresContext, &aContent, widget, action, sOrigin);
}

// static
void IMEStateManager::UpdateIMEState(const IMEState& aNewIMEState,
                                     nsIContent* aContent,
                                     EditorBase& aEditorBase) {
  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("UpdateIMEState(aNewIMEState=%s, aContent=0x%p, aEditorBase=0x%p), "
       "sPresContext=0x%p, sContent=0x%p, sWidget=0x%p (available: %s), "
       "sActiveIMEContentObserver=0x%p, sIsGettingNewIMEState=%s",
       ToString(aNewIMEState).c_str(), aContent, &aEditorBase,
       sPresContext.get(), sContent.get(), sWidget,
       GetBoolName(sWidget && !sWidget->Destroyed()),
       sActiveIMEContentObserver.get(), GetBoolName(sIsGettingNewIMEState)));

  if (sIsGettingNewIMEState) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  UpdateIMEState(), "
             "does nothing because of called while getting new IME state"));
    return;
  }

  RefPtr<PresShell> presShell(aEditorBase.GetPresShell());
  if (NS_WARN_IF(!presShell)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  UpdateIMEState(), FAILED due to "
             "editor doesn't have PresShell"));
    return;
  }

  nsPresContext* presContext = presShell->GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  UpdateIMEState(), FAILED due to "
             "editor doesn't have PresContext"));
    return;
  }

  // IMEStateManager::UpdateIMEState() should be called after
  // IMEStateManager::OnChangeFocus() is called for setting focus to aContent
  // and aEditorBase.  However, when aEditorBase is an HTMLEditor, this may be
  // called by nsIEditor::PostCreate() before IMEStateManager::OnChangeFocus().
  // Similarly, when aEditorBase is a TextEditor, this may be called by
  // nsIEditor::SetFlags().  In such cases, this method should do nothing
  // because input context should be updated when
  // IMEStateManager::OnChangeFocus() is called later.
  if (sPresContext != presContext) {
    MOZ_LOG(sISMLog, LogLevel::Warning,
            ("  UpdateIMEState(), does nothing due to "
             "the editor hasn't managed by IMEStateManager yet"));
    return;
  }

  // If IMEStateManager doesn't manage any document, this cannot update IME
  // state of any widget.
  if (NS_WARN_IF(!sPresContext)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  UpdateIMEState(), FAILED due to "
             "no managing nsPresContext"));
    return;
  }

  if (NS_WARN_IF(!sWidget) || NS_WARN_IF(sWidget->Destroyed())) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  UpdateIMEState(), FAILED due to "
             "the widget for the managing nsPresContext has gone"));
    return;
  }

  OwningNonNull<nsIWidget> widget(*sWidget);

  MOZ_ASSERT(!sPresContext->GetTextInputHandlingWidget() ||
             sPresContext->GetTextInputHandlingWidget() == widget);

  // TODO: Investigate if we could put off to initialize IMEContentObserver
  //       later because a lot of callers need to be marked as
  //       MOZ_CAN_RUN_SCRIPT otherwise.

  // Even if there is active IMEContentObserver, it may not be observing the
  // editor with current editable root content due to reframed.  In such case,
  // We should try to reinitialize the IMEContentObserver.
  if (sActiveIMEContentObserver && IsIMEObserverNeeded(aNewIMEState)) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  UpdateIMEState(), try to reinitialize the "
             "active IMEContentObserver"));
    RefPtr<IMEContentObserver> contentObserver = sActiveIMEContentObserver;
    OwningNonNull<nsPresContext> presContext(*sPresContext);
    if (!contentObserver->MaybeReinitialize(widget, presContext, aContent,
                                            aEditorBase)) {
      MOZ_LOG(sISMLog, LogLevel::Error,
              ("  UpdateIMEState(), failed to reinitialize the "
               "active IMEContentObserver"));
    }
    if (NS_WARN_IF(widget->Destroyed())) {
      MOZ_LOG(sISMLog, LogLevel::Error,
              ("  UpdateIMEState(), widget has gone during reinitializing the "
               "active IMEContentObserver"));
      return;
    }
  }

  // If there is no active IMEContentObserver or it isn't observing the
  // editor correctly, we should recreate it.
  bool createTextStateManager =
      (!sActiveIMEContentObserver ||
       !sActiveIMEContentObserver->IsManaging(sPresContext, aContent));

  bool updateIMEState =
      (widget->GetInputContext().mIMEState.mEnabled != aNewIMEState.mEnabled);
  if (NS_WARN_IF(widget->Destroyed())) {
    MOZ_LOG(
        sISMLog, LogLevel::Error,
        ("  UpdateIMEState(), widget has gone during getting input context"));
    return;
  }

  if (updateIMEState) {
    // commit current composition before modifying IME state.
    NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, widget, sFocusedIMEBrowserParent);
    if (NS_WARN_IF(widget->Destroyed())) {
      MOZ_LOG(sISMLog, LogLevel::Error,
              ("  UpdateIMEState(), widget has gone during committing "
               "composition"));
      return;
    }
  }

  if (createTextStateManager) {
    DestroyIMEContentObserver();
  }

  if (updateIMEState) {
    InputContextAction action(InputContextAction::CAUSE_UNKNOWN,
                              InputContextAction::FOCUS_NOT_CHANGED);
    SetIMEState(aNewIMEState, presContext, aContent, widget, action, sOrigin);
    if (NS_WARN_IF(widget->Destroyed())) {
      MOZ_LOG(
          sISMLog, LogLevel::Error,
          ("  UpdateIMEState(), widget has gone during setting input context"));
      return;
    }
  }

  // XXX Update sContent when aContent is focused content?
  NS_ASSERTION(sContent.get() == aContent,
               "sContent and aContent are mismatched.");

  if (createTextStateManager) {
    // XXX In this case, it might not be enough safe to notify IME of anything.
    //     So, don't try to flush pending notifications of IMEContentObserver
    //     here.
    CreateIMEContentObserver(aEditorBase);
  }
}

// static
IMEState IMEStateManager::GetNewIMEState(nsPresContext* aPresContext,
                                         nsIContent* aContent) {
  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("GetNewIMEState(aPresContext=0x%p, aContent=0x%p), "
       "sInstalledMenuKeyboardListener=%s",
       aPresContext, aContent, GetBoolName(sInstalledMenuKeyboardListener)));

  if (!CanHandleWith(aPresContext)) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  GetNewIMEState() returns IMEEnabled::Disabled because "
             "the nsPresContext has been destroyed"));
    return IMEState(IMEEnabled::Disabled);
  }

  // On Printing or Print Preview, we don't need IME.
  if (aPresContext->Type() == nsPresContext::eContext_PrintPreview ||
      aPresContext->Type() == nsPresContext::eContext_Print) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  GetNewIMEState() returns IMEEnabled::Disabled because "
             "the nsPresContext is for print or print preview"));
    return IMEState(IMEEnabled::Disabled);
  }

  if (sInstalledMenuKeyboardListener) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  GetNewIMEState() returns IMEEnabled::Disabled because "
             "menu keyboard listener was installed"));
    return IMEState(IMEEnabled::Disabled);
  }

  if (!aContent) {
    // Even if there are no focused content, the focused document might be
    // editable, such case is design mode.
    Document* doc = aPresContext->Document();
    if (doc && doc->HasFlag(NODE_IS_EDITABLE)) {
      MOZ_LOG(sISMLog, LogLevel::Debug,
              ("  GetNewIMEState() returns IMEEnabled::Enabled because "
               "design mode editor has focus"));
      return IMEState(IMEEnabled::Enabled);
    }
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  GetNewIMEState() returns IMEEnabled::Disabled because "
             "no content has focus"));
    return IMEState(IMEEnabled::Disabled);
  }

  // nsIContent::GetDesiredIMEState() may cause a call of UpdateIMEState()
  // from EditorBase::PostCreate() because GetDesiredIMEState() needs to
  // retrieve an editor instance for the element if it's editable element.
  // For avoiding such nested IME state updates, we should set
  // sIsGettingNewIMEState here and UpdateIMEState() should check it.
  GettingNewIMEStateBlocker blocker;

  IMEState newIMEState = aContent->GetDesiredIMEState();
  MOZ_LOG(sISMLog, LogLevel::Debug,
          ("  GetNewIMEState() returns %s", ToString(newIMEState).c_str()));
  return newIMEState;
}

static bool MayBeIMEUnawareWebApp(nsINode* aNode) {
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
void IMEStateManager::ResetActiveChildInputContext() {
  sActiveChildInputContext.mIMEState.mEnabled = IMEEnabled::Unknown;
}

// static
bool IMEStateManager::HasActiveChildSetInputContext() {
  return sActiveChildInputContext.mIMEState.mEnabled != IMEEnabled::Unknown;
}

// static
void IMEStateManager::SetInputContextForChildProcess(
    BrowserParent* aBrowserParent, const InputContext& aInputContext,
    const InputContextAction& aAction) {
  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("SetInputContextForChildProcess(aBrowserParent=0x%p, "
       "aInputContext=%s , aAction={ mCause=%s, mAction=%s }), "
       "sPresContext=0x%p (available: %s), sWidget=0x%p (available: %s), "
       "BrowserParent::GetFocused()=0x%p, sInstalledMenuKeyboardListener=%s",
       aBrowserParent, ToString(aInputContext).c_str(),
       ToString(aAction.mCause).c_str(), ToString(aAction.mFocusChange).c_str(),
       sPresContext.get(), GetBoolName(CanHandleWith(sPresContext)), sWidget,
       GetBoolName(sWidget && !sWidget->Destroyed()),
       BrowserParent::GetFocused(),
       GetBoolName(sInstalledMenuKeyboardListener)));

  if (aBrowserParent != BrowserParent::GetFocused()) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  SetInputContextForChildProcess(), FAILED, "
             "because non-focused tab parent tries to set input context"));
    return;
  }

  if (NS_WARN_IF(!CanHandleWith(sPresContext))) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  SetInputContextForChildProcess(), FAILED, "
             "due to no focused presContext"));
    return;
  }

  if (NS_WARN_IF(!sWidget) || NS_WARN_IF(sWidget->Destroyed())) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  SetInputContextForChildProcess(), FAILED, "
             "due to the widget for the nsPresContext has gone"));
    return;
  }

  nsCOMPtr<nsIWidget> widget(sWidget);

  MOZ_ASSERT(!sPresContext->GetTextInputHandlingWidget() ||
             sPresContext->GetTextInputHandlingWidget() == widget);
  MOZ_ASSERT(aInputContext.mOrigin == InputContext::ORIGIN_CONTENT);

  sActiveChildInputContext = aInputContext;
  MOZ_ASSERT(HasActiveChildSetInputContext());

  // If input context is changed in remote process while menu keyboard listener
  // is installed, this process shouldn't set input context now.  When it's
  // uninstalled, input context should be restored from
  // sActiveChildInputContext.
  if (sInstalledMenuKeyboardListener) {
    MOZ_LOG(sISMLog, LogLevel::Info,
            ("  SetInputContextForChildProcess(), waiting to set input context "
             "until menu keyboard listener is uninstalled"));
    return;
  }

  SetInputContext(widget, aInputContext, aAction);
}

static bool IsNextFocusableElementTextControl(Element* aInputContent) {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return false;
  }
  nsCOMPtr<nsIContent> nextContent;
  nsresult rv = fm->DetermineElementToMoveFocus(
      aInputContent->OwnerDoc()->GetWindow(), aInputContent,
      nsIFocusManager::MOVEFOCUS_FORWARD, true, false,
      getter_AddRefs(nextContent));
  if (NS_WARN_IF(NS_FAILED(rv)) || !nextContent) {
    return false;
  }
  nextContent = nextContent->FindFirstNonChromeOnlyAccessContent();
  nsCOMPtr<nsIFormControl> nextControl = do_QueryInterface(nextContent);
  if (!nextControl || !nextControl->IsTextControl(false)) {
    return false;
  }

  // XXX We don't consider next element is date/time control yet.
  nsGenericHTMLElement* nextElement =
      nsGenericHTMLElement::FromNodeOrNull(nextContent);
  if (!nextElement) {
    return false;
  }
  bool focusable = false;
  nextElement->IsHTMLFocusable(false, &focusable, nullptr);
  if (!focusable) {
    return false;
  }

  // Check readonly attribute.
  if (nextElement->IsHTMLElement(nsGkAtoms::textarea)) {
    HTMLTextAreaElement* textAreaElement =
        HTMLTextAreaElement::FromNodeOrNull(nextElement);
    return !textAreaElement->ReadOnly();
  }

  // If neither textarea nor input, what element type?
  MOZ_DIAGNOSTIC_ASSERT(nextElement->IsHTMLElement(nsGkAtoms::input));

  HTMLInputElement* inputElement =
      HTMLInputElement::FromNodeOrNull(nextElement);
  return !inputElement->ReadOnly();
}

static void GetActionHint(nsIContent& aContent, nsAString& aActionHint) {
  // If enterkeyhint is set, we don't infer action hint.
  if (!aActionHint.IsEmpty()) {
    return;
  }

  // XXX This is old compatibility, but we might be able to remove this.
  aContent.AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::moz_action_hint,
                                aActionHint);

  if (!aActionHint.IsEmpty()) {
    ToLowerCase(aActionHint);
    return;
  }

  // Get the input content corresponding to the focused node,
  // which may be an anonymous child of the input content.
  nsIContent* inputContent = aContent.FindFirstNonChromeOnlyAccessContent();
  if (!inputContent->IsHTMLElement(nsGkAtoms::input)) {
    return;
  }

  // If we don't have an action hint and
  // return won't submit the form, use "maybenext".
  bool willSubmit = false;
  bool isLastElement = false;
  nsCOMPtr<nsIFormControl> control(do_QueryInterface(inputContent));
  if (control) {
    HTMLFormElement* formElement = control->GetFormElement();
    // is this a form and does it have a default submit element?
    if (formElement) {
      if (formElement->IsLastActiveElement(control)) {
        isLastElement = true;
      }

      if (formElement->GetDefaultSubmitElement()) {
        willSubmit = true;
        // is this an html form...
      } else {
        // ... and does it only have a single text input element ?
        if (!formElement->ImplicitSubmissionIsDisabled() ||
            // ... or is this the last non-disabled element?
            isLastElement) {
          willSubmit = true;
        }
      }
    }

    if (!isLastElement && formElement) {
      // If next tabbable content in form is text control, hint should be "next"
      // even there is submit in form.
      if (IsNextFocusableElementTextControl(inputContent->AsElement())) {
        // This is focusable text control
        // XXX What good hint for read only field?
        aActionHint.AssignLiteral("maybenext");
        return;
      }
    }
  }

  if (!willSubmit) {
    return;
  }

  if (control->ControlType() == NS_FORM_INPUT_SEARCH) {
    aActionHint.AssignLiteral("search");
    return;
  }

  aActionHint.AssignLiteral("go");
}

// static
void IMEStateManager::SetIMEState(const IMEState& aState,
                                  nsPresContext* aPresContext,
                                  nsIContent* aContent, nsIWidget* aWidget,
                                  InputContextAction aAction,
                                  InputContext::Origin aOrigin) {
  MOZ_LOG(sISMLog, LogLevel::Info,
          ("SetIMEState(aState=%s, aContent=0x%p (BrowserParent=0x%p), "
           "aWidget=0x%p, aAction={ mCause=%s, mFocusChange=%s }, aOrigin=%s)",
           ToString(aState).c_str(), aContent, BrowserParent::GetFrom(aContent),
           aWidget, ToString(aAction.mCause).c_str(),
           ToString(aAction.mFocusChange).c_str(), ToChar(aOrigin)));

  NS_ENSURE_TRUE_VOID(aWidget);

  InputContext context;
  context.mIMEState = aState;
  context.mOrigin = aOrigin;
  context.mMayBeIMEUnaware =
      context.mIMEState.IsEditable() &&
      StaticPrefs::
          intl_ime_hack_on_ime_unaware_apps_fire_key_events_for_composition() &&
      MayBeIMEUnawareWebApp(aContent);

  context.mHasHandledUserInput =
      aPresContext && aPresContext->PresShell()->HasHandledUserInput();

  context.mInPrivateBrowsing =
      aPresContext &&
      nsContentUtils::IsInPrivateBrowsing(aPresContext->Document());

  if (aContent && aContent->IsHTMLElement()) {
    if (aState.IsEditable() && StaticPrefs::dom_forms_enterkeyhint()) {
      nsGenericHTMLElement::FromNode(aContent)->GetEnterKeyHint(
          context.mActionHint);
    }

    if (aContent->IsHTMLElement(nsGkAtoms::input)) {
      HTMLInputElement* inputElement = HTMLInputElement::FromNode(aContent);
      if (inputElement->HasBeenTypePassword() && aState.IsEditable()) {
        context.mHTMLInputType.AssignLiteral("password");
      } else {
        inputElement->GetType(context.mHTMLInputType);
      }

      GetActionHint(*aContent, context.mActionHint);
    } else if (aContent->IsHTMLElement(nsGkAtoms::textarea)) {
      context.mHTMLInputType.Assign(nsGkAtoms::textarea->GetUTF16String());
      GetActionHint(*aContent, context.mActionHint);
    }

    if (aState.IsEditable() &&
        (StaticPrefs::dom_forms_inputmode() ||
         nsContentUtils::IsChromeDoc(aContent->OwnerDoc()))) {
      aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::inputmode,
                                     context.mHTMLInputInputmode);
      if (aContent->IsHTMLElement(nsGkAtoms::input) &&
          context.mHTMLInputInputmode.EqualsLiteral("mozAwesomebar")) {
        if (!nsContentUtils::IsChromeDoc(aContent->OwnerDoc())) {
          // mozAwesomebar should be allowed only in chrome
          context.mHTMLInputInputmode.Truncate();
        }
      } else {
        // Except to mozAwesomebar, inputmode should be lower case.
        ToLowerCase(context.mHTMLInputInputmode);
      }
    }

    if (aContent->IsHTMLElement() && aState.IsEditable() &&
        StaticPrefs::dom_forms_autocapitalize() &&
        context.IsAutocapitalizeSupported()) {
      nsGenericHTMLElement::FromNode(aContent)->GetAutocapitalize(
          context.mAutocapitalize);
    }
  }

  if (aAction.mCause == InputContextAction::CAUSE_UNKNOWN &&
      nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    aAction.mCause = InputContextAction::CAUSE_UNKNOWN_CHROME;
  }

  if ((aAction.mCause == InputContextAction::CAUSE_UNKNOWN ||
       aAction.mCause == InputContextAction::CAUSE_UNKNOWN_CHROME) &&
      UserActivation::IsHandlingUserInput()) {
    aAction.mCause =
        UserActivation::IsHandlingKeyboardInput()
            ? InputContextAction::CAUSE_UNKNOWN_DURING_KEYBOARD_INPUT
            : InputContextAction::CAUSE_UNKNOWN_DURING_NON_KEYBOARD_INPUT;
  }

  SetInputContext(aWidget, context, aAction);
}

// static
void IMEStateManager::SetInputContext(nsIWidget* aWidget,
                                      const InputContext& aInputContext,
                                      const InputContextAction& aAction) {
  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("SetInputContext(aWidget=0x%p, aInputContext=%s, "
       "aAction={ mCause=%s, mAction=%s }), BrowserParent::GetFocused()=0x%p",
       aWidget, ToString(aInputContext).c_str(),
       ToString(aAction.mCause).c_str(), ToString(aAction.mFocusChange).c_str(),
       BrowserParent::GetFocused()));

  MOZ_RELEASE_ASSERT(aWidget);

  nsCOMPtr<nsIWidget> widget(aWidget);
  widget->SetInputContext(aInputContext, aAction);
  sActiveInputContextWidget = widget;
}

// static
void IMEStateManager::EnsureTextCompositionArray() {
  if (sTextCompositions) {
    return;
  }
  sTextCompositions = new TextCompositionArray();
}

// static
void IMEStateManager::DispatchCompositionEvent(
    nsINode* aEventTargetNode, nsPresContext* aPresContext,
    BrowserParent* aBrowserParent, WidgetCompositionEvent* aCompositionEvent,
    nsEventStatus* aStatus, EventDispatchingCallback* aCallBack,
    bool aIsSynthesized) {
  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("DispatchCompositionEvent(aNode=0x%p, "
       "aPresContext=0x%p, aCompositionEvent={ mMessage=%s, "
       "mNativeIMEContext={ mRawNativeIMEContext=0x%" PRIXPTR ", "
       "mOriginProcessID=0x%" PRIX64 " }, mWidget(0x%p)={ "
       "GetNativeIMEContext()={ mRawNativeIMEContext=0x%" PRIXPTR ", "
       "mOriginProcessID=0x%" PRIX64 " }, Destroyed()=%s }, "
       "mFlags={ mIsTrusted=%s, mPropagationStopped=%s } }, "
       "aIsSynthesized=%s), browserParent=%p",
       aEventTargetNode, aPresContext, ToChar(aCompositionEvent->mMessage),
       aCompositionEvent->mNativeIMEContext.mRawNativeIMEContext,
       aCompositionEvent->mNativeIMEContext.mOriginProcessID,
       aCompositionEvent->mWidget.get(),
       aCompositionEvent->mWidget->GetNativeIMEContext().mRawNativeIMEContext,
       aCompositionEvent->mWidget->GetNativeIMEContext().mOriginProcessID,
       GetBoolName(aCompositionEvent->mWidget->Destroyed()),
       GetBoolName(aCompositionEvent->mFlags.mIsTrusted),
       GetBoolName(aCompositionEvent->mFlags.mPropagationStopped),
       GetBoolName(aIsSynthesized), aBrowserParent));

  if (!aCompositionEvent->IsTrusted() ||
      aCompositionEvent->PropagationStopped()) {
    return;
  }

  MOZ_ASSERT(aCompositionEvent->mMessage != eCompositionUpdate,
             "compositionupdate event shouldn't be dispatched manually");

  EnsureTextCompositionArray();

  RefPtr<TextComposition> composition =
      sTextCompositions->GetCompositionFor(aCompositionEvent);
  if (!composition) {
    // If synthesized event comes after delayed native composition events
    // for request of commit or cancel, we should ignore it.
    if (NS_WARN_IF(aIsSynthesized)) {
      return;
    }
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  DispatchCompositionEvent(), "
             "adding new TextComposition to the array"));
    MOZ_ASSERT(aCompositionEvent->mMessage == eCompositionStart);
    composition = new TextComposition(aPresContext, aEventTargetNode,
                                      aBrowserParent, aCompositionEvent);
    sTextCompositions->AppendElement(composition);
  }
#ifdef DEBUG
  else {
    MOZ_ASSERT(aCompositionEvent->mMessage != eCompositionStart);
  }
#endif  // #ifdef DEBUG

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
        sTextCompositions->IndexOf(aCompositionEvent->mWidget);
    if (i != TextCompositionArray::NoIndex) {
      MOZ_LOG(
          sISMLog, LogLevel::Debug,
          ("  DispatchCompositionEvent(), "
           "removing TextComposition from the array since NS_COMPOSTION_END "
           "was dispatched"));
      sTextCompositions->ElementAt(i)->Destroy();
      sTextCompositions->RemoveElementAt(i);
    }
  }
}

// static
IMEContentObserver* IMEStateManager::GetActiveContentObserver() {
  return sActiveIMEContentObserver;
}

// static
nsIContent* IMEStateManager::GetRootContent(nsPresContext* aPresContext) {
  Document* doc = aPresContext->Document();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }
  return doc->GetRootElement();
}

// static
void IMEStateManager::HandleSelectionEvent(
    nsPresContext* aPresContext, nsIContent* aEventTargetContent,
    WidgetSelectionEvent* aSelectionEvent) {
  RefPtr<BrowserParent> browserParent = GetActiveBrowserParent();
  if (!browserParent) {
    browserParent = BrowserParent::GetFrom(aEventTargetContent
                                               ? aEventTargetContent
                                               : GetRootContent(aPresContext));
  }

  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("HandleSelectionEvent(aPresContext=0x%p, "
       "aEventTargetContent=0x%p, aSelectionEvent={ mMessage=%s, "
       "mFlags={ mIsTrusted=%s } }), browserParent=%p",
       aPresContext, aEventTargetContent, ToChar(aSelectionEvent->mMessage),
       GetBoolName(aSelectionEvent->mFlags.mIsTrusted), browserParent.get()));

  if (!aSelectionEvent->IsTrusted()) {
    return;
  }

  RefPtr<TextComposition> composition =
      sTextCompositions
          ? sTextCompositions->GetCompositionFor(aSelectionEvent->mWidget)
          : nullptr;
  if (composition) {
    // When there is a composition, TextComposition should guarantee that the
    // selection event will be handled in same target as composition events.
    composition->HandleSelectionEvent(aSelectionEvent);
  } else {
    // When there is no composition, the selection event should be handled
    // in the aPresContext or browserParent.
    TextComposition::HandleSelectionEvent(aPresContext, browserParent,
                                          aSelectionEvent);
  }
}

// static
void IMEStateManager::OnCompositionEventDiscarded(
    WidgetCompositionEvent* aCompositionEvent) {
  // Note that this method is never called for synthesized events for emulating
  // commit or cancel composition.

  MOZ_LOG(
      sISMLog, LogLevel::Info,
      ("OnCompositionEventDiscarded(aCompositionEvent={ "
       "mMessage=%s, mNativeIMEContext={ mRawNativeIMEContext=0x%" PRIXPTR ", "
       "mOriginProcessID=0x%" PRIX64 " }, mWidget(0x%p)={ "
       "GetNativeIMEContext()={ mRawNativeIMEContext=0x%" PRIXPTR ", "
       "mOriginProcessID=0x%" PRIX64 " }, Destroyed()=%s }, "
       "mFlags={ mIsTrusted=%s } })",
       ToChar(aCompositionEvent->mMessage),
       aCompositionEvent->mNativeIMEContext.mRawNativeIMEContext,
       aCompositionEvent->mNativeIMEContext.mOriginProcessID,
       aCompositionEvent->mWidget.get(),
       aCompositionEvent->mWidget->GetNativeIMEContext().mRawNativeIMEContext,
       aCompositionEvent->mWidget->GetNativeIMEContext().mOriginProcessID,
       GetBoolName(aCompositionEvent->mWidget->Destroyed()),
       GetBoolName(aCompositionEvent->mFlags.mIsTrusted)));

  if (!aCompositionEvent->IsTrusted()) {
    return;
  }

  // Ignore compositionstart for now because sTextCompositions may not have
  // been created yet.
  if (aCompositionEvent->mMessage == eCompositionStart) {
    return;
  }

  RefPtr<TextComposition> composition =
      sTextCompositions->GetCompositionFor(aCompositionEvent->mWidget);
  if (!composition) {
    // If the PresShell has been being destroyed during composition,
    // a TextComposition instance for the composition was already removed from
    // the array and destroyed in OnDestroyPresContext().  Therefore, we may
    // fail to retrieve a TextComposition instance here.
    MOZ_LOG(sISMLog, LogLevel::Info,
            ("  OnCompositionEventDiscarded(), "
             "TextComposition instance for the widget has already gone"));
    return;
  }
  composition->OnCompositionEventDiscarded(aCompositionEvent);
}

// static
nsresult IMEStateManager::NotifyIME(IMEMessage aMessage, nsIWidget* aWidget,
                                    BrowserParent* aBrowserParent) {
  return IMEStateManager::NotifyIME(IMENotification(aMessage), aWidget,
                                    aBrowserParent);
}

// static
nsresult IMEStateManager::NotifyIME(const IMENotification& aNotification,
                                    nsIWidget* aWidget,
                                    BrowserParent* aBrowserParent) {
  MOZ_LOG(sISMLog, LogLevel::Info,
          ("NotifyIME(aNotification={ mMessage=%s }, "
           "aWidget=0x%p, aBrowserParent=0x%p), sFocusedIMEWidget=0x%p, "
           "BrowserParent::GetFocused()=0x%p, sFocusedIMEBrowserParent=0x%p, "
           "aBrowserParent == BrowserParent::GetFocused()=%s, "
           "aBrowserParent == sFocusedIMEBrowserParent=%s, "
           "CanSendNotificationToWidget()=%s",
           ToChar(aNotification.mMessage), aWidget, aBrowserParent,
           sFocusedIMEWidget, BrowserParent::GetFocused(),
           sFocusedIMEBrowserParent.get(),
           GetBoolName(aBrowserParent == BrowserParent::GetFocused()),
           GetBoolName(aBrowserParent == sFocusedIMEBrowserParent),
           GetBoolName(CanSendNotificationToWidget())));

  if (NS_WARN_IF(!aWidget)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  NotifyIME(), FAILED due to no widget"));
    return NS_ERROR_INVALID_ARG;
  }

  switch (aNotification.mMessage) {
    case NOTIFY_IME_OF_FOCUS: {
      MOZ_ASSERT(CanSendNotificationToWidget());

      // If focus notification comes from a remote browser which already lost
      // focus, we shouldn't accept the focus notification.  Then, following
      // notifications from the process will be ignored.
      if (aBrowserParent != BrowserParent::GetFocused()) {
        MOZ_LOG(sISMLog, LogLevel::Warning,
                ("  NotifyIME(), WARNING, the received focus notification is "
                 "ignored, because its associated BrowserParent did not match"
                 "the focused BrowserParent."));
        return NS_OK;
      }
      // If IME focus is already set, first blur the currently-focused
      // IME widget
      if (sFocusedIMEWidget) {
        // XXX Why don't we first request the previously-focused IME
        // widget to commit the composition?
        MOZ_ASSERT(
            sFocusedIMEBrowserParent || aBrowserParent,
            "This case shouldn't be caused by focus move in this process");
        MOZ_LOG(sISMLog, LogLevel::Warning,
                ("  NotifyIME(), WARNING, received focus notification with ")
                 "non-null sFocusedIMEWidget. How come "
                 "OnFocusMovedBetweenBrowsers did not blur it already?");
        nsCOMPtr<nsIWidget> focusedIMEWidget(sFocusedIMEWidget);
        sFocusedIMEWidget = nullptr;
        sFocusedIMEBrowserParent = nullptr;
        focusedIMEWidget->NotifyIME(IMENotification(NOTIFY_IME_OF_BLUR));
      }
#ifdef DEBUG
      if (aBrowserParent) {
        nsCOMPtr<nsIWidget> browserParentWidget =
            aBrowserParent->GetTextInputHandlingWidget();
        MOZ_ASSERT(browserParentWidget == aWidget);
      }
#endif
      sFocusedIMEBrowserParent = aBrowserParent;
      sFocusedIMEWidget = aWidget;
      nsCOMPtr<nsIWidget> widget(aWidget);
      MOZ_LOG(
          sISMLog, LogLevel::Info,
          ("  NotifyIME(), about to call widget->NotifyIME() for IME focus"));
      return widget->NotifyIME(aNotification);
    }
    case NOTIFY_IME_OF_BLUR: {
      if (aBrowserParent != sFocusedIMEBrowserParent) {
        MOZ_LOG(sISMLog, LogLevel::Warning,
                ("  NotifyIME(), WARNING, the received blur notification is "
                 "ignored "
                 "because it's not from current focused IME browser"));
        return NS_OK;
      }
      if (!sFocusedIMEWidget) {
        MOZ_LOG(
            sISMLog, LogLevel::Error,
            ("  NotifyIME(), WARNING, received blur notification but there is "
             "no focused IME widget"));
        return NS_OK;
      }
      if (NS_WARN_IF(sFocusedIMEWidget != aWidget)) {
        MOZ_LOG(sISMLog, LogLevel::Warning,
                ("  NotifyIME(), WARNING, the received blur notification is "
                 "ignored "
                 "because it's not for current focused IME widget"));
        return NS_OK;
      }
      nsCOMPtr<nsIWidget> focusedIMEWidget(sFocusedIMEWidget);
      sFocusedIMEWidget = nullptr;
      sFocusedIMEBrowserParent = nullptr;
      return CanSendNotificationToWidget()
                 ? focusedIMEWidget->NotifyIME(
                       IMENotification(NOTIFY_IME_OF_BLUR))
                 : NS_OK;
    }
    case NOTIFY_IME_OF_SELECTION_CHANGE:
    case NOTIFY_IME_OF_TEXT_CHANGE:
    case NOTIFY_IME_OF_POSITION_CHANGE:
    case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
    case NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED: {
      if (aBrowserParent != sFocusedIMEBrowserParent) {
        MOZ_LOG(
            sISMLog, LogLevel::Warning,
            ("  NotifyIME(), WARNING, the received content change notification "
             "is ignored because it's not from current focused IME browser"));
        return NS_OK;
      }
      if (!sFocusedIMEWidget) {
        MOZ_LOG(
            sISMLog, LogLevel::Warning,
            ("  NotifyIME(), WARNING, the received content change notification "
             "is ignored because there is no focused IME widget"));
        return NS_OK;
      }
      if (NS_WARN_IF(sFocusedIMEWidget != aWidget)) {
        MOZ_LOG(
            sISMLog, LogLevel::Warning,
            ("  NotifyIME(), WARNING, the received content change notification "
             "is ignored because it's not for current focused IME widget"));
        return NS_OK;
      }
      if (!CanSendNotificationToWidget()) {
        return NS_OK;
      }
      nsCOMPtr<nsIWidget> widget(aWidget);
      return widget->NotifyIME(aNotification);
    }
    default:
      // Other notifications should be sent only when there is composition.
      // So, we need to handle the others below.
      break;
  }

  if (!sTextCompositions) {
    MOZ_LOG(sISMLog, LogLevel::Info,
            ("  NotifyIME(), the request to IME is ignored because "
             "there have been no compositions yet"));
    return NS_OK;
  }

  RefPtr<TextComposition> composition =
      sTextCompositions->GetCompositionFor(aWidget);
  if (!composition) {
    MOZ_LOG(sISMLog, LogLevel::Info,
            ("  NotifyIME(), the request to IME is ignored because "
             "there is no active composition"));
    return NS_OK;
  }

  if (aBrowserParent != composition->GetBrowserParent()) {
    MOZ_LOG(
        sISMLog, LogLevel::Warning,
        ("  NotifyIME(), WARNING, the request to IME is ignored because "
         "it does not come from the remote browser which has the composition "
         "on aWidget"));
    return NS_OK;
  }

  switch (aNotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      return composition->RequestToCommit(aWidget, false);
    case REQUEST_TO_CANCEL_COMPOSITION:
      return composition->RequestToCommit(aWidget, true);
    default:
      MOZ_CRASH("Unsupported notification");
  }
  MOZ_CRASH(
      "Failed to handle the notification for non-synthesized composition");
  return NS_ERROR_FAILURE;
}

// static
nsresult IMEStateManager::NotifyIME(IMEMessage aMessage,
                                    nsPresContext* aPresContext,
                                    BrowserParent* aBrowserParent) {
  MOZ_LOG(sISMLog, LogLevel::Info,
          ("NotifyIME(aMessage=%s, aPresContext=0x%p, aBrowserParent=0x%p)",
           ToChar(aMessage), aPresContext, aBrowserParent));

  if (NS_WARN_IF(!CanHandleWith(aPresContext))) {
    return NS_ERROR_INVALID_ARG;
  }

  nsIWidget* widget = aPresContext->GetTextInputHandlingWidget();
  if (NS_WARN_IF(!widget)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  NotifyIME(), FAILED due to no widget for the "
             "nsPresContext"));
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NotifyIME(aMessage, widget, aBrowserParent);
}

// static
bool IMEStateManager::IsEditable(nsINode* node) {
  if (node->IsEditable()) {
    return true;
  }
  // |node| might be readwrite (for example, a text control)
  if (node->IsElement() &&
      node->AsElement()->State().HasState(NS_EVENT_STATE_READWRITE)) {
    return true;
  }
  return false;
}

// static
nsINode* IMEStateManager::GetRootEditableNode(nsPresContext* aPresContext,
                                              nsIContent* aContent) {
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
      if (node->IsContent() && node->AsContent()->HasIndependentSelection()) {
        return node;
      }
      root = node;
      node = node->GetParentNode();
    }
    return root;
  }
  if (aPresContext) {
    Document* document = aPresContext->Document();
    if (document && document->IsEditable()) {
      return document;
    }
  }
  return nullptr;
}

// static
bool IMEStateManager::IsIMEObserverNeeded(const IMEState& aState) {
  return aState.IsEditable();
}

// static
void IMEStateManager::DestroyIMEContentObserver() {
  MOZ_LOG(sISMLog, LogLevel::Info,
          ("DestroyIMEContentObserver(), sActiveIMEContentObserver=0x%p",
           sActiveIMEContentObserver.get()));

  if (!sActiveIMEContentObserver) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  DestroyIMEContentObserver() does nothing"));
    return;
  }

  MOZ_LOG(sISMLog, LogLevel::Debug,
          ("  DestroyIMEContentObserver(), destroying "
           "the active IMEContentObserver..."));
  RefPtr<IMEContentObserver> tsm = sActiveIMEContentObserver.get();
  sActiveIMEContentObserver = nullptr;
  tsm->Destroy();
}

// static
void IMEStateManager::CreateIMEContentObserver(EditorBase& aEditorBase) {
  MOZ_LOG(sISMLog, LogLevel::Info,
          ("CreateIMEContentObserver(aEditorBase=0x%p), "
           "sPresContext=0x%p, sContent=0x%p, sWidget=0x%p (available: %s), "
           "sActiveIMEContentObserver=0x%p, "
           "sActiveIMEContentObserver->IsManaging(sPresContext, sContent)=%s",
           &aEditorBase, sPresContext.get(), sContent.get(), sWidget,
           GetBoolName(sWidget && !sWidget->Destroyed()),
           sActiveIMEContentObserver.get(),
           GetBoolName(sActiveIMEContentObserver
                           ? sActiveIMEContentObserver->IsManaging(sPresContext,
                                                                   sContent)
                           : false)));

  if (NS_WARN_IF(sActiveIMEContentObserver)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  CreateIMEContentObserver(), FAILED due to "
             "there is already an active IMEContentObserver"));
    MOZ_ASSERT(sActiveIMEContentObserver->IsManaging(sPresContext, sContent));
    return;
  }

  if (!sWidget || NS_WARN_IF(sWidget->Destroyed())) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  CreateIMEContentObserver(), FAILED due to "
             "the widget for the nsPresContext has gone"));
    return;  // Sometimes, there are no widgets.
  }

  OwningNonNull<nsIWidget> widget(*sWidget);

  // If it's not text editable, we don't need to create IMEContentObserver.
  if (!IsIMEObserverNeeded(widget->GetInputContext().mIMEState)) {
    MOZ_LOG(sISMLog, LogLevel::Debug,
            ("  CreateIMEContentObserver() doesn't create "
             "IMEContentObserver because of non-editable IME state"));
    return;
  }

  if (NS_WARN_IF(widget->Destroyed())) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  CreateIMEContentObserver(), FAILED due to "
             "the widget for the nsPresContext has gone"));
    return;
  }

  if (NS_WARN_IF(!sPresContext)) {
    MOZ_LOG(sISMLog, LogLevel::Error,
            ("  CreateIMEContentObserver(), FAILED due to "
             "the nsPresContext is nullptr"));
    return;
  }

  MOZ_ASSERT(sPresContext->GetTextInputHandlingWidget() == widget);

  MOZ_LOG(sISMLog, LogLevel::Debug,
          ("  CreateIMEContentObserver() is creating an "
           "IMEContentObserver instance..."));
  sActiveIMEContentObserver = new IMEContentObserver();

  // IMEContentObserver::Init() might create another IMEContentObserver
  // instance.  So, sActiveIMEContentObserver would be replaced with new one.
  // We should hold the current instance here.
  RefPtr<IMEContentObserver> activeIMEContentObserver(
      sActiveIMEContentObserver);
  OwningNonNull<nsPresContext> presContext(*sPresContext);
  RefPtr<nsIContent> content = sContent;
  activeIMEContentObserver->Init(widget, presContext, content, aEditorBase);
}

// static
nsresult IMEStateManager::GetFocusSelectionAndRoot(Selection** aSelection,
                                                   nsIContent** aRootContent) {
  if (!sActiveIMEContentObserver) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return sActiveIMEContentObserver->GetSelectionAndRoot(aSelection,
                                                        aRootContent);
}

// static
already_AddRefed<TextComposition> IMEStateManager::GetTextCompositionFor(
    nsIWidget* aWidget) {
  if (!sTextCompositions) {
    return nullptr;
  }
  RefPtr<TextComposition> textComposition =
      sTextCompositions->GetCompositionFor(aWidget);
  return textComposition.forget();
}

// static
already_AddRefed<TextComposition> IMEStateManager::GetTextCompositionFor(
    const WidgetCompositionEvent* aCompositionEvent) {
  if (!sTextCompositions) {
    return nullptr;
  }
  RefPtr<TextComposition> textComposition =
      sTextCompositions->GetCompositionFor(aCompositionEvent);
  return textComposition.forget();
}

// static
already_AddRefed<TextComposition> IMEStateManager::GetTextCompositionFor(
    nsPresContext* aPresContext) {
  if (!sTextCompositions) {
    return nullptr;
  }
  RefPtr<TextComposition> textComposition =
      sTextCompositions->GetCompositionFor(aPresContext);
  return textComposition.forget();
}

}  // namespace mozilla
