/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kyle Yuan (kyle.yuan@sun.com)
 *   John Sun (john.sun@sun.com)
 *   Alexander Surkov <surkov.alexander@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsAccEvent_H_
#define _nsAccEvent_H_

#include "nsIAccessibleEvent.h"

#include "nsAccessible.h"

class nsDocAccessible;

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
class nsAccEvent: public nsIAccessibleEvent
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

    // eCoalesceFromSameDocument : For events of the same type from the same
    //    document, only the newest event will be emitted.
    eCoalesceFromSameDocument,

     // eRemoveDupes : For repeat events, only the newest event in queue
     //    will be emitted.
     eRemoveDupes,

     // eDoNotEmit : This event is confirmed as a duplicate, do not emit it.
     eDoNotEmit
  };

  // Initialize with an nsIAccessible
  nsAccEvent(PRUint32 aEventType, nsAccessible *aAccessible,
             PRBool aIsAsynch = PR_FALSE,
             EIsFromUserInput aIsFromUserInput = eAutoDetect,
             EEventRule aEventRule = eRemoveDupes);
  // Initialize with an nsIDOMNode
  nsAccEvent(PRUint32 aEventType, nsINode *aNode, PRBool aIsAsynch = PR_FALSE,
             EIsFromUserInput aIsFromUserInput = eAutoDetect,
             EEventRule aEventRule = eRemoveDupes);
  virtual ~nsAccEvent() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsAccEvent)

  NS_DECL_NSIACCESSIBLEEVENT

  // nsAccEvent
  PRUint32 GetEventType() const { return mEventType; }
  EEventRule GetEventRule() const { return mEventRule; }
  PRBool IsAsync() const { return mIsAsync; }
  PRBool IsFromUserInput() const { return mIsFromUserInput; }

  nsAccessible *GetAccessible();
  nsDocAccessible* GetDocAccessible();
  nsINode* GetNode();

  enum EventGroup {
    eGenericEvent,
    eReorderEvent,
    eStateChangeEvent,
    eTextChangeEvent,
    eHideEvent,
    eCaretMoveEvent,
    eTableChangeEvent
  };

  static const EventGroup kEventGroup = eGenericEvent;
  virtual unsigned int GetEventGroups() const
  {
    return 1U << eGenericEvent;
  }

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

  PRBool mIsFromUserInput;

  PRUint32 mEventType;
  EEventRule mEventRule;
  PRPackedBool mIsAsync;
  nsRefPtr<nsAccessible> mAccessible;
  nsCOMPtr<nsINode> mNode;

  friend class nsAccEventQueue;
};


/**
 * Accessible reorder event.
 */
class nsAccReorderEvent : public nsAccEvent
{
public:
  nsAccReorderEvent(nsAccessible *aAccTarget, PRBool aIsAsynch,
                    PRBool aIsUnconditional, nsINode *aReasonNode);

  NS_DECL_ISUPPORTS_INHERITED

  // nsAccEvent
  static const EventGroup kEventGroup = eReorderEvent;
  virtual unsigned int GetEventGroups() const
  {
    return nsAccEvent::GetEventGroups() | (1U << eReorderEvent);
  }

  // nsAccReorderEvent
  /**
   * Return true if event is unconditional, i.e. must be fired.
   */
  PRBool IsUnconditionalEvent();

  /**
   * Return true if changed DOM node has accessible in its tree.
   */
  PRBool HasAccessibleInReasonSubtree();

private:
  PRBool mUnconditionalEvent;
  nsCOMPtr<nsINode> mReasonNode;
};


/**
 * Accessible state change event.
 */
class nsAccStateChangeEvent: public nsAccEvent,
                             public nsIAccessibleStateChangeEvent
{
public:
  nsAccStateChangeEvent(nsAccessible *aAccessible,
                        PRUint32 aState, PRBool aIsExtraState,
                        PRBool aIsEnabled, PRBool aIsAsynch = PR_FALSE,
                        EIsFromUserInput aIsFromUserInput = eAutoDetect);

  nsAccStateChangeEvent(nsINode *aNode, PRUint32 aState, PRBool aIsExtraState,
                        PRBool aIsEnabled);

  nsAccStateChangeEvent(nsINode *aNode, PRUint32 aState, PRBool aIsExtraState);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLESTATECHANGEEVENT

  // nsAccEvent
  static const EventGroup kEventGroup = eStateChangeEvent;
  virtual unsigned int GetEventGroups() const
  {
    return nsAccEvent::GetEventGroups() | (1U << eStateChangeEvent);
  }

  // nsAccStateChangeEvent
  PRUint32 GetState() const { return mState; }
  PRBool IsExtraState() const { return mIsExtraState; }
  PRBool IsStateEnabled() const { return mIsEnabled; }

private:
  PRUint32 mState;
  PRBool mIsExtraState;
  PRBool mIsEnabled;
};


/**
 * Accessible text change event.
 */
class nsAccTextChangeEvent: public nsAccEvent,
                            public nsIAccessibleTextChangeEvent
{
public:
  nsAccTextChangeEvent(nsAccessible *aAccessible, PRInt32 aStart,
                       nsAString& aModifiedText,
                       PRBool aIsInserted, PRBool aIsAsynch = PR_FALSE,
                       EIsFromUserInput aIsFromUserInput = eAutoDetect);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETEXTCHANGEEVENT

  // nsAccEvent
  static const EventGroup kEventGroup = eTextChangeEvent;
  virtual unsigned int GetEventGroups() const
  {
    return nsAccEvent::GetEventGroups() | (1U << eTextChangeEvent);
  }

  // nsAccTextChangeEvent
  PRInt32 GetStartOffset() const { return mStart; }
  PRUint32 GetLength() const { return mModifiedText.Length(); }
  PRBool IsTextInserted() const { return mIsInserted; }

private:
  PRInt32 mStart;
  PRBool mIsInserted;
  nsString mModifiedText;

  friend class nsAccEventQueue;
};


/**
 * Accessible hide events.
 */
class AccHideEvent : public nsAccEvent
{
public:
  AccHideEvent(nsAccessible* aTarget, nsINode* aTargetNode,
               PRBool aIsAsynch, EIsFromUserInput aIsFromUserInput);

  // nsAccEvent
  static const EventGroup kEventGroup = eHideEvent;
  virtual unsigned int GetEventGroups() const
  {
    return nsAccEvent::GetEventGroups() | (1U << eHideEvent);
  }

protected:
  nsRefPtr<nsAccessible> mParent;
  nsRefPtr<nsAccessible> mNextSibling;
  nsRefPtr<nsAccessible> mPrevSibling;
  nsRefPtr<nsAccTextChangeEvent> mTextChangeEvent;

  friend class nsAccEventQueue;
};


/**
 * Accessible caret move event.
 */
class nsAccCaretMoveEvent: public nsAccEvent,
                           public nsIAccessibleCaretMoveEvent
{
public:
  nsAccCaretMoveEvent(nsAccessible *aAccessible, PRInt32 aCaretOffset);
  nsAccCaretMoveEvent(nsINode *aNode);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLECARETMOVEEVENT

  // nsAccEvent
  static const EventGroup kEventGroup = eCaretMoveEvent;
  virtual unsigned int GetEventGroups() const
  {
    return nsAccEvent::GetEventGroups() | (1U << eCaretMoveEvent);
  }

  // nsAccCaretMoveEvent
  PRInt32 GetCaretOffset() const { return mCaretOffset; }

private:
  PRInt32 mCaretOffset;
};


/**
 * Accessible table change event.
 */
class nsAccTableChangeEvent : public nsAccEvent,
                              public nsIAccessibleTableChangeEvent
{
public:
  nsAccTableChangeEvent(nsAccessible *aAccessible, PRUint32 aEventType,
                        PRInt32 aRowOrColIndex, PRInt32 aNumRowsOrCols,
                        PRBool aIsAsynch);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETABLECHANGEEVENT

  // nsAccEvent
  static const EventGroup kEventGroup = eTableChangeEvent;
  virtual unsigned int GetEventGroups() const
  {
    return nsAccEvent::GetEventGroups() | (1U << eTableChangeEvent);
  }

  // nsAccTableChangeEvent
  PRUint32 GetIndex() const { return mRowOrColIndex; }
  PRUint32 GetCount() const { return mNumRowsOrCols; }

private:
  PRUint32 mRowOrColIndex;   // the start row/column after which the rows are inserted/deleted.
  PRUint32 mNumRowsOrCols;   // the number of inserted/deleted rows/columns
};


/**
 * Downcast the generic accessible event object to derived type.
 */
class downcast_accEvent
{
public:
  downcast_accEvent(nsAccEvent *e) : mRawPtr(e) { }

  template<class Destination>
  operator Destination*() {
    if (!mRawPtr)
      return nsnull;

    return mRawPtr->GetEventGroups() & (1U << Destination::kEventGroup) ?
      static_cast<Destination*>(mRawPtr) : nsnull;
  }

private:
  nsAccEvent *mRawPtr;
};

#endif

