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
#include "nsIDOMNSDocument.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMEventCapturer.h"
#include "nsIDOMEventTarget.h"
#include "nsXIFConverter.h"
#include "nsIJSScriptObject.h"
#include "nsIContent.h"

class nsIEventListenerManager;
class nsDOMStyleSheetCollection;
class nsIDOMSelection;


class nsPostData : public nsIPostData {
public:
  nsPostData(PRBool aIsFile, char* aData);

  NS_DECL_ISUPPORTS

  virtual PRBool      IsFile();
  virtual const char* GetData();
  virtual PRInt32     GetDataLength();

protected:
  virtual ~nsPostData();

  PRBool  mIsFile;
  char*   mData;
  PRInt32 mDataLen;
};

class nsDocHeaderData
{
public:
  nsDocHeaderData(nsIAtom* aField, const nsString& aData)
  {
    mField = aField;
    NS_IF_ADDREF(mField);
    mData = aData;
    mNext = nsnull;
  }
  ~nsDocHeaderData(void)
  {
    NS_IF_RELEASE(mField);
    if (nsnull != mNext) {
      delete mNext;
      mNext = nsnull;
    }
  }

  nsIAtom*         mField;
  nsAutoString     mData;
  nsDocHeaderData* mNext;
};

// Base class for our document implementations
class nsDocument : public nsIDocument, 
                   public nsIDOMDocument, 
                   public nsIDOMNSDocument,
                   public nsIScriptObjectOwner, 
                   public nsIDOMEventCapturer, 
                   public nsIJSScriptObject
{
public:
  NS_DECL_ISUPPORTS

  virtual nsIArena* GetArena();

  NS_IMETHOD StartDocumentLoad(nsIURL *aUrl, 
                               nsIContentViewerContainer* aContainer,
                               nsIStreamListener **aDocListener,
                               const char* aCommand);

  /**
   * Return the title of the document. May return null.
   */
  virtual const nsString* GetDocumentTitle() const;

  /**
   * Return the URL for the document. May return null.
   */
  virtual nsIURL* GetDocumentURL() const;

  /**
   * Return the URLGroup for the document. May return null.
   */
  virtual nsIURLGroup* GetDocumentURLGroup() const;

  /**
   * Return the base URL for realtive URLs in the document. May return null (or the document URL).
   */
  NS_IMETHOD GetBaseURL(nsIURL*& aURL) const;

  /**
   * Return a standard name for the document's character set. This will
   * trigger a startDocumentLoad if necessary to answer the question.
   */
  virtual nsString* GetDocumentCharacterSet() const;
  virtual void SetDocumentCharacterSet(nsString* aCharSetID);

  /**
   * Return the Line Breaker for the document
   */
  NS_IMETHOD GetLineBreaker(nsILineBreaker** aResult)  ;
  NS_IMETHOD SetLineBreaker(nsILineBreaker* aLineBreaker) ;
  NS_IMETHOD GetWordBreaker(nsIWordBreaker** aResult)  ;
  NS_IMETHOD SetWordBreaker(nsIWordBreaker* aWordBreaker) ;

  /**
   * Access HTTP header data (this may also get set from other sources, like
   * HTML META tags).
   */
  NS_IMETHOD GetHeaderData(nsIAtom* aHeaderField, nsString& aData) const;
  NS_IMETHOD SetHeaderData(nsIAtom* aheaderField, const nsString& aData);

  /**
   * Create a new presentation shell that will use aContext for
   * it's presentation context (presentation context's <b>must not</b> be
   * shared among multiple presentation shell's).
   */
#if 0
  // XXX Temp hack: moved to nsMarkupDocument
  virtual nsresult CreateShell(nsIPresContext* aContext,
                               nsIViewManager* aViewManager,
                               nsIStyleSet* aStyleSet,
                               nsIPresShell** aInstancePtrResult);
#endif
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
   * These are ordered, highest priority last
   */
  virtual PRInt32 GetNumberOfStyleSheets();
  virtual nsIStyleSheet* GetStyleSheetAt(PRInt32 aIndex);
  virtual PRInt32 GetIndexOfStyleSheet(nsIStyleSheet* aSheet);
  virtual void AddStyleSheet(nsIStyleSheet* aSheet);
  virtual void SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                          PRBool mDisabled);

  /**
   * Set the object from which a document can get a script context.
   * This is the context within which all scripts (during document 
   * creation and during event handling) will run.
   */
  virtual nsIScriptContextOwner *GetScriptContextOwner();
  virtual void SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner);

  /** 
   * Get the name space manager for this document
   */
  NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager);

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
  NS_IMETHOD BeginLoad();
  NS_IMETHOD EndLoad();
  NS_IMETHOD ContentChanged(nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentStateChanged(nsIContent* aContent);
  NS_IMETHOD AttributeChanged(nsIContent* aChild,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);
  NS_IMETHOD ContentAppended(nsIContent* aContainer,
                             PRInt32 aNewIndexInContainer);
  NS_IMETHOD ContentInserted(nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);

  NS_IMETHOD StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint); // See nsStyleConsts fot hint values
  NS_IMETHOD StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule);
  NS_IMETHOD StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);

  /**
    * Returns the Selection Object
   */
  NS_IMETHOD GetSelection(nsIDOMSelection ** aSelection);
  /**
    * Selects all the Content
   */
  NS_IMETHOD SelectAll();

  /**
    * Finds text in content
   */
  NS_IMETHOD FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

  /**
    * Converts the document or a selection of the 
    * document to XIF (XML Interchange Format)
    * and places the result in aBuffer.
    */
  virtual void CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection);
  virtual void ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);
  virtual void BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);
  virtual void ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);
  virtual void FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);


public:
  
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIDOMDocument interface
  NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDoctype);
  NS_IMETHOD    GetImplementation(nsIDOMDOMImplementation** aImplementation);
  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement);

  NS_IMETHOD    CreateElement(const nsString& aTagName, nsIDOMElement** aReturn);
  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn);
  NS_IMETHOD    CreateTextNode(const nsString& aData, nsIDOMText** aReturn);
  NS_IMETHOD    CreateComment(const nsString& aData, nsIDOMComment** aReturn);
  NS_IMETHOD    CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn);
  NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn);
  NS_IMETHOD    CreateAttribute(const nsString& aName, nsIDOMAttr** aReturn);
  NS_IMETHOD    CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn);
  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn);
  NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets);
  NS_IMETHOD    CreateElementWithNameSpace(const nsString& aTagName, 
                                           const nsString& aNameSpace, 
                                           nsIDOMElement** aReturn);
                     
  // nsIDOMNode interface
  NS_IMETHOD    GetNodeName(nsString& aNodeName);
  NS_IMETHOD    GetNodeValue(nsString& aNodeValue);
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue);
  NS_IMETHOD    GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode);
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_IMETHOD    HasChildNodes(PRBool* aHasChildNodes);
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild);
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild);
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling);
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn);
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);
  NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

  // nsIDOMEventCapturer interface
  NS_IMETHOD    CaptureEvent(const nsString& aType);
  NS_IMETHOD    ReleaseEvent(const nsString& aType);

  // nsIDOMEventReceiver interface
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);

  // nsIDOMEventTarget interface
  NS_IMETHOD AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                              PRBool aPostProcess, PRBool aUseCapture);
  NS_IMETHOD RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                 PRBool aPostProcess, PRBool aUseCapture);


  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext, 
                            nsEvent* aEvent, 
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus);


  virtual PRBool IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const;
  virtual PRBool IsInSelection(nsIDOMSelection* aSelection, const nsIContent *aContent) const;
  virtual PRBool IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const;
  virtual nsIContent* GetPrevContent(const nsIContent *aContent) const;
  virtual nsIContent* GetNextContent(const nsIContent *aContent) const;
  virtual void SetDisplaySelection(PRBool aToggle);
  virtual PRBool GetDisplaySelection() const;

  // nsIJSScriptObject interface
  virtual PRBool    AddProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    GetProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    SetProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    EnumerateProperty(JSContext *aContext);
  virtual PRBool    Resolve(JSContext *aContext, jsval aID);
  virtual PRBool    Convert(JSContext *aContext, jsval aID);
  virtual void      Finalize(JSContext *aContext);

protected:
  nsIContent* FindContent(const nsIContent* aStartNode,
                          const nsIContent* aTest1, 
                          const nsIContent* aTest2) const;
  virtual nsresult Reset(nsIURL* aURL);

protected:

  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet);  // subclass hook for sheet ordering

  nsDocument();
  virtual ~nsDocument(); 
  nsresult Init();

  nsIArena* mArena;
  nsString* mDocumentTitle;
  nsIURL* mDocumentURL;
  nsIURLGroup* mDocumentURLGroup;
  nsString* mCharacterSet;
  nsIDocument* mParentDocument;
  nsVoidArray mSubDocuments;
  nsVoidArray mPresShells;
  nsIContent* mRootContent;
  nsVoidArray mStyleSheets;
  nsVoidArray mObservers;
  void* mScriptObject;
  nsIScriptContextOwner *mScriptContextOwner;
  nsIEventListenerManager* mListenerManager;
  PRBool mDisplaySelection;
  PRBool mInDestructor;
  nsDOMStyleSheetCollection *mDOMStyleSheets;
  nsINameSpaceManager* mNameSpaceManager;
  nsDocHeaderData* mHeaderData;
  nsILineBreaker* mLineBreaker;
  nsIWordBreaker* mWordBreaker;
};

#endif /* nsDocument_h___ */
