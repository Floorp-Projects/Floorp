/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
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

#ifndef nsTextServicesDocument_h__
#define nsTextServicesDocument_h__

#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIDOMDocument.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIEditor.h"
#include "nsIEditActionListener.h"
#include "nsITextServicesDocument.h"
#include "nsVoidArray.h"
#include "nsTSDNotifier.h"

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

  typedef enum { eUninitialized=0, eInvalid, eValid, ePrev, eNext } TSDIteratorStatus;

  nsCOMPtr<nsIDOMDocument>        mDOMDocument;
  nsCOMPtr<nsIPresShell>          mPresShell;
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

  /* nsITextServicesDocumentInternal method implementations. */
  NS_IMETHOD Init(nsIDOMDocument *aDOMDocument, nsIPresShell *aPresShell);
  NS_IMETHOD SetEditor(nsIEditor *aEditor);

  /* nsITextServicesDocument method implementations. */
  NS_IMETHOD GetCurrentTextBlock(nsString *aStr);
  NS_IMETHOD FirstBlock();
  NS_IMETHOD LastBlock();
  NS_IMETHOD PrevBlock();
  NS_IMETHOD NextBlock();
  NS_IMETHOD IsDone();
  NS_IMETHOD SetSelection(PRInt32 aOffset, PRInt32 aLength);
  NS_IMETHOD DeleteSelection();
  NS_IMETHOD InsertText(const nsString *aText);

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
  nsresult InitContentIterator();
  nsresult AdjustContentIterator();

  nsresult FirstTextNodeInCurrentBlock();
  nsresult FirstTextNodeInPrevBlock();
  nsresult FirstTextNodeInNextBlock();
  nsresult FindFirstTextNodeInPrevBlock(nsIContent **aContent);
  nsresult FindFirstTextNodeInNextBlock(nsIContent **aContent);

  PRBool IsBlockNode(nsIContent *aContent);
  PRBool IsTextNode(nsIContent *aContent);

  PRBool HasSameBlockNodeParent(nsIContent *aContent1, nsIContent *aContent2);

  PRBool SelectionIsCollapsed();
  PRBool SelectionIsValid();

  nsresult RemoveInvalidOffsetEntries();
  nsresult ClearOffsetTable();
  nsresult SplitOffsetEntry(PRInt32 aTableIndex, PRInt32 aOffsetIntoEntry);

  nsresult NodeHasOffsetEntry(nsIDOMNode *aNode, PRBool *aHasEntry, PRInt32 *aEntryIndex);

  /* DEBUG */
  void PrintOffsetTable();
  void PrintContentNode(nsIContent *aContent);

};

#endif // nsTextServicesDocument_h__
