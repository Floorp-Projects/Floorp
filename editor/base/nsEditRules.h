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

#ifndef nsEditRules_h__
#define nsEditRules_h__

class nsHTMLEditor;
class nsIDOMSelection;

/***************************************************************************
 * base for an object to encapsulate any additional info needed to be passed
 * to rules system by the editor
 */
class nsRulesInfo
{
  public:
  
  nsRulesInfo(int aAction) : action(aAction) {}
  virtual ~nsRulesInfo() {}
  
  int action;
};

MOZ_DECL_CTOR_COUNTER(nsEditRules);

/***************************************************************************
 * Interface of editing rules.
 *  
 */
class nsEditRules
{
public:
  nsEditRules()          { MOZ_COUNT_CTOR(nsEditRules); }
  virtual ~nsEditRules() { MOZ_COUNT_DTOR(nsEditRules); }
  NS_IMETHOD Init(nsHTMLEditor *aEditor, PRUint32 aFlags)=0;
  NS_IMETHOD BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection)=0;
  NS_IMETHOD AfterEdit(PRInt32 action, nsIEditor::EDirection aDirection, PRBool aSetSelection)=0;
  NS_IMETHOD WillDoAction(nsIDOMSelection *aSelection, nsRulesInfo *aInfo, PRBool *aCancel, PRBool *aHandled)=0;
  NS_IMETHOD DidDoAction(nsIDOMSelection *aSelection, nsRulesInfo *aInfo, nsresult aResult)=0;
  NS_IMETHOD GetFlags(PRUint32 *aFlags)=0;
  NS_IMETHOD SetFlags(PRUint32 aFlags)=0;
  NS_IMETHOD DocumentIsEmpty(PRBool *aDocumentIsEmpty)=0;
};


#endif //nsEditRules_h__

