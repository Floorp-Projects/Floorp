/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMSelection_h__
#define nsIDOMSelection_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNode;
class nsIDOMSelectionListener;
class nsIEnumerator;
class nsIDOMRange;

#define NS_IDOMSELECTION_IID \
 { 0xa6cf90e1, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMSelection : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMSELECTION_IID; return iid; }

  NS_IMETHOD    GetAnchorNode(nsIDOMNode** aAnchorNode)=0;

  NS_IMETHOD    GetAnchorOffset(PRInt32* aAnchorOffset)=0;

  NS_IMETHOD    GetFocusNode(nsIDOMNode** aFocusNode)=0;

  NS_IMETHOD    GetFocusOffset(PRInt32* aFocusOffset)=0;

  NS_IMETHOD    GetIsCollapsed(PRBool* aIsCollapsed)=0;

  NS_IMETHOD    GetRangeCount(PRInt32* aRangeCount)=0;

  NS_IMETHOD    GetRangeAt(PRInt32 aIndex, nsIDOMRange** aReturn)=0;

  NS_IMETHOD    ClearSelection()=0;

  NS_IMETHOD    Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset)=0;

  NS_IMETHOD    Extend(nsIDOMNode* aParentNode, PRInt32 aOffset)=0;

  NS_IMETHOD    CollapseToStart()=0;

  NS_IMETHOD    CollapseToEnd()=0;

  NS_IMETHOD    ContainsNode(nsIDOMNode* aNode, PRBool aRecursive, PRBool* aReturn)=0;

  NS_IMETHOD    DeleteFromDocument()=0;

  NS_IMETHOD    AddRange(nsIDOMRange* aRange)=0;

  NS_IMETHOD    RemoveRange(nsIDOMRange* aRange)=0;

  NS_IMETHOD    StartBatchChanges()=0;

  NS_IMETHOD    EndBatchChanges()=0;

  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener)=0;

  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove)=0;

  NS_IMETHOD    SetHint(PRBool aRight)=0;

  NS_IMETHOD    GetHint(PRBool* aReturn)=0;

  NS_IMETHOD    GetEnumerator(nsIEnumerator** aReturn)=0;

  NS_IMETHOD    ToString(const nsAReadableString& aFormatType, PRUint32 aFlags, PRInt32 aWrapCount, nsAWritableString& aReturn)=0;
};


#define NS_DECL_IDOMSELECTION   \
  NS_IMETHOD    GetAnchorNode(nsIDOMNode** aAnchorNode);  \
  NS_IMETHOD    GetAnchorOffset(PRInt32* aAnchorOffset);  \
  NS_IMETHOD    GetFocusNode(nsIDOMNode** aFocusNode);  \
  NS_IMETHOD    GetFocusOffset(PRInt32* aFocusOffset);  \
  NS_IMETHOD    GetIsCollapsed(PRBool* aIsCollapsed);  \
  NS_IMETHOD    GetRangeCount(PRInt32* aRangeCount);  \
  NS_IMETHOD    GetRangeAt(PRInt32 aIndex, nsIDOMRange** aReturn);  \
  NS_IMETHOD    ClearSelection();  \
  NS_IMETHOD    Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset);  \
  NS_IMETHOD    Extend(nsIDOMNode* aParentNode, PRInt32 aOffset);  \
  NS_IMETHOD    CollapseToStart();  \
  NS_IMETHOD    CollapseToEnd();  \
  NS_IMETHOD    ContainsNode(nsIDOMNode* aNode, PRBool aRecursive, PRBool* aReturn);  \
  NS_IMETHOD    DeleteFromDocument();  \
  NS_IMETHOD    AddRange(nsIDOMRange* aRange);  \
  NS_IMETHOD    RemoveRange(nsIDOMRange* aRange);  \
  NS_IMETHOD    StartBatchChanges();  \
  NS_IMETHOD    EndBatchChanges();  \
  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener);  \
  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove);  \
  NS_IMETHOD    SetHint(PRBool aRight);  \
  NS_IMETHOD    GetHint(PRBool* aReturn);  \
  NS_IMETHOD    GetEnumerator(nsIEnumerator** aReturn);  \
  NS_IMETHOD    ToString(const nsAReadableString& aFormatType, PRUint32 aFlags, PRInt32 aWrapCount, nsAWritableString& aReturn);  \



#define NS_FORWARD_IDOMSELECTION(_to)  \
  NS_IMETHOD    GetAnchorNode(nsIDOMNode** aAnchorNode) { return _to GetAnchorNode(aAnchorNode); } \
  NS_IMETHOD    GetAnchorOffset(PRInt32* aAnchorOffset) { return _to GetAnchorOffset(aAnchorOffset); } \
  NS_IMETHOD    GetFocusNode(nsIDOMNode** aFocusNode) { return _to GetFocusNode(aFocusNode); } \
  NS_IMETHOD    GetFocusOffset(PRInt32* aFocusOffset) { return _to GetFocusOffset(aFocusOffset); } \
  NS_IMETHOD    GetIsCollapsed(PRBool* aIsCollapsed) { return _to GetIsCollapsed(aIsCollapsed); } \
  NS_IMETHOD    GetRangeCount(PRInt32* aRangeCount) { return _to GetRangeCount(aRangeCount); } \
  NS_IMETHOD    GetRangeAt(PRInt32 aIndex, nsIDOMRange** aReturn) { return _to GetRangeAt(aIndex, aReturn); }  \
  NS_IMETHOD    ClearSelection() { return _to ClearSelection(); }  \
  NS_IMETHOD    Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset) { return _to Collapse(aParentNode, aOffset); }  \
  NS_IMETHOD    Extend(nsIDOMNode* aParentNode, PRInt32 aOffset) { return _to Extend(aParentNode, aOffset); }  \
  NS_IMETHOD    CollapseToStart() { return _to CollapseToStart(); }  \
  NS_IMETHOD    CollapseToEnd() { return _to CollapseToEnd(); }  \
  NS_IMETHOD    ContainsNode(nsIDOMNode* aNode, PRBool aRecursive, PRBool* aReturn) { return _to ContainsNode(aNode, aRecursive, aReturn); }  \
  NS_IMETHOD    DeleteFromDocument() { return _to DeleteFromDocument(); }  \
  NS_IMETHOD    AddRange(nsIDOMRange* aRange) { return _to AddRange(aRange); }  \
  NS_IMETHOD    RemoveRange(nsIDOMRange* aRange) { return _to RemoveRange(aRange); }  \
  NS_IMETHOD    StartBatchChanges() { return _to StartBatchChanges(); }  \
  NS_IMETHOD    EndBatchChanges() { return _to EndBatchChanges(); }  \
  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener) { return _to AddSelectionListener(aNewListener); }  \
  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove) { return _to RemoveSelectionListener(aListenerToRemove); }  \
  NS_IMETHOD    SetHint(PRBool aRight) { return _to SetHint(aRight); }  \
  NS_IMETHOD    GetHint(PRBool* aReturn) { return _to GetHint(aReturn); }  \
  NS_IMETHOD    GetEnumerator(nsIEnumerator** aReturn) { return _to GetEnumerator(aReturn); }  \
  NS_IMETHOD    ToString(const nsAReadableString& aFormatType, PRUint32 aFlags, PRInt32 aWrapCount, nsAWritableString& aReturn) { return _to ToString(aFormatType, aFlags, aWrapCount, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitSelectionClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptSelection(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMSelection_h__
