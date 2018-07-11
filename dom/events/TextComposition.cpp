/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentEventHandler.h"
#include "IMEContentObserver.h"
#include "IMEStateManager.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/TabParent.h"

#ifdef XP_MACOSX
// Some defiens will be conflict with OSX SDK
#define TextRange _TextRange
#define TextRangeArray _TextRangeArray
#define Comment _Comment
#endif

#include "nsPluginInstanceOwner.h"

#ifdef XP_MACOSX
#undef TextRange
#undef TextRangeArray
#undef Comment
#endif

using namespace mozilla::widget;

namespace mozilla {

#define IDEOGRAPHIC_SPACE (NS_LITERAL_STRING(u"\x3000"))

/******************************************************************************
 * TextComposition
 ******************************************************************************/

bool TextComposition::sHandlingSelectionEvent = false;

TextComposition::TextComposition(nsPresContext* aPresContext,
                                 nsINode* aNode,
                                 TabParent* aTabParent,
                                 WidgetCompositionEvent* aCompositionEvent)
  : mPresContext(aPresContext)
  , mNode(aNode)
  , mTabParent(aTabParent)
  , mNativeContext(aCompositionEvent->mNativeIMEContext)
  , mCompositionStartOffset(0)
  , mTargetClauseOffsetInComposition(0)
  , mCompositionStartOffsetInTextNode(UINT32_MAX)
  , mCompositionLengthInTextNode(UINT32_MAX)
  , mIsSynthesizedForTests(aCompositionEvent->mFlags.mIsSynthesizedForTests)
  , mIsComposing(false)
  , mIsEditorHandlingEvent(false)
  , mIsRequestingCommit(false)
  , mIsRequestingCancel(false)
  , mRequestedToCommitOrCancel(false)
  , mHasDispatchedDOMTextEvent(false)
  , mHasReceivedCommitEvent(false)
  , mWasNativeCompositionEndEventDiscarded(false)
  , mAllowControlCharacters(
      Preferences::GetBool("dom.compositionevent.allow_control_characters",
                           false))
  , mWasCompositionStringEmpty(true)
{
  MOZ_ASSERT(aCompositionEvent->mNativeIMEContext.IsValid());
}

void
TextComposition::Destroy()
{
  mPresContext = nullptr;
  mNode = nullptr;
  mTabParent = nullptr;
  mContainerTextNode = nullptr;
  mCompositionStartOffsetInTextNode = UINT32_MAX;
  mCompositionLengthInTextNode = UINT32_MAX;
  // TODO: If the editor is still alive and this is held by it, we should tell
  //       this being destroyed for cleaning up the stuff.
}

bool
TextComposition::IsValidStateForComposition(nsIWidget* aWidget) const
{
  return !Destroyed() && aWidget && !aWidget->Destroyed() &&
         mPresContext->GetPresShell() &&
         !mPresContext->GetPresShell()->IsDestroying();
}

bool
TextComposition::MaybeDispatchCompositionUpdate(
                   const WidgetCompositionEvent* aCompositionEvent)
{
  MOZ_RELEASE_ASSERT(!mTabParent);

  if (!IsValidStateForComposition(aCompositionEvent->mWidget)) {
    return false;
  }

  // Note that we don't need to dispatch eCompositionUpdate event even if
  // mHasDispatchedDOMTextEvent is false and eCompositionCommit event is
  // dispatched with empty string immediately after eCompositionStart
  // because composition string has never been changed from empty string to
  // non-empty string in such composition even if selected string was not
  // empty string (mLastData isn't set to selected text when this receives
  // eCompositionStart).
  if (mLastData == aCompositionEvent->mData) {
    return true;
  }
  CloneAndDispatchAs(aCompositionEvent, eCompositionUpdate);
  return IsValidStateForComposition(aCompositionEvent->mWidget);
}

BaseEventFlags
TextComposition::CloneAndDispatchAs(
                   const WidgetCompositionEvent* aCompositionEvent,
                   EventMessage aMessage,
                   nsEventStatus* aStatus,
                   EventDispatchingCallback* aCallBack)
{
  MOZ_RELEASE_ASSERT(!mTabParent);

  MOZ_ASSERT(IsValidStateForComposition(aCompositionEvent->mWidget),
             "Should be called only when it's safe to dispatch an event");

  WidgetCompositionEvent compositionEvent(aCompositionEvent->IsTrusted(),
                                          aMessage, aCompositionEvent->mWidget);
  compositionEvent.mTime = aCompositionEvent->mTime;
  compositionEvent.mTimeStamp = aCompositionEvent->mTimeStamp;
  compositionEvent.mData = aCompositionEvent->mData;
  compositionEvent.mNativeIMEContext = aCompositionEvent->mNativeIMEContext;
  compositionEvent.mOriginalMessage = aCompositionEvent->mMessage;
  compositionEvent.mFlags.mIsSynthesizedForTests =
    aCompositionEvent->mFlags.mIsSynthesizedForTests;

  nsEventStatus dummyStatus = nsEventStatus_eConsumeNoDefault;
  nsEventStatus* status = aStatus ? aStatus : &dummyStatus;
  if (aMessage == eCompositionUpdate) {
    mLastData = compositionEvent.mData;
    mLastRanges = aCompositionEvent->mRanges;
  }

  DispatchEvent(&compositionEvent, status, aCallBack, aCompositionEvent);
  return compositionEvent.mFlags;
}

void
TextComposition::DispatchEvent(WidgetCompositionEvent* aDispatchEvent,
                               nsEventStatus* aStatus,
                               EventDispatchingCallback* aCallBack,
                               const WidgetCompositionEvent *aOriginalEvent)
{
  nsPluginInstanceOwner::GeneratePluginEvent(aOriginalEvent,
                                             aDispatchEvent);

  EventDispatcher::Dispatch(mNode, mPresContext,
                            aDispatchEvent, nullptr, aStatus, aCallBack);

  OnCompositionEventDispatched(aDispatchEvent);
}

void
TextComposition::OnCompositionEventDiscarded(
                   WidgetCompositionEvent* aCompositionEvent)
{
  // Note that this method is never called for synthesized events for emulating
  // commit or cancel composition.

  MOZ_ASSERT(aCompositionEvent->IsTrusted(),
             "Shouldn't be called with untrusted event");

  if (mTabParent) {
    // The composition event should be discarded in the child process too.
    Unused << mTabParent->SendCompositionEvent(*aCompositionEvent);
  }

  // XXX If composition events are discarded, should we dispatch them with
  //     runnable event?  However, even if we do so, it might make native IME
  //     confused due to async modification.  Especially when native IME is
  //     TSF.
  if (!aCompositionEvent->CausesDOMCompositionEndEvent()) {
    return;
  }

  mWasNativeCompositionEndEventDiscarded = true;
}

static inline bool
IsControlChar(uint32_t aCharCode)
{
  return aCharCode < ' ' || aCharCode == 0x7F;
}

static size_t
FindFirstControlCharacter(const nsAString& aStr)
{
  const char16_t* sourceBegin = aStr.BeginReading();
  const char16_t* sourceEnd = aStr.EndReading();

  for (const char16_t* source = sourceBegin; source < sourceEnd; ++source) {
    if (*source != '\t' && IsControlChar(*source)) {
      return source - sourceBegin;
    }
  }

  return -1;
}

static void
RemoveControlCharactersFrom(nsAString& aStr, TextRangeArray* aRanges)
{
  size_t firstControlCharOffset = FindFirstControlCharacter(aStr);
  if (firstControlCharOffset == (size_t)-1) {
    return;
  }

  nsAutoString copy(aStr);
  const char16_t* sourceBegin = copy.BeginReading();
  const char16_t* sourceEnd = copy.EndReading();

  char16_t* dest = aStr.BeginWriting();
  if (NS_WARN_IF(!dest)) {
    return;
  }

  char16_t* curDest = dest + firstControlCharOffset;
  size_t i = firstControlCharOffset;
  for (const char16_t* source = sourceBegin + firstControlCharOffset;
       source < sourceEnd; ++source) {
    if (*source == '\t' || *source == '\n' || !IsControlChar(*source)) {
      *curDest = *source;
      ++curDest;
      ++i;
    } else if (aRanges) {
      aRanges->RemoveCharacter(i);
    }
  }

  aStr.SetLength(curDest - dest);
}

void
TextComposition::DispatchCompositionEvent(
                   WidgetCompositionEvent* aCompositionEvent,
                   nsEventStatus* aStatus,
                   EventDispatchingCallback* aCallBack,
                   bool aIsSynthesized)
{
  mWasCompositionStringEmpty = mString.IsEmpty();

  if (aCompositionEvent->IsFollowedByCompositionEnd()) {
    mHasReceivedCommitEvent = true;
  }

  // If this instance has requested to commit or cancel composition but
  // is not synthesizing commit event, that means that the IME commits or
  // cancels the composition asynchronously.  Typically, iBus behaves so.
  // Then, synthesized events which were dispatched immediately after
  // the request has already committed our editor's composition string and
  // told it to web apps.  Therefore, we should ignore the delayed events.
  if (mRequestedToCommitOrCancel && !aIsSynthesized) {
    *aStatus = nsEventStatus_eConsumeNoDefault;
    return;
  }

  // If the content is a container of TabParent, composition should be in the
  // remote process.
  if (mTabParent) {
    Unused << mTabParent->SendCompositionEvent(*aCompositionEvent);
    aCompositionEvent->StopPropagation();
    if (aCompositionEvent->CausesDOMTextEvent()) {
      mLastData = aCompositionEvent->mData;
      mLastRanges = aCompositionEvent->mRanges;
      // Although, the composition event hasn't been actually handled yet,
      // emulate an editor to be handling the composition event.
      EditorWillHandleCompositionChangeEvent(aCompositionEvent);
      EditorDidHandleCompositionChangeEvent();
    }
    return;
  }

  if (!mAllowControlCharacters) {
    RemoveControlCharactersFrom(aCompositionEvent->mData,
                                aCompositionEvent->mRanges);
  }
  if (aCompositionEvent->mMessage == eCompositionCommitAsIs) {
    NS_ASSERTION(!aCompositionEvent->mRanges,
                 "mRanges of eCompositionCommitAsIs should be null");
    aCompositionEvent->mRanges = nullptr;
    NS_ASSERTION(aCompositionEvent->mData.IsEmpty(),
                 "mData of eCompositionCommitAsIs should be empty string");
    bool removePlaceholderCharacter =
      Preferences::GetBool("intl.ime.remove_placeholder_character_at_commit",
                           false);
    if (removePlaceholderCharacter && mLastData == IDEOGRAPHIC_SPACE) {
      // If the last data is an ideographic space (FullWidth space), it might be
      // a placeholder character of some Chinese IME.  So, committing with
      // this data might not be expected by users.  Let's use empty string.
      aCompositionEvent->mData.Truncate();
    } else {
      aCompositionEvent->mData = mLastData;
    }
  } else if (aCompositionEvent->mMessage == eCompositionCommit) {
    NS_ASSERTION(!aCompositionEvent->mRanges,
                 "mRanges of eCompositionCommit should be null");
    aCompositionEvent->mRanges = nullptr;
  }

  if (!IsValidStateForComposition(aCompositionEvent->mWidget)) {
    *aStatus = nsEventStatus_eConsumeNoDefault;
    return;
  }

  // IME may commit composition with empty string for a commit request or
  // with non-empty string for a cancel request.  We should prevent such
  // unexpected result.  E.g., web apps may be confused if they implement
  // autocomplete which attempts to commit composition forcibly when the user
  // selects one of suggestions but composition string is cleared by IME.
  // Note that most Chinese IMEs don't expose actual composition string to us.
  // They typically tell us an IDEOGRAPHIC SPACE or empty string as composition
  // string.  Therefore, we should hack it only when:
  // 1. committing string is empty string at requesting commit but the last
  //    data isn't IDEOGRAPHIC SPACE.
  // 2. non-empty string is committed at requesting cancel.
  if (!aIsSynthesized && (mIsRequestingCommit || mIsRequestingCancel)) {
    nsString* committingData = nullptr;
    switch (aCompositionEvent->mMessage) {
      case eCompositionEnd:
      case eCompositionChange:
      case eCompositionCommitAsIs:
      case eCompositionCommit:
        committingData = &aCompositionEvent->mData;
        break;
      default:
        NS_WARNING("Unexpected event comes during committing or "
                   "canceling composition");
        break;
    }
    if (committingData) {
      if (mIsRequestingCommit && committingData->IsEmpty() &&
          mLastData != IDEOGRAPHIC_SPACE) {
        committingData->Assign(mLastData);
      } else if (mIsRequestingCancel && !committingData->IsEmpty()) {
        committingData->Truncate();
      }
    }
  }

  bool dispatchEvent = true;
  bool dispatchDOMTextEvent = aCompositionEvent->CausesDOMTextEvent();

  // When mIsComposing is false but the committing string is different from
  // the last data (E.g., previous eCompositionChange event made the
  // composition string empty or didn't have clause information), we don't
  // need to dispatch redundant DOM text event.  (But note that we need to
  // dispatch eCompositionChange event if we have not dispatched
  // eCompositionChange event yet and commit string replaces selected string
  // with empty string since selected string hasn't been replaced with empty
  // string yet.)
  if (dispatchDOMTextEvent &&
      aCompositionEvent->mMessage != eCompositionChange &&
      !mIsComposing && mHasDispatchedDOMTextEvent &&
      mLastData == aCompositionEvent->mData) {
    dispatchEvent = dispatchDOMTextEvent = false;
  }

  // widget may dispatch redundant eCompositionChange event
  // which modifies neither composition string, clauses nor caret
  // position.  In such case, we shouldn't dispatch DOM events.
  if (dispatchDOMTextEvent &&
      aCompositionEvent->mMessage == eCompositionChange &&
      mLastData == aCompositionEvent->mData &&
      mRanges && aCompositionEvent->mRanges &&
      mRanges->Equals(*aCompositionEvent->mRanges)) {
    dispatchEvent = dispatchDOMTextEvent = false;
  }

  if (dispatchDOMTextEvent) {
    if (!MaybeDispatchCompositionUpdate(aCompositionEvent)) {
      return;
    }
  }

  if (dispatchEvent) {
    // If the composition event should cause a DOM text event, we should
    // overwrite the event message as eCompositionChange because due to
    // the limitation of mapping between event messages and DOM event types,
    // we cannot map multiple event messages to a DOM event type.
    if (dispatchDOMTextEvent &&
        aCompositionEvent->mMessage != eCompositionChange) {
      mHasDispatchedDOMTextEvent = true;
      aCompositionEvent->mFlags =
        CloneAndDispatchAs(aCompositionEvent, eCompositionChange,
                           aStatus, aCallBack);
    } else {
      if (aCompositionEvent->mMessage == eCompositionChange) {
        mHasDispatchedDOMTextEvent = true;
      }
      DispatchEvent(aCompositionEvent, aStatus, aCallBack);
    }
  } else {
    *aStatus = nsEventStatus_eConsumeNoDefault;
  }

  if (!IsValidStateForComposition(aCompositionEvent->mWidget)) {
    return;
  }

  // Emulate editor behavior of compositionchange event (DOM text event) handler
  // if no editor handles composition events.
  if (dispatchDOMTextEvent && !HasEditor()) {
    EditorWillHandleCompositionChangeEvent(aCompositionEvent);
    EditorDidHandleCompositionChangeEvent();
  }

  if (aCompositionEvent->CausesDOMCompositionEndEvent()) {
    // Dispatch a compositionend event if it's necessary.
    if (aCompositionEvent->mMessage != eCompositionEnd) {
      CloneAndDispatchAs(aCompositionEvent, eCompositionEnd);
    }
    MOZ_ASSERT(!mIsComposing, "Why is the editor still composing?");
    MOZ_ASSERT(!HasEditor(), "Why does the editor still keep to hold this?");
  }

  MaybeNotifyIMEOfCompositionEventHandled(aCompositionEvent);
}

// static
void
TextComposition::HandleSelectionEvent(nsPresContext* aPresContext,
                                      TabParent* aTabParent,
                                      WidgetSelectionEvent* aSelectionEvent)
{
  // If the content is a container of TabParent, composition should be in the
  // remote process.
  if (aTabParent) {
    Unused << aTabParent->SendSelectionEvent(*aSelectionEvent);
    aSelectionEvent->StopPropagation();
    return;
  }

  ContentEventHandler handler(aPresContext);
  AutoRestore<bool> saveHandlingSelectionEvent(sHandlingSelectionEvent);
  sHandlingSelectionEvent = true;
  // XXX During setting selection, a selection listener may change selection
  //     again.  In such case, sHandlingSelectionEvent doesn't indicate if
  //     the selection change is caused by a selection event.  However, it
  //     must be non-realistic scenario.
  handler.OnSelectionEvent(aSelectionEvent);
}

uint32_t
TextComposition::GetSelectionStartOffset()
{
  nsCOMPtr<nsIWidget> widget = mPresContext->GetRootWidget();
  WidgetQueryContentEvent selectedTextEvent(true, eQuerySelectedText, widget);
  // Due to a bug of widget, mRanges may not be nullptr even though composition
  // string is empty.  So, we need to check it here for avoiding to return
  // odd start offset.
  if (!mLastData.IsEmpty() && mRanges && mRanges->HasClauses()) {
    selectedTextEvent.InitForQuerySelectedText(
                        ToSelectionType(mRanges->GetFirstClause()->mRangeType));
  } else {
    NS_WARNING_ASSERTION(
      !mLastData.IsEmpty() || !mRanges || !mRanges->HasClauses(),
      "Shouldn't have empty clause info when composition string is empty");
    selectedTextEvent.InitForQuerySelectedText(SelectionType::eNormal);
  }

  // The editor which has this composition is observed by active
  // IMEContentObserver, we can use the cache of it.
  RefPtr<IMEContentObserver> contentObserver =
    IMEStateManager::GetActiveContentObserver();
  bool doQuerySelection = true;
  if (contentObserver) {
    if (contentObserver->IsManaging(this)) {
      doQuerySelection = false;
      contentObserver->HandleQueryContentEvent(&selectedTextEvent);
    }
    // If another editor already has focus, we cannot retrieve selection
    // in the editor which has this composition...
    else if (NS_WARN_IF(contentObserver->GetPresContext() == mPresContext)) {
      return 0;  // XXX Is this okay?
    }
  }

  // Otherwise, using slow path (i.e., compute every time with
  // ContentEventHandler)
  if (doQuerySelection) {
    ContentEventHandler handler(mPresContext);
    handler.HandleQueryContentEvent(&selectedTextEvent);
  }

  if (NS_WARN_IF(!selectedTextEvent.mSucceeded)) {
    return 0; // XXX Is this okay?
  }
  return selectedTextEvent.mReply.mOffset;
}

void
TextComposition::OnCompositionEventDispatched(
                   const WidgetCompositionEvent* aCompositionEvent)
{
  MOZ_RELEASE_ASSERT(!mTabParent);

  if (!IsValidStateForComposition(aCompositionEvent->mWidget)) {
    return;
  }

  // Every composition event may cause changing composition start offset,
  // especially when there is no composition string.  Therefore, we need to
  // update mCompositionStartOffset with the latest offset.

  MOZ_ASSERT(aCompositionEvent->mMessage != eCompositionStart ||
               mWasCompositionStringEmpty,
             "mWasCompositionStringEmpty should be true if the dispatched "
             "event is eCompositionStart");

  if (mWasCompositionStringEmpty &&
      !aCompositionEvent->CausesDOMCompositionEndEvent()) {
    // If there was no composition string, current selection start may be the
    // offset for inserting composition string.
    // Update composition start offset with current selection start.
    mCompositionStartOffset = GetSelectionStartOffset();
    mTargetClauseOffsetInComposition = 0;
  }

  if (aCompositionEvent->CausesDOMTextEvent()) {
    mTargetClauseOffsetInComposition = aCompositionEvent->TargetClauseOffset();
  }
}

void
TextComposition::OnStartOffsetUpdatedInChild(uint32_t aStartOffset)
{
  mCompositionStartOffset = aStartOffset;
}

void
TextComposition::MaybeNotifyIMEOfCompositionEventHandled(
                   const WidgetCompositionEvent* aCompositionEvent)
{
  if (aCompositionEvent->mMessage != eCompositionStart &&
      !aCompositionEvent->CausesDOMTextEvent()) {
    return;
  }

  RefPtr<IMEContentObserver> contentObserver =
    IMEStateManager::GetActiveContentObserver();
  // When IMEContentObserver is managing the editor which has this composition,
  // composition event handled notification should be sent after the observer
  // notifies all pending notifications.  Therefore, we should use it.
  // XXX If IMEContentObserver suddenly loses focus after here and notifying
  //     widget of pending notifications, we won't notify widget of composition
  //     event handled.  Although, this is a bug but it should be okay since
  //     destroying IMEContentObserver notifies IME of blur.  So, native IME
  //     handler can treat it as this notification too.
  if (contentObserver && contentObserver->IsManaging(this)) {
    contentObserver->MaybeNotifyCompositionEventHandled();
    return;
  }
  // Otherwise, e.g., this composition is in non-active window, we should
  // notify widget directly.
  NotifyIME(NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED);
}

void
TextComposition::DispatchCompositionEventRunnable(EventMessage aEventMessage,
                                                  const nsAString& aData,
                                                  bool aIsSynthesizingCommit)
{
  nsContentUtils::AddScriptRunner(
    new CompositionEventDispatcher(this, mNode, aEventMessage, aData,
                                   aIsSynthesizingCommit));
}

nsresult
TextComposition::RequestToCommit(nsIWidget* aWidget, bool aDiscard)
{
  // If this composition is already requested to be committed or canceled,
  // or has already finished in IME, we don't need to request it again because
  // request from this instance shouldn't cause committing nor canceling current
  // composition in IME, and even if the first request failed, new request
  // won't success, probably.  And we shouldn't synthesize events for
  // committing or canceling composition twice or more times.
  if (!CanRequsetIMEToCommitOrCancelComposition()) {
    return NS_OK;
  }

  RefPtr<TextComposition> kungFuDeathGrip(this);
  const nsAutoString lastData(mLastData);

  {
    AutoRestore<bool> saveRequestingCancel(mIsRequestingCancel);
    AutoRestore<bool> saveRequestingCommit(mIsRequestingCommit);
    if (aDiscard) {
      mIsRequestingCancel = true;
      mIsRequestingCommit = false;
    } else {
      mIsRequestingCancel = false;
      mIsRequestingCommit = true;
    }
    // FYI: CompositionEvents caused by a call of NotifyIME() may be
    //      discarded by PresShell if it's not safe to dispatch the event.
    nsresult rv =
      aWidget->NotifyIME(IMENotification(aDiscard ?
                                           REQUEST_TO_CANCEL_COMPOSITION :
                                           REQUEST_TO_COMMIT_COMPOSITION));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mRequestedToCommitOrCancel = true;

  // If the request is performed synchronously, this must be already destroyed.
  if (Destroyed()) {
    return NS_OK;
  }

  // Otherwise, synthesize the commit in content.
  nsAutoString data(aDiscard ? EmptyString() : lastData);
  if (data == mLastData) {
    DispatchCompositionEventRunnable(eCompositionCommitAsIs, EmptyString(),
                                     true);
  } else {
    DispatchCompositionEventRunnable(eCompositionCommit, data, true);
  }
  return NS_OK;
}

nsresult
TextComposition::NotifyIME(IMEMessage aMessage)
{
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_NOT_AVAILABLE);
  return IMEStateManager::NotifyIME(aMessage, mPresContext, mTabParent);
}

void
TextComposition::EditorWillHandleCompositionChangeEvent(
                   const WidgetCompositionEvent* aCompositionChangeEvent)
{
  mIsComposing = aCompositionChangeEvent->IsComposing();
  mRanges = aCompositionChangeEvent->mRanges;
  mIsEditorHandlingEvent = true;

  MOZ_ASSERT(mLastData == aCompositionChangeEvent->mData,
    "The text of a compositionchange event must be same as previous data "
    "attribute value of the latest compositionupdate event");
}

void
TextComposition::OnEditorDestroyed()
{
  MOZ_RELEASE_ASSERT(!mTabParent);

  MOZ_ASSERT(!mIsEditorHandlingEvent,
             "The editor should have stopped listening events");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (NS_WARN_IF(!widget)) {
    // XXX If this could happen, how do we notify IME of destroying the editor?
    return;
  }

  // Try to cancel the composition.
  RequestToCommit(widget, true);
}

void
TextComposition::EditorDidHandleCompositionChangeEvent()
{
  mString = mLastData;
  mIsEditorHandlingEvent = false;
}

void
TextComposition::StartHandlingComposition(EditorBase* aEditorBase)
{
  MOZ_RELEASE_ASSERT(!mTabParent);

  MOZ_ASSERT(!HasEditor(), "There is a handling editor already");
  mEditorBaseWeak = do_GetWeakReference(static_cast<nsIEditor*>(aEditorBase));
}

void
TextComposition::EndHandlingComposition(EditorBase* aEditorBase)
{
  MOZ_RELEASE_ASSERT(!mTabParent);

#ifdef DEBUG
  RefPtr<EditorBase> editorBase = GetEditorBase();
  MOZ_ASSERT(editorBase == aEditorBase,
             "Another editor handled the composition?");
#endif // #ifdef DEBUG
  mEditorBaseWeak = nullptr;
}

already_AddRefed<EditorBase>
TextComposition::GetEditorBase() const
{
  nsCOMPtr<nsIEditor> editor = do_QueryReferent(mEditorBaseWeak);
  RefPtr<EditorBase> editorBase = static_cast<EditorBase*>(editor.get());
  return editorBase.forget();
}

bool
TextComposition::HasEditor() const
{
  return mEditorBaseWeak && mEditorBaseWeak->IsAlive();
}

/******************************************************************************
 * TextComposition::CompositionEventDispatcher
 ******************************************************************************/

TextComposition::CompositionEventDispatcher::CompositionEventDispatcher(
  TextComposition* aComposition,
  nsINode* aEventTarget,
  EventMessage aEventMessage,
  const nsAString& aData,
  bool aIsSynthesizedEvent)
  : Runnable("TextComposition::CompositionEventDispatcher")
  , mTextComposition(aComposition)
  , mEventTarget(aEventTarget)
  , mData(aData)
  , mEventMessage(aEventMessage)
  , mIsSynthesizedEvent(aIsSynthesizedEvent)
{
}

NS_IMETHODIMP
TextComposition::CompositionEventDispatcher::Run()
{
  // The widget can be different from the widget which has dispatched
  // composition events because GetWidget() returns a widget which is proper
  // for calling NotifyIME().  However, this must no be problem since both
  // widget should share native IME context.  Therefore, even if an event
  // handler uses the widget for requesting IME to commit or cancel, it works.
  nsCOMPtr<nsIWidget> widget(mTextComposition->GetWidget());
  if (!mTextComposition->IsValidStateForComposition(widget)) {
    return NS_OK; // cannot dispatch any events anymore
  }

  RefPtr<nsPresContext> presContext = mTextComposition->mPresContext;
  nsEventStatus status = nsEventStatus_eIgnore;
  switch (mEventMessage) {
    case eCompositionStart: {
      WidgetCompositionEvent compStart(true, eCompositionStart, widget);
      compStart.mNativeIMEContext = mTextComposition->mNativeContext;
      WidgetQueryContentEvent selectedText(true, eQuerySelectedText, widget);
      ContentEventHandler handler(presContext);
      handler.OnQuerySelectedText(&selectedText);
      NS_ASSERTION(selectedText.mSucceeded, "Failed to get selected text");
      compStart.mData = selectedText.mReply.mString;
      compStart.mFlags.mIsSynthesizedForTests =
        mTextComposition->IsSynthesizedForTests();
      IMEStateManager::DispatchCompositionEvent(mEventTarget, presContext,
                                                &compStart, &status, nullptr,
                                                mIsSynthesizedEvent);
      break;
    }
    case eCompositionChange:
    case eCompositionCommitAsIs:
    case eCompositionCommit: {
      WidgetCompositionEvent compEvent(true, mEventMessage, widget);
      compEvent.mNativeIMEContext = mTextComposition->mNativeContext;
      if (mEventMessage != eCompositionCommitAsIs) {
        compEvent.mData = mData;
      }
      compEvent.mFlags.mIsSynthesizedForTests =
        mTextComposition->IsSynthesizedForTests();
      IMEStateManager::DispatchCompositionEvent(mEventTarget, presContext,
                                                &compEvent, &status, nullptr,
                                                mIsSynthesizedEvent);
      break;
    }
    default:
      MOZ_CRASH("Unsupported event");
  }
  return NS_OK;
}

/******************************************************************************
 * TextCompositionArray
 ******************************************************************************/

TextCompositionArray::index_type
TextCompositionArray::IndexOf(const NativeIMEContext& aNativeIMEContext)
{
  if (!aNativeIMEContext.IsValid()) {
    return NoIndex;
  }
  for (index_type i = Length(); i > 0; --i) {
    if (ElementAt(i - 1)->GetNativeIMEContext() == aNativeIMEContext) {
      return i - 1;
    }
  }
  return NoIndex;
}

TextCompositionArray::index_type
TextCompositionArray::IndexOf(nsIWidget* aWidget)
{
  return IndexOf(aWidget->GetNativeIMEContext());
}

TextCompositionArray::index_type
TextCompositionArray::IndexOf(nsPresContext* aPresContext)
{
  for (index_type i = Length(); i > 0; --i) {
    if (ElementAt(i - 1)->GetPresContext() == aPresContext) {
      return i - 1;
    }
  }
  return NoIndex;
}

TextCompositionArray::index_type
TextCompositionArray::IndexOf(nsPresContext* aPresContext,
                              nsINode* aNode)
{
  index_type index = IndexOf(aPresContext);
  if (index == NoIndex) {
    return NoIndex;
  }
  nsINode* node = ElementAt(index)->GetEventTargetNode();
  return node == aNode ? index : NoIndex;
}

TextComposition*
TextCompositionArray::GetCompositionFor(nsIWidget* aWidget)
{
  index_type i = IndexOf(aWidget);
  if (i == NoIndex) {
    return nullptr;
  }
  return ElementAt(i);
}

TextComposition*
TextCompositionArray::GetCompositionFor(
                        const WidgetCompositionEvent* aCompositionEvent)
{
  index_type i = IndexOf(aCompositionEvent->mNativeIMEContext);
  if (i == NoIndex) {
    return nullptr;
  }
  return ElementAt(i);
}

TextComposition*
TextCompositionArray::GetCompositionFor(nsPresContext* aPresContext)
{
  index_type i = IndexOf(aPresContext);
  if (i == NoIndex) {
    return nullptr;
  }
  return ElementAt(i);
}

TextComposition*
TextCompositionArray::GetCompositionFor(nsPresContext* aPresContext,
                                           nsINode* aNode)
{
  index_type i = IndexOf(aPresContext, aNode);
  if (i == NoIndex) {
    return nullptr;
  }
  return ElementAt(i);
}

TextComposition*
TextCompositionArray::GetCompositionInContent(nsPresContext* aPresContext,
                                              nsIContent* aContent)
{
  // There should be only one composition per content object.
  for (index_type i = Length(); i > 0; --i) {
    nsINode* node = ElementAt(i - 1)->GetEventTargetNode();
    if (node && nsContentUtils::ContentIsDescendantOf(node, aContent)) {
      return ElementAt(i - 1);
    }
  }
  return nullptr;
}

} // namespace mozilla
