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

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCRT.h"
#include "nsIDocument.h"
#include "nsWeakReference.h"
#include "nsWeakPtr.h"
#include "nsVoidArray.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOM3Document.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentStyle.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDocumentObserver.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMEventTarget.h"
#include "nsIContent.h"
#include "nsIEventListenerManager.h"
#include "nsGenericDOMNodeList.h"
#include "nsIDOM3Node.h"
#include "nsIPrincipal.h"
#include "nsIBindingManager.h"
#include "nsINodeInfo.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOM3DocumentEvent.h"
#include "nsCOMArray.h"
#include "nsHashtable.h"
#include "nsIWordBreakerFactory.h"
#include "nsILineBreakerFactory.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsScriptLoader.h"
#include "nsICSSLoader.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIRadioGroupContainer.h"
#include "nsIScriptEventManager.h"

#include "pldhash.h"


#define XML_DECLARATION_BITS_DECLARATION_EXISTS   (1 << 0)
#define XML_DECLARATION_BITS_ENCODING_EXISTS      (1 << 1)
#define XML_DECLARATION_BITS_STANDALONE_EXISTS    (1 << 2)
#define XML_DECLARATION_BITS_STANDALONE_YES       (1 << 3)


class nsIEventListenerManager;
class nsDOMStyleSheetList;
class nsIOutputStream;
class nsDocument;
class nsIDTD;
class nsXPathDocumentTearoff;
class nsIRadioVisitor;
class nsIFormControl;
struct nsRadioGroupStruct;


class nsDocHeaderData
{
public:
  nsDocHeaderData(nsIAtom* aField, const nsAString& aData)
    : mField(aField), mData(aData), mNext(nsnull)
  {
  }

  ~nsDocHeaderData(void)
  {
    delete mNext;
  }

  nsCOMPtr<nsIAtom> mField;
  nsString          mData;
  nsDocHeaderData*  mNext;
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
  nsDocumentChildNodes(); // Not implemented

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

  NS_IMETHOD BeginUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType) {
    return NS_OK;
  }
  NS_IMETHOD EndUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType) {
    return NS_OK;
  }
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
                                  nsIContent* aContent2,
                                  PRInt32 aStateMask) { return NS_OK; }
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              PRInt32      aNameSpaceID,
                              nsIAtom*     aAttribute,
                              PRInt32      aModType) { return NS_OK; }
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
  NS_IMETHOD StyleSheetApplicableStateChanged(nsIDocument *aDocument,
                                              nsIStyleSheet* aStyleSheet,
                                              PRBool aApplicable) { return NS_OK; }
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aOldStyleRule,
                              nsIStyleRule* aNewStyleRule) { return NS_OK; }
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
                   public nsIDOM3DocumentEvent,
                   public nsIDOMDocumentStyle,
                   public nsIDOMDocumentView,
                   public nsIDOMDocumentRange,
                   public nsIDOMDocumentTraversal,
                   public nsIDOMDocumentXBL,
                   public nsIDOM3Document,
                   public nsSupportsWeakReference,
                   public nsIDOMEventReceiver,
                   public nsIDOM3EventTarget,
                   public nsIScriptObjectPrincipal,
                   public nsIRadioGroupContainer
{
public:
  NS_DECL_ISUPPORTS

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_IMETHOD Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);
  NS_IMETHOD ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup);

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
  NS_IMETHOD GetDocumentTitle(nsAString& aTitle) const;

  /**
   * Return the URL for the document. May return null.
   */
  NS_IMETHOD GetDocumentURL(nsIURI** aURI) const;

  /**
   * Return the principal responsible for this document.
   */
  NS_IMETHOD GetPrincipal(nsIPrincipal **aPrincipal);

  /**
   * Set the principal responsible for this document.
   */
  NS_IMETHOD SetPrincipal(nsIPrincipal *aPrincipal);

  /**
   * Get the Content-Type of this document.
   */
  // NS_IMETHOD GetContentType(nsAString& aContentType);
  // Already declared in nsIDOMNSDocument

  /**
   * Set the Content-Type of this document.
   */
  NS_IMETHOD SetContentType(const nsAString& aContentType);

  /**
   * Return the content language of this document.
   */
  NS_IMETHOD GetContentLanguage(nsAString& aContentLanguage) const;

  /**
   * Return the LoadGroup for the document. May return null.
   */
  NS_IMETHOD GetDocumentLoadGroup(nsILoadGroup **aGroup) const;

  /**
   * Return the base URL for relative URLs in the document. May return null (or the document URL).
   */
  NS_IMETHOD GetBaseURL(nsIURI** aURL) const;
  NS_IMETHOD SetBaseURL(nsIURI* aURL);

  /**
   * Get/Set the base target of a link in a document.
   */
  NS_IMETHOD GetBaseTarget(nsAString &aBaseTarget);
  NS_IMETHOD SetBaseTarget(const nsAString &aBaseTarget);

  /**
   * Return a standard name for the document's character set. This will
   * trigger a startDocumentLoad if necessary to answer the question.
   */
  NS_IMETHOD GetDocumentCharacterSet(nsACString& oCharsetID);
  NS_IMETHOD SetDocumentCharacterSet(const nsACString& aCharSetID);

  NS_IMETHOD GetDocumentCharacterSetSource(PRInt32* aCharsetSource);
  NS_IMETHOD SetDocumentCharacterSetSource(PRInt32 aCharsetSource);

  /**
   * Add an observer that gets notified whenever the charset changes.
   */
  NS_IMETHOD AddCharSetObserver(nsIObserver* aObserver);

  /**
   * Remove a charset observer.
   */
  NS_IMETHOD RemoveCharSetObserver(nsIObserver* aObserver);

  /**
   *  Check if the document contains bidi data.
   *  If so, we have to apply the Unicode Bidi Algorithm.
   */
  NS_IMETHOD GetBidiEnabled(PRBool* aBidiEnabled) const;
  /**
   *  Indicate the document contains RTL characters.
   */
  NS_IMETHOD SetBidiEnabled(PRBool aBidiEnabled);

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
                           nsAString& aData) const;
  NS_IMETHOD SetHeaderData(nsIAtom* aheaderField,
                           const nsAString& aData);

  /**
   * Create a new presentation shell that will use aContext for
   * it's presentation context (presentation context's <b>must not</b> be
   * shared among multiple presentation shell's).
   */
  NS_IMETHOD CreateShell(nsIPresContext* aContext,
                         nsIViewManager* aViewManager,
                         nsIStyleSet* aStyleSet,
                         nsIPresShell** aInstancePtrResult);
  NS_IMETHOD_(PRBool) DeleteShell(nsIPresShell* aShell);
  NS_IMETHOD_(PRUint32) GetNumberOfShells() const;
  NS_IMETHOD_(nsIPresShell *) GetShellAt(PRUint32 aIndex) const;

  /**
   * Return the parent document of this document. Will return null
   * unless this document is within a compound document and has a parent.
   */
  NS_IMETHOD GetParentDocument(nsIDocument** aParent);
  NS_IMETHOD SetParentDocument(nsIDocument* aParent);

  NS_IMETHOD SetSubDocumentFor(nsIContent *aContent, nsIDocument* aSubDoc);
  NS_IMETHOD GetSubDocumentFor(nsIContent *aContent, nsIDocument** aSubDoc);
  NS_IMETHOD FindContentForSubDocument(nsIDocument *aDocument,
                                       nsIContent **aContent);

  /**
   * Return the root content object for this document.
   */
  NS_IMETHOD GetRootContent(nsIContent** aRoot);
  NS_IMETHOD SetRootContent(nsIContent* aRoot);

  /**
   * Get the direct children of the document - content in
   * the prolog, the root content and content in the epilog.
   */
  NS_IMETHOD_(nsIContent *) GetChildAt(PRUint32 aIndex) const;
  NS_IMETHOD_(PRInt32) IndexOf(nsIContent* aPossibleChild) const;
  NS_IMETHOD_(PRUint32) GetChildCount() const;

  /**
   * Get the style sheets owned by this document.
   * These are ordered, highest priority last
   */
  NS_IMETHOD GetNumberOfStyleSheets(PRBool aIncludeSpecialSheets,
                                    PRInt32* aCount);
  NS_IMETHOD GetStyleSheetAt(PRInt32 aIndex, PRBool aIncludeSpecialSheets,
                             nsIStyleSheet** aSheet);
  NS_IMETHOD GetIndexOfStyleSheet(nsIStyleSheet* aSheet, PRInt32* aIndex);
  virtual void AddStyleSheet(nsIStyleSheet* aSheet, PRUint32 aFlags);
  virtual void RemoveStyleSheet(nsIStyleSheet* aSheet);

  NS_IMETHOD UpdateStyleSheets(nsCOMArray<nsIStyleSheet>& aOldSheets,
                               nsCOMArray<nsIStyleSheet>& aNewSheets);
  virtual void AddStyleSheetToStyleSets(nsIStyleSheet* aSheet);
  virtual void RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet);

  NS_IMETHOD InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex);
  virtual void SetStyleSheetApplicableState(nsIStyleSheet* aSheet,
                                            PRBool aApplicable);

  /**
   * Set the object from which a document can get a script context.
   * This is the context within which all scripts (during document
   * creation and during event handling) will run.
   */
  NS_IMETHOD GetScriptGlobalObject(nsIScriptGlobalObject** aGlobalObject);
  NS_IMETHOD SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject);

  /**
   * Get the script loader for this document
   */
  NS_IMETHOD GetScriptLoader(nsIScriptLoader** aScriptLoader);

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

  // Observation hooks used to propagate notifications to document
  // observers.
  NS_IMETHOD BeginUpdate(nsUpdateType aUpdateType);
  NS_IMETHOD EndUpdate(nsUpdateType aUpdateType);
  NS_IMETHOD BeginLoad();
  NS_IMETHOD EndLoad();
  NS_IMETHOD ContentChanged(nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentStatesChanged(nsIContent* aContent1,
                                  nsIContent* aContent2,
                                  PRInt32 aStateMask);

  NS_IMETHOD AttributeWillChange(nsIContent* aChild,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aAttribute);
  NS_IMETHOD AttributeChanged(nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);
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
                              nsIStyleRule* aOldStyleRule,
                              nsIStyleRule* aNewStyleRule);
  NS_IMETHOD StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule);
  NS_IMETHOD StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);

  NS_IMETHOD FlushPendingNotifications(PRBool aFlushReflows = PR_TRUE,
                                       PRBool aUpdateViews = PR_FALSE);
  NS_IMETHOD GetAndIncrementContentID(PRInt32* aID);
  NS_IMETHOD GetBindingManager(nsIBindingManager** aResult);
  NS_IMETHOD GetNodeInfoManager(nsINodeInfoManager** aNodeInfoManager);
  NS_IMETHOD AddReference(void *aKey, nsISupports *aReference);
  NS_IMETHOD RemoveReference(void *aKey, nsISupports **aOldReference);
  NS_IMETHOD SetContainer(nsISupports *aContainer);
  NS_IMETHOD GetContainer(nsISupports **aContainer);
  NS_IMETHOD GetScriptEventManager(nsIScriptEventManager **aResult);
  NS_IMETHOD SetXMLDeclaration(const nsAString& aVersion,
                               const nsAString& aEncoding,
                               const nsAString& Standalone);
  NS_IMETHOD GetXMLDeclaration(nsAString& aVersion,
                               nsAString& aEncoding,
                               nsAString& Standalone);
  NS_IMETHOD_(PRBool) IsCaseSensitive();
  NS_IMETHOD_(PRBool) IsScriptEnabled();

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName,
                            nsIRadioVisitor* aVisitor);
  NS_IMETHOD SetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement* aRadio);
  NS_IMETHOD GetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement** aRadio);
  NS_IMETHOD AddToRadioGroup(const nsAString& aName,
                             nsIFormControl* aRadio);
  NS_IMETHOD RemoveFromRadioGroup(const nsAString& aName,
                                  nsIFormControl* aRadio);

  // for radio group
  nsresult GetRadioGroup(const nsAString& aName,
                         nsRadioGroupStruct **aRadioGroup);

  // nsIDOMNode
  NS_DECL_NSIDOMNODE

  // nsIDOM3Node
  NS_DECL_NSIDOM3NODE

  // nsIDOMDocument
  NS_DECL_NSIDOMDOCUMENT

  // nsIDOM3Document
  NS_DECL_NSIDOM3DOCUMENT

  // nsIDOMXMLDocument
  NS_DECL_NSIDOMXMLDOCUMENT

  // nsIDOMNSDocument
  NS_DECL_NSIDOMNSDOCUMENT

  // nsIDOMDocumentEvent
  NS_DECL_NSIDOMDOCUMENTEVENT

  // nsIDOM3DocumentEvent
  NS_DECL_NSIDOM3DOCUMENTEVENT

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
  NS_IMETHOD GetSystemEventGroup(nsIDOMEventGroup** aGroup);

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOM3EventTarget
  NS_DECL_NSIDOM3EVENTTARGET

  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);

  NS_IMETHOD SetDocumentURL(nsIURI* aURI);

  virtual nsresult Init();

protected:
  // subclass hooks for sheet ordering
  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet,
                                     PRUint32 aFlags);
  virtual void InternalInsertStyleSheetAt(nsIStyleSheet* aSheet,
                                          PRInt32 aIndex);
  virtual already_AddRefed<nsIStyleSheet> InternalGetStyleSheetAt(PRInt32 aIndex);
  virtual PRInt32 InternalGetNumberOfStyleSheets();

  virtual void RetrieveRelevantHeaders(nsIChannel *aChannel);

  nsresult doCreateShell(nsIPresContext* aContext,
                         nsIViewManager* aViewManager, nsIStyleSet* aStyleSet,
                         nsCompatibility aCompatMode,
                         nsIPresShell** aInstancePtrResult);

  nsDocument();
  virtual ~nsDocument();

  nsString mDocumentTitle;
  nsCString mLastModified;
  nsCOMPtr<nsIURI> mDocumentURL;
  nsCOMPtr<nsIURI> mDocumentBaseURL;
  nsCOMPtr<nsIPrincipal> mPrincipal;

  nsWeakPtr mDocumentLoadGroup;

  nsWeakPtr mDocumentContainer;

  nsCString mCharacterSet;
  PRInt32 mCharacterSetSource;

  nsVoidArray mCharSetObservers;
  nsIDocument* mParentDocument;

  PLDHashTable *mSubDocuments;

  nsSmallVoidArray mPresShells;

  // Array of owning references to all children
  nsCOMArray<nsIContent> mChildren;

  // A weak reference to the only element in mChildren, or null if no
  // such element exists.
  nsIContent* mRootContent;

  nsCOMArray<nsIStyleSheet> mStyleSheets;

  // Basically always has at least 1 entry
  nsAutoVoidArray mObservers;
  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobalObject;
  nsCOMPtr<nsIEventListenerManager> mListenerManager;
  nsCOMPtr<nsIDOMStyleSheetList> mDOMStyleSheets;
  nsCOMPtr<nsIScriptLoader> mScriptLoader;
  nsDocHeaderData* mHeaderData;
  nsCOMPtr<nsILineBreaker> mLineBreaker;
  nsCOMPtr<nsIWordBreaker> mWordBreaker;

  nsRefPtr<nsDocumentChildNodes> mChildNodes;

  // A content ID counter used to give a monotonically increasing ID
  // to the content objects in the document's content model
  PRInt32 mNextContentID;

  nsHashtable mRadioGroups;

  nsCOMPtr<nsIBindingManager> mBindingManager;
  nsCOMPtr<nsINodeInfoManager> mNodeInfoManager;

  // True if the document is being destroyed.
  PRPackedBool mIsGoingAway;

  // True if BIDI is enabled.
  PRPackedBool mBidiEnabled;

  // True if the document is being destroyed.
  PRPackedBool mInDestructor;

  PRUint8 mXMLDeclarationBits;

  nsSupportsHashtable* mBoxObjectTable;

  nsSupportsHashtable mContentWrapperHash;

  nsCOMPtr<nsICSSLoader> mCSSLoader;

  nsXPIDLCString mContentLanguage;
  nsCString mContentType;

  nsCOMPtr<nsIScriptEventManager> mScriptEventManager;

private:
  nsresult IsAllowedAsChild(PRUint16 aNodeType, nsIContent* aRefContent);

  // These are not implemented and not supported.
  nsDocument(const nsDocument& aOther);
  nsDocument& operator=(const nsDocument& aOther);

  nsXPathDocumentTearoff* mXPathDocument;
};



#endif /* nsDocument_h___ */
