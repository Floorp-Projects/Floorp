/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsContentList_h___
#define nsContentList_h___

#include "nsISupports.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMNodeList.h"
#include "nsIDocumentObserver.h"
#include "nsIContentList.h"

typedef PRBool (*nsContentListMatchFunc)(nsIContent* aContent,
                                         nsString* aData);

class nsIDocument;
class nsIDOMHTMLFormElement;


class nsBaseContentList : public nsIDOMNodeList
{
public:
  nsBaseContentList();
  virtual ~nsBaseContentList();

  NS_DECL_ISUPPORTS

  // nsIDOMNodeList
  NS_DECL_NSIDOMNODELIST

  NS_IMETHOD AppendElement(nsIContent *aContent);
  NS_IMETHOD RemoveElement(nsIContent *aContent);
  NS_IMETHOD IndexOf(nsIContent *aContent, PRInt32& aIndex);
  NS_IMETHOD Reset();

protected:
  nsAutoVoidArray mElements;
};


// This class is used only by form element code and this is a static
// list of elements. NOTE! This list holds strong references to
// the elements in the list.
class nsFormContentList : public nsBaseContentList
{
public:
  nsFormContentList(nsIDOMHTMLFormElement *aForm,
                    nsBaseContentList& aContentList);
  virtual ~nsFormContentList();

  NS_IMETHOD AppendElement(nsIContent *aContent);
  NS_IMETHOD RemoveElement(nsIContent *aContent);

  NS_IMETHOD Reset();
};

class nsContentList : public nsBaseContentList,
                      public nsIDOMHTMLCollection,
                      public nsIDocumentObserver,
                      public nsIContentList
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsContentList(const nsContentList& aContentList);
  nsContentList(nsIDocument *aDocument);
  nsContentList(nsIDocument *aDocument, 
                nsIAtom* aMatchAtom, 
                PRInt32 aMatchNameSpaceId,
                nsIContent* aRootContent=nsnull);
  nsContentList(nsIDocument *aDocument, 
                nsContentListMatchFunc aFunc,
                const nsAReadableString& aData,
                nsIContent* aRootContent=nsnull);
  virtual ~nsContentList();

  // nsIDOMHTMLCollection
  NS_IMETHOD GetLength(PRUint32* aLength);
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMNode** aReturn);
  NS_IMETHOD NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn);

  /// nsIContentList
  NS_IMETHOD GetLength(PRUint32* aLength, PRBool aDoFlush);
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMNode** aReturn,
                  PRBool aDoFlush);
  NS_IMETHOD NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn,
                       PRBool aDoFlush);
  NS_IMETHOD IndexOf(nsIContent *aContent, PRInt32& aIndex,
                     PRBool aDoFlush);

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
                              PRInt32      aNameSpaceID,
                              nsIAtom*     aAttribute,
                              PRInt32      aModType,
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
  void Init(nsIDocument *aDocument);
  void PopulateWith(nsIContent *aContent, PRBool aIncludeRoot);
  PRBool MatchSelf(nsIContent *aContent);
  void PopulateSelf();
  void DisconnectFromDocument();
  PRBool IsDescendantOfRoot(nsIContent* aContainer);
  PRBool ContainsRoot(nsIContent* aContent);
  nsresult CheckDocumentExistence();

  nsIAtom* mMatchAtom;
  PRInt32 mMatchNameSpaceId;
  nsContentListMatchFunc mFunc;
  nsString* mData;
  nsIDocument* mDocument;
  nsIContent* mRootContent;
  PRBool mMatchAll;
};

#endif // nsContentList_h___
