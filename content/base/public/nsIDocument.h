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
#ifndef nsIDocument_h___
#define nsIDocument_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsIUnicharInputStream.h"
#include "nsGUIEvent.h"

class nsIAtom;
class nsIArena;
class nsIContent;
class nsIDocumentContainer;
class nsIDocumentObserver;
class nsIPresContext;
class nsIPresShell;

class nsIStreamListener;
class nsIStreamObserver;
class nsIStyleSet;
class nsIStyleSheet;
class nsIStyleRule;
class nsIURL;
class nsIURLGroup;
class nsIViewManager;
class nsString;
class nsIScriptContextOwner;
class nsIContentViewerContainer;
class nsIDOMEvent;
class nsIDeviceContext;
class nsIParser;
class nsIDOMNode;
class nsXIFConverter;
class nsINameSpaceManager;
class nsIDOMSelection;
class nsIDOMDocumentFragment;
class nsILineBreaker;
class nsIWordBreaker;

// IID for the nsIDocument interface
#define NS_IDOCUMENT_IID      \
{ 0x94c6ceb0, 0x9447, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// specification for data to be sent via form "post"
// IID for the nsIPostData interface -  152ab6e0-ff13-11d1-beb9-00805f8a66dc
#define NS_IPOSTDATA_IID      \
{ 0x152ab6e0, 0xff13, 0x11d1, \
  {0xbe, 0xb9, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

class nsIPostData : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IPOSTDATA_IID; return iid; }

  virtual PRBool       IsFile()  = 0;    // is the data a file (or raw data)
  virtual const char*  GetData() = 0;    // get the file name or raw data
  virtual PRInt32      GetDataLength() = 0;
};

//----------------------------------------------------------------------

// Document interface
class nsIDocument : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOCUMENT_IID; return iid; }

  // All documents have a memory arena associated with them which is
  // used for memory allocation during document creation. This call
  // returns the arena associated with this document.
  virtual nsIArena* GetArena() = 0;

  NS_IMETHOD StartDocumentLoad(nsIURL *aUrl, 
                               nsIContentViewerContainer* aContainer,
                               nsIStreamListener **aDocListener,
                               const char* aCommand) = 0;

  /**
   * Return the title of the document. May return null.
   */
  virtual const nsString* GetDocumentTitle() const = 0;

  /**
   * Return the URL for the document. May return null.
   */
  virtual nsIURL* GetDocumentURL() const = 0;

  /**
   * Return the URLGroup for the document. May return null.
   */
  virtual nsIURLGroup* GetDocumentURLGroup() const = 0;

  /**
   * Return the base URL for realtive URLs in the document. May return null (or the document URL).
   */
  NS_IMETHOD GetBaseURL(nsIURL*& aURL) const = 0;

  /**
   * Return a standard name for the document's character set. This will
   * trigger a startDocumentLoad if necessary to answer the question.
   */
  virtual nsString* GetDocumentCharacterSet() const = 0;
  virtual void SetDocumentCharacterSet(nsString* aCharSetID) = 0;

  /**
   * Return the Line Breaker for the document
   */
  NS_IMETHOD GetLineBreaker(nsILineBreaker** aResult) = 0;
  NS_IMETHOD SetLineBreaker(nsILineBreaker* aLineBreaker) = 0;
  NS_IMETHOD GetWordBreaker(nsIWordBreaker** aResult) = 0;
  NS_IMETHOD SetWordBreaker(nsIWordBreaker* aWordBreaker) = 0;

  /**
   * Access HTTP header data (this may also get set from other sources, like
   * HTML META tags).
   */
  NS_IMETHOD GetHeaderData(nsIAtom* aHeaderField, nsString& aData) const = 0;
  NS_IMETHOD SetHeaderData(nsIAtom* aheaderField, const nsString& aData) = 0;

  /**
   * Create a new presentation shell that will use aContext for
   * it's presentation context (presentation context's <b>must not</b> be
   * shared among multiple presentation shell's).
   */
  virtual nsresult CreateShell(nsIPresContext* aContext,
                               nsIViewManager* aViewManager,
                               nsIStyleSet* aStyleSet,
                               nsIPresShell** aInstancePtrResult) = 0;
  virtual PRBool DeleteShell(nsIPresShell* aShell) = 0;
  virtual PRInt32 GetNumberOfShells() = 0;
  virtual nsIPresShell* GetShellAt(PRInt32 aIndex) = 0;

  /**
   * Return the parent document of this document. Will return null
   * unless this document is within a compound document and has a parent.
   */
  virtual nsIDocument* GetParentDocument() = 0;
  virtual void SetParentDocument(nsIDocument* aParent) = 0;
  virtual void AddSubDocument(nsIDocument* aSubDoc) = 0;
  virtual PRInt32 GetNumberOfSubDocuments() = 0;
  virtual nsIDocument* GetSubDocumentAt(PRInt32 aIndex) = 0;

  /**
   * Return the root content object for this document.
   */
  virtual nsIContent* GetRootContent() = 0;
  virtual void SetRootContent(nsIContent* aRoot) = 0;

  /**
   * Methods to append to the prolog and epilog of
   * a document. The prolog is the content before the document
   * element, the epilog after.
   */
  NS_IMETHOD AppendToProlog(nsIContent* aContent) = 0;
  NS_IMETHOD AppendToEpilog(nsIContent* aContent) = 0;

  /** 
   * Get the direct children of the document - content in
   * the prolog, the root content and content in the epilog.
   */
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const = 0;
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const = 0;
  NS_IMETHOD GetChildCount(PRInt32& aCount) = 0;

  /**
   * Get the style sheets owned by this document.
   * Style sheets are ordered, most significant last.
   */
  virtual PRInt32 GetNumberOfStyleSheets() = 0;
  virtual nsIStyleSheet* GetStyleSheetAt(PRInt32 aIndex) = 0;
  virtual PRInt32 GetIndexOfStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual void AddStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual void SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                          PRBool mDisabled) = 0;

  /**
   * Set the object from which a document can get a script context.
   * This is the context within which all scripts (during document 
   * creation and during event handling) will run.
   */
  virtual nsIScriptContextOwner *GetScriptContextOwner() = 0;
  virtual void SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner) = 0;

  /**
   * Get the name space manager for this document
   */
  NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager) = 0;

  //----------------------------------------------------------------------

  // Document notification API's

  /**
   * Add a new observer of document change notifications. Whenever
   * content is changed, appended, inserted or removed the observers are
   * informed.
   */
  virtual void AddObserver(nsIDocumentObserver* aObserver) = 0;

  /**
   * Remove an observer of document change notifications. This will
   * return false if the observer cannot be found.
   */
  virtual PRBool RemoveObserver(nsIDocumentObserver* aObserver) = 0;

  // Observation hooks used by content nodes to propagate
  // notifications to document observers.
  NS_IMETHOD BeginLoad() = 0;
  NS_IMETHOD EndLoad() = 0;
  NS_IMETHOD ContentChanged(nsIContent* aContent,
                            nsISupports* aSubContent) = 0;
  // notify that one or two content nodes changed state
  // either may be nsnull, but not both
  NS_IMETHOD ContentStatesChanged(nsIContent* aContent1,
                                  nsIContent* aContent2) = 0;
  NS_IMETHOD AttributeChanged(nsIContent* aChild,
                              nsIAtom* aAttribute,
                              PRInt32 aHint) = 0; // See nsStyleConsts fot hint values
  NS_IMETHOD ContentAppended(nsIContent* aContainer,
                             PRInt32 aNewIndexInContainer) = 0;
  NS_IMETHOD ContentInserted(nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer) = 0;
  NS_IMETHOD ContentReplaced(nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer) = 0;
  NS_IMETHOD ContentRemoved(nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer) = 0;

  // Observation hooks for style data to propogate notifications
  // to document observers
  NS_IMETHOD StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) = 0; // See nsStyleConsts fot hint values
  NS_IMETHOD StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) = 0;
  NS_IMETHOD StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) = 0;

  /**
    * Returns the Selection Object
   */
  NS_IMETHOD GetSelection(nsIDOMSelection ** aSelection) = 0;
  /**
    * Selects all the Content
   */
  NS_IMETHOD SelectAll() = 0;

  /**
    * Finds text in content
   */
  NS_IMETHOD FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound) = 0;

  /**
    * Converts the document or a selection of the 
    * document to XIF (XML Interchange Format)
    * and places the result in aBuffer.
    
    * NOTE: we may way to place the result in a stream,
    * but we will use a string for now -- gpk
  */
  virtual void CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection = nsnull) = 0;
  virtual void ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode) = 0;
  virtual void BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode) = 0;
  virtual void ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode) = 0;
  virtual void FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode) = 0;

  /* Helper methods to help determine the logical positioning of content */
  virtual PRBool IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const = 0;
  virtual PRBool IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const = 0;
  virtual PRBool IsInSelection(nsIDOMSelection* aSelection, const nsIContent *aContent) const = 0;
  virtual nsIContent* GetPrevContent(const nsIContent *aContent) const = 0;
  virtual nsIContent* GetNextContent(const nsIContent *aContent) const = 0;
  virtual void SetDisplaySelection(PRBool aToggle) = 0;
  virtual PRBool GetDisplaySelection() const = 0;

  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext, 
                            nsEvent* aEvent, 
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus) = 0;




};


// XXX These belong somewhere else
extern NS_LAYOUT nsresult
   NS_NewHTMLDocument(nsIDocument** aInstancePtrResult);

extern NS_LAYOUT nsresult
   NS_NewXMLDocument(nsIDocument** aInstancePtrResult);

extern NS_LAYOUT nsresult
   NS_NewImageDocument(nsIDocument** aInstancePtrResult);

extern NS_LAYOUT nsresult
   NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
                          nsIDocument* aOwnerDocument);

// Note: The buffer passed into NewPostData(...) becomes owned by the IPostData
//       instance and is freed when the instance is destroyed...
//
extern NS_LAYOUT nsresult
   NS_NewPostData(PRBool aIsFile, char *aData, nsIPostData** aInstancePtrResult);

#endif /* nsIDocument_h___ */
