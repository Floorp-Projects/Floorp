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

  NS_IMETHOD    GetAnchorNodeAndOffset(nsIDOMNode** aAnchorNode, PRInt32* aOffset)=0;

  NS_IMETHOD    GetFocusNodeAndOffset(nsIDOMNode** aFocusNode, PRInt32* aOffset)=0;

  NS_IMETHOD    ClearSelection()=0;

  NS_IMETHOD    Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset)=0;

  NS_IMETHOD    Extend(nsIDOMNode* aParentNode, PRInt32 aOffset)=0;

  NS_IMETHOD    DeleteFromDocument()=0;

  NS_IMETHOD    AddRange(nsIDOMRange* aRange)=0;

  NS_IMETHOD    StartBatchChanges()=0;

  NS_IMETHOD    EndBatchChanges()=0;

  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener)=0;

  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove)=0;
};


#define NS_DECL_IDOMSELECTION   \
  NS_IMETHOD    GetAnchorNode(nsIDOMNode** aAnchorNode);  \
  NS_IMETHOD    GetAnchorOffset(PRInt32* aAnchorOffset);  \
  NS_IMETHOD    GetFocusNode(nsIDOMNode** aFocusNode);  \
  NS_IMETHOD    GetFocusOffset(PRInt32* aFocusOffset);  \
  NS_IMETHOD    GetIsCollapsed(PRBool* aIsCollapsed);  \
  NS_IMETHOD    GetAnchorNodeAndOffset(nsIDOMNode** aAnchorNode, PRInt32* aOffset);  \
  NS_IMETHOD    GetFocusNodeAndOffset(nsIDOMNode** aFocusNode, PRInt32* aOffset);  \
  NS_IMETHOD    ClearSelection();  \
  NS_IMETHOD    Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset);  \
  NS_IMETHOD    Extend(nsIDOMNode* aParentNode, PRInt32 aOffset);  \
  NS_IMETHOD    DeleteFromDocument();  \
  NS_IMETHOD    AddRange(nsIDOMRange* aRange);  \
  NS_IMETHOD    StartBatchChanges();  \
  NS_IMETHOD    EndBatchChanges();  \
  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener);  \
  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove);  \



#define NS_FORWARD_IDOMSELECTION(_to)  \
  NS_IMETHOD    GetAnchorNode(nsIDOMNode** aAnchorNode) { return _to##GetAnchorNode(aAnchorNode); } \
  NS_IMETHOD    GetAnchorOffset(PRInt32* aAnchorOffset) { return _to##GetAnchorOffset(aAnchorOffset); } \
  NS_IMETHOD    GetFocusNode(nsIDOMNode** aFocusNode) { return _to##GetFocusNode(aFocusNode); } \
  NS_IMETHOD    GetFocusOffset(PRInt32* aFocusOffset) { return _to##GetFocusOffset(aFocusOffset); } \
  NS_IMETHOD    GetIsCollapsed(PRBool* aIsCollapsed) { return _to##GetIsCollapsed(aIsCollapsed); } \
  NS_IMETHOD    GetAnchorNodeAndOffset(nsIDOMNode** aAnchorNode, PRInt32* aOffset) { return _to##GetAnchorNodeAndOffset(aAnchorNode, aOffset); }  \
  NS_IMETHOD    GetFocusNodeAndOffset(nsIDOMNode** aFocusNode, PRInt32* aOffset) { return _to##GetFocusNodeAndOffset(aFocusNode, aOffset); }  \
  NS_IMETHOD    ClearSelection() { return _to##ClearSelection(); }  \
  NS_IMETHOD    Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset) { return _to##Collapse(aParentNode, aOffset); }  \
  NS_IMETHOD    Extend(nsIDOMNode* aParentNode, PRInt32 aOffset) { return _to##Extend(aParentNode, aOffset); }  \
  NS_IMETHOD    DeleteFromDocument() { return _to##DeleteFromDocument(); }  \
  NS_IMETHOD    AddRange(nsIDOMRange* aRange) { return _to##AddRange(aRange); }  \
  NS_IMETHOD    StartBatchChanges() { return _to##StartBatchChanges(); }  \
  NS_IMETHOD    EndBatchChanges() { return _to##EndBatchChanges(); }  \
  NS_IMETHOD    AddSelectionListener(nsIDOMSelectionListener* aNewListener) { return _to##AddSelectionListener(aNewListener); }  \
  NS_IMETHOD    RemoveSelectionListener(nsIDOMSelectionListener* aListenerToRemove) { return _to##RemoveSelectionListener(aListenerToRemove); }  \


extern "C" NS_DOM nsresult NS_InitSelectionClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptSelection(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMSelection_h__
