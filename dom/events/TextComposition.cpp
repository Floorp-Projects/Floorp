/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentEventHandler.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIEditor.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"

using namespace mozilla::widget;

namespace mozilla {

#define IDEOGRAPHIC_SPACE (NS_LITERAL_STRING("\x3000"))

/******************************************************************************
 * TextComposition
 ******************************************************************************/

TextComposition::TextComposition(nsPresContext* aPresContext,
                                 nsINode* aNode,
                                 WidgetCompositionEvent* aCompositionEvent)
  : mPresContext(aPresContext)
  , mNode(aNode)
  , mNativeContext(
      aCompositionEvent->widget->GetInputContext().mNativeIMEContext)
  , mCompositionStartOffset(0)
  , mCompositionTargetOffset(0)
  , mIsSynthesizedForTests(aCompositionEvent->mFlags.mIsSynthesizedForTests)
  , mIsComposing(false)
  , mIsEditorHandlingEvent(false)
  , mIsRequestingCommit(false)
  , mIsRequestingCancel(false)
  , mRequestedToCommitOrCancel(false)
  , mWasNativeCompositionEndEventDiscarded(false)
{
}

void
TextComposition::Destroy()
{
  mPresContext = nullptr;
  mNode = nullptr;
  // TODO: If the editor is still alive and this is held by it, we should tell
  //       this being destroyed for cleaning up the stuff.
}

bool
TextComposition::MatchesNativeContext(nsIWidget* aWidget) const
{
  return mNativeContext == aWidget->GetInputContext().mNativeIMEContext;
}

bool
TextComposition::MaybeDispatchCompositionUpdate(
                   const WidgetCompositionEvent* aCompositionEvent)
{
  if (Destroyed()) {
    return false;
  }

  if (mLastData == aCompositionEvent->mData) {
    return true;
  }

  WidgetCompositionEvent compositionUpdate(aCompositionEvent->mFlags.mIsTrusted,
                                           NS_COMPOSITION_UPDATE,
                                           aCompositionEvent->widget);
  compositionUpdate.time = aCompositionEvent->time;
  compositionUpdate.timeStamp = aCompositionEvent->timeStamp;
  compositionUpdate.mData = aCompositionEvent->mData;
  compositionUpdate.mFlags.mIsSynthesizedForTests =
    aCompositionEvent->mFlags.mIsSynthesizedForTests;

  nsEventStatus status = nsEventStatus_eConsumeNoDefault;
  mLastData = compositionUpdate.mData;
  EventDispatcher::Dispatch(mNode, mPresContext,
                            &compositionUpdate, nullptr, &status, nullptr);
  return !Destroyed();
}

void
TextComposition::OnCompositionEventDiscarded(
                   const WidgetCompositionEvent* aCompositionEvent)
{
  // Note that this method is never called for synthesized events for emulating
  // commit or cancel composition.

  MOZ_ASSERT(aCompositionEvent->mFlags.mIsTrusted,
             "Shouldn't be called with untrusted event");

  // XXX If composition events are discarded, should we dispatch them with
  //     runnable event?  However, even if we do so, it might make native IME
  //     confused due to async modification.  Especially when native IME is
  //     TSF.
  if (aCompositionEvent->message != NS_COMPOSITION_END) {
    return;
  }

  mWasNativeCompositionEndEventDiscarded = true;
}

void
TextComposition::DispatchCompositionEvent(
                   WidgetCompositionEvent* aCompositionEvent,
                   nsEventStatus* aStatus,
                   EventDispatchingCallback* aCallBack,
                   bool aIsSynthesized)
{
  if (Destroyed()) {
    *aStatus = nsEventStatus_eConsumeNoDefault;
    return;
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
    switch (aCompositionEvent->message) {
      case NS_COMPOSITION_END:
      case NS_COMPOSITION_CHANGE:
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

  if (aCompositionEvent->message == NS_COMPOSITION_CHANGE) {
    if (!MaybeDispatchCompositionUpdate(aCompositionEvent)) {
      return;
    }
  }

  EventDispatcher::Dispatch(mNode, mPresContext,
                            aCompositionEvent, nullptr, aStatus, aCallBack);

  if (NS_WARN_IF(Destroyed())) {
    return;
  }

  // Emulate editor behavior of compositionchange event (DOM text event) handler
  // if no editor handles composition events.
  if (aCompositionEvent->message == NS_COMPOSITION_CHANGE && !HasEditor()) {
    EditorWillHandleCompositionChangeEvent(aCompositionEvent);
    EditorDidHandleCompositionChangeEvent();
  }

#ifdef DEBUG
  else if (aCompositionEvent->message == NS_COMPOSITION_END) {
    MOZ_ASSERT(!mIsComposing, "Why is the editor still composing?");
    MOZ_ASSERT(!HasEditor(), "Why does the editor still keep to hold this?");
  }
#endif // #ifdef DEBUG

  // Notify composition update to widget if possible
  NotityUpdateComposition(aCompositionEvent);
}

void
TextComposition::NotityUpdateComposition(
                   const WidgetCompositionEvent* aCompositionEvent)
{
  nsEventStatus status;

  // When compositon start, notify the rect of first offset character.
  // When not compositon start, notify the rect of selected composition
  // string if compositionchange event.
  if (aCompositionEvent->message == NS_COMPOSITION_START) {
    nsCOMPtr<nsIWidget> widget = mPresContext->GetRootWidget();
    // Update composition start offset
    WidgetQueryContentEvent selectedTextEvent(true,
                                              NS_QUERY_SELECTED_TEXT,
                                              widget);
    widget->DispatchEvent(&selectedTextEvent, status);
    if (selectedTextEvent.mSucceeded) {
      mCompositionStartOffset = selectedTextEvent.mReply.mOffset;
    } else {
      // Unknown offset
      NS_WARNING("Cannot get start offset of IME composition");
      mCompositionStartOffset = 0;
    }
    mCompositionTargetOffset = mCompositionStartOffset;
  } else if (aCompositionEvent->message == NS_COMPOSITION_CHANGE) {
    mCompositionTargetOffset =
      mCompositionStartOffset + aCompositionEvent->TargetClauseOffset();
  } else {
    return;
  }

  NotifyIME(NOTIFY_IME_OF_COMPOSITION_UPDATE);
}

void
TextComposition::DispatchCompositionEventRunnable(uint32_t aEventMessage,
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
  // we don't need to request it again because even if the first request
  // failed, new request won't success, probably.  And we shouldn't synthesize
  // events for committing or canceling composition twice or more times.
  if (mRequestedToCommitOrCancel) {
    return NS_OK;
  }

  nsRefPtr<TextComposition> kungFuDeathGrip(this);
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
    if (!mIsSynthesizedForTests) {
      // FYI: CompositionEvents caused by a call of NotifyIME() may be
      //      discarded by PresShell if it's not safe to dispatch the event.
      nsresult rv =
        aWidget->NotifyIME(IMENotification(aDiscard ?
                                             REQUEST_TO_CANCEL_COMPOSITION :
                                             REQUEST_TO_COMMIT_COMPOSITION));
      if (rv == NS_ERROR_NOT_IMPLEMENTED) {
        return rv;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // Emulates to commit or cancel the composition
      // FYI: These events may be discarded by PresShell if it's not safe to
      //      dispatch the event.
      nsCOMPtr<nsIWidget> widget(aWidget);
      nsAutoString commitData(aDiscard ? EmptyString() : lastData);
      bool changingData = lastData != commitData;

      WidgetCompositionEvent changeEvent(true, NS_COMPOSITION_CHANGE, widget);
      changeEvent.mData = commitData;
      changeEvent.mFlags.mIsSynthesizedForTests = true;

      MaybeDispatchCompositionUpdate(&changeEvent);

      // If changing the data or committing string isn't empty, we need to
      // dispatch compositionchange event for setting the composition string
      // without IME selection.
      if (!Destroyed() && !widget->Destroyed() &&
          (changingData || !commitData.IsEmpty())) {
        nsEventStatus status = nsEventStatus_eIgnore;
        widget->DispatchEvent(&changeEvent, status);
      }

      if (!Destroyed() && !widget->Destroyed()) {
        nsEventStatus status = nsEventStatus_eIgnore;
        WidgetCompositionEvent endEvent(true, NS_COMPOSITION_END, widget);
        endEvent.mData = commitData;
        endEvent.mFlags.mIsSynthesizedForTests = true;
        widget->DispatchEvent(&endEvent, status);
      }
    }
  }

  mRequestedToCommitOrCancel = true;

  // If the request is performed synchronously, this must be already destroyed.
  if (Destroyed()) {
    return NS_OK;
  }

  // Otherwise, synthesize the commit in content.
  nsAutoString data(aDiscard ? EmptyString() : lastData);
  // If the last composition string and new data are different, we need to
  // dispatch compositionchange event for removing IME selection.  However, if
  // the commit string is empty string and it's not changed from the last data,
  // we don't need to dispatch compositionchange event.
  if (lastData != data || !data.IsEmpty()) {
    DispatchCompositionEventRunnable(NS_COMPOSITION_CHANGE, data, true);
  }
  DispatchCompositionEventRunnable(NS_COMPOSITION_END, data, true);

  return NS_OK;
}

nsresult
TextComposition::NotifyIME(IMEMessage aMessage)
{
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_NOT_AVAILABLE);
  return IMEStateManager::NotifyIME(aMessage, mPresContext);
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
TextComposition::EditorDidHandleCompositionChangeEvent()
{
  mString = mLastData;
  mIsEditorHandlingEvent = false;
}

void
TextComposition::StartHandlingComposition(nsIEditor* aEditor)
{
  MOZ_ASSERT(!HasEditor(), "There is a handling editor already");
  mEditorWeak = do_GetWeakReference(aEditor);
}

void
TextComposition::EndHandlingComposition(nsIEditor* aEditor)
{
#ifdef DEBUG
  nsCOMPtr<nsIEditor> editor = GetEditor();
  MOZ_ASSERT(editor == aEditor, "Another editor handled the composition?");
#endif // #ifdef DEBUG
  mEditorWeak = nullptr;
}

already_AddRefed<nsIEditor>
TextComposition::GetEditor() const
{
  nsCOMPtr<nsIEditor> editor = do_QueryReferent(mEditorWeak);
  return editor.forget();
}

bool
TextComposition::HasEditor() const
{
  nsCOMPtr<nsIEditor> editor = GetEditor();
  return !!editor;
}

/******************************************************************************
 * TextComposition::CompositionEventDispatcher
 ******************************************************************************/

TextComposition::CompositionEventDispatcher::CompositionEventDispatcher(
                                               TextComposition* aComposition,
                                               nsINode* aEventTarget,
                                               uint32_t aEventMessage,
                                               const nsAString& aData,
                                               bool aIsSynthesizedEvent)
  : mTextComposition(aComposition)
  , mEventTarget(aEventTarget)
  , mEventMessage(aEventMessage)
  , mData(aData)
  , mIsSynthesizedEvent(aIsSynthesizedEvent)
{
}

NS_IMETHODIMP
TextComposition::CompositionEventDispatcher::Run()
{
  nsRefPtr<nsPresContext> presContext = mTextComposition->mPresContext;
  if (!presContext || !presContext->GetPresShell() ||
      presContext->GetPresShell()->IsDestroying()) {
    return NS_OK; // cannot dispatch any events anymore
  }

  // The widget can be different from the widget which has dispatched
  // composition events because GetWidget() returns a widget which is proper
  // for calling NotifyIME().  However, this must no be problem since both
  // widget should share native IME context.  Therefore, even if an event
  // handler uses the widget for requesting IME to commit or cancel, it works.
  nsCOMPtr<nsIWidget> widget(mTextComposition->GetWidget());
  if (NS_WARN_IF(!widget)) {
    return NS_OK; // cannot dispatch any events anymore
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  switch (mEventMessage) {
    case NS_COMPOSITION_START: {
      WidgetCompositionEvent compStart(true, NS_COMPOSITION_START, widget);
      WidgetQueryContentEvent selectedText(true, NS_QUERY_SELECTED_TEXT,
                                           widget);
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
    case NS_COMPOSITION_END:
    case NS_COMPOSITION_CHANGE: {
      WidgetCompositionEvent compEvent(true, mEventMessage, widget);
      compEvent.mData = mData;
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
TextCompositionArray::IndexOf(nsIWidget* aWidget)
{
  for (index_type i = Length(); i > 0; --i) {
    if (ElementAt(i - 1)->MatchesNativeContext(aWidget)) {
      return i - 1;
    }
  }
  return NoIndex;
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
  return i != NoIndex ? ElementAt(i) : nullptr;
}

TextComposition*
TextCompositionArray::GetCompositionFor(nsPresContext* aPresContext,
                                           nsINode* aNode)
{
  index_type i = IndexOf(aPresContext, aNode);
  return i != NoIndex ? ElementAt(i) : nullptr;
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
