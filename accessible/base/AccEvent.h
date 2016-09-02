/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _AccEvent_H_
#define _AccEvent_H_

#include "nsIAccessibleEvent.h"

#include "mozilla/a11y/Accessible.h"

class nsEventShell;
namespace mozilla {

namespace dom {
class Selection;
}

namespace a11y {

class DocAccessible;

// Constants used to point whether the event is from user input.
enum EIsFromUserInput
{
  // eNoUserInput: event is not from user input
  eNoUserInput = 0,
  // eFromUserInput: event is from user input
  eFromUserInput = 1,
  // eAutoDetect: the value should be obtained from event state manager
  eAutoDetect = -1
};

/**
 * Generic accessible event.
 */
class AccEvent
{
public:

  // Rule for accessible events.
  // The rule will be applied when flushing pending events.
  enum EEventRule {
    // eAllowDupes : More than one event of the same type is allowed.
    //    This event will always be emitted. This flag is used for events that
    //    don't support coalescence.
    eAllowDupes,

     // eCoalesceReorder : For reorder events from the same subtree or the same
     //    node, only the umbrella event on the ancestor will be emitted.
    eCoalesceReorder,

    // eCoalesceOfSameType : For events of the same type, only the newest event
    // will be processed.
    eCoalesceOfSameType,

    // eCoalesceSelectionChange: coalescence of selection change events.
    eCoalesceSelectionChange,

    // eCoalesceStateChange: coalesce state change events.
    eCoalesceStateChange,

    // eCoalesceTextSelChange: coalescence of text selection change events.
    eCoalesceTextSelChange,

     // eRemoveDupes : For repeat events, only the newest event in queue
     //    will be emitted.
    eRemoveDupes,

     // eDoNotEmit : This event is confirmed as a duplicate, do not emit it.
    eDoNotEmit
  };

  // Initialize with an accessible.
  AccEvent(uint32_t aEventType, Accessible* aAccessible,
           EIsFromUserInput aIsFromUserInput = eAutoDetect,
           EEventRule aEventRule = eRemoveDupes);

  // AccEvent
  uint32_t GetEventType() const { return mEventType; }
  EEventRule GetEventRule() const { return mEventRule; }
  bool IsFromUserInput() const { return mIsFromUserInput; }
  EIsFromUserInput FromUserInput() const
    { return static_cast<EIsFromUserInput>(mIsFromUserInput); }

  Accessible* GetAccessible() const { return mAccessible; }
  DocAccessible* Document() const { return mAccessible->Document(); }

  /**
   * Down casting.
   */
  enum EventGroup {
    eGenericEvent,
    eStateChangeEvent,
    eTextChangeEvent,
    eMutationEvent,
    eReorderEvent,
    eHideEvent,
    eShowEvent,
    eCaretMoveEvent,
    eTextSelChangeEvent,
    eSelectionChangeEvent,
    eTableChangeEvent,
    eVirtualCursorChangeEvent,
    eObjectAttrChangedEvent
  };

  static const EventGroup kEventGroup = eGenericEvent;
  virtual unsigned int GetEventGroups() const
  {
    return 1U << eGenericEvent;
  }

  /**
   * Reference counting and cycle collection.
   */
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AccEvent)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(AccEvent)

protected:
  virtual ~AccEvent() {}

  bool mIsFromUserInput;
  uint32_t mEventType;
  EEventRule mEventRule;
  RefPtr<Accessible> mAccessible;

  friend class EventQueue;
  friend class EventTree;
  friend class ::nsEventShell;
};


/**
 * Accessible state change event.
 */
class AccStateChangeEvent: public AccEvent
{
public:
  AccStateChangeEvent(Accessible* aAccessible, uint64_t aState,
                      bool aIsEnabled,
                      EIsFromUserInput aIsFromUserInput = eAutoDetect) :
    AccEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, aAccessible,
             aIsFromUserInput, eCoalesceStateChange),
             mState(aState), mIsEnabled(aIsEnabled) { }

  AccStateChangeEvent(Accessible* aAccessible, uint64_t aState) :
    AccEvent(::nsIAccessibleEvent::EVENT_STATE_CHANGE, aAccessible,
             eAutoDetect, eCoalesceStateChange), mState(aState)
    { mIsEnabled = (mAccessible->State() & mState) != 0; }

  // AccEvent
  static const EventGroup kEventGroup = eStateChangeEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccEvent::GetEventGroups() | (1U << eStateChangeEvent);
  }

  // AccStateChangeEvent
  uint64_t GetState() const { return mState; }
  bool IsStateEnabled() const { return mIsEnabled; }

private:
  uint64_t mState;
  bool mIsEnabled;

  friend class EventQueue;
};


/**
 * Accessible text change event.
 */
class AccTextChangeEvent: public AccEvent
{
public:
  AccTextChangeEvent(Accessible* aAccessible, int32_t aStart,
                     const nsAString& aModifiedText, bool aIsInserted,
                     EIsFromUserInput aIsFromUserInput = eAutoDetect);

  // AccEvent
  static const EventGroup kEventGroup = eTextChangeEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccEvent::GetEventGroups() | (1U << eTextChangeEvent);
  }

  // AccTextChangeEvent
  int32_t GetStartOffset() const { return mStart; }
  uint32_t GetLength() const { return mModifiedText.Length(); }
  bool IsTextInserted() const { return mIsInserted; }
  void GetModifiedText(nsAString& aModifiedText)
    { aModifiedText = mModifiedText; }
  const nsString& ModifiedText() const { return mModifiedText; }

private:
  int32_t mStart;
  bool mIsInserted;
  nsString mModifiedText;

  friend class EventTree;
};

/**
 * A base class for events related to tree mutation, either an AccMutation
 * event, or an AccReorderEvent.
 */
class AccTreeMutationEvent : public AccEvent
{
public:
  AccTreeMutationEvent(uint32_t aEventType, Accessible* aTarget) :
    AccEvent(aEventType, aTarget, eAutoDetect, eCoalesceReorder) {}
};

/**
 * Base class for show and hide accessible events.
 */
class AccMutationEvent: public AccTreeMutationEvent
{
public:
  AccMutationEvent(uint32_t aEventType, Accessible* aTarget) :
    AccTreeMutationEvent(aEventType, aTarget)
  {
    // Don't coalesce these since they are coalesced by reorder event. Coalesce
    // contained text change events.
    mParent = mAccessible->Parent();
  }
  virtual ~AccMutationEvent() { }

  // Event
  static const EventGroup kEventGroup = eMutationEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccEvent::GetEventGroups() | (1U << eMutationEvent);
  }

  // MutationEvent
  bool IsShow() const { return mEventType == nsIAccessibleEvent::EVENT_SHOW; }
  bool IsHide() const { return mEventType == nsIAccessibleEvent::EVENT_HIDE; }

  Accessible* Parent() const { return mParent; }

protected:
  nsCOMPtr<nsINode> mNode;
  RefPtr<Accessible> mParent;
  RefPtr<AccTextChangeEvent> mTextChangeEvent;

  friend class EventTree;
};


/**
 * Accessible hide event.
 */
class AccHideEvent: public AccMutationEvent
{
public:
  explicit AccHideEvent(Accessible* aTarget, bool aNeedsShutdown = true);

  // Event
  static const EventGroup kEventGroup = eHideEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccMutationEvent::GetEventGroups() | (1U << eHideEvent);
  }

  // AccHideEvent
  Accessible* TargetParent() const { return mParent; }
  Accessible* TargetNextSibling() const { return mNextSibling; }
  Accessible* TargetPrevSibling() const { return mPrevSibling; }
  bool NeedsShutdown() const { return mNeedsShutdown; }

protected:
  bool mNeedsShutdown;
  RefPtr<Accessible> mNextSibling;
  RefPtr<Accessible> mPrevSibling;

  friend class EventTree;
};


/**
 * Accessible show event.
 */
class AccShowEvent: public AccMutationEvent
{
public:
  explicit AccShowEvent(Accessible* aTarget);

  // Event
  static const EventGroup kEventGroup = eShowEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccMutationEvent::GetEventGroups() | (1U << eShowEvent);
  }

  uint32_t InsertionIndex() const { return mInsertionIndex; }

private:
  nsTArray<RefPtr<AccHideEvent>> mPrecedingEvents;
  uint32_t mInsertionIndex;

  friend class EventTree;
};


/**
 * Class for reorder accessible event. Takes care about
 */
class AccReorderEvent : public AccTreeMutationEvent
{
public:
  explicit AccReorderEvent(Accessible* aTarget) :
    AccTreeMutationEvent(::nsIAccessibleEvent::EVENT_REORDER, aTarget) { }
  virtual ~AccReorderEvent() { }

  // Event
  static const EventGroup kEventGroup = eReorderEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccEvent::GetEventGroups() | (1U << eReorderEvent);
  }
};


/**
 * Accessible caret move event.
 */
class AccCaretMoveEvent: public AccEvent
{
public:
  AccCaretMoveEvent(Accessible* aAccessible, int32_t aCaretOffset,
                    EIsFromUserInput aIsFromUserInput = eAutoDetect) :
    AccEvent(::nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED, aAccessible,
             aIsFromUserInput),
    mCaretOffset(aCaretOffset) { }
  virtual ~AccCaretMoveEvent() { }

  // AccEvent
  static const EventGroup kEventGroup = eCaretMoveEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccEvent::GetEventGroups() | (1U << eCaretMoveEvent);
  }

  // AccCaretMoveEvent
  int32_t GetCaretOffset() const { return mCaretOffset; }

private:
  int32_t mCaretOffset;
};


/**
 * Accessible text selection change event.
 */
class AccTextSelChangeEvent : public AccEvent
{
public:
  AccTextSelChangeEvent(HyperTextAccessible* aTarget,
                        dom::Selection* aSelection,
                        int32_t aReason);
  virtual ~AccTextSelChangeEvent();

  // AccEvent
  static const EventGroup kEventGroup = eTextSelChangeEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccEvent::GetEventGroups() | (1U << eTextSelChangeEvent);
  }

  // AccTextSelChangeEvent

  /**
   * Return true if the text selection change wasn't caused by pure caret move.
   */
  bool IsCaretMoveOnly() const;

private:
  RefPtr<dom::Selection> mSel;
  int32_t mReason;

  friend class EventQueue;
  friend class SelectionManager;
};


/**
 * Accessible widget selection change event.
 */
class AccSelChangeEvent : public AccEvent
{
public:
  enum SelChangeType {
    eSelectionAdd,
    eSelectionRemove
  };

  AccSelChangeEvent(Accessible* aWidget, Accessible* aItem,
                    SelChangeType aSelChangeType);

  virtual ~AccSelChangeEvent() { }

  // AccEvent
  static const EventGroup kEventGroup = eSelectionChangeEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccEvent::GetEventGroups() | (1U << eSelectionChangeEvent);
  }

  // AccSelChangeEvent
  Accessible* Widget() const { return mWidget; }

private:
  RefPtr<Accessible> mWidget;
  RefPtr<Accessible> mItem;
  SelChangeType mSelChangeType;
  uint32_t mPreceedingCount;
  AccSelChangeEvent* mPackedEvent;

  friend class EventQueue;
};


/**
 * Accessible table change event.
 */
class AccTableChangeEvent : public AccEvent
{
public:
  AccTableChangeEvent(Accessible* aAccessible, uint32_t aEventType,
                      int32_t aRowOrColIndex, int32_t aNumRowsOrCols);

  // AccEvent
  static const EventGroup kEventGroup = eTableChangeEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccEvent::GetEventGroups() | (1U << eTableChangeEvent);
  }

  // AccTableChangeEvent
  uint32_t GetIndex() const { return mRowOrColIndex; }
  uint32_t GetCount() const { return mNumRowsOrCols; }

private:
  uint32_t mRowOrColIndex;   // the start row/column after which the rows are inserted/deleted.
  uint32_t mNumRowsOrCols;   // the number of inserted/deleted rows/columns
};

/**
 * Accessible virtual cursor change event.
 */
class AccVCChangeEvent : public AccEvent
{
public:
  AccVCChangeEvent(Accessible* aAccessible,
                   Accessible* aOldAccessible,
                   int32_t aOldStart, int32_t aOldEnd,
                   int16_t aReason,
                   EIsFromUserInput aIsFromUserInput = eFromUserInput);

  virtual ~AccVCChangeEvent() { }

  // AccEvent
  static const EventGroup kEventGroup = eVirtualCursorChangeEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccEvent::GetEventGroups() | (1U << eVirtualCursorChangeEvent);
  }

  // AccTableChangeEvent
  Accessible* OldAccessible() const { return mOldAccessible; }
  int32_t OldStartOffset() const { return mOldStart; }
  int32_t OldEndOffset() const { return mOldEnd; }
  int32_t Reason() const { return mReason; }

private:
  RefPtr<Accessible> mOldAccessible;
  int32_t mOldStart;
  int32_t mOldEnd;
  int16_t mReason;
};

/**
 * Accessible object attribute changed event.
 */
class AccObjectAttrChangedEvent: public AccEvent
{
public:
  AccObjectAttrChangedEvent(Accessible* aAccessible, nsIAtom* aAttribute) :
    AccEvent(::nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED, aAccessible),
    mAttribute(aAttribute) { }

  // AccEvent
  static const EventGroup kEventGroup = eObjectAttrChangedEvent;
  virtual unsigned int GetEventGroups() const override
  {
    return AccEvent::GetEventGroups() | (1U << eObjectAttrChangedEvent);
  }

  // AccObjectAttrChangedEvent
  nsIAtom* GetAttribute() const { return mAttribute; }

private:
  nsCOMPtr<nsIAtom> mAttribute;

  virtual ~AccObjectAttrChangedEvent() { }
};

/**
 * Downcast the generic accessible event object to derived type.
 */
class downcast_accEvent
{
public:
  explicit downcast_accEvent(AccEvent* e) : mRawPtr(e) { }

  template<class Destination>
  operator Destination*() {
    if (!mRawPtr)
      return nullptr;

    return mRawPtr->GetEventGroups() & (1U << Destination::kEventGroup) ?
      static_cast<Destination*>(mRawPtr) : nullptr;
  }

private:
  AccEvent* mRawPtr;
};

/**
 * Return a new xpcom accessible event for the given internal one.
 */
already_AddRefed<nsIAccessibleEvent>
MakeXPCEvent(AccEvent* aEvent);

} // namespace a11y
} // namespace mozilla

#endif

