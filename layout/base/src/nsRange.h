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
#ifndef nsRange_h___
#define nsRange_h___

/*
 * nsRange.h: interface of the nsRange object.
 */

#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsCOMPtr.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIScriptObjectOwner.h"
#include "prmon.h"

class nsVoidArray;

class nsRange : public nsIDOMRange,
                public nsIDOMNSRange,
                public nsIScriptObjectOwner
{
public:
  NS_DECL_ISUPPORTS

  nsRange();
  virtual ~nsRange();

  // nsIDOMRange interface
  
  NS_IMETHOD    GetIsPositioned(PRBool* aIsPositioned);

  NS_IMETHOD    GetStartContainer(nsIDOMNode** aStartParent);
  NS_IMETHOD    GetStartOffset(PRInt32* aStartOffset);

  NS_IMETHOD    GetEndContainer(nsIDOMNode** aEndParent);
  NS_IMETHOD    GetEndOffset(PRInt32* aEndOffset);

  NS_IMETHOD    GetCollapsed(PRBool* aIsCollapsed);

  NS_IMETHOD    GetCommonAncestorContainer(nsIDOMNode** aCommonParent);

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

  NS_IMETHOD    CompareBoundaryPoints(PRUint16 how, nsIDOMRange* srcRange, PRInt32* ret);

  NS_IMETHOD    DeleteContents();

  NS_IMETHOD    ExtractContents(nsIDOMDocumentFragment** aReturn);
  NS_IMETHOD    CloneContents(nsIDOMDocumentFragment** aReturn);

  NS_IMETHOD    InsertNode(nsIDOMNode* aN);
  NS_IMETHOD    SurroundContents(nsIDOMNode* aN);

  NS_IMETHOD    CloneRange(nsIDOMRange** aReturn);

  NS_IMETHOD    Detach();

  NS_IMETHOD    ToString(nsAWritableString& aReturn);

/*BEGIN nsIDOMNSRange interface implementations*/
  NS_IMETHOD    CreateContextualFragment(const nsAReadableString& aFragment, 
                                         nsIDOMDocumentFragment** aReturn);
  NS_IMETHOD    IsValidFragment(const nsAReadableString& aFragment, PRBool* aReturn);

  NS_IMETHOD    IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset,
                               PRBool* aResult);
  NS_IMETHOD    ComparePoint(nsIDOMNode* aParent, PRInt32 aOffset,
                             PRInt16* aResult);
  NS_IMETHOD    IntersectsNode(nsIDOMNode* aNode, PRBool* aReturn);
  NS_IMETHOD    CompareNode(nsIDOMNode* aNode, PRUint16* aReturn);
/*END nsIDOMNSRange interface implementations*/
  
  NS_IMETHOD    GetHasGeneratedBefore(PRBool *aBool);
  NS_IMETHOD    GetHasGeneratedAfter(PRBool *aBool);
  NS_IMETHOD    SetHasGeneratedBefore(PRBool aBool);
  NS_IMETHOD    SetHasGeneratedAfter(PRBool aBool);
  NS_IMETHOD    SetBeforeAndAfter(PRBool aBefore, PRBool aAfter);

/*BEGIN nsIScriptObjectOwner interface implementations*/
  NS_IMETHOD 		GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD 		SetScriptObject(void *aScriptObject);
/*END nsIScriptObjectOwner interface implementations*/


  // nsRange interface extensions
  
  static NS_METHOD    OwnerGone(nsIContent* aParentNode);
  
  static NS_METHOD    OwnerChildInserted(nsIContent* aParentNode, PRInt32 aOffset);
  
  static NS_METHOD    OwnerChildRemoved(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aRemovedNode);
  
  static NS_METHOD    OwnerChildReplaced(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aReplacedNode);
  
  static NS_METHOD    TextOwnerChanged(nsIContent* aTextNode, PRInt32 aStartOffset, PRInt32 aEndOffset, PRInt32 aReplaceLength);

//private: I wish VC++ would give me a &&*@!#$ break
  PRBool       mIsPositioned;
  PRInt32      mStartOffset;
  PRInt32      mEndOffset;
  nsCOMPtr<nsIDOMNode> mStartParent;
  nsCOMPtr<nsIDOMNode> mEndParent;
  static PRMonitor    *mMonitor;              // monitor to protect the following statics
  static nsVoidArray  *mStartAncestors;       // just keeping these static to avoid reallocing the arrays.
  static nsVoidArray  *mEndAncestors;         // the contents of these arrays are discarded across calls.
  static nsVoidArray  *mStartAncestorOffsets; // this also makes nsRange objects lighter weight.
  static nsVoidArray  *mEndAncestorOffsets;   // 

  // no copy's or assigns
  nsRange(const nsRange&);
  nsRange& operator=(const nsRange&);
  
  // helper routines
  
  static PRBool        InSameDoc(nsIDOMNode* aNode1, nsIDOMNode* aNode2);
  static PRInt32       IndexOf(nsIDOMNode* aNode);
  static PRInt32       FillArrayWithAncestors(nsVoidArray* aArray,nsIDOMNode* aNode);
  static nsCOMPtr<nsIDOMNode>   CommonParent(nsIDOMNode* aNode1, nsIDOMNode* aNode2);
  static nsresult      GetDOMNodeFromContent(nsIContent* inContentNode, nsCOMPtr<nsIDOMNode>* outDomNode);
  static nsresult      GetContentFromDOMNode(nsIDOMNode* inDomNode, nsCOMPtr<nsIContent>* outContentNode);
  static nsresult      PopRanges(nsIDOMNode* aDestNode, PRInt32 aOffset, nsIContent* aSourceNode);
  static nsresult      Lock();
  static nsresult      Unlock();
  
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
                       
  nsresult      ComparePointToRange(nsIDOMNode* aParent, PRInt32 aOffset, PRInt32* aResult);
  
  
  PRInt32       GetAncestorsAndOffsets(nsIDOMNode* aNode, PRInt32 aOffset,
                        nsVoidArray* aAncestorNodes, nsVoidArray* aAncestorOffsets);
  
  nsresult      AddToListOf(nsIDOMNode* aNode);
  
  nsresult      RemoveFromListOf(nsIDOMNode* aNode);
 
  nsresult      ContentOwnsUs(nsIDOMNode* domNode);

  protected:
  	void*				mScriptObject;
	PRBool mBeforeGenContent;
	PRBool mAfterGenContent;
  	
};

// Make a new nsIDOMRange object
nsresult NS_NewRange(nsIDOMRange** aInstancePtrResult);


/*************************************************************************************
 *  Utility routine to compare two "points", were a point is a node/offset pair
 *  Returns -1 if point1 < point2, 1, if point1 > point2,
 *  0 if error or if point1 == point2.
 ************************************************************************************/
PRInt32 ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                      nsIDOMNode* aParent2, PRInt32 aOffset2);


/*************************************************************************************
 *  Utility routine to detect if a content node intersects a range
 ************************************************************************************/
PRBool IsNodeIntersectsRange(nsIContent* aNode, nsIDOMRange* aRange);


/*************************************************************************************
 *  Utility routine to detect if a content node starts before a range and/or 
 *  ends after a range.  If neither it is contained inside the range.
 *  
 *  XXX - callers responsibility to ensure node in same doc as range!
 *
 ************************************************************************************/
nsresult CompareNodeToRange(nsIContent* aNode, 
                            nsIDOMRange* aRange,
                            PRBool *outNodeBefore,
                            PRBool *outNodeAfter);


/*************************************************************************************
 * Utility routine to create a pair of dom points to represent 
 * the start and end locations of a single node.  Return false
 * if we dont' succeed.
 ************************************************************************************/
PRBool GetNodeBracketPoints(nsIContent* aNode, 
                            nsCOMPtr<nsIDOMNode>* outParent,
                            PRInt32* outStartOffset,
                            PRInt32* outEndOffset);

#endif /* nsRange_h___ */
