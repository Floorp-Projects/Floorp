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
#ifndef nsDocument_h___
#define nsDocument_h___

#include "nsIDocument.h"
#include "nsVoidArray.h"
#include "nsIDOMDocument.h"
#include "nsIScriptObjectOwner.h"

// Base class for our document implementations
class nsDocument : public nsIDocument, public nsIDOMDocument, public nsIScriptObjectOwner {
public:
  NS_DECL_ISUPPORTS

  virtual nsIArena* GetArena();

  virtual void StartDocumentLoad();
  virtual void PauseDocumentLoad();
  virtual void StopDocumentLoad();
  virtual void WaitForDocumentLoad();
  virtual PRBool IsDocumentLoaded();

  /**
   * Return the title of the document. May return null.
   */
  virtual const nsString* GetDocumentTitle() const;

  /**
   * Return the URL for the document. May return null.
   */
  virtual nsIURL* GetDocumentURL() const;

  /**
   * Return a standard name for the document's character set. This will
   * trigger a startDocumentLoad if necessary to answer the question.
   */
  virtual nsCharSetID GetDocumentCharacterSet() const;
  virtual void SetDocumentCharacterSet(nsCharSetID aCharSetID);

  /**
   * Create a new presentation shell that will use aContext for
   * it's presentation context (presentation context's <b>must not</b> be
   * shared among multiple presentation shell's).
   */
  virtual nsIPresShell* CreateShell(nsIPresContext* aContext,
                                    nsIViewManager* aViewManager,
                                    nsIStyleSet* aStyleSet);
  virtual PRBool DeleteShell(nsIPresShell* aShell);
  virtual PRInt32 GetNumberOfShells();
  virtual nsIPresShell* GetShellAt(PRInt32 aIndex);

  /**
   * Return the parent document of this document. Will return null
   * unless this document is within a compound document and has a parent.
   */
  virtual nsIDocument* GetParentDocument();
  virtual void SetParentDocument(nsIDocument* aParent);
  virtual void AddSubDocument(nsIDocument* aSubDoc);
  virtual PRInt32 GetNumberOfSubDocuments();
  virtual nsIDocument* GetSubDocumentAt(PRInt32 aIndex);

  /**
   * Return the root content object for this document.
   */
  virtual nsIContent* GetRootContent();
  virtual void SetRootContent(nsIContent* aRoot);

  /**
   * Get the style sheets owned by this document.
   */
  virtual PRInt32 GetNumberOfStyleSheets();
  virtual nsIStyleSheet* GetStyleSheetAt(PRInt32 aIndex);
  virtual void AddStyleSheet(nsIStyleSheet* aSheet);

  /**
   * Add a new observer of document change notifications. Whenever
   * content is changed, appended, inserted or removed the observers are
   * informed.
   */
  virtual void AddObserver(nsIDocumentObserver* aObserver);

  /**
   * Remove an observer of document change notifications. This will
   * return false if the observer cannot be found.
   */
  virtual PRBool RemoveObserver(nsIDocumentObserver* aObserver);

  // Observation hooks used by content nodes to propagate
  // notifications to document observers.
  virtual void ContentChanged(nsIContent* aContent,
                              nsISubContent* aSubContent,
                              PRInt32 aChangeType);

  virtual void ContentAppended(nsIContent* aContainer);

  virtual void ContentInserted(nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer);

  virtual void ContentReplaced(nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer);

  virtual void ContentWillBeRemoved(nsIContent* aContainer,
                                    nsIContent* aChild,
                                    PRInt32 aIndexInContainer);

  virtual void ContentHasBeenRemoved(nsIContent* aContainer,
                                     nsIContent* aChild,
                                     PRInt32 aIndexInContainer);

public:
  
  virtual nsresult            GetScriptObject(JSContext *aContext, void** aScriptObject);
  virtual nsresult            ResetScriptObject();

  // nsIDOMDocument interface
  virtual nsresult            GetNodeType(PRInt32 *aType);
  virtual nsresult            GetParentNode(nsIDOMNode **aNode);
  virtual nsresult            GetChildNodes(nsIDOMNodeIterator **aIterator);
  virtual nsresult            HasChildNodes();
  virtual nsresult            GetFirstChild(nsIDOMNode **aNode);
  virtual nsresult            GetPreviousSibling(nsIDOMNode **aNode);
  virtual nsresult            GetNextSibling(nsIDOMNode **aNode);
  virtual nsresult            InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild);
  virtual nsresult            ReplaceChild(nsIDOMNode *newChild, nsIDOMNode *oldChild);
  virtual nsresult            RemoveChild(nsIDOMNode *oldChild);
  virtual nsresult            GetMasterDoc(nsIDOMDocument **aDocument);
  virtual nsresult            SetMasterDoc(nsIDOMDocument *aDocument);
  virtual nsresult            GetDocumentType(nsIDOMNode **aDocType); 
  virtual nsresult            SetDocumentType(nsIDOMNode *aNode); 
  virtual nsresult            GetDocumentElement(nsIDOMElement **aElement);
  virtual nsresult            SetDocumentElement(nsIDOMElement *aElement); 
  virtual nsresult            GetDocumentContext(nsIDOMDocumentContext **aDocContext);
  virtual nsresult            SetDocumentContext(nsIDOMDocumentContext *aContext);
  virtual nsresult            CreateDocumentContext(nsIDOMDocumentContext **aDocContext);
  virtual nsresult            CreateElement(nsString &aTagName, 
                                            nsIDOMAttributeList *aAttributes, 
                                            nsIDOMElement **aElement);
  virtual nsresult            CreateTextNode(nsString &aData, nsIDOMText** aTextNode);
  virtual nsresult            CreateComment(nsString &aData, nsIDOMComment **aComment);
  virtual nsresult            CreatePI(nsString &aName, nsString &aData, nsIDOMPI **aPI);
  virtual nsresult            CreateAttribute(nsString &aName, 
                                              nsIDOMNode *value, 
                                              nsIDOMAttribute **aAttribute);
  virtual nsresult            CreateAttributeList(nsIDOMAttributeList **aAttributesList);
  virtual nsresult            CreateTreeIterator(nsIDOMNode **aNode, nsIDOMTreeIterator **aTreeIterator);
  virtual nsresult            GetElementsByTagName(nsIDOMNodeIterator **aIterator);

protected:
  virtual void AddStyleSheetToSet(nsIStyleSheet* aSheet, nsIStyleSet* aSet);  // subclass hook

  nsDocument();
  virtual ~nsDocument();
  nsresult Init();

  nsIArena* mArena;
  nsString* mDocumentTitle;
  nsIURL* mDocumentURL;
  nsCharSetID mCharacterSet;
  nsIDocument* mParentDocument;
  nsVoidArray mSubDocuments;
  nsVoidArray mPresShells;
  nsIContent* mRootContent;
  nsVoidArray mStyleSheets;
  nsVoidArray mObservers;
  void* mScriptObject;
};

#endif /* nsDocument_h___ */
