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

#ifndef nsTextServicesDocument_h__
#define nsTextServicesDocument_h__

#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIEditor.h"
#include "nsIEditActionListener.h"
#include "nsITextServicesDocument.h"
#include "nsVoidArray.h"
#include "nsTSDNotifier.h"
#include "nsISelectionController.h"

/** implementation of a text services object.
 *
 */
class nsTextServicesDocument : public nsITextServicesDocument
{
private:
  static nsIAtom *sAAtom;
  static nsIAtom *sAddressAtom;
  static nsIAtom *sBigAtom;
  static nsIAtom *sBlinkAtom;
  static nsIAtom *sBAtom;
  static nsIAtom *sCiteAtom;
  static nsIAtom *sCodeAtom;
  static nsIAtom *sDfnAtom;
  static nsIAtom *sEmAtom;
  static nsIAtom *sFontAtom;
  static nsIAtom *sIAtom;
  static nsIAtom *sKbdAtom;
  static nsIAtom *sKeygenAtom;
  static nsIAtom *sNobrAtom;
  static nsIAtom *sSAtom;
  static nsIAtom *sSampAtom;
  static nsIAtom *sSmallAtom;
  static nsIAtom *sSpacerAtom;
  static nsIAtom *sSpanAtom;      
  static nsIAtom *sStrikeAtom;
  static nsIAtom *sStrongAtom;
  static nsIAtom *sSubAtom;
  static nsIAtom *sSupAtom;
  static nsIAtom *sTtAtom;
  static nsIAtom *sUAtom;
  static nsIAtom *sVarAtom;
  static nsIAtom *sWbrAtom;

  static PRInt32 sInstanceCount;

  typedef enum { eIsDone=0,        // No iterator (I), or itertor doesn't point to anything valid.
                 eValid,           // I points to first text node (TN) in current block (CB).
                 ePrev,            // No TN in CB, I points to first TN in prev block.
                 eNext             // No TN in CB, I points to first TN in next block.
  } TSDIteratorStatus;

  nsCOMPtr<nsIDOMDocument>        mDOMDocument;
  nsCOMPtr<nsISelectionController>mSelCon;
  nsCOMPtr<nsIEditor>             mEditor;
  nsCOMPtr<nsIContentIterator>    mIterator;
  TSDIteratorStatus               mIteratorStatus;
  nsCOMPtr<nsIContent>            mPrevTextBlock;
  nsCOMPtr<nsIContent>            mNextTextBlock;
  nsCOMPtr<nsIEditActionListener> mNotifier;
  nsVoidArray                     mOffsetTable;

  PRInt32                         mSelStartIndex;
  PRInt32                         mSelStartOffset;
  PRInt32                         mSelEndIndex;
  PRInt32                         mSelEndOffset;

public:

  /** The default constructor.
   */
  nsTextServicesDocument();

  /** The default destructor.
   */
  virtual ~nsTextServicesDocument();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsITextServicesDocument method implementations. */
  NS_IMETHOD InitWithDocument(nsIDOMDocument *aDOMDocument, nsIPresShell *aPresShell);
  NS_IMETHOD InitWithEditor(nsIEditor *aEditor);
  NS_IMETHOD CanEdit(PRBool *aCanEdit);
  NS_IMETHOD GetCurrentTextBlock(nsString *aStr);
  NS_IMETHOD FirstBlock();
  NS_IMETHOD LastBlock();
  NS_IMETHOD FirstSelectedBlock(TSDBlockSelectionStatus *aSelStatus, PRInt32 *aSelOffset, PRInt32 *aSelLength);
  NS_IMETHOD LastSelectedBlock(TSDBlockSelectionStatus *aSelStatus, PRInt32 *aSelOffset, PRInt32 *aSelLength);
  NS_IMETHOD PrevBlock();
  NS_IMETHOD NextBlock();
  NS_IMETHOD IsDone(PRBool *aIsDone);
  NS_IMETHOD SetSelection(PRInt32 aOffset, PRInt32 aLength);
  NS_IMETHOD ScrollSelectionIntoView();
  NS_IMETHOD DeleteSelection();
  NS_IMETHOD InsertText(const nsString *aText);
  NS_IMETHOD SetDisplayStyle(TSDDisplayStyle aStyle);

  /* nsIEditActionListener method implementations. */
  nsresult InsertNode(nsIDOMNode * aNode,
                      nsIDOMNode * aParent,
                      PRInt32      aPosition);
  nsresult DeleteNode(nsIDOMNode * aChild);
  nsresult SplitNode(nsIDOMNode * aExistingRightNode,
                     PRInt32      aOffset,
                     nsIDOMNode * aNewLeftNode);
  nsresult JoinNodes(nsIDOMNode  *aLeftNode,
                     nsIDOMNode  *aRightNode,
                     nsIDOMNode  *aParent);

private:

  /* nsTextServicesDocument private methods. */

  nsresult CreateContentIterator(nsIDOMRange *aRange, nsIContentIterator **aIterator);

  nsresult GetDocumentContentRootNode(nsIDOMNode **aNode);
  nsresult CreateDocumentContentRange(nsIDOMRange **aRange);
  nsresult CreateDocumentContentRootToNodeOffsetRange(nsIDOMNode *aParent, PRInt32 aOffset, PRBool aToStart, nsIDOMRange **aRange);
  nsresult CreateDocumentContentIterator(nsIContentIterator **aIterator);

  nsresult AdjustContentIterator();

  nsresult FirstTextNodeInCurrentBlock(nsIContentIterator *aIterator);
  nsresult FirstTextNodeInPrevBlock(nsIContentIterator *aIterator);
  nsresult FirstTextNodeInNextBlock(nsIContentIterator *aIterator);
  nsresult GetFirstTextNodeInPrevBlock(nsIContent **aContent);
  nsresult GetFirstTextNodeInNextBlock(nsIContent **aContent);

  PRBool IsBlockNode(nsIContent *aContent);
  PRBool IsTextNode(nsIContent *aContent);
  PRBool IsTextNode(nsIDOMNode *aNode);

  PRBool HasSameBlockNodeParent(nsIContent *aContent1, nsIContent *aContent2);

  nsresult SetSelectionInternal(PRInt32 aOffset, PRInt32 aLength, PRBool aDoUpdate);
  nsresult GetSelection(TSDBlockSelectionStatus *aSelStatus, PRInt32 *aSelOffset, PRInt32 *aSelLength);
  nsresult GetCollapsedSelection(TSDBlockSelectionStatus *aSelStatus, PRInt32 *aSelOffset, PRInt32 *aSelLength);
  nsresult GetUncollapsedSelection(TSDBlockSelectionStatus *aSelStatus, PRInt32 *aSelOffset, PRInt32 *aSelLength);

  PRBool SelectionIsCollapsed();
  PRBool SelectionIsValid();

  nsresult ComparePoints(nsIDOMNode *aParent1, PRInt32 aOffset1, nsIDOMNode *aParent2, PRInt32 aOffset2, PRInt32 *aResult);
  nsresult GetRangeEndPoints(nsIDOMRange *aRange, nsIDOMNode **aParent1, PRInt32 *aOffset1, nsIDOMNode **aParent2, PRInt32 *aOffset2);
  nsresult CreateRange(nsIDOMNode *aStartParent, PRInt32 aStartOffset, nsIDOMNode *aEndParent, PRInt32 aEndOffset, nsIDOMRange **aRange);

  nsresult RemoveInvalidOffsetEntries();
  nsresult CreateOffsetTable(nsString *aStr=0);
  nsresult ClearOffsetTable();
  nsresult SplitOffsetEntry(PRInt32 aTableIndex, PRInt32 aOffsetIntoEntry);

  nsresult NodeHasOffsetEntry(nsIDOMNode *aNode, PRBool *aHasEntry, PRInt32 *aEntryIndex);

  /* DEBUG */
  void PrintOffsetTable();
  void PrintContentNode(nsIContent *aContent);

};

#endif // nsTextServicesDocument_h__
