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
#ifndef nsDocument_h___
#define nsDocument_h___

#include "nsIDocument.h"
#include "nsWeakReference.h"
#include "nsWeakPtr.h"
#include "nsVoidArray.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentStyle.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDiskDocument.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMEventTarget.h"
#include "nsIXIFConverter.h"
#include "nsIJSScriptObject.h"
#include "nsIContent.h"
#include "nsGenericDOMNodeList.h"
#include "nsIPrincipal.h"
#include "nsIBindingManager.h"
#include "nsINodeInfo.h"
#include "nsIDOMDocumentEvent.h"
#include "nsISupportsArray.h"

class nsIEventListenerManager;
class nsDOMStyleSheetList;
class nsIOutputStream;
class nsDocument;
class nsIDTD;

#if 0
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
#endif

class nsDocHeaderData
{
public:
  nsDocHeaderData(nsIAtom* aField, const nsAReadableString& aData)
  {
    mField = aField;
    NS_IF_ADDREF(mField);
    mData.Assign(aData);
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

// Represents the children of a document (prolog, epilog and
// document element)
class nsDocumentChildNodes : public nsGenericDOMNodeList
{
public:
  nsDocumentChildNodes(nsIDocument* aDocument);
  ~nsDocumentChildNodes();

  NS_IMETHOD    GetLength(PRUint32* aLength);
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);

  void DropReference();

protected:
  nsIDocument* mDocument;
};

class nsAnonymousContentList : public nsGenericDOMNodeList 
{
public:
  nsAnonymousContentList(nsISupportsArray* aElements);
  virtual ~nsAnonymousContentList();

  // nsIDOMNodeList interface
  NS_DECL_IDOMNODELIST
  
private:
  nsISupportsArray* mElements;
};

// Base class for our document implementations
class nsDocument : public nsIDocument, 
                   public nsIDOMDocument, 
                   public nsIDOMNSDocument,
                   public nsIDOMDocumentEvent,
                   public nsIDOMDocumentStyle,
                   public nsIDOMDocumentView,
                   public nsIDOMDocumentXBL,
                   public nsIDiskDocument,
                   public nsIJSScriptObject,
                   public nsSupportsWeakReference,
                   public nsIDOMEventReceiver,
                   public nsIScriptObjectPrincipal
{
public:
  NS_DECL_ISUPPORTS

  virtual nsIArena* GetArena();

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               PRBool aReset = PR_TRUE);

  NS_IMETHOD StopDocumentLoad();

  /**
   * Return the title of the document. May return null.
   */
  virtual const nsString* GetDocumentTitle() const;

  /**
   * Return the URL for the document. May return null.
   */
  virtual nsIURI* GetDocumentURL() const;

  /**
   * Return the principal responsible for this document.
   */
  NS_IMETHOD GetPrincipal(nsIPrincipal **aPrincipal);

  /**
   * Update principal responsible for this document to the intersection
   * of its previous value and aPrincipal, and return its new value.
   */
  NS_IMETHOD AddPrincipal(nsIPrincipal *aPrincipal);

  /**
   * Return the content (mime) type of this document.
   */
  NS_IMETHOD GetContentType(nsAWritableString& aContentType) const;

  /**
   * Return the LoadGroup for the document. May return null.
   */
  NS_IMETHOD GetDocumentLoadGroup(nsILoadGroup **aGroup) const;

  /**
   * Return the base URL for realtive URLs in the document. May return null (or the document URL).
   */
  NS_IMETHOD GetBaseURL(nsIURI*& aURL) const;

  /**
   * Return a standard name for the document's character set. This will
   * trigger a startDocumentLoad if necessary to answer the question.
   */
  NS_IMETHOD GetDocumentCharacterSet(nsAWritableString& oCharsetID);
  NS_IMETHOD SetDocumentCharacterSet(const nsAReadableString& aCharSetID);

  /**
   * Add an observer that gets notified whenever the charset changes.
   */
  NS_IMETHOD AddCharSetObserver(nsIObserver* aObserver);

  /**
   * Remove a charset observer.
   */
  NS_IMETHOD RemoveCharSetObserver(nsIObserver* aObserver);

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
  NS_IMETHOD GetHeaderData(nsIAtom* aHeaderField, nsAWritableString& aData) const;
  NS_IMETHOD SetHeaderData(nsIAtom* aheaderField, const nsAReadableString& aData);

  /**
   * Create a new presentation shell that will use aContext for
   * it's presentation context (presentation context's <b>must not</b> be
   * shared among multiple presentation shell's).
   */
#if 0
  // XXX Temp hack: moved to nsMarkupDocument
  NS_IMETHOD CreateShell(nsIPresContext* aContext,
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
   * Methods to append to the prolog and epilog of
   * a document. The prolog is the content before the document
   * element, the epilog after.
   */
  NS_IMETHOD AppendToProlog(nsIContent* aContent);
  NS_IMETHOD AppendToEpilog(nsIContent* aContent);

  /** 
   * Get the direct children of the document - content in
   * the prolog, the root content and content in the epilog.
   */
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const;
  NS_IMETHOD GetChildCount(PRInt32& aCount);

  /**
   * Get the style sheets owned by this document.
   * These are ordered, highest priority last
   */
  virtual PRInt32 GetNumberOfStyleSheets();
  virtual nsIStyleSheet* GetStyleSheetAt(PRInt32 aIndex);
  virtual PRInt32 GetIndexOfStyleSheet(nsIStyleSheet* aSheet);
  virtual void AddStyleSheet(nsIStyleSheet* aSheet);
  virtual void RemoveStyleSheet(nsIStyleSheet* aSheet);
  
  NS_IMETHOD UpdateStyleSheets(nsISupportsArray* aOldSheets, nsISupportsArray* aNewSheets);
  virtual void AddStyleSheetToStyleSets(nsIStyleSheet* aSheet);
  virtual void RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet);

  NS_IMETHOD InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex, PRBool aNotify);
  virtual void SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                          PRBool mDisabled);

  /**
   * Set the object from which a document can get a script context.
   * This is the context within which all scripts (during document 
   * creation and during event handling) will run.
   */
  NS_IMETHOD GetScriptGlobalObject(nsIScriptGlobalObject** aGlobalObject);
  NS_IMETHOD SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject);

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
  NS_IMETHOD BeginUpdate();
  NS_IMETHOD EndUpdate();
  NS_IMETHOD BeginLoad();
  NS_IMETHOD EndLoad();
  NS_IMETHOD ContentChanged(nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentStatesChanged(nsIContent* aContent1, nsIContent* aContent2);
  NS_IMETHOD AttributeChanged(nsIContent* aChild,
                              PRInt32 aNameSpaceID,
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
    * Finds text in content
   */
  NS_IMETHOD FindNext(const nsAReadableString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

  /**
    * Converts the document or a selection of the 
    * document to XIF (XML Interchange Format)
    * and places the result in aBuffer.
    */
  NS_IMETHOD   CreateXIF(nsAWritableString & aBuffer, nsIDOMSelection* aSelection);
  NS_IMETHOD   ToXIF(nsIXIFConverter * aConverter, nsIDOMNode* aNode);
  virtual void BeginConvertToXIF(nsIXIFConverter * aConverter, nsIDOMNode* aNode);
  virtual void ConvertChildrenToXIF(nsIXIFConverter * aConverter, nsIDOMNode* aNode);
  virtual void FinishConvertToXIF(nsIXIFConverter * aConverter, nsIDOMNode* aNode);

  NS_IMETHOD FlushPendingNotifications();
  NS_IMETHOD GetAndIncrementContentID(PRInt32* aID);
  NS_IMETHOD GetBindingManager(nsIBindingManager** aResult);
  NS_IMETHOD GetNodeInfoManager(nsINodeInfoManager*& aNodeInfoManager);

public:
  
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIDOMDocument interface
  NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDoctype);
  NS_IMETHOD    GetImplementation(nsIDOMDOMImplementation** aImplementation);
  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement);

  NS_IMETHOD    CreateElement(const nsAReadableString& aTagName, nsIDOMElement** aReturn);
  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn);
  NS_IMETHOD    CreateTextNode(const nsAReadableString& aData, nsIDOMText** aReturn);
  NS_IMETHOD    CreateComment(const nsAReadableString& aData, nsIDOMComment** aReturn);
  NS_IMETHOD    CreateCDATASection(const nsAReadableString& aData, nsIDOMCDATASection** aReturn);
  NS_IMETHOD    CreateProcessingInstruction(const nsAReadableString& aTarget, const nsAReadableString& aData, nsIDOMProcessingInstruction** aReturn);
  NS_IMETHOD    CreateAttribute(const nsAReadableString& aName, nsIDOMAttr** aReturn);
  NS_IMETHOD    CreateEntityReference(const nsAReadableString& aName, nsIDOMEntityReference** aReturn);
  NS_IMETHOD    GetElementsByTagName(const nsAReadableString& aTagname, nsIDOMNodeList** aReturn);
  NS_IMETHOD    GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI, const nsAReadableString& aLocalName, nsIDOMNodeList** aReturn);
  NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetList** aStyleSheets);
  NS_IMETHOD    GetCharacterSet(nsAWritableString& aCharacterSet);
  NS_IMETHOD    ImportNode(nsIDOMNode* aImportedNode,
                           PRBool aDeep,
                           nsIDOMNode** aReturn);
  NS_IMETHOD    GetLocation(jsval* aLocation);
  NS_IMETHOD    SetLocation(jsval aLocation);
  NS_IMETHOD    CreateRange(nsIDOMRange** aReturn);
  NS_IMETHOD    Load (const nsAReadableString& aUrl);
  NS_IMETHOD    GetPlugins(nsIDOMPluginArray** aPlugins);
 
  // nsIDOMNode interface
  NS_DECL_IDOMNODE

  // nsIDOMDocumentView
  NS_DECL_IDOMDOCUMENTVIEW

  // nsIDOMDocumentXBL
  NS_DECL_IDOMDOCUMENTXBL

  // nsIDOMDocumentEvent
  NS_DECL_IDOMDOCUMENTEVENT

  // nsIDOMEventReceiver interface
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

  // nsIDiskDocument inteface
  NS_IMETHOD  InitDiskDocument(nsFileSpec *aFileSpec);
  NS_IMETHOD  SaveFile(nsFileSpec*     aFileSpec,
                       PRBool          aReplaceExisting,
                       PRBool          aSaveCopy,
                       const nsString& aSaveFileType,
                       const nsString& aSaveCharset,
                       PRUint32        aFlags);

  NS_IMETHOD  GetFileSpec(nsFileSpec& aFileSpec);
  NS_IMETHOD  GetModCount(PRInt32 *outModCount);
  NS_IMETHOD  ResetModCount();
  NS_IMETHOD  IncrementModCount(PRInt32 aNumMods);

  // nsIDOMEventTarget interface
  NS_IMETHOD AddEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, 
                              PRBool aUseCapture);
  NS_IMETHOD RemoveEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, 
                                 PRBool aUseCapture);
  NS_IMETHOD DispatchEvent(nsIDOMEvent* aEvent);


  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, 
                            nsEvent* aEvent, 
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);


  virtual PRBool IsInSelection(nsIDOMSelection* aSelection, const nsIContent *aContent) const;
  virtual nsIContent* GetPrevContent(const nsIContent *aContent) const;
  virtual nsIContent* GetNextContent(const nsIContent *aContent) const;

  // nsIJSScriptObject interface
  virtual PRBool    AddProperty(JSContext *aContext, JSObject *aObj, 
                                jsval aID, jsval *aVp);
  virtual PRBool    DeleteProperty(JSContext *aContext, 
                                JSObject *aObj, jsval aID, jsval *aVp);
  virtual PRBool    GetProperty(JSContext *aContext, JSObject *aObj, 
                                jsval aID, jsval *aVp);
  virtual PRBool    SetProperty(JSContext *aContext, JSObject *aObj, 
                                jsval aID, jsval *aVp);
  virtual PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj);
  virtual PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID);
  virtual PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID);
  virtual void      Finalize(JSContext *aContext, JSObject *aObj);

  virtual nsresult Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);
  virtual nsresult SetDocumentURL(nsIURI* aURI);

protected:
  nsIContent* FindContent(const nsIContent* aStartNode,
                          const nsIContent* aTest1, 
                          const nsIContent* aTest2) const;
  NS_IMETHOD GetDTD(nsIDTD** aDTD) const;

protected:

  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet);  // subclass hooks for sheet ordering
  virtual void InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex);

  nsDocument();
  virtual ~nsDocument(); 
  nsresult Init();

  nsIArena* mArena;
  nsString* mDocumentTitle;
  nsIURI* mDocumentURL;
  nsIPrincipal* mPrincipal;
  nsWeakPtr mDocumentLoadGroup;
  nsString mCharacterSet;
  nsVoidArray mCharSetObservers;
  nsIDocument* mParentDocument;
  nsVoidArray mSubDocuments;
  nsVoidArray mPresShells;
  nsIContent* mRootContent;
  nsVoidArray mStyleSheets;
  nsVoidArray mObservers;
  void* mScriptObject;
  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobalObject;
  nsIEventListenerManager* mListenerManager;
  PRInt8 mDisplaySelection;
  PRBool mInDestructor;
  nsDOMStyleSheetList *mDOMStyleSheets;
  nsINameSpaceManager* mNameSpaceManager;
  nsDocHeaderData* mHeaderData;
  nsILineBreaker* mLineBreaker;
  nsVoidArray *mProlog;
  nsVoidArray *mEpilog;
  nsDocumentChildNodes* mChildNodes;
  nsIWordBreaker* mWordBreaker;
  // A content ID counter used to give a monotonically increasing ID to the content
  // objects in the document's content model
  PRInt32 mNextContentID;

  // disk file members
  nsFileSpec*     mFileSpec;
  PRInt32         mModCount;

  nsIDTD* mDTD;

  nsCOMPtr<nsIBindingManager> mBindingManager;
  nsCOMPtr<nsINodeInfoManager> mNodeInfoManager; // OWNER
};

#endif /* nsDocument_h___ */
