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

protected:

  enum IterDirection
  {
    kIterForward,
    kIterBackward
  };

  // nsHTMLEditRules implementation methods
  nsresult WillInsertText(nsIDOMSelection  *aSelection, 
                            PRBool         *aCancel,
                            PlaceholderTxn **aTxn,
                            const nsString *inString,
                            nsString       *outString,
                            TypeInState     typeInState,
                            PRInt32         aMaxLength);
  nsresult WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult WillDeleteSelection(nsIDOMSelection *aSelection, nsIEditor::ECollapsedSelectionAction aAction, PRBool *aCancel);
  nsresult WillMakeList(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult WillIndent(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult WillOutdent(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult WillAlign(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult WillMakeHeader(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult WillMakeAddress(nsIDOMSelection *aSelection, PRBool *aCancel);
  nsresult WillMakePRE(nsIDOMSelection *aSelection, PRBool *aCancel);

  nsresult InsertTab(nsIDOMSelection *aSelection, PRBool *aCancel, PlaceholderTxn **aTxn, nsString *outString);
  nsresult InsertSpace(nsIDOMSelection *aSelection, PRBool *aCancel, PlaceholderTxn **aTxn, nsString *outString);

  nsresult ReturnInHeader(nsIDOMSelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset);
  nsresult ReturnInParagraph(nsIDOMSelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset, PRBool *aCancel);
  nsresult ReturnInListItem(nsIDOMSelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset);

  // helper methods
  static nsresult GetTabAsNBSPs(nsString *outString);
  static nsresult GetTabAsNBSPsAndSpace(nsString *outString);
    
  static PRBool IsHeader(nsIDOMNode *aNode);
  static PRBool IsParagraph(nsIDOMNode *aNode);
  static PRBool IsListItem(nsIDOMNode *aNode);
  static PRBool IsBreak(nsIDOMNode *aNode);
   
};

#endif //nsHTMLEditRules_h__

