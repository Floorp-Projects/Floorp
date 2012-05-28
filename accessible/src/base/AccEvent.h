/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _AccEvent_H_
#define _AccEvent_H_

#include "nsIAccessibleEvent.h"

#include "nsAccessible.h"

class nsAccEvent;
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
     //    This event will always be emitted.
     eAllowDupes,

     // eCoalesceFromSameSubtree : For events of the same type from the same
     //    subtree or the same node, only the umbrella event on the ancestor
     //    will be emitted.
     eCoalesceFromSameSubtree,

    // eCoalesceOfSameType : For events of the same type, only the newest event
    // will be processed.
    eCoalesceOfSameType,

    // eCoalesceSelectionChange: coalescence of selection change events.
    eCoalesceSelectionChange,

     // eRemoveDupes : For repeat events, only the newest event in queue
     //    will be emitted.
     eRemoveDupes,

     // eDoNotEmit : This event is confirmed as a duplicate, do not emit it.
     eDoNotEmit
  };

  // Initialize with an nsIAccessible
  AccEvent(PRUint32 aEventType, nsAccessible* aAccessible,
           EIsFromUserInput aIsFromUserInput = eAutoDetect,
           EEventRule aEventRule = eRemoveDupes);
  // Initialize with an nsIDOMNode
  AccEvent(PRUint32 aEventType, nsINode* aNode,
           EIsFromUserInput aIsFromUserInput = eAutoDetect,
           EEventRule aEventRule = eRemoveDupes);
  virtual ~AccEvent() {}

  // AccEvent
  PRUint32 GetEventType() const { return mEventType; }
  EEventRule GetEventRule() const { return mEventRule; }
  bool IsFromUserInput() const { return mIsFromUserInput; }

  nsAccessible *GetAccessible();
  DocAccessible* GetDocAccessible();
  nsINode* GetNode();

  /**
   * Create and return an XPCOM object for accessible event object.
   */
  virtual already_AddRefed<nsAccEvent> CreateXPCOMObject();

  /**
   * Down casting.
   */
  enum EventGroup {
    eGenericEvent,
    eStateChangeEvent,
    eTextChangeEvent,
    eMutationEvent,
    eHideEvent,
    eShowEvent,
    eCaretMoveEvent,
    eSelectionChangeEvent,
    eTableChangeEvent,
    eVirtualCursorChangeEvent
  };

  static const EventGroup kEventGroup = eGenericEvent;
  virtual unsigned int GetEventGroups() const
  {
    return 1U << eGenericEvent;
  }

  /**
   * Reference counting and cycle collection.
   */
  NS_INLINE_DECL_REFCOUNTING(AccEvent)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(AccEvent)

protected:
  /**
   * Get an accessible from event target node.
   */
  nsAccessible *GetAccessibleForNode() const;

  /**
   * Determine whether the event is from user input by event state manager if
   * it's not pointed explicetly.
   */
  void CaptureIsFromUserInput(EIsFromUserInput aIsFromUserInput);

  bool mIsFromUserInput;
  PRUint32 mEventType;
  EEventRule mEventRule;
  nsRefPtr<nsAccessible> mAccessible;
  nsCOMPtr<nsINode> mNode;

  friend class NotificationController;
};


/**
 * Accessible state change event.
 */
class AccStateChangeEvent: public AccEvent
{
public:
  AccStateChangeEvent(nsAccessible* aAccessible, PRUint64 aState,
                      bool aIsEnabled,
                      EIsFromUserInput aIsFromUserInput = eAutoDetect);

  AccStateChangeEvent(nsINode* aNode, PRUint64 aState, bool aIsEnabled);

  AccStateChangeEvent(nsINode* aNode, PRUint64 aState);

  // AccEvent
  virtual already_AddRefed<nsAccEvent> CreateXPCOMObject();

  static const EventGroup kEventGroup = eStateChangeEvent;
  virtual unsigned int GetEventGroups() const
  {
    return AccEvent::GetEventGroups() | (1U << eStateChangeEvent);
  }

  // AccStateChangeEvent
  PRUint64 GetState() const { return mState; }
  bool IsStateEnabled() const { return mIsEnabled; }

private:
  PRUint64 mState;
  bool mIsEnabled;
};


/**
 * Accessible text change event.
 */
class AccTextChangeEvent: public AccEvent
{
public:
  AccTextChangeEvent(nsAccessible* aAccessible, PRInt32 aStart,
                     const nsAString& aModifiedText, bool aIsInserted,
                     EIsFromUserInput aIsFromUserInput = eAutoDetect);

  // AccEvent
  virtual already_AddRefed<nsAccEvent> CreateXPCOMObject();

  static const EventGroup kEventGroup = eTextChangeEvent;
  virtual unsigned int GetEventGroups() const
  {
    return AccEvent::GetEventGroups() | (1U << eTextChangeEvent);
  }

  // AccTextChangeEvent
  PRInt32 GetStartOffset() const { return mStart; }
  PRUint32 GetLength() const { return mModifiedText.Length(); }
  bool IsTextInserted() const { return mIsInserted; }
  void GetModifiedText(nsAString& aModifiedText)
    { aModifiedText = mModifiedText; }

private:
  PRInt32 mStart;
  bool mIsInserted;
  nsString mModifiedText;

  friend class NotificationController;
};


/**
 * Base class for show and hide accessible events.
 */
class AccMutationEvent: public AccEvent
{
public:
  AccMutationEvent(PRUint32 aEventType, nsAccessible* aTarget,
                   nsINode* aTargetNode);

  // Event
  static const EventGroup kEventGroup = eMutationEvent;
  virtual unsigned int GetEventGroups() const
  {
    return AccEvent::GetEventGroups() | (1U << eMutationEvent);
  }

  // MutationEvent
  bool IsShow() const { return mEventType == nsIAccessibleEvent::EVENT_SHOW; }
  bool IsHide() const { return mEventType == nsIAccessibleEvent::EVENT_HIDE; }

protected:
  nsRefPtr<AccTextChangeEvent> mTextChangeEvent;

  friend class NotificationController;
};


/**
 * Accessible hide event.
 */
class AccHideEvent: public AccMutationEvent
{
public:
  AccHideEvent(nsAccessible* aTarget, nsINode* aTargetNode);

  // Event
  virtual already_AddRefed<nsAccEvent> CreateXPCOMObject();

  static const EventGroup kEventGroup = eHideEvent;
  virtual unsigned int GetEventGroups() const
  {
    return AccMutationEvent::GetEventGroups() | (1U << eHideEvent);
  }

  // AccHideEvent
  nsAccessible* TargetParent() const { return mParent; }
  nsAccessible* TargetNextSibling() const { return mNextSibling; }
  nsAccessible* TargetPrevSibling() const { return mPrevSibling; }

protected:
  nsRefPtr<nsAccessible> mParent;
  nsRefPtr<nsAccessible> mNextSibling;
  nsRefPtr<nsAccessible> mPrevSibling;

  friend class NotificationController;
};


/**
 * Accessible show event.
 */
class AccShowEvent: public AccMutationEvent
{
public:
  AccShowEvent(nsAccessible* aTarget, nsINode* aTargetNode);

  // Event
  static const EventGroup kEventGroup = eShowEvent;
  virtual unsigned int GetEventGroups() const
  {
    return AccMutationEvent::GetEventGroups() | (1U << eShowEvent);
  }
};


/**
 * Accessible caret move event.
 */
class AccCaretMoveEvent: public AccEvent
{
public:
  AccCaretMoveEvent(nsAccessible* aAccessible, PRInt32 aCaretOffset);
  AccCaretMoveEvent(nsINode* aNode);

  // AccEvent
  virtual already_AddRefed<nsAccEvent> CreateXPCOMObject();

  static const EventGroup kEventGroup = eCaretMoveEvent;
  virtual unsigned int GetEventGroups() const
  {
    return AccEvent::GetEventGroups() | (1U << eCaretMoveEvent);
  }

  // AccCaretMoveEvent
  PRInt32 GetCaretOffset() const { return mCaretOffset; }

private:
  PRInt32 mCaretOffset;
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

  AccSelChangeEvent(nsAccessible* aWidget, nsAccessible* aItem,
                    SelChangeType aSelChangeType);

  virtual ~AccSelChangeEvent() { }

  // AccEvent
  static const EventGroup kEventGroup = eSelectionChangeEvent;
  virtual unsigned int GetEventGroups() const
  {
    return AccEvent::GetEventGroups() | (1U << eSelectionChangeEvent);
  }

  // AccSelChangeEvent
  nsAccessible* Widget() const { return mWidget; }

private:
  nsRefPtr<nsAccessible> mWidget;
  nsRefPtr<nsAccessible> mItem;
  SelChangeType mSelChangeType;
  PRUint32 mPreceedingCount;
  AccSelChangeEvent* mPackedEvent;

  friend class NotificationController;
};


/**
 * Accessible table change event.
 */
class AccTableChangeEvent : public AccEvent
{
public:
  AccTableChangeEvent(nsAccessible* aAccessible, PRUint32 aEventType,
                      PRInt32 aRowOrColIndex, PRInt32 aNumRowsOrCols);

  // AccEvent
  virtual already_AddRefed<nsAccEvent> CreateXPCOMObject();

  static const EventGroup kEventGroup = eTableChangeEvent;
  virtual unsigned int GetEventGroups() const
  {
    return AccEvent::GetEventGroups() | (1U << eTableChangeEvent);
  }

  // AccTableChangeEvent
  PRUint32 GetIndex() const { return mRowOrColIndex; }
  PRUint32 GetCount() const { return mNumRowsOrCols; }

private:
  PRUint32 mRowOrColIndex;   // the start row/column after which the rows are inserted/deleted.
  PRUint32 mNumRowsOrCols;   // the number of inserted/deleted rows/columns
};

/**
 * Accessible virtual cursor change event.
 */
class AccVCChangeEvent : public AccEvent
{
public:
  AccVCChangeEvent(nsAccessible* aAccessible,
                   nsIAccessible* aOldAccessible,
                   PRInt32 aOldStart, PRInt32 aOldEnd);

  virtual ~AccVCChangeEvent() { }

  // AccEvent
  virtual already_AddRefed<nsAccEvent> CreateXPCOMObject();

  static const EventGroup kEventGroup = eVirtualCursorChangeEvent;
  virtual unsigned int GetEventGroups() const
  {
    return AccEvent::GetEventGroups() | (1U << eVirtualCursorChangeEvent);
  }

  // AccTableChangeEvent
  nsIAccessible* OldAccessible() const { return mOldAccessible; }
  PRInt32 OldStartOffset() const { return mOldStart; }
  PRInt32 OldEndOffset() const { return mOldEnd; }

private:
  nsRefPtr<nsIAccessible> mOldAccessible;
  PRInt32 mOldStart;
  PRInt32 mOldEnd;
};

/**
 * Downcast the generic accessible event object to derived type.
 */
class downcast_accEvent
{
public:
  downcast_accEvent(AccEvent* e) : mRawPtr(e) { }

  template<class Destination>
  operator Destination*() {
    if (!mRawPtr)
      return nsnull;

    return mRawPtr->GetEventGroups() & (1U << Destination::kEventGroup) ?
      static_cast<Destination*>(mRawPtr) : nsnull;
  }

private:
  AccEvent* mRawPtr;
};

#endif

