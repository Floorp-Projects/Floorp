/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccEvent.h"

#include "ApplicationAccessibleWrap.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "nsIAccessibleText.h"
#include "nsAccEvent.h"
#include "States.h"

#include "nsEventStateManager.h"
#include "nsIServiceManager.h"
#ifdef MOZ_XUL
#include "nsIDOMXULMultSelectCntrlEl.h"
#endif

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// AccEvent
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// AccEvent constructors

AccEvent::AccEvent(uint32_t aEventType, Accessible* aAccessible,
                   EIsFromUserInput aIsFromUserInput, EEventRule aEventRule) :
  mEventType(aEventType), mEventRule(aEventRule), mAccessible(aAccessible)
{
  CaptureIsFromUserInput(aIsFromUserInput);
}

////////////////////////////////////////////////////////////////////////////////
// AccEvent public methods

already_AddRefed<nsAccEvent>
AccEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccEvent(this);
  NS_IF_ADDREF(event);
  return event;
}

////////////////////////////////////////////////////////////////////////////////
// AccEvent cycle collection

NS_IMPL_CYCLE_COLLECTION_NATIVE_CLASS(AccEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(AccEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAccessible)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(AccEvent)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mAccessible");
  cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mAccessible));
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AccEvent, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AccEvent, Release)

////////////////////////////////////////////////////////////////////////////////
// AccEvent protected methods

void
AccEvent::CaptureIsFromUserInput(EIsFromUserInput aIsFromUserInput)
{
  if (aIsFromUserInput != eAutoDetect) {
    mIsFromUserInput = aIsFromUserInput == eFromUserInput ? true : false;
    return;
  }

  DocAccessible* document = mAccessible->Document();
  if (!document) {
    NS_ASSERTION(mAccessible == ApplicationAcc(),
                 "Accessible other than application should always have a doc!");
    return;
  }

  mIsFromUserInput =
    document->PresContext()->EventStateManager()->IsHandlingUserInputExternal();
}


////////////////////////////////////////////////////////////////////////////////
// AccStateChangeEvent
////////////////////////////////////////////////////////////////////////////////

already_AddRefed<nsAccEvent>
AccStateChangeEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccStateChangeEvent(this);
  NS_IF_ADDREF(event);
  return event;
}


////////////////////////////////////////////////////////////////////////////////
// AccTextChangeEvent
////////////////////////////////////////////////////////////////////////////////

// Note: we pass in eAllowDupes to the base class because we don't support text
// events coalescence. We fire delayed text change events in DocAccessible but
// we continue to base the event off the accessible object rather than just the
// node. This means we won't try to create an accessible based on the node when
// we are ready to fire the event and so we will no longer assert at that point
// if the node was removed from the document. Either way, the AT won't work with
// a defunct accessible so the behaviour should be equivalent.
AccTextChangeEvent::
  AccTextChangeEvent(Accessible* aAccessible, int32_t aStart,
                     const nsAString& aModifiedText, bool aIsInserted,
                     EIsFromUserInput aIsFromUserInput)
  : AccEvent(aIsInserted ?
             static_cast<uint32_t>(nsIAccessibleEvent::EVENT_TEXT_INSERTED) :
             static_cast<uint32_t>(nsIAccessibleEvent::EVENT_TEXT_REMOVED),
             aAccessible, aIsFromUserInput, eAllowDupes)
  , mStart(aStart)
  , mIsInserted(aIsInserted)
  , mModifiedText(aModifiedText)
{
  // XXX We should use IsFromUserInput here, but that isn't always correct
  // when the text change isn't related to content insertion or removal.
   mIsFromUserInput = mAccessible->State() &
    (states::FOCUSED | states::EDITABLE);
}

already_AddRefed<nsAccEvent>
AccTextChangeEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccTextChangeEvent(this);
  NS_IF_ADDREF(event);
  return event;
}


////////////////////////////////////////////////////////////////////////////////
// AccReorderEvent
////////////////////////////////////////////////////////////////////////////////

uint32_t
AccReorderEvent::IsShowHideEventTarget(const Accessible* aTarget) const
{
  uint32_t count = mDependentEvents.Length();
  for (uint32_t index = count - 1; index < count; index--) {
    if (mDependentEvents[index]->mAccessible == aTarget &&
        mDependentEvents[index]->mEventType == nsIAccessibleEvent::EVENT_SHOW ||
        mDependentEvents[index]->mEventType == nsIAccessibleEvent::EVENT_HIDE) {
      return mDependentEvents[index]->mEventType;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// AccHideEvent
////////////////////////////////////////////////////////////////////////////////

AccHideEvent::
  AccHideEvent(Accessible* aTarget, nsINode* aTargetNode) :
  AccMutationEvent(::nsIAccessibleEvent::EVENT_HIDE, aTarget, aTargetNode)
{
  mNextSibling = mAccessible->NextSibling();
  mPrevSibling = mAccessible->PrevSibling();
}

already_AddRefed<nsAccEvent>
AccHideEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccHideEvent(this);
  NS_ADDREF(event);
  return event;
}


////////////////////////////////////////////////////////////////////////////////
// AccShowEvent
////////////////////////////////////////////////////////////////////////////////

AccShowEvent::
  AccShowEvent(Accessible* aTarget, nsINode* aTargetNode) :
  AccMutationEvent(::nsIAccessibleEvent::EVENT_SHOW, aTarget, aTargetNode)
{
}


////////////////////////////////////////////////////////////////////////////////
// AccCaretMoveEvent
////////////////////////////////////////////////////////////////////////////////

already_AddRefed<nsAccEvent>
AccCaretMoveEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccCaretMoveEvent(this);
  NS_IF_ADDREF(event);
  return event;
}


////////////////////////////////////////////////////////////////////////////////
// AccSelChangeEvent
////////////////////////////////////////////////////////////////////////////////

AccSelChangeEvent::
  AccSelChangeEvent(Accessible* aWidget, Accessible* aItem,
                    SelChangeType aSelChangeType) :
    AccEvent(0, aItem, eAutoDetect, eCoalesceSelectionChange),
    mWidget(aWidget), mItem(aItem), mSelChangeType(aSelChangeType),
    mPreceedingCount(0), mPackedEvent(nullptr)
{
  if (aSelChangeType == eSelectionAdd) {
    if (mWidget->GetSelectedItem(1))
      mEventType = nsIAccessibleEvent::EVENT_SELECTION_ADD;
    else
      mEventType = nsIAccessibleEvent::EVENT_SELECTION;
  } else {
    mEventType = nsIAccessibleEvent::EVENT_SELECTION_REMOVE;
  }
}


////////////////////////////////////////////////////////////////////////////////
// AccTableChangeEvent
////////////////////////////////////////////////////////////////////////////////

AccTableChangeEvent::
  AccTableChangeEvent(Accessible* aAccessible, uint32_t aEventType,
                      int32_t aRowOrColIndex, int32_t aNumRowsOrCols) :
  AccEvent(aEventType, aAccessible),
  mRowOrColIndex(aRowOrColIndex), mNumRowsOrCols(aNumRowsOrCols)
{
}

already_AddRefed<nsAccEvent>
AccTableChangeEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccTableChangeEvent(this);
  NS_IF_ADDREF(event);
  return event;
}


////////////////////////////////////////////////////////////////////////////////
// AccVCChangeEvent
////////////////////////////////////////////////////////////////////////////////

AccVCChangeEvent::
  AccVCChangeEvent(Accessible* aAccessible,
                   nsIAccessible* aOldAccessible,
                   int32_t aOldStart, int32_t aOldEnd,
                   int16_t aReason) :
    AccEvent(::nsIAccessibleEvent::EVENT_VIRTUALCURSOR_CHANGED, aAccessible),
    mOldAccessible(aOldAccessible), mOldStart(aOldStart), mOldEnd(aOldEnd),
    mReason(aReason)
{
}

already_AddRefed<nsAccEvent>
AccVCChangeEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccVirtualCursorChangeEvent(this);
  NS_ADDREF(event);
  return event;
}
