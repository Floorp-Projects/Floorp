/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

#ifndef __inDOMDataSource_h__
#define __inDOMDataSource_h__

#include "inIDOMDataSource.h"
#include "inDOMRDFResource.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"
#include "nsIDocumentObserver.h"
#include "nsIDOMDocument.h"

#define IN_DOMDATASOURCE_ID "Inspector_DOM"

class inDOMDataSource : public inIDOMDataSource,
                        public nsIRDFDataSource,
                        public nsIDocumentObserver
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_INIDOMDATASOURCE
	NS_DECL_NSIRDFDATASOURCE

	inDOMDataSource();
	virtual ~inDOMDataSource();
	nsresult Init();

  // nsIDocumentObserver
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell) { return NS_OK; }
  NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell) { return NS_OK; } 
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
                              PRInt32      aHint);
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
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument, nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument, nsIStyleSheet* aStyleSheet) { return NS_OK; }
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
  nsCOMPtr<nsIRDFResource> kINS_DOMRoot;
  nsCOMPtr<nsIRDFResource> kINS_Child;
  nsCOMPtr<nsIRDFResource> kINS_Attribute;
  nsCOMPtr<nsIRDFResource> kINS_NodeName;
  nsCOMPtr<nsIRDFResource> kINS_NodeValue;
  nsCOMPtr<nsIRDFResource> kINS_NodeType;
  nsCOMPtr<nsIRDFResource> kINS_Prefix;
  nsCOMPtr<nsIRDFResource> kINS_LocalName;
  nsCOMPtr<nsIRDFResource> kINS_NamespaceURI;
  nsCOMPtr<nsIRDFResource> kINS_Anonymous;
  nsCOMPtr<nsIRDFResource> kINS_HasChildren;

protected:
  nsCOMPtr<nsIDOMDocument> mDocument;
  PRBool mShowAnonymousContent;
  PRBool mShowSubDocuments;
  PRUint16 mFilters;
  nsCOMPtr<nsIRDFService> mRDFService;
  nsAutoVoidArray *mObservers;

  nsIRDFService* GetRDFService();
  // Filters
  PRUint16 GetNodeTypeKey(PRUint16 aType);
  // Datasource Construction
  nsresult GetTargetsForKnownObject(nsISupports *object, nsIRDFResource *aProperty, PRBool aShowAnon, nsISupportsArray *arcs);
  nsresult CreateDOMNodeArcs(nsIDOMNode *node, PRBool aShowAnon, nsISupportsArray* arcs);
  nsresult CreateChildNodeArcs(nsIDOMNode *aNode, nsISupportsArray *aArcs);
  nsresult CreateDOMNodeListArcs(nsIDOMNodeList *nodelist, nsISupportsArray *arcs);
  nsresult CreateDOMNamedNodeMapArcs(nsIDOMNamedNodeMap *aNodeMap, nsISupportsArray *aArcs);
  nsresult CreateDOMNodeTarget(nsIDOMNode *node, nsIRDFResource *aProperty, nsIRDFNode **aResult);
  nsresult CreateRootTarget(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode **aResult);
  nsresult CreateLiteral(nsString& str, nsIRDFNode **aResult);
  nsresult GetFieldNameFromRes(nsIRDFResource* aProperty, nsAutoString* aResult);
  
  // Observer Notification
  static PRBool PR_CALLBACK ChangeEnumFunc(void *aElement, void *aData);
  static PRBool PR_CALLBACK AssertEnumFunc(void *aElement, void *aData);
  static PRBool PR_CALLBACK UnassertEnumFunc(void *aElement, void *aData);
  nsresult NotifyObservers(nsIRDFResource *subject, nsIRDFResource *property, nsIRDFNode *object, PRUint32 aType);
  // Misc
  PRBool IsObjectInCache(nsISupports* aObject);
  nsresult RemoveResourceForObject(nsISupports* aObject);
  PRBool HasChildren(nsIDOMNode* aContainer);
  static nsresult FindAttrRes(nsIContent* aContent, PRInt32 aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aAttrRes);
  static PRBool PR_CALLBACK FindAttrResEnumFunc(nsHashKey *aKey, void *aData, void* closure);
  static PRBool HasChild(nsIDOMNode* aContainer, nsIDOMNode* aChild);
  static PRBool HasAttribute(nsIDOMNode* aContainer, nsIDOMNode* aAttr);
  static void DumpResourceValue(nsIRDFResource* aRes);
};


//////////

typedef struct _inDOMDSNotification {
  nsIRDFDataSource *datasource;
  nsIRDFResource *subject;
  nsIRDFResource *property;
  nsIRDFNode *object;
} inDOMDSNotification;

typedef struct _inDOMDSFindAttrInfo {
  nsIContent *mCrap;
  nsIContent *mContent;
  PRInt32 mNameSpaceID;
  nsIAtom *mAttribute;
  nsIRDFResource **mAttrRes;
} inDOMDSFindAttrInfo;

#endif // __inDOMDataSource_h__


