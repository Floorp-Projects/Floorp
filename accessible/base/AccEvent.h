/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _AccEvent_H_
#define _AccEvent_H_

#include "nsIAccessibleEvent.h"

#include "mozilla/a11y/LocalAccessible.h"

class nsEventShell;
namespace mozilla {

namespace dom {
class Selection;
}

namespace a11y {

class DocAccessible;
class EventQueue;
class TextRange;

// Constants used to point whether the event is from user input.
enum EIsFromUserInput {
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
class AccEvent {
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
  AccEvent(uint32_t aEventType, LocalAccessible* aAccessible,
           EIsFromUserInput aIsFromUserInput = eAutoDetect,
           EEventRule aEventRule = eRemoveDupes);

  // AccEvent
  uint32_t GetEventType() const { return mEventType; }
  EEventRule GetEventRule() const { return mEventRule; }
  bool IsFromUserInput() const { return mIsFromUserInput; }
  EIsFromUserInput FromUserInput() const {
    return static_cast<EIsFromUserInput>(mIsFromUserInput);
  }

  LocalAccessible* GetAccessible() const { return mAccessible; }
  DocAccessible* Document() const { return mAccessible->Document(); }

  /**
   * Down casting.
   */
  enum EventGroup {
    eGenericEvent,
    eStateChangeEvent,
    eTextChangeEvent,
    eTreeMutationEvent,
    eMutationEvent,
    eReorderEvent,
    eHideEvent,
    eShowEvent,
    eCaretMoveEvent,
    eTextSelChangeEvent,
    eSelectionChangeEvent,
    eObjectAttrChangedEvent,
    eScrollingEvent,
    eAnnouncementEvent,
  };

  static const EventGroup kEventGroup = eGenericEvent;
  virtual unsigned int GetEventGroups() const { return 1U << eGenericEvent; }

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
  RefPtr<LocalAccessible> mAccessible;

  friend class EventQueue;
  friend class EventTree;
  friend class ::nsEventShell;
  friend class NotificationController;
};

/**
 * Accessible state change event.
 */
class AccStateChangeEvent : public AccEvent {
 public:
  AccStateChangeEvent(LocalAccessible* aAccessible, uint64_t aState,
                      bool aIsEnabled,
                      EIsFromUserInput aIsFromUserInput = eAutoDetect)
      : AccEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, aAccessible,
                 aIsFromUserInput, eCoalesceStateChange),
        mState(aState),
        mIsEnabled(aIsEnabled) {}

  AccStateChangeEvent(LocalAccessible* aAccessible, uint64_t aState)
      : AccEvent(::nsIAccessibleEvent::EVENT_STATE_CHANGE, aAccessible,
                 eAutoDetect, eCoalesceStateChange),
        mState(aState) {
    mIsEnabled = (mAccessible->State() & mState) != 0;
  }

  // AccEvent
  static const EventGroup kEventGroup = eStateChangeEvent;
  virtual unsigned int GetEventGroups() const override {
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
class AccTextChangeEvent : public AccEvent {
 public:
  AccTextChangeEvent(LocalAccessible* aAccessible, int32_t aStart,
                     const nsAString& aModifiedText, bool aIsInserted,
                     EIsFromUserInput aIsFromUserInput = eAutoDetect);

  // AccEvent
  static const EventGroup kEventGroup = eTextChangeEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccEvent::GetEventGroups() | (1U << eTextChangeEvent);
  }

  // AccTextChangeEvent
  int32_t GetStartOffset() const { return mStart; }
  uint32_t GetLength() const { return mModifiedText.Length(); }
  bool IsTextInserted() const { return mIsInserted; }
  void GetModifiedText(nsAString& aModifiedText) {
    aModifiedText = mModifiedText;
  }
  const nsString& ModifiedText() const { return mModifiedText; }

 private:
  int32_t mStart;
  bool mIsInserted;
  nsString mModifiedText;

  friend class EventTree;
  friend class NotificationController;
};

/**
 * A base class for events related to tree mutation, either an AccMutation
 * event, or an AccReorderEvent.
 */
class AccTreeMutationEvent : public AccEvent {
 public:
  AccTreeMutationEvent(uint32_t aEventType, LocalAccessible* aTarget)
      : AccEvent(aEventType, aTarget, eAutoDetect, eCoalesceReorder),
        mGeneration(0) {}

  // Event
  static const EventGroup kEventGroup = eTreeMutationEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccEvent::GetEventGroups() | (1U << eTreeMutationEvent);
  }

  void SetNextEvent(AccTreeMutationEvent* aNext) { mNextEvent = aNext; }
  void SetPrevEvent(AccTreeMutationEvent* aPrev) { mPrevEvent = aPrev; }
  AccTreeMutationEvent* NextEvent() const { return mNextEvent; }
  AccTreeMutationEvent* PrevEvent() const { return mPrevEvent; }

  /**
   * A sequence number to know when this event was fired.
   */
  uint32_t EventGeneration() const { return mGeneration; }
  void SetEventGeneration(uint32_t aGeneration) { mGeneration = aGeneration; }

 private:
  RefPtr<AccTreeMutationEvent> mNextEvent;
  RefPtr<AccTreeMutationEvent> mPrevEvent;
  uint32_t mGeneration;
};

/**
 * Base class for show and hide accessible events.
 */
class AccMutationEvent : public AccTreeMutationEvent {
 public:
  AccMutationEvent(uint32_t aEventType, LocalAccessible* aTarget)
      : AccTreeMutationEvent(aEventType, aTarget) {
    // Don't coalesce these since they are coalesced by reorder event. Coalesce
    // contained text change events.
    mParent = mAccessible->LocalParent();
  }
  virtual ~AccMutationEvent() {}

  // Event
  static const EventGroup kEventGroup = eMutationEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccTreeMutationEvent::GetEventGroups() | (1U << eMutationEvent);
  }

  // MutationEvent
  bool IsShow() const { return mEventType == nsIAccessibleEvent::EVENT_SHOW; }
  bool IsHide() const { return mEventType == nsIAccessibleEvent::EVENT_HIDE; }

  LocalAccessible* LocalParent() const { return mParent; }

 protected:
  RefPtr<LocalAccessible> mParent;
  RefPtr<AccTextChangeEvent> mTextChangeEvent;

  friend class EventTree;
  friend class NotificationController;
};

/**
 * Accessible hide event.
 */
class AccHideEvent : public AccMutationEvent {
 public:
  explicit AccHideEvent(LocalAccessible* aTarget, bool aNeedsShutdown = true);

  // Event
  static const EventGroup kEventGroup = eHideEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccMutationEvent::GetEventGroups() | (1U << eHideEvent);
  }

  // AccHideEvent
  LocalAccessible* TargetParent() const { return mParent; }
  LocalAccessible* TargetNextSibling() const { return mNextSibling; }
  LocalAccessible* TargetPrevSibling() const { return mPrevSibling; }
  bool NeedsShutdown() const { return mNeedsShutdown; }

 protected:
  bool mNeedsShutdown;
  RefPtr<LocalAccessible> mNextSibling;
  RefPtr<LocalAccessible> mPrevSibling;

  friend class EventTree;
  friend class NotificationController;
};

/**
 * Accessible show event.
 */
class AccShowEvent : public AccMutationEvent {
 public:
  explicit AccShowEvent(LocalAccessible* aTarget)
      : AccMutationEvent(::nsIAccessibleEvent::EVENT_SHOW, aTarget) {}

  // Event
  static const EventGroup kEventGroup = eShowEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccMutationEvent::GetEventGroups() | (1U << eShowEvent);
  }
};

/**
 * Class for reorder accessible event. Takes care about
 */
class AccReorderEvent : public AccTreeMutationEvent {
 public:
  explicit AccReorderEvent(LocalAccessible* aTarget)
      : AccTreeMutationEvent(::nsIAccessibleEvent::EVENT_REORDER, aTarget) {}
  virtual ~AccReorderEvent() {}

  // Event
  static const EventGroup kEventGroup = eReorderEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccTreeMutationEvent::GetEventGroups() | (1U << eReorderEvent);
  }

  /*
   * Make this an inner reorder event that is coalesced into
   * a reorder event of an ancestor.
   */
  void SetInner() { mEventType = ::nsIAccessibleEvent::EVENT_INNER_REORDER; }
};

/**
 * Accessible caret move event.
 */
class AccCaretMoveEvent : public AccEvent {
 public:
  AccCaretMoveEvent(LocalAccessible* aAccessible, int32_t aCaretOffset,
                    bool aIsSelectionCollapsed, bool aIsAtEndOfLine,
                    int32_t aGranularity,
                    EIsFromUserInput aIsFromUserInput = eAutoDetect)
      : AccEvent(::nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED, aAccessible,
                 aIsFromUserInput),
        mCaretOffset(aCaretOffset),
        mIsSelectionCollapsed(aIsSelectionCollapsed),
        mIsAtEndOfLine(aIsAtEndOfLine),
        mGranularity(aGranularity) {}
  virtual ~AccCaretMoveEvent() {}

  // AccEvent
  static const EventGroup kEventGroup = eCaretMoveEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccEvent::GetEventGroups() | (1U << eCaretMoveEvent);
  }

  // AccCaretMoveEvent
  int32_t GetCaretOffset() const { return mCaretOffset; }

  bool IsSelectionCollapsed() const { return mIsSelectionCollapsed; }
  bool IsAtEndOfLine() { return mIsAtEndOfLine; }

  int32_t GetGranularity() const { return mGranularity; }

 private:
  int32_t mCaretOffset;

  bool mIsSelectionCollapsed;
  bool mIsAtEndOfLine;
  int32_t mGranularity;
};

/**
 * Accessible text selection change event.
 */
class AccTextSelChangeEvent : public AccEvent {
 public:
  AccTextSelChangeEvent(HyperTextAccessible* aTarget,
                        dom::Selection* aSelection, int32_t aReason,
                        int32_t aGranularity);
  virtual ~AccTextSelChangeEvent();

  // AccEvent
  static const EventGroup kEventGroup = eTextSelChangeEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccEvent::GetEventGroups() | (1U << eTextSelChangeEvent);
  }

  // AccTextSelChangeEvent

  /**
   * Return true if the text selection change wasn't caused by pure caret move.
   */
  bool IsCaretMoveOnly() const;

  int32_t GetGranularity() const { return mGranularity; }

  /**
   * Return selection ranges in document/control.
   */
  void SelectionRanges(nsTArray<a11y::TextRange>* aRanges) const;

 private:
  RefPtr<dom::Selection> mSel;
  int32_t mReason;
  int32_t mGranularity;

  friend class EventQueue;
  friend class SelectionManager;
};

/**
 * Accessible widget selection change event.
 */
class AccSelChangeEvent : public AccEvent {
 public:
  enum SelChangeType { eSelectionAdd, eSelectionRemove };

  AccSelChangeEvent(LocalAccessible* aWidget, LocalAccessible* aItem,
                    SelChangeType aSelChangeType);

  virtual ~AccSelChangeEvent() {}

  // AccEvent
  static const EventGroup kEventGroup = eSelectionChangeEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccEvent::GetEventGroups() | (1U << eSelectionChangeEvent);
  }

  // AccSelChangeEvent
  LocalAccessible* Widget() const { return mWidget; }

 private:
  RefPtr<LocalAccessible> mWidget;
  RefPtr<LocalAccessible> mItem;
  SelChangeType mSelChangeType;
  uint32_t mPreceedingCount;
  AccSelChangeEvent* mPackedEvent;

  friend class EventQueue;
};

/**
 * Accessible object attribute changed event.
 */
class AccObjectAttrChangedEvent : public AccEvent {
 public:
  AccObjectAttrChangedEvent(LocalAccessible* aAccessible, nsAtom* aAttribute)
      : AccEvent(::nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED,
                 aAccessible),
        mAttribute(aAttribute) {}

  // AccEvent
  static const EventGroup kEventGroup = eObjectAttrChangedEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccEvent::GetEventGroups() | (1U << eObjectAttrChangedEvent);
  }

  // AccObjectAttrChangedEvent
  nsAtom* GetAttribute() const { return mAttribute; }

 private:
  RefPtr<nsAtom> mAttribute;

  virtual ~AccObjectAttrChangedEvent() {}
};

/**
 * Accessible scroll event.
 */
class AccScrollingEvent : public AccEvent {
 public:
  AccScrollingEvent(uint32_t aEventType, LocalAccessible* aAccessible,
                    uint32_t aScrollX, uint32_t aScrollY, uint32_t aMaxScrollX,
                    uint32_t aMaxScrollY)
      : AccEvent(aEventType, aAccessible),
        mScrollX(aScrollX),
        mScrollY(aScrollY),
        mMaxScrollX(aMaxScrollX),
        mMaxScrollY(aMaxScrollY) {}

  virtual ~AccScrollingEvent() {}

  // AccEvent
  static const EventGroup kEventGroup = eScrollingEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccEvent::GetEventGroups() | (1U << eScrollingEvent);
  }

  // The X scrolling offset of the container when the event was fired.
  uint32_t ScrollX() { return mScrollX; }
  // The Y scrolling offset of the container when the event was fired.
  uint32_t ScrollY() { return mScrollY; }
  // The max X offset of the container.
  uint32_t MaxScrollX() { return mMaxScrollX; }
  // The max Y offset of the container.
  uint32_t MaxScrollY() { return mMaxScrollY; }

 private:
  uint32_t mScrollX;
  uint32_t mScrollY;
  uint32_t mMaxScrollX;
  uint32_t mMaxScrollY;
};

/**
 * Accessible announcement event.
 */
class AccAnnouncementEvent : public AccEvent {
 public:
  AccAnnouncementEvent(LocalAccessible* aAccessible,
                       const nsAString& aAnnouncement, uint16_t aPriority)
      : AccEvent(nsIAccessibleEvent::EVENT_ANNOUNCEMENT, aAccessible),
        mAnnouncement(aAnnouncement),
        mPriority(aPriority) {}

  virtual ~AccAnnouncementEvent() {}

  // AccEvent
  static const EventGroup kEventGroup = eAnnouncementEvent;
  virtual unsigned int GetEventGroups() const override {
    return AccEvent::GetEventGroups() | (1U << eAnnouncementEvent);
  }

  const nsString& Announcement() const { return mAnnouncement; }

  uint16_t Priority() { return mPriority; }

 private:
  nsString mAnnouncement;
  uint16_t mPriority;
};

/**
 * Downcast the generic accessible event object to derived type.
 */
class downcast_accEvent {
 public:
  explicit downcast_accEvent(AccEvent* e) : mRawPtr(e) {}

  template <class Destination>
  operator Destination*() {
    if (!mRawPtr) return nullptr;

    return mRawPtr->GetEventGroups() & (1U << Destination::kEventGroup)
               ? static_cast<Destination*>(mRawPtr)
               : nullptr;
  }

 private:
  AccEvent* mRawPtr;
};

/**
 * Return a new xpcom accessible event for the given internal one.
 */
already_AddRefed<nsIAccessibleEvent> MakeXPCEvent(AccEvent* aEvent);

}  // namespace a11y
}  // namespace mozilla

#endif
