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
#include "nsIContent.h"

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

  // nsHTMLEditRules implementation methods
  nsresult WillInsertText(nsIDOMSelection  *aSelection, 
                            PRBool          *aCancel,
                            PlaceholderTxn **aTxn,
                            const nsString *inString,
                            nsString       *outString,
                            TypeInState    typeInState);
  nsresult DidInsertText(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult DidInsertBreak(nsIDOMSelection *aSelection, nsresult aResult);

  nsresult InsertTab(nsIDOMSelection *aSelection, PRBool *aCancel, PlaceholderTxn **aTxn, nsString *outString);
  nsresult InsertSpace(nsIDOMSelection *aSelection, PRBool *aCancel, PlaceholderTxn **aTxn, nsString *outString);

  // helper methods
  PRBool IsBlockNode(nsCOMPtr <nsIContent> aContent);
  nsCOMPtr<nsIContent> GetBlockNodeParent(nsCOMPtr<nsIContent> aContent);
  nsCOMPtr<nsIDOMNode> GetStartNode(nsIDOMSelection *aSelection);
  nsresult IsPreformatted(nsCOMPtr<nsIDOMNode> aNode, PRBool *aResult);

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
  
  static PRInt32 sInstanceCount;
};

#endif //nsHTMLEditRules_h__

