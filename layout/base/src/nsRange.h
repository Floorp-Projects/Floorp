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

/*
 * nsRange.h: interface of the nsRange object.
 */

#include "nsIDOMRange.h"

class nsIDOMNode;
class nsIDOMDocumentFragment;
class nsVoidArray;
class nsIContent;

class nsRange : public nsIDOMRange
{
public:
  NS_DECL_ISUPPORTS

  nsRange();
  virtual ~nsRange();

  // nsIDOMRange interface
  
  NS_IMETHOD    GetIsPositioned(PRBool* aIsPositioned);

  NS_IMETHOD    GetStartParent(nsIDOMNode** aStartParent);
  NS_IMETHOD    GetStartOffset(PRInt32* aStartOffset);

  NS_IMETHOD    GetEndParent(nsIDOMNode** aEndParent);
  NS_IMETHOD    GetEndOffset(PRInt32* aEndOffset);

  NS_IMETHOD    GetIsCollapsed(PRBool* aIsCollapsed);

  NS_IMETHOD    GetCommonParent(nsIDOMNode** aCommonParent);

  NS_IMETHOD    SetStart(nsIDOMNode* aParent, PRInt32 aOffset);
  NS_IMETHOD    SetStartBefore(nsIDOMNode* aSibling);
  NS_IMETHOD    SetStartAfter(nsIDOMNode* aSibling);

  NS_IMETHOD    SetEnd(nsIDOMNode* aParent, PRInt32 aOffset);
  NS_IMETHOD    SetEndBefore(nsIDOMNode* aSibling);
  NS_IMETHOD    SetEndAfter(nsIDOMNode* aSibling);

  NS_IMETHOD    Collapse(PRBool aToStart);

  NS_IMETHOD    Unposition();

  NS_IMETHOD    SelectNode(nsIDOMNode* aN);
  NS_IMETHOD    SelectNodeContents(nsIDOMNode* aN);

  NS_IMETHOD    CompareEndPoints(PRUint16 how, nsIDOMRange* srcRange, PRInt32* ret);

  NS_IMETHOD    DeleteContents();

  NS_IMETHOD    ExtractContents(nsIDOMDocumentFragment** aReturn);
  NS_IMETHOD    CloneContents(nsIDOMDocumentFragment** aReturn);

  NS_IMETHOD    InsertNode(nsIDOMNode* aN);
  NS_IMETHOD    SurroundContents(nsIDOMNode* aN);

  NS_IMETHOD    Clone(nsIDOMRange** aReturn);

  NS_IMETHOD    ToString(nsString& aReturn);
  
  // nsRange interface extensions
  
  static NS_METHOD    OwnerGone(nsIContent* aParentNode);
  
  static NS_METHOD    OwnerChildInserted(nsIContent* aParentNode, PRInt32 aOffset);
  
  static NS_METHOD    OwnerChildRemoved(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aRemovedNode);
  
  static NS_METHOD    OwnerChildReplaced(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aReplacedNode);
  
  static NS_METHOD    TextOwnerChanged(nsIContent* aTextNode, PRInt32 aStartOffset, PRInt32 aEndOffset, PRInt32 aReplaceLength);

private:
  PRBool       mIsPositioned;
  nsIDOMNode   *mStartParent;
  nsIDOMNode   *mEndParent;
  PRInt32      mStartOffset;
  PRInt32      mEndOffset;
  static nsVoidArray  *mStartAncestors;       // just keeping these around to avoid reallocing the arrays.
  static nsVoidArray  *mEndAncestors;         // the contents of these arrays are discarded across calls.
  static nsVoidArray  *mStartAncestorOffsets; //
  static nsVoidArray  *mEndAncestorOffsets;   //

  // no copy's or assigns
  nsRange(const nsRange&);
  nsRange& operator=(const nsRange&);
  
  // helper routines
  
  static PRBool        InSameDoc(nsIDOMNode* aNode1, nsIDOMNode* aNode2);
  static PRInt32       IndexOf(nsIDOMNode* aNode);
  static PRInt32       FillArrayWithAncestors(nsVoidArray* aArray,nsIDOMNode* aNode);
  static nsIDOMNode*   CommonParent(nsIDOMNode* aNode1, nsIDOMNode* aNode2);
  static nsresult      GetDOMNodeFromContent(nsIContent* inContentNode, nsIDOMNode** outDomNode);
  static nsresult      GetContentFromDOMNode(nsIDOMNode* inDomNode, nsIContent** outContentNode);
  static nsresult      PopRanges(nsIDOMNode* aDestNode, PRInt32 aOffset, nsIContent* aSourceNode);
  
  static nsresult CloneSibsAndParents(nsIDOMNode* parentNode,
                                      PRInt32 nodeOffset,
                                      nsIDOMNode* clonedNode,
                                      nsIDOMNode* commonParent,
                                      nsIDOMDocumentFragment* docfrag,
                                      PRBool leftP);

  nsresult      DoSetRange(nsIDOMNode* aStartN, PRInt32 aStartOffset,
                             nsIDOMNode* aEndN, PRInt32 aEndOffset);

  PRBool        IsIncreasing(nsIDOMNode* aStartN, PRInt32 aStartOff,
                             nsIDOMNode* aEndN, PRInt32 aEndOff);
                       
  nsresult      IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset, PRBool* aResult);
  
  nsresult      ComparePointToRange(nsIDOMNode* aParent, PRInt32 aOffset, PRInt32* aResult);
  
  
  PRInt32       GetAncestorsAndOffsets(nsIDOMNode* aNode, PRInt32 aOffset,
                        nsVoidArray* aAncestorNodes, nsVoidArray* aAncestorOffsets);
  
  nsresult      AddToListOf(nsIDOMNode* aNode);
  
  nsresult      RemoveFromListOf(nsIDOMNode* aNode);
 
  nsresult      ContentOwnsUs(nsIDOMNode* domNode);
};

// Make a new nsIDOMRange object
nsresult NS_NewRange(nsIDOMRange** aInstancePtrResult);

//
// Utility routine to compare two "points", were a point is a node/offset pair
// Returns -1 if point1 < point2, 1, if point1 > point2,
// 0 if error or if point1 == point2.
//
PRInt32 ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                      nsIDOMNode* aParent2, PRInt32 aOffset2);
