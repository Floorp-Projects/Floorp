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

class nsAccEvent: public nsIAccessibleEvent
{
public:
  // Initialize with an nsIAccessible
  nsAccEvent(PRUint32 aEventType, nsIAccessible *aAccessible, void *aEventData);
  // Initialize with an nsIDOMNode
  nsAccEvent(PRUint32 aEventType, nsIDOMNode *aDOMNode, void *aEventData);
  virtual ~nsAccEvent() {};

  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLEEVENT

private:
  PRUint32 mEventType;
  nsCOMPtr<nsIAccessible> mAccessible;
  nsCOMPtr<nsIDOMNode> mDOMNode;
  nsCOMPtr<nsIAccessibleDocument> mDocAccessible;
  void *mEventData;
};

class nsAccStateChangeEvent: public nsAccEvent,
                             public nsIAccessibleStateChangeEvent
{
public:
  nsAccStateChangeEvent(nsIAccessible *aAccessible,
                        PRUint32 aState, PRBool aIsExtraState,
                        PRBool aIsEnabled);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIACCESSIBLEEVENT(nsAccEvent::)
  NS_DECL_NSIACCESSIBLESTATECHANGEEVENT

private:
  PRUint32 mState;
  PRBool mIsExtraState;
  PRBool mIsEnabled;
};

// XXX todo: We might want to use XPCOM interfaces instead of structs
//     e.g., nsAccessibleTextChangeEvent: public nsIAccessibleTextChangeEvent

enum AtkProperty {
  PROP_0,           // gobject convention
  PROP_NAME,
  PROP_DESCRIPTION,
  PROP_PARENT,      // ancestry has changed
  PROP_ROLE,
  PROP_LAYER,
  PROP_MDI_ZORDER,
  PROP_TABLE_CAPTION,
  PROP_TABLE_COLUMN_DESCRIPTION,
  PROP_TABLE_COLUMN_HEADER,
  PROP_TABLE_ROW_DESCRIPTION,
  PROP_TABLE_ROW_HEADER,
  PROP_TABLE_SUMMARY,
  PROP_LAST         // gobject convention
};

struct AtkPropertyChange {
  PRInt32 type;     // property type as listed above 
  void *oldvalue;  
  void *newvalue;
};

struct AtkChildrenChange {
  PRInt32      index;  // index of child in parent 
  nsIAccessible *child;   
  PRBool        add;    // true for add, false for delete
};

struct AtkTextChange {
  PRInt32  start;
  PRUint32 length;
  PRBool   add;     // true for add, false for delete
};

struct AtkTableChange {
  PRUint32 index;   // the start row/column after which the rows are inserted/deleted.
  PRUint32 count;   // the number of inserted/deleted rows/columns
};

#endif  
