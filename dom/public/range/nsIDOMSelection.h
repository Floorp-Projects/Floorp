/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMSelection_h__
#define nsIDOMSelection_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNode;
class nsIDOMSelectionListener;
class nsIDOMRange;
class nsIEnumerator;

#define NS_IDOMSELECTION_IID \
 { 0xa6cf90e1, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

#ifndef SELECTIONTYPE
enum SelectionType{SELECTION_NORMAL = 0, 
                   SELECTION_SPELLCHECK, 
                   SELECTION_IME_SOLID, 
                   SELECTION_IME_DASHED, 
                   NUM_SELECTIONTYPES};
#define SELECTIONTYPE
#endif

class nsIDOMSelection : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMSELECTION_IID; return iid; }

  NS_IMETHOD    GetAnchorNode(SelectionType aType, nsIDOMNode** aAnchorNode)=0;

  NS_IMETHOD    GetAnchorOffset(SelectionType aType, PRInt32* aAnchorOffset)=0;

  NS_IMETHOD    GetFocusNode(SelectionType aType, nsIDOMNode** aFocusNode)=0;

  NS_IMETHOD    GetFocusOffset(SelectionType aType, PRInt32* aFocusOffset)=0;

  NS_IMETHOD    GetIsCollapsed(SelectionType aType, PRBool* aIsCollapsed)=0;

  NS_IMETHOD    GetRangeCount(SelectionType aType, PRInt32* aRangeCount)=0;

  NS_IMETHOD    GetRangeAt(PRInt32 aIndex, SelectionType aType, nsIDOMRange** aReturn)=0;

  NS_IMETHOD    ClearSelection(SelectionType aType)=0;

  NS_IMETHOD    Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset, SelectionType aType)=0;

  NS_IMETHOD    Extend(nsIDOMNode* aParentNode, PRInt32 aOffset, SelectionType aType)=0;

  NS_IMETHOD    DeleteFromDocument()=0;

  NS_IMETHOD    AddRange(nsIDOMRange* aRange, SelectionType aType)=0;

  NS_IMETHOD    StartBatchChanges()=0;

  NS_IMETHOD    EndBatchChanges()=0;

  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener)=0;

  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove)=0;

  NS_IMETHOD    GetEnumerator(SelectionType aType, nsIEnumerator **aIterator)=0;

};


#define NS_DECL_IDOMSELECTION   \
  NS_IMETHOD    GetAnchorNode(SelectionType aType, nsIDOMNode** aAnchorNode);  \
  NS_IMETHOD    GetAnchorOffset(SelectionType aType, PRInt32* aAnchorOffset);  \
  NS_IMETHOD    GetFocusNode(SelectionType aType, nsIDOMNode** aFocusNode);  \
  NS_IMETHOD    GetFocusOffset(SelectionType aType, PRInt32* aFocusOffset);  \
  NS_IMETHOD    GetIsCollapsed(SelectionType aType, PRBool* aIsCollapsed);  \
  NS_IMETHOD    GetRangeCount(SelectionType aType, PRInt32* aRangeCount);  \
  NS_IMETHOD    GetRangeAt(PRInt32 aIndex, SelectionType aType, nsIDOMRange** aReturn);  \
  NS_IMETHOD    ClearSelection(SelectionType aType);  \
  NS_IMETHOD    Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset, SelectionType aType);  \
  NS_IMETHOD    Extend(nsIDOMNode* aParentNode, PRInt32 aOffset, SelectionType aType);  \
  NS_IMETHOD    DeleteFromDocument();  \
  NS_IMETHOD    AddRange(nsIDOMRange* aRange, SelectionType aType);  \
  NS_IMETHOD    StartBatchChanges();  \
  NS_IMETHOD    EndBatchChanges();  \
  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener);  \
  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove);  \
  NS_IMETHOD    GetEnumerator(SelectionType aType, nsIEnumerator **aIterator); \



#define NS_FORWARD_IDOMSELECTION(_to)  \
  NS_IMETHOD    GetAnchorNode(SelectionType aType, nsIDOMNode** aAnchorNode) { return _to GetAnchorNode(aType, aAnchorNode); } \
  NS_IMETHOD    GetAnchorOffset(SelectionType aType, PRInt32* aAnchorOffset) { return _to GetAnchorOffset(aType, aAnchorOffset, aType); } \
  NS_IMETHOD    GetFocusNode(SelectionType aType, nsIDOMNode** aFocusNode) { return _to GetFocusNode(aType, aFocusNode, aType); } \
  NS_IMETHOD    GetFocusOffset(SelectionType aType, PRInt32* aFocusOffset) { return _to GetFocusOffset(aType, aFocusOffset, aType); } \
  NS_IMETHOD    GetIsCollapsed(SelectionType aType, PRBool* aIsCollapsed) { return _to GetIsCollapsed(aType, aIsCollapsed, aType); } \
  NS_IMETHOD    GetRangeCount(SelectionType aType, PRInt32* aRangeCount) { return _to GetRangeCount(aType, aRangeCount, aType); } \
  NS_IMETHOD    GetRangeAt(PRInt32 aIndex, SelectionType aType, nsIDOMRange** aReturn) { return _to GetRangeAt(aIndex, aType, aReturn); }  \
  NS_IMETHOD    ClearSelection(SelectionType aType) { return _to ClearSelection(); }  \
  NS_IMETHOD    Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset, SelectionType aType) { return _to Collapse(aParentNode, aOffset, aType); }  \
  NS_IMETHOD    Extend(nsIDOMNode* aParentNode, PRInt32 aOffset, SelectionType aType) { return _to Extend(aParentNode, aOffset, aType); }  \
  NS_IMETHOD    DeleteFromDocument() { return _to DeleteFromDocument(); }  \
  NS_IMETHOD    AddRange(nsIDOMRange* aRange, SelectionType aType) { return _to AddRange(aRange, aType); }  \
  NS_IMETHOD    StartBatchChanges() { return _to StartBatchChanges(); }  \
  NS_IMETHOD    EndBatchChanges() { return _to EndBatchChanges(); }  \
  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener) { return _to AddSelectionListener(aNewListener); }  \
  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove) { return _to RemoveSelectionListener(aListenerToRemove); }  \
  NS_IMETHOD    GetEnumerator(SelectionType aType, nsIEnumerator **aIterator){ return _to GetEnumerator(aType, aIterator); }  \


extern "C" NS_DOM nsresult NS_InitSelectionClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptSelection(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMSelection_h__
