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

#ifndef nsHTMLEditRules_h__
#define nsHTMLEditRules_h__

#include "nsTextEditRules.h"
#include "nsCOMPtr.h"

class nsHTMLEditRules : public nsTextEditRules
{
public:

  nsHTMLEditRules();
  virtual ~nsHTMLEditRules();

  // nsEditRules methods
  NS_IMETHOD WillDoAction(nsIDOMSelection *aSelection, nsRulesInfo *aInfo, PRBool *aCancel);
  NS_IMETHOD DidDoAction(nsIDOMSelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);

  // nsHTMLEditRules action id's
  enum 
  {
    kInsertBreak = 3000
  };
  
protected:

  enum IterDirection
  {
    kIterForward,
    kIterBackward
  };

  // nsHTMLEditRules implementation methods
  nsresult WillInsertText(nsIDOMSelection  *aSelection, 
                            PRBool          *aCancel,
                            PlaceholderTxn **aTxn,
                            const nsString *inString,
                            nsString       *outString,
                            TypeInState    typeInState);
  nsresult WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult WillDeleteSelection(nsIDOMSelection *aSelection, nsIEditor::Direction aDir, PRBool *aCancel);

  nsresult InsertTab(nsIDOMSelection *aSelection, PRBool *aCancel, PlaceholderTxn **aTxn, nsString *outString);
  nsresult InsertSpace(nsIDOMSelection *aSelection, PRBool *aCancel, PlaceholderTxn **aTxn, nsString *outString);

  nsresult ReturnInHeader(nsIDOMSelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset);
  nsresult ReturnInParagraph(nsIDOMSelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset, PRBool *aCancel);
  nsresult ReturnInListItem(nsIDOMSelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset);

  // helper methods
  static nsresult GetRightmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);
  static nsresult GetLeftmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);
  static nsresult GetPriorNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);
  static nsresult GetNextNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);
  
  static nsresult GetTabAsNBSPs(nsString *outString);
  static nsresult GetTabAsNBSPsAndSpace(nsString *outString);
  
  static nsCOMPtr<nsIAtom> GetTag(nsIDOMNode *aNode);
  static PRBool IsInlineNode(nsIDOMNode *aNode);
  static PRBool IsBlockNode(nsIDOMNode *aNode);
  static nsCOMPtr<nsIDOMNode> GetBlockNodeParent(nsIDOMNode *aNode);
  static PRBool HasSameBlockNodeParent(nsIDOMNode *aNode1, nsIDOMNode *aNode2);
  
  static PRBool IsTextOrElementNode(nsIDOMNode *aNode);
  static PRBool IsTextNode(nsIDOMNode *aNode);
  static PRBool IsEmptyTextNode(nsIDOMNode *aNode);
  
  static PRInt32 GetIndexOf(nsIDOMNode *aParent, nsIDOMNode *aChild);
  
  static PRBool IsHeader(nsIDOMNode *aNode);
  static PRBool IsParagraph(nsIDOMNode *aNode);
  static PRBool IsListItem(nsIDOMNode *aNode);
  static PRBool IsBreak(nsIDOMNode *aNode);
 
  static nsCOMPtr<nsIDOMNode> NextNodeInBlock(nsIDOMNode *aNode, IterDirection aDir);
  static nsresult GetStartNodeAndOffset(nsIDOMSelection *aSelection, nsCOMPtr<nsIDOMNode> *outStartNode, PRInt32 *outStartOffset);
  static nsresult GetEndNodeAndOffset(nsIDOMSelection *aSelection, nsCOMPtr<nsIDOMNode> *outEndNode, PRInt32 *outEndOffset);
  
  nsresult IsPreformatted(nsIDOMNode *aNode, PRBool *aResult);
  nsresult IsNextCharWhitespace(nsIDOMNode *aParentNode, PRInt32 aOffset, PRBool *aResult);
  nsresult IsPrevCharWhitespace(nsIDOMNode *aParentNode, PRInt32 aOffset, PRBool *aResult);

  nsresult SplitNodeDeep(nsIDOMNode *aNode, nsIDOMNode *aSplitPointParent, PRInt32 aSplitPointOffset);

  // data
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
 
  static nsIAtom *sH1Atom;
  static nsIAtom *sH2Atom;
  static nsIAtom *sH3Atom;
  static nsIAtom *sH4Atom;
  static nsIAtom *sH5Atom;
  static nsIAtom *sH6Atom;
  static nsIAtom *sParagraphAtom;
  static nsIAtom *sListItemAtom;
  static nsIAtom *sBreakAtom;
  
  
  static PRInt32 sInstanceCount;
};

#endif //nsHTMLEditRules_h__

