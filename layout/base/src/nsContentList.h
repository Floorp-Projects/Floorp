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
#ifndef nsContentList_h___
#define nsContentList_h___

#include "nsISupports.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMNodeList.h"
#include "nsIDocumentObserver.h"
#include "nsIScriptObjectOwner.h"

typedef PRBool (*nsContentListMatchFunc)(nsIContent* aContent, void* aData);

class nsIDocument;

class nsContentList : public nsIDOMNodeList, 
                      public nsIDOMHTMLCollection, 
                      public nsIScriptObjectOwner, 
                      public nsIDocumentObserver {
protected:
  nsContentList(nsIDocument *aDocument);
public:
  nsContentList(nsIDocument *aDocument, 
                nsIAtom* aMatchAtom, 
                PRInt32 aMatchNameSpaceId,
                nsIContent* aRootContent=nsnull);
  nsContentList(nsIDocument *aDocument, 
                nsContentListMatchFunc aFunc,
                void* aData,
                nsIContent* aRootContent=nsnull);
  virtual ~nsContentList();

  NS_DECL_ISUPPORTS

  // nsIDOMHTMLCollection
  NS_IMETHOD GetLength(PRUint32* aLength);

  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMNode** aReturn);

  NS_IMETHOD NamedItem(const nsString& aName, nsIDOMNode** aReturn);

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);
  
  // nsIDocumentObserver
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginReflow(nsIDocument *aDocument,
			                   nsIPresShell* aShell) { return NS_OK; }
  NS_IMETHOD EndReflow(nsIDocument *aDocument,
		                   nsIPresShell* aShell) { return NS_OK; } 
  NS_IMETHOD ContentChanged(nsIDocument *aDocument,
			                      nsIContent* aContent,
                            nsISupports* aSubContent) { return NS_OK; }
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2) { return NS_OK; }
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint) { return NS_OK; }
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer);
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aDisabled) { return NS_OK; }
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) { return NS_OK; }
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument);

protected:
  nsresult Match(nsIContent *aContent, PRBool *aMatch);
  nsresult Add(nsIContent *aContent);
  nsresult Remove(nsIContent *aContent);
  nsresult Reset();
  void Init(nsIDocument *aDocument);
  void PopulateWith(nsIContent *aContent, PRBool aIncludeRoot);
  PRBool MatchSelf(nsIContent *aContent);
  void PopulateSelf();
  void DisconnectFromDocument();
  PRBool IsDescendantOfRoot(nsIContent* aContainer);
  PRBool ContainsRoot(nsIContent* aContent);
  nsresult CheckDocumentExistence();

  static nsIAtom* gWildCardAtom;

  nsIAtom* mMatchAtom;
  PRInt32 mMatchNameSpaceId;
  nsContentListMatchFunc mFunc;
  void* mData;
  nsVoidArray mContent;
  void *mScriptObject;
  nsIDocument *mDocument;
  nsIContent* mRootContent;
  PRBool mMatchAll;
};

#endif // nsContentList_h___
