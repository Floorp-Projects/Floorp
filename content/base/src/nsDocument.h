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
#ifndef nsDocument_h___
#define nsDocument_h___

#include "nsIDocument.h"
#include "nsWeakReference.h"
#include "nsWeakPtr.h"
#include "nsVoidArray.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentStyle.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDocumentObserver.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDiskDocument.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMEventTarget.h"
#include "nsIContent.h"
#include "nsGenericDOMNodeList.h"
#include "nsIPrincipal.h"
#include "nsIBindingManager.h"
#include "nsINodeInfo.h"
#include "nsIDOMDocumentEvent.h"
#include "nsISupportsArray.h"
#include "nsHashtable.h"
#include "nsIWordBreakerFactory.h"
#include "nsILineBreakerFactory.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsScriptLoader.h"
#include "nsICSSLoader.h"

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
  nsString         mData;
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


class nsDOMStyleSheetList : public nsIDOMStyleSheetList,
                            public nsIDocumentObserver
{
public:
  nsDOMStyleSheetList(nsIDocument *aDocument);
  virtual ~nsDOMStyleSheetList();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMSTYLESHEETLIST
  
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
                             PRInt32     aNewIndexInContainer) 
                             { return NS_OK; }
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet);
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
  PRInt32       mLength;
  nsIDocument*  mDocument;
  void*         mScriptObject;
};

// Base class for our document implementations.
//
// Note that this class *implements* nsIDOMXMLDocument, but it's not
// really an nsIDOMXMLDocument. The reason for implementing
// nsIDOMXMLDocument on this class is to avoid having to duplicate all
// its inherited methods on document classes that *are*
// nsIDOMXMLDocument's. nsDocument's QI should *not* claim to support
// nsIDOMXMLDocument unless someone writes a real implementation of
// the interface.
class nsDocument : public nsIDocument, 
                   public nsIDOMXMLDocument, // inherits nsIDOMDocument 
                   public nsIDOMNSDocument,
                   public nsIDOMDocumentEvent,
                   public nsIDOMDocumentStyle,
                   public nsIDOMDocumentView,
                   public nsIDOMDocumentRange,
                   public nsIDOMDocumentTraversal,
                   public nsIDOMDocumentXBL,
                   public nsIDOM3Node,
                   public nsIDiskDocument,
                   public nsSupportsWeakReference,
                   public nsIDOMEventReceiver,
                   public nsIScriptObjectPrincipal
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetArena(nsIArena** aArena);

  NS_IMETHOD Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               PRBool aReset = PR_TRUE,
                               nsIContentSink* aContentSink = nsnull);

  NS_IMETHOD StopDocumentLoad();

  /**
   * Return the title of the document. May return null.
   */
  virtual const nsString* GetDocumentTitle() const;

  /**
   * Return the URL for the document. May return null.
   */
  NS_IMETHOD GetDocumentURL(nsIURI** aURI) const;

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
   * Return the content language of this document.
   */
  NS_IMETHOD GetContentLanguage(nsAWritableString& aContentLanguage) const;

  /**
   * Return the LoadGroup for the document. May return null.
   */
  NS_IMETHOD GetDocumentLoadGroup(nsILoadGroup **aGroup) const;

  /**
   * Return the base URL for realtive URLs in the document. May return null (or the document URL).
   */
  NS_IMETHOD GetBaseURL(nsIURI*& aURL) const;
  NS_IMETHOD SetBaseURL(nsIURI* aURL);

  /**
   * Get/Set the base target of a link in a document.
   */
  NS_IMETHOD GetBaseTarget(nsAWritableString &aBaseTarget);
  NS_IMETHOD SetBaseTarget(const nsAReadableString &aBaseTarget);

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

#ifdef IBMBIDI
  /**
   *  Check if the document contains bidi data.
   *  If so, we have to apply the Unicode Bidi Algorithm.
   */
  NS_IMETHOD GetBidiEnabled(PRBool* aBidiEnabled) const;
  /**
   *  Indicate the document contains RTL characters.
   */
  NS_IMETHOD SetBidiEnabled(PRBool aBidiEnabled);
#endif // IBMBIDI

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
  NS_IMETHOD GetHeaderData(nsIAtom* aHeaderField,
                           nsAWritableString& aData) const;
  NS_IMETHOD SetHeaderData(nsIAtom* aheaderField,
                           const nsAReadableString& aData);

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
  NS_IMETHOD GetShellAt(PRInt32 aIndex, nsIPresShell** aShell);

  /**
   * Return the parent document of this document. Will return null
   * unless this document is within a compound document and has a parent.
   */
  NS_IMETHOD GetParentDocument(nsIDocument** aParent);
  NS_IMETHOD SetParentDocument(nsIDocument* aParent);
  NS_IMETHOD AddSubDocument(nsIDocument* aSubDoc);
  NS_IMETHOD GetNumberOfSubDocuments(PRInt32* aCount);
  NS_IMETHOD GetSubDocumentAt(PRInt32 aIndex, nsIDocument** aSubDoc);

  /**
   * Return the root content object for this document.
   */
  NS_IMETHOD GetRootContent(nsIContent** aRoot);
  NS_IMETHOD SetRootContent(nsIContent* aRoot);

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
  NS_IMETHOD GetNumberOfStyleSheets(PRInt32* aCount);
  NS_IMETHOD GetStyleSheetAt(PRInt32 aIndex, nsIStyleSheet** aSheet);
  NS_IMETHOD GetIndexOfStyleSheet(nsIStyleSheet* aSheet, PRInt32* aIndex);
  virtual void AddStyleSheet(nsIStyleSheet* aSheet);
  virtual void RemoveStyleSheet(nsIStyleSheet* aSheet);
  
  NS_IMETHOD UpdateStyleSheets(nsISupportsArray* aOldSheets,
                               nsISupportsArray* aNewSheets);
  virtual void AddStyleSheetToStyleSets(nsIStyleSheet* aSheet);
  virtual void RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet);

  NS_IMETHOD InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex,
                                PRBool aNotify);
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
   * Get the script loader for this document
   */ 
  NS_IMETHOD GetScriptLoader(nsIScriptLoader** aScriptLoader);

  /**
   * Get the focus controller for this document
   * This can usually be gotten through the ScriptGlobalObject, but
   * it is set to null during document destruction, when we still might
   * need to fire focus events.
   */
  NS_IMETHOD GetFocusController(nsIFocusController** aFocusController);

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
  NS_IMETHOD ContentStatesChanged(nsIContent* aContent1,
                                  nsIContent* aContent2);

  NS_IMETHOD AttributeWillChange(nsIContent* aChild,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aAttribute);
  NS_IMETHOD AttributeChanged(nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType,
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

  NS_IMETHOD FlushPendingNotifications(PRBool aFlushReflows = PR_TRUE,
                                       PRBool aUpdateViews = PR_FALSE);
  NS_IMETHOD GetAndIncrementContentID(PRInt32* aID);
  NS_IMETHOD GetBindingManager(nsIBindingManager** aResult);
  NS_IMETHOD GetNodeInfoManager(nsINodeInfoManager*& aNodeInfoManager);
  NS_IMETHOD AddReference(void *aKey, nsISupports *aReference);
  NS_IMETHOD RemoveReference(void *aKey, nsISupports **aOldReference);

  // nsIDOMNode
  NS_DECL_NSIDOMNODE

  // nsIDOM3Node
  NS_DECL_NSIDOM3NODE

  // nsIDOMDocument
  NS_DECL_NSIDOMDOCUMENT

  // nsIDOMXMLDocument
  NS_DECL_NSIDOMXMLDOCUMENT

  // nsIDOMNSDocument
  NS_DECL_NSIDOMNSDOCUMENT

  // nsIDOMDocumentEvent
  NS_DECL_NSIDOMDOCUMENTEVENT

  // nsIDOMDocumentStyle
  NS_DECL_NSIDOMDOCUMENTSTYLE

  // nsIDOMDocumentView
  NS_DECL_NSIDOMDOCUMENTVIEW

  // nsIDOMDocumentRange
  NS_DECL_NSIDOMDOCUMENTRANGE

  // nsIDOMDocumentTraversal
  NS_DECL_NSIDOMDOCUMENTTRAVERSAL

  // nsIDOMDocumentXBL
  NS_DECL_NSIDOMDOCUMENTXBL

  // nsIDOMEventReceiver interface
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

  // nsIDiskDocument inteface
  NS_DECL_NSIDISKDOCUMENT

  // nsIDOMEventTarget interface
  NS_IMETHOD AddEventListener(const nsAReadableString& aType,
                              nsIDOMEventListener* aListener, 
                              PRBool aUseCapture);
  NS_IMETHOD RemoveEventListener(const nsAReadableString& aType,
                                 nsIDOMEventListener* aListener, 
                                 PRBool aUseCapture);
  NS_IMETHOD DispatchEvent(nsIDOMEvent* aEvent, PRBool *_retval);


  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, 
                            nsEvent* aEvent, 
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);

  NS_IMETHOD_(PRBool) EventCaptureRegistration(PRInt32 aCapturerIncrement);


  NS_IMETHOD SetDocumentURL(nsIURI* aURI);

protected:
  NS_IMETHOD GetDTD(nsIDTD** aDTD) const;

  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet);  // subclass hooks for sheet ordering
  virtual void InternalInsertStyleSheetAt(nsIStyleSheet* aSheet,
                                          PRInt32 aIndex);

  nsDocument();
  virtual ~nsDocument(); 
  nsresult Init();

  nsIArena* mArena;
  nsString mDocumentTitle;
  nsIURI* mDocumentURL;
  nsCOMPtr<nsIURI> mDocumentBaseURL;
  nsIPrincipal* mPrincipal;
  nsWeakPtr mDocumentLoadGroup;
  nsString mCharacterSet;
  nsVoidArray mCharSetObservers;
  nsIDocument* mParentDocument;
  nsVoidArray mSubDocuments;
  nsVoidArray mPresShells;
  nsCOMPtr<nsISupportsArray> mChildren; // contains owning references
  nsIContent* mRootContent; // a weak reference to the only element in
                            // mChildren, or null if no such element exists.
  nsAutoVoidArray mStyleSheets;
  nsAutoVoidArray mObservers; // basically always has at least 1 entry
  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobalObject;
  nsIEventListenerManager* mListenerManager;
  PRBool mInDestructor;
  nsCOMPtr<nsIDOMStyleSheetList> mDOMStyleSheets;
  nsINameSpaceManager* mNameSpaceManager;
  nsCOMPtr<nsIScriptLoader> mScriptLoader;
  nsDocHeaderData* mHeaderData;
  nsCOMPtr<nsILineBreaker> mLineBreaker;
  nsCOMPtr<nsIWordBreaker> mWordBreaker;
  nsDocumentChildNodes* mChildNodes;
  // A content ID counter used to give a monotonically increasing ID to the content
  // objects in the document's content model
  PRInt32 mNextContentID;

#ifdef IBMBIDI
  PRBool        mBidiEnabled;
#endif // IBMBIDI

  // disk file members
  nsCOMPtr<nsIURI>    mDocumentURI;
  PRInt32             mModCount;

  nsIDTD* mDTD;

  nsCOMPtr<nsIBindingManager> mBindingManager;
  nsCOMPtr<nsINodeInfoManager> mNodeInfoManager; // OWNER

  PRBool mIsGoingAway; // True if the document is being destroyed.

  nsSupportsHashtable* mBoxObjectTable;
  PRInt32 mNumCapturers; //Number of capturing event handlers in doc.  Used to optimize event delivery.

  nsSupportsHashtable mContentWrapperHash;

  nsCOMPtr<nsICSSLoader> mCSSLoader;
  nsWeakPtr mFocusController;

  nsString mContentLanguage;
  nsString mContentType;

private:
  // These are not implemented and not supported.
  nsDocument(const nsDocument& aOther);
  nsDocument& operator=(const nsDocument& aOther);
};



#endif /* nsDocument_h___ */
