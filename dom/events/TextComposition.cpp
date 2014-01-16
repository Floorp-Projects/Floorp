/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextComposition.h"
#include "nsContentEventHandler.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsIContent.h"
#include "nsIMEStateManager.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/TextEvents.h"

namespace mozilla {

/******************************************************************************
 * TextComposition
 ******************************************************************************/

TextComposition::TextComposition(nsPresContext* aPresContext,
                                 nsINode* aNode,
                                 WidgetGUIEvent* aEvent) :
  mPresContext(aPresContext), mNode(aNode),
  mNativeContext(aEvent->widget->GetInputContext().mNativeIMEContext),
  mCompositionStartOffset(0), mCompositionTargetOffset(0),
  mIsSynthesizedForTests(aEvent->mFlags.mIsSynthesizedForTests)
{
}

TextComposition::TextComposition(const TextComposition& aOther)
{
  mNativeContext = aOther.mNativeContext;
  mPresContext = aOther.mPresContext;
  mNode = aOther.mNode;
  mLastData = aOther.mLastData;
  mCompositionStartOffset = aOther.mCompositionStartOffset;
  mCompositionTargetOffset = aOther.mCompositionTargetOffset;
  mIsSynthesizedForTests = aOther.mIsSynthesizedForTests;
}

bool
TextComposition::MatchesNativeContext(nsIWidget* aWidget) const
{
  return mNativeContext == aWidget->GetInputContext().mNativeIMEContext;
}

void
TextComposition::DispatchEvent(WidgetGUIEvent* aEvent,
                               nsEventStatus* aStatus,
                               nsDispatchingCallback* aCallBack)
{
  if (aEvent->message == NS_COMPOSITION_UPDATE) {
    mLastData = aEvent->AsCompositionEvent()->data;
  }

  nsEventDispatcher::Dispatch(mNode, mPresContext,
                              aEvent, nullptr, aStatus, aCallBack);

  // Notify composition update to widget if possible
  NotityUpdateComposition(aEvent);
}

void
TextComposition::NotityUpdateComposition(WidgetGUIEvent* aEvent)
{
  nsEventStatus status;

  // When compositon start, notify the rect of first offset character.
  // When not compositon start, notify the rect of selected composition
  // string if text event.
  if (aEvent->message == NS_COMPOSITION_START) {
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
  } else if (aEvent->eventStructType != NS_TEXT_EVENT) {
    return;
  } else {
    WidgetTextEvent* textEvent = aEvent->AsTextEvent();
    mCompositionTargetOffset = mCompositionStartOffset;

    for (uint32_t i = 0; i < textEvent->rangeCount; i++) {
      TextRange& range = textEvent->rangeArray[i];
      if (range.mRangeType == NS_TEXTRANGE_SELECTEDRAWTEXT ||
          range.mRangeType == NS_TEXTRANGE_SELECTEDCONVERTEDTEXT) {
        mCompositionTargetOffset += range.mStartOffset;
        break;
      }
    }
  }

  NotifyIME(widget::NotificationToIME::NOTIFY_IME_OF_COMPOSITION_UPDATE);
}

void
TextComposition::DispatchCompsotionEventRunnable(uint32_t aEventMessage,
                                                 const nsAString& aData)
{
  nsContentUtils::AddScriptRunner(
    new CompositionEventDispatcher(mPresContext, mNode,
                                   aEventMessage, aData));
}

void
TextComposition::SynthesizeCommit(bool aDiscard)
{
  // backup this instance and use it since this instance might be destroyed
  // by nsIMEStateManager if this is managed by it.
  TextComposition composition = *this;
  nsAutoString data(aDiscard ? EmptyString() : composition.mLastData);
  if (composition.mLastData != data) {
    composition.DispatchCompsotionEventRunnable(NS_COMPOSITION_UPDATE, data);
    composition.DispatchCompsotionEventRunnable(NS_TEXT_TEXT, data);
  }
  composition.DispatchCompsotionEventRunnable(NS_COMPOSITION_END, data);
}

nsresult
TextComposition::NotifyIME(widget::NotificationToIME aNotification)
{
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_NOT_AVAILABLE);
  return nsIMEStateManager::NotifyIME(aNotification, mPresContext);
}

/******************************************************************************
 * TextComposition::CompositionEventDispatcher
 ******************************************************************************/

TextComposition::CompositionEventDispatcher::CompositionEventDispatcher(
                                               nsPresContext* aPresContext,
                                               nsINode* aEventTarget,
                                               uint32_t aEventMessage,
                                               const nsAString& aData) :
  mPresContext(aPresContext), mEventTarget(aEventTarget),
  mEventMessage(aEventMessage), mData(aData)
{
  mWidget = mPresContext->GetRootWidget();
}

NS_IMETHODIMP
TextComposition::CompositionEventDispatcher::Run()
{
  if (!mPresContext->GetPresShell() ||
      mPresContext->GetPresShell()->IsDestroying()) {
    return NS_OK; // cannot dispatch any events anymore
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  switch (mEventMessage) {
    case NS_COMPOSITION_START: {
      WidgetCompositionEvent compStart(true, NS_COMPOSITION_START, mWidget);
      WidgetQueryContentEvent selectedText(true, NS_QUERY_SELECTED_TEXT,
                                           mWidget);
      nsContentEventHandler handler(mPresContext);
      handler.OnQuerySelectedText(&selectedText);
      NS_ASSERTION(selectedText.mSucceeded, "Failed to get selected text");
      compStart.data = selectedText.mReply.mString;
      nsIMEStateManager::DispatchCompositionEvent(mEventTarget, mPresContext,
                                                  &compStart, &status, nullptr);
      break;
    }
    case NS_COMPOSITION_UPDATE:
    case NS_COMPOSITION_END: {
      WidgetCompositionEvent compEvent(true, mEventMessage, mWidget);
      compEvent.data = mData;
      nsIMEStateManager::DispatchCompositionEvent(mEventTarget, mPresContext,
                                                  &compEvent, &status, nullptr);
      break;
    }
    case NS_TEXT_TEXT: {
      WidgetTextEvent textEvent(true, NS_TEXT_TEXT, mWidget);
      textEvent.theText = mData;
      nsIMEStateManager::DispatchCompositionEvent(mEventTarget, mPresContext,
                                                  &textEvent, &status, nullptr);
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
    if (ElementAt(i - 1).MatchesNativeContext(aWidget)) {
      return i - 1;
    }
  }
  return NoIndex;
}

TextCompositionArray::index_type
TextCompositionArray::IndexOf(nsPresContext* aPresContext)
{
  for (index_type i = Length(); i > 0; --i) {
    if (ElementAt(i - 1).GetPresContext() == aPresContext) {
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
  nsINode* node = ElementAt(index).GetEventTargetNode();
  return node == aNode ? index : NoIndex;
}

TextComposition*
TextCompositionArray::GetCompositionFor(nsIWidget* aWidget)
{
  index_type i = IndexOf(aWidget);
  return i != NoIndex ? &ElementAt(i) : nullptr;
}

TextComposition*
TextCompositionArray::GetCompositionFor(nsPresContext* aPresContext,
                                           nsINode* aNode)
{
  index_type i = IndexOf(aPresContext, aNode);
  return i != NoIndex ? &ElementAt(i) : nullptr;
}

TextComposition*
TextCompositionArray::GetCompositionInContent(nsPresContext* aPresContext,
                                              nsIContent* aContent)
{
  // There should be only one composition per content object.
  for (index_type i = Length(); i > 0; --i) {
    nsINode* node = ElementAt(i - 1).GetEventTargetNode();
    if (node && nsContentUtils::ContentIsDescendantOf(node, aContent)) {
      return &ElementAt(i - 1);
    }
  }
  return nullptr;
}

} // namespace mozilla
