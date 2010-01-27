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

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIAccessibleEvent.h"
#include "nsIAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIDOMNode.h"
#include "nsString.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAccUtils.h"

class nsIPresShell;

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

#define NS_ACCEVENT_IMPL_CID                            \
{  /* 39bde096-317e-4294-b23b-4af4a9b283f7 */           \
  0x39bde096,                                           \
  0x317e,                                               \
  0x4294,                                               \
  { 0xb2, 0x3b, 0x4a, 0xf4, 0xa9, 0xb2, 0x83, 0xf7 }    \
}

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
     // eRemoveDupes : For repeat events, only the newest event in queue
     //    will be emitted.
     eRemoveDupes,
     // eDoNotEmit : This event is confirmed as a duplicate, do not emit it.
     eDoNotEmit
  };

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ACCEVENT_IMPL_CID)

  // Initialize with an nsIAccessible
  nsAccEvent(PRUint32 aEventType, nsIAccessible *aAccessible,
             PRBool aIsAsynch = PR_FALSE,
             EIsFromUserInput aIsFromUserInput = eAutoDetect,
             EEventRule aEventRule = eRemoveDupes);
  // Initialize with an nsIDOMNode
  nsAccEvent(PRUint32 aEventType, nsIDOMNode *aDOMNode,
             PRBool aIsAsynch = PR_FALSE,
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
  nsIAccessible* GetAccessible() const { return mAccessible; }

protected:
  /**
   * Get an accessible from event target node.
   */
  already_AddRefed<nsIAccessible> GetAccessibleByNode();

  /**
   * Determine whether the event is from user input by event state manager if
   * it's not pointed explicetly.
   */
  void CaptureIsFromUserInput(EIsFromUserInput aIsFromUserInput);

  PRBool mIsFromUserInput;

  PRUint32 mEventType;
  EEventRule mEventRule;
  PRPackedBool mIsAsync;
  nsCOMPtr<nsIAccessible> mAccessible;
  nsCOMPtr<nsINode> mNode;
  nsCOMPtr<nsIAccessibleDocument> mDocAccessible;

  friend class nsAccEventQueue;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAccEvent, NS_ACCEVENT_IMPL_CID)


#define NS_ACCREORDEREVENT_IMPL_CID                     \
{  /* f2629eb8-2458-4358-868c-3912b15b767a */           \
  0xf2629eb8,                                           \
  0x2458,                                               \
  0x4358,                                               \
  { 0x86, 0x8c, 0x39, 0x12, 0xb1, 0x5b, 0x76, 0x7a }    \
}

class nsAccReorderEvent : public nsAccEvent
{
public:

  nsAccReorderEvent(nsIAccessible *aAccTarget, PRBool aIsAsynch,
                    PRBool aIsUnconditional, nsIDOMNode *aReasonNode);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ACCREORDEREVENT_IMPL_CID)

  NS_DECL_ISUPPORTS_INHERITED

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
  nsCOMPtr<nsIDOMNode> mReasonNode;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAccReorderEvent, NS_ACCREORDEREVENT_IMPL_CID)


class nsAccStateChangeEvent: public nsAccEvent,
                             public nsIAccessibleStateChangeEvent
{
public:
  nsAccStateChangeEvent(nsIAccessible *aAccessible,
                        PRUint32 aState, PRBool aIsExtraState,
                        PRBool aIsEnabled);

  nsAccStateChangeEvent(nsIDOMNode *aNode,
                        PRUint32 aState, PRBool aIsExtraState,
                        PRBool aIsEnabled);

  nsAccStateChangeEvent(nsIDOMNode *aNode,
                        PRUint32 aState, PRBool aIsExtraState);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLESTATECHANGEEVENT

private:
  PRUint32 mState;
  PRBool mIsExtraState;
  PRBool mIsEnabled;
};

class nsAccTextChangeEvent: public nsAccEvent,
                            public nsIAccessibleTextChangeEvent
{
public:
  nsAccTextChangeEvent(nsIAccessible *aAccessible, PRInt32 aStart, PRUint32 aLength,
                       PRBool aIsInserted, PRBool aIsAsynch = PR_FALSE,
                       EIsFromUserInput aIsFromUserInput = eAutoDetect);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETEXTCHANGEEVENT

private:
  PRInt32 mStart;
  PRUint32 mLength;
  PRBool mIsInserted;
  nsString mModifiedText;
};

class nsAccCaretMoveEvent: public nsAccEvent,
                           public nsIAccessibleCaretMoveEvent
{
public:
  nsAccCaretMoveEvent(nsIAccessible *aAccessible, PRInt32 aCaretOffset);
  nsAccCaretMoveEvent(nsIDOMNode *aNode);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLECARETMOVEEVENT

private:
  PRInt32 mCaretOffset;
};

class nsAccTableChangeEvent : public nsAccEvent,
                              public nsIAccessibleTableChangeEvent {
public:
  nsAccTableChangeEvent(nsIAccessible *aAccessible, PRUint32 aEventType,
                        PRInt32 aRowOrColIndex, PRInt32 aNumRowsOrCols,
                        PRBool aIsAsynch);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLETABLECHANGEEVENT

private:
  PRUint32 mRowOrColIndex;   // the start row/column after which the rows are inserted/deleted.
  PRUint32 mNumRowsOrCols;   // the number of inserted/deleted rows/columns
};

#endif

