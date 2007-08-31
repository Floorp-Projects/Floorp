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

#ifndef _nsAccessibleEventData_H_
#define _nsAccessibleEventData_H_

#include "nsCOMPtr.h"
#include "nsIAccessibleEvent.h"
#include "nsIAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIDOMNode.h"
#include "nsString.h"

class nsIPresShell;

class nsAccEvent: public nsIAccessibleEvent
{
public:
  // Initialize with an nsIAccessible
  nsAccEvent(PRUint32 aEventType, nsIAccessible *aAccessible, void *aEventData, PRBool aIsAsynch = PR_FALSE);
  // Initialize with an nsIDOMNode
  nsAccEvent(PRUint32 aEventType, nsIDOMNode *aDOMNode, void *aEventData, PRBool aIsAsynch = PR_FALSE);
  virtual ~nsAccEvent() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLEEVENT

  static void GetLastEventAttributes(nsIDOMNode *aNode,
                                      nsIPersistentProperties *aAttributes);
  static nsIDOMNode* GetLastEventAtomicRegion(nsIDOMNode *aNode);

protected:
  already_AddRefed<nsIAccessible> GetAccessibleByNode();

  void CaptureIsFromUserInput(PRBool aIsAsynch);
  PRBool mIsFromUserInput;

private:
  PRUint32 mEventType;
  nsCOMPtr<nsIAccessible> mAccessible;
  nsCOMPtr<nsIDOMNode> mDOMNode;
  nsCOMPtr<nsIAccessibleDocument> mDocAccessible;

  static PRBool gLastEventFromUserInput;
  static nsIDOMNode* gLastEventNodeWeak;

public:
  /**
   * Find and cache the last input state. This will be called automatically
   * for synchronous events. For asynchronous events it should be
   * called from the synchronous code which is the true source of the event,
   * before the event is fired.
   */
  static void PrepareForEvent(nsIDOMNode *aChangeNode,
                              PRBool aForceIsFromUserInput = PR_FALSE);

  /**
   * The input state was previously stored with the nsIAccessibleEvent,
   * so use that state now -- call this when about to flush an event that 
   * was waiting in an event queue
   */
  static void PrepareForEvent(nsIAccessibleEvent *aEvent);

  void *mEventData;
};

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
  NS_FORWARD_NSIACCESSIBLEEVENT(nsAccEvent::)
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
                       PRBool aIsInserted, PRBool aIsAsynch = PR_FALSE);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIACCESSIBLEEVENT(nsAccEvent::)
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
  NS_FORWARD_NSIACCESSIBLEEVENT(nsAccEvent::)
  NS_DECL_NSIACCESSIBLECARETMOVEEVENT

private:
  PRInt32 mCaretOffset;
};

// XXX todo: We might want to use XPCOM interfaces instead of struct
//     e.g., nsAccessibleTableChangeEvent: public nsIAccessibleTableChangeEvent

struct AtkTableChange {
  PRUint32 index;   // the start row/column after which the rows are inserted/deleted.
  PRUint32 count;   // the number of inserted/deleted rows/columns
};

#endif

