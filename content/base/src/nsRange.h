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
#ifndef nsRange_h___
#define nsRange_h___

/*
 * nsRange.h: interface of the nsRange object.
 */

#include "nsIDOMRange.h"
#include "nsIRangeUtils.h"
#include "nsIDOMNSRange.h"
#include "nsCOMPtr.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "prmon.h"

class nsVoidArray;

// -------------------------------------------------------------------------------

class nsRangeUtils : public nsIRangeUtils
{
public:
  NS_DECL_ISUPPORTS

  nsRangeUtils();
  virtual ~nsRangeUtils();

  // nsIRangeUtils interface
  NS_IMETHOD_(PRInt32) ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                                     nsIDOMNode* aParent2, PRInt32 aOffset2);
                               
  NS_IMETHOD_(PRBool) IsNodeIntersectsRange(nsIContent* aNode, nsIDOMRange* aRange);
  
  NS_IMETHOD CompareNodeToRange(nsIContent* aNode, 
                                nsIDOMRange* aRange,
                                PRBool *outNodeBefore,
                                PRBool *outNodeAfter);
};

// -------------------------------------------------------------------------------

class nsRange : public nsIDOMRange,
                public nsIDOMNSRange
{
public:
  NS_DECL_ISUPPORTS

  nsRange();
  virtual ~nsRange();

  // for layout module destructor
  static void Shutdown();

  // nsIDOMRange interface
  NS_DECL_NSIDOMRANGE

/*BEGIN nsIDOMNSRange interface implementations*/
  NS_IMETHOD    CreateContextualFragment(const nsAString& aFragment, 
                                         nsIDOMDocumentFragment** aReturn);
  NS_IMETHOD    IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset,
                               PRBool* aResult);
  NS_IMETHOD    ComparePoint(nsIDOMNode* aParent, PRInt32 aOffset,
                             PRInt16* aResult);
  NS_IMETHOD    IntersectsNode(nsIDOMNode* aNode, PRBool* aReturn);
  NS_IMETHOD    CompareNode(nsIDOMNode* aNode, PRUint16* aReturn);
  NS_IMETHOD    NSDetach();
/*END nsIDOMNSRange interface implementations*/
  
  NS_IMETHOD    GetHasGeneratedBefore(PRBool *aBool);
  NS_IMETHOD    GetHasGeneratedAfter(PRBool *aBool);
  NS_IMETHOD    SetHasGeneratedBefore(PRBool aBool);
  NS_IMETHOD    SetHasGeneratedAfter(PRBool aBool);
  NS_IMETHOD    SetBeforeAndAfter(PRBool aBefore, PRBool aAfter);

  // nsRange interface extensions
  
  static NS_METHOD    OwnerGone(nsIContent* aParentNode);
  
  static NS_METHOD    OwnerChildInserted(nsIContent* aParentNode, PRInt32 aOffset);
  
  static NS_METHOD    OwnerChildRemoved(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aRemovedNode);
  
  static NS_METHOD    OwnerChildReplaced(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aReplacedNode);
  
  static NS_METHOD    TextOwnerChanged(nsIContent* aTextNode, PRInt32 aStartOffset, PRInt32 aEndOffset, PRInt32 aReplaceLength);

protected:

  PRPackedBool mBeforeGenContent;
  PRPackedBool mAfterGenContent;

  PRPackedBool mIsPositioned;
  PRPackedBool mIsDetached;

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
 
public:
  // helper routines
  
  static PRInt32       IndexOf(nsIDOMNode* aNode);
  static nsresult      PopRanges(nsIDOMNode* aDestNode, PRInt32 aOffset, nsIContent* aSourceNode);
  static nsresult      Lock();
  static nsresult      Unlock();

  static nsresult CloneParentsBetween(nsIDOMNode* aAncestor, 
                                      nsIDOMNode* aNode,
                                      nsIDOMNode** closestAncestor,
                                      nsIDOMNode** farthestAncestor);

  /**
   *  Utility routine to compare two "points", were a point is a node/offset pair
   *  Returns -1 if point1 < point2, 1, if point1 > point2,
   *  0 if error or if point1 == point2.
   */
  static PRInt32 ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                               nsIDOMNode* aParent2, PRInt32 aOffset2);

  /**
   *  Utility routine to detect if a content node intersects a range
   */
  static PRBool IsNodeIntersectsRange(nsIContent* aNode, nsIDOMRange* aRange);

/******************************************************************************
 *  Utility routine to detect if a content node starts before a range and/or 
 *  ends after a range.  If neither it is contained inside the range.
 *  
 *  XXX - callers responsibility to ensure node in same doc as range!
 *
 *****************************************************************************/
  static nsresult CompareNodeToRange(nsIContent* aNode, nsIDOMRange* aRange,
                                     PRBool *outNodeBefore,
                                     PRBool *outNodeAfter);

protected:

  // CollapseRangeAfterDelete() should only be called from DeleteContents()
  // or ExtractContents() since it makes certain assumptions about the state
  // of the range used. It's purpose is to collapse the range according to
  // the range spec after the removal of nodes within the range.
  static nsresult CollapseRangeAfterDelete(nsIDOMRange *aRange);

  static PRInt32  GetNodeLength(nsIDOMNode *aNode);
  
  nsresult      DoSetRange(nsIDOMNode* aStartN, PRInt32 aStartOffset,
                             nsIDOMNode* aEndN, PRInt32 aEndOffset);

  static PRBool IsIncreasing(nsIDOMNode* aStartN, PRInt32 aStartOff,
                             nsIDOMNode* aEndN, PRInt32 aEndOff);
  PRBool        IsDetached(){return mIsDetached;}
                       
  nsresult      ComparePointToRange(nsIDOMNode* aParent, PRInt32 aOffset, PRInt32* aResult);
  
  
  nsresult      AddToListOf(nsIDOMNode* aNode);
  
  void          RemoveFromListOf(nsIDOMNode* aNode);
 
  nsresult      ContentOwnsUs(nsIDOMNode* domNode);

  nsresult      GetIsPositioned(PRBool* aIsPositioned);
};

// Make a new nsIDOMRange object
nsresult NS_NewRange(nsIDOMRange** aInstancePtrResult);

// Make a new nsIRangeUtils object
nsresult NS_NewRangeUtils(nsIRangeUtils** aInstancePtrResult);


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
