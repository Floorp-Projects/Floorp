/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIDocument_h___
#define nsIDocument_h___

#include "nsISupports.h"
#include "nsEvent.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsIDocumentObserver.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIBindingManager.h"
#include "nsWeakPtr.h"
#include "nsIWeakReferenceUtils.h"
#include "nsILoadGroup.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "mozFlushType.h"
#include "nsPropertyTable.h"
#include "nsHashSets.h"
#include "nsAutoPtr.h"

class nsIAtom;
class nsIContent;
class nsPresContext;
class nsIPresShell;

class nsIStreamListener;
class nsIStreamObserver;
class nsStyleSet;
class nsIStyleSheet;
class nsIStyleRule;
class nsIViewManager;
class nsIScriptGlobalObject;
class nsIDOMEvent;
class nsIDeviceContext;
class nsIParser;
class nsIDOMNode;
class nsIDOMDocumentFragment;
class nsILineBreaker;
class nsIWordBreaker;
class nsISelection;
class nsIChannel;
class nsIPrincipal;
class nsIDOMDocument;
class nsIDOMDocumentType;
class nsIObserver;
class nsISupportsArray;
class nsIScriptLoader;
class nsIContentSink;
class nsIScriptEventManager;
class nsNodeInfoManager;
class nsICSSLoader;
class nsHTMLStyleSheet;
class nsIHTMLCSSStyleSheet;
class nsILayoutHistoryState;

// IID for the nsIDocument interface
#define NS_IDOCUMENT_IID      \
{ 0xd7c47f55, 0x480b, 0x4a60, \
  { 0x9a, 0xdf, 0xca, 0x49, 0x87, 0x3c, 0x71, 0xe2 } }

// The base value for the content ID counter.
// This counter is used by the document to 
// assign a monotonically increasing ID to each content
// object it creates
#define NS_CONTENT_ID_COUNTER_BASE 10000


// Flag for AddStyleSheet().
#define NS_STYLESHEET_FROM_CATALOG                (1 << 0)

#define NS_LINK_VISITED_EVENT_TOPIC "link-visited"

//----------------------------------------------------------------------

// Document interface
class nsIDocument : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_IID)
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsIDocument()
    : mCharacterSet(NS_LITERAL_CSTRING("ISO-8859-1")),
      mNextContentID(NS_CONTENT_ID_COUNTER_BASE),
      mNodeInfoManager(nsnull),
      mPartID(0)
  {
  }

  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     PRBool aReset,
                                     nsIContentSink* aSink = nsnull) = 0;

  virtual void StopDocumentLoad() = 0;

  /**
   * Return the title of the document. May return null.
   */
  const nsAFlatString& GetDocumentTitle() const
  {
    return mDocumentTitle;
  }

  /**
   * Return the URI for the document. May return null.
   */
  nsIURI* GetDocumentURI() const
  {
    return mDocumentURI;
  }
  void SetDocumentURI(nsIURI* aURI)
  {
    mDocumentURI = aURI;
  }

  /**
   * Return the principal responsible for this document.
   */
  virtual nsIPrincipal* GetPrincipal() = 0;

  /**
   * Set the principal responsible for this document.
   */
  virtual void SetPrincipal(nsIPrincipal *aPrincipal) = 0;

  /**
   * Return the LoadGroup for the document. May return null.
   */
  already_AddRefed<nsILoadGroup> GetDocumentLoadGroup() const
  {
    nsILoadGroup *group = nsnull;
    if (mDocumentLoadGroup)
      CallQueryReferent(mDocumentLoadGroup.get(), &group);

    return group;
  }

  /**
   * Return the base URI for relative URIs in the document (the document uri
   * unless it's overridden by SetBaseURI, HTML <base> tags, etc.).  The
   * returned URI could be null if there is no document URI.
   */
  nsIURI* GetBaseURI() const
  {
    return mDocumentBaseURI ? mDocumentBaseURI : mDocumentURI;
  }
  virtual nsresult SetBaseURI(nsIURI* aURI) = 0;

  /**
   * Get/Set the base target of a link in a document.
   */
  virtual void GetBaseTarget(nsAString &aBaseTarget) const = 0;
  virtual void SetBaseTarget(const nsAString &aBaseTarget) = 0;

  /**
   * Return a standard name for the document's character set. This
   * will trigger a startDocumentLoad if necessary to answer the
   * question.
   */
  const nsAFlatCString& GetDocumentCharacterSet() const
  {
    return mCharacterSet;
  }

  /**
   * Set the document's character encoding. |aCharSetID| should be canonical. 
   * That is, callers are responsible for the charset alias resolution. 
   */
  virtual void SetDocumentCharacterSet(const nsACString& aCharSetID) = 0;

  PRInt32 GetDocumentCharacterSetSource() const
  {
    return mCharacterSetSource;
  }

  void SetDocumentCharacterSetSource(PRInt32 aCharsetSource)
  {
    mCharacterSetSource = aCharsetSource;
  }

  /**
   * Add an observer that gets notified whenever the charset changes.
   */
  virtual nsresult AddCharSetObserver(nsIObserver* aObserver) = 0;

  /**
   * Remove a charset observer.
   */
  virtual void RemoveCharSetObserver(nsIObserver* aObserver) = 0;

  /**
   * Get the Content-Type of this document.
   * (This will always return NS_OK, but has this signature to be compatible
   *  with nsIDOMNSDocument::GetContentType())
   */
  NS_IMETHOD GetContentType(nsAString& aContentType) = 0;

  /**
   * Set the Content-Type of this document.
   */
  virtual void SetContentType(const nsAString& aContentType) = 0;

  /**
   * Return the language of this document.
   */
  void GetContentLanguage(nsAString& aContentLanguage) const
  {
    CopyASCIItoUCS2(mContentLanguage, aContentLanguage);
  }

  // The state BidiEnabled should persist across multiple views
  // (screen, print) of the same document.

  /**
   * Check if the document contains bidi data.
   * If so, we have to apply the Unicode Bidi Algorithm.
   */
  PRBool GetBidiEnabled() const
  {
    return mBidiEnabled;
  }

  /**
   * Indicate the document contains bidi data.
   * Currently, we cannot disable bidi, because once bidi is enabled,
   * it affects a frame model irreversibly, and plays even though
   * the document no longer contains bidi data.
   */
  void SetBidiEnabled(PRBool aBidiEnabled)
  {
    mBidiEnabled = aBidiEnabled;
  }

  /**
   * Return the Line Breaker for the document
   */
  virtual nsILineBreaker* GetLineBreaker() = 0;
  virtual void SetLineBreaker(nsILineBreaker* aLineBreaker) = 0;
  virtual nsIWordBreaker* GetWordBreaker() = 0;
  virtual void SetWordBreaker(nsIWordBreaker* aWordBreaker) = 0;

  /**
   * Access HTTP header data (this may also get set from other
   * sources, like HTML META tags).
   */
  virtual void GetHeaderData(nsIAtom* aHeaderField, nsAString& aData) const = 0;
  virtual void SetHeaderData(nsIAtom* aheaderField, const nsAString& aData) = 0;

  /**
   * Create a new presentation shell that will use aContext for its
   * presentation context (presentation contexts <b>must not</b> be
   * shared among multiple presentation shells).
   */
  virtual nsresult CreateShell(nsPresContext* aContext,
                               nsIViewManager* aViewManager,
                               nsStyleSet* aStyleSet,
                               nsIPresShell** aInstancePtrResult) = 0;
  virtual PRBool DeleteShell(nsIPresShell* aShell) = 0;
  virtual PRUint32 GetNumberOfShells() const = 0;
  virtual nsIPresShell *GetShellAt(PRUint32 aIndex) const = 0;

  /**
   * Return the parent document of this document. Will return null
   * unless this document is within a compound document and has a
   * parent. Note that this parent chain may cross chrome boundaries.
   */
  nsIDocument *GetParentDocument() const
  {
    return mParentDocument;
  }

  /**
   * Set the parent document of this document.
   */
  void SetParentDocument(nsIDocument* aParent)
  {
    mParentDocument = aParent;
  }

  /**
   * Set the sub document for aContent to aSubDoc.
   */
  virtual nsresult SetSubDocumentFor(nsIContent *aContent,
                                     nsIDocument* aSubDoc) = 0;

  /**
   * Get the sub document for aContent
   */
  virtual nsIDocument *GetSubDocumentFor(nsIContent *aContent) const = 0;

  /**
   * Find the content node for which aDocument is a sub document.
   */
  virtual nsIContent *FindContentForSubDocument(nsIDocument *aDocument) const = 0;

  /**
   * Return the root content object for this document.
   */
  nsIContent *GetRootContent() const
  {
    return mRootContent;
  }

  /**
   * Set aRoot as the root content object for this document.  If aRoot is
   * non-null, this should not be called on documents that currently have a
   * root content without first clearing out the document's children.  Passing
   * in null to unbind the existing root content is allowed.  This method will
   * bind aRoot to the document; the caller need not call BindToTree on aRoot.
   *
   * Note that this method never sends out nsIDocumentObserver notifications;
   * doing that is the caller's responsibility.
   */
  virtual nsresult SetRootContent(nsIContent* aRoot) = 0;

  /** 
   * Get the direct children of the document - content in
   * the prolog, the root content and content in the epilog.
   */
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const = 0;
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const = 0;
  virtual PRUint32 GetChildCount() const = 0;

  /**
   * Accessors to the collection of stylesheets owned by this document.
   * Style sheets are ordered, most significant last.
   */

  /**
   * Get the number of stylesheets
   *
   * @return the number of stylesheets
   * @throws no exceptions
   */
  virtual PRInt32 GetNumberOfStyleSheets() const = 0;
  
  /**
   * Get a particular stylesheet
   * @param aIndex the index the stylesheet lives at.  This is zero-based
   * @return the stylesheet at aIndex.  Null if aIndex is out of range.
   * @throws no exceptions
   */
  virtual nsIStyleSheet* GetStyleSheetAt(PRInt32 aIndex) const = 0;
  
  /**
   * Insert a sheet at a particular spot in the stylesheet list (zero-based)
   * @param aSheet the sheet to insert
   * @param aIndex the index to insert at.  This index will be
   *   adjusted for the "special" sheets.
   * @throws no exceptions
   */
  virtual void InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex) = 0;

  /**
   * Get the index of a particular stylesheet.  This will _always_
   * consider the "special" sheets as part of the sheet list.
   * @param aSheet the sheet to get the index of
   * @return aIndex the index of the sheet in the full list
   */
  virtual PRInt32 GetIndexOfStyleSheet(nsIStyleSheet* aSheet) const = 0;

  /**
   * Replace the stylesheets in aOldSheets with the stylesheets in
   * aNewSheets. The two lists must have equal length, and the sheet
   * at positon J in the first list will be replaced by the sheet at
   * position J in the second list.  Some sheets in the second list
   * may be null; if so the corresponding sheets in the first list
   * will simply be removed.
   */
  virtual void UpdateStyleSheets(nsCOMArray<nsIStyleSheet>& aOldSheets,
                                 nsCOMArray<nsIStyleSheet>& aNewSheets) = 0;

  /**
   * Add a stylesheet to the document
   */
  virtual void AddStyleSheet(nsIStyleSheet* aSheet) = 0;

  /**
   * Remove a stylesheet from the document
   */
  virtual void RemoveStyleSheet(nsIStyleSheet* aSheet) = 0;

  /**
   * Notify the document that the applicable state of the sheet changed
   * and that observers should be notified and style sets updated
   */
  virtual void SetStyleSheetApplicableState(nsIStyleSheet* aSheet,
                                            PRBool aApplicable) = 0;  

  /**
   * Just like the style sheet API, but for "catalog" sheets,
   * extra sheets inserted at the UA level.
   */
  virtual PRInt32 GetNumberOfCatalogStyleSheets() const = 0;
  virtual nsIStyleSheet* GetCatalogStyleSheetAt(PRInt32 aIndex) const = 0;
  virtual void AddCatalogStyleSheet(nsIStyleSheet* aSheet) = 0;
  virtual void EnsureCatalogStyleSheet(const char *aStyleSheetURI) = 0;

  /**
   * Get this document's CSSLoader.  This is guaranteed to not return null.
   */
  nsICSSLoader* CSSLoader() const {
    return mCSSLoader;
  }

  /**
   * Get the channel that was passed to StartDocumentLoad for this
   * document.  Note that this may be null in some cases (eg if
   * StartDocumentLoad was never called)
   */
  virtual nsIChannel* GetChannel() const = 0;

  /**
   * Get this document's attribute stylesheet.  May return null if
   * there isn't one.
   */
  virtual nsHTMLStyleSheet* GetAttributeStyleSheet() const = 0;

  /**
   * Get this document's inline style sheet.  May return null if there
   * isn't one
   */
  virtual nsIHTMLCSSStyleSheet* GetInlineStyleSheet() const = 0;
  
  /**
   * Get/set the object from which a document can get a script context
   * and scope. This is the context within which all scripts (during
   * document creation and during event handling) will run. Note that
   * this is the *inner* window object.
   */
  virtual nsIScriptGlobalObject* GetScriptGlobalObject() const = 0;
  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject) = 0;

  /**
   * Get the script loader for this document
   */ 
  virtual nsIScriptLoader* GetScriptLoader() = 0;

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

  // Observation hooks used to propagate notifications to document observers.
  // BeginUpdate must be called before any batch of modifications of the
  // content model or of style data, EndUpdate must be called afterward.
  // To make this easy and painless, use the mozAutoDocUpdate helper class.
  virtual void BeginUpdate(nsUpdateType aUpdateType) = 0;
  virtual void EndUpdate(nsUpdateType aUpdateType) = 0;
  virtual void BeginLoad() = 0;
  virtual void EndLoad() = 0;
  virtual void CharacterDataChanged(nsIContent* aContent, PRBool aAppend) = 0;
  // notify that one or two content nodes changed state
  // either may be nsnull, but not both
  virtual void ContentStatesChanged(nsIContent* aContent1,
                                    nsIContent* aContent2,
                                    PRInt32 aStateMask) = 0;
  virtual void AttributeWillChange(nsIContent* aChild,
                                   PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute) = 0;
  virtual void AttributeChanged(nsIContent* aChild,
                                PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute,
                                PRInt32 aModType) = 0;
  virtual void ContentAppended(nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer) = 0;
  virtual void ContentInserted(nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer) = 0;
  virtual void ContentRemoved(nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer) = 0;

  // Observation hooks for style data to propagate notifications
  // to document observers
  virtual void StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aOldStyleRule,
                                nsIStyleRule* aNewStyleRule) = 0;
  virtual void StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) = 0;
  virtual void StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule) = 0;

  virtual nsresult HandleDOMEvent(nsPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus) = 0;

  /**
   * Flush notifications for this document and its parent documents
   * (since those may affect the layout of this one).
   */
  virtual void FlushPendingNotifications(mozFlushType aType) = 0;

  PRInt32 GetAndIncrementContentID()
  {
    return mNextContentID++;
  }

  nsIBindingManager* BindingManager() const
  {
    return mBindingManager;
  }

  /**
   * Only to be used inside Gecko, you can't really do anything with the
   * pointer outside Gecko anyway.
   */
  nsNodeInfoManager* NodeInfoManager() const
  {
    return mNodeInfoManager;
  }

  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) = 0;
  /**
   * Reset this document to aURI and aLoadGroup.  aURI must not be null.
   */
  virtual void ResetToURI(nsIURI *aURI, nsILoadGroup* aLoadGroup) = 0;

  /**
   * Set the container (docshell) for this document.
   */
  void SetContainer(nsISupports *aContainer)
  {
    mDocumentContainer = do_GetWeakReference(aContainer);
  }

  /**
   * Get the container (docshell) for this document.
   */
  already_AddRefed<nsISupports> GetContainer() const
  {
    nsISupports* container = nsnull;
    if (mDocumentContainer)
      CallQueryReferent(mDocumentContainer.get(), &container);

    return container;
  }

  virtual nsIScriptEventManager* GetScriptEventManager() = 0;

  /**
   * Set and get XML declaration. If aVersion is null there is no declaration.
   * aStandalone takes values -1, 0 and 1 indicating respectively that there
   * was no standalone parameter in the declaration, that it was given as no,
   * or that it was given as yes.
   */
  virtual void SetXMLDeclaration(const PRUnichar *aVersion,
                                 const PRUnichar *aEncoding,
                                 const PRInt32 aStandalone) = 0;
  virtual void GetXMLDeclaration(nsAString& aVersion,
                                 nsAString& aEncoding,
                                 nsAString& Standalone) = 0;

  virtual PRBool IsCaseSensitive()
  {
    return PR_TRUE;
  }

  virtual PRBool IsScriptEnabled() = 0;

  virtual nsresult AddXMLEventsContent(nsIContent * aXMLEventsElement) = 0;

  virtual PRBool IsLoadedAsData()
  {
    return PR_FALSE;
  }

  /**
   * Create an element with the specified name, prefix and namespace ID.
   * If aDocumentDefaultType is true we create an element of the default type
   * for that document (currently XHTML in HTML documents and XUL in XUL
   * documents), otherwise we use the type specified by the namespace ID.
   */
  virtual nsresult CreateElem(nsIAtom *aName, nsIAtom *aPrefix,
                              PRInt32 aNamespaceID,
                              PRBool aDocumentDefaultType,
                              nsIContent** aResult) = 0;

  /**
   * Get the security info (i.e. SSL state etc) that the document got
   * from the channel/document that created the content of the
   * document.
   *
   * @see nsIChannel
   */
  nsISupports *GetSecurityInfo()
  {
    return mSecurityInfo;
  }

  /**
   * Returns the default namespace ID used for elements created in this
   * document.
   */
  virtual PRInt32 GetDefaultNamespaceID() const = 0;

  virtual void* GetProperty(nsIAtom *aPropertyName,
                            nsresult *aStatus = nsnull) const = 0;

  virtual nsresult SetProperty(nsIAtom            *aPropertyName,
                               void               *aValue,
                               NSPropertyDtorFunc  aDtor = nsnull) = 0;

  virtual nsresult DeleteProperty(nsIAtom *aPropertyName) = 0;

  virtual void* UnsetProperty(nsIAtom  *aPropertyName,
                              nsresult *aStatus = nsnull) = 0;

  nsPropertyTable* PropertyTable() { return &mPropertyTable; }

  /**
   * Sets the ID used to identify this part of the multipart document
   */
  void SetPartID(PRUint32 aID) {
    mPartID = aID;
  }

  /**
   * Return the ID used to identify this part of the multipart document
   */
  PRUint32 GetPartID() const {
    return mPartID;
  }

  /**
   * Sanitize the document by resetting all input elements and forms that have
   * autocomplete=off to their default values.
   */
  virtual nsresult Sanitize() = 0;

  /**
   * Enumerate all subdocuments.
   * The enumerator callback should return PR_TRUE to continue enumerating, or
   * PR_FALSE to stop.
   */
  typedef PRBool (*nsSubDocEnumFunc)(nsIDocument *aDocument, void *aData);
  virtual void EnumerateSubDocuments(nsSubDocEnumFunc aCallback,
                                     void *aData) = 0;

  /**
   * Check whether it is safe to cache the presentation of this document
   * and all of its subdocuments. This method checks the following conditions
   * recursively:
   *  - Some document types, such as plugin documents, cannot be safely cached.
   *  - If there are any pending requests, we don't allow the presentation
   *    to be cached.  Ideally these requests would be suspended and resumed,
   *    but that is difficult in some cases, such as XMLHttpRequest.
   *  - If there are any beforeunload or unload listeners, we must fire them
   *    for correctness, but this likely puts the document into a state where
   *    it would not function correctly if restored.
   *
   * |aNewRequest| should be the request for a new document which will
   * replace this document in the docshell.  The new document's request
   * will be ignored when checking for active requests.  If there is no
   * request associated with the new document, this parameter may be null.
   */
  virtual PRBool CanSavePresentation(nsIRequest *aNewRequest) = 0;

  /**
   * Notify the document that its associated ContentViewer is being destroyed.
   * This releases circular references so that the document can go away.
   * Destroy() is only called on documents that have a content viewer.
   */
  virtual void Destroy() = 0;

  /**
   * Get the layout history state that should be used to save and restore state
   * for nodes in this document.  This may return null; if that happens state
   * saving and restoration is not possible.
   */
  virtual already_AddRefed<nsILayoutHistoryState> GetLayoutHistoryState() const = 0;

  /**
   * Methods that can be used to prevent onload firing while an event that
   * should block onload is posted.  onload is guaranteed to not fire until
   * either all calls to BlockOnload() have been matched by calls to
   * UnblockOnload() or the load has been stopped altogether (by the user
   * pressing the Stop button, say).  onload may fire synchronously from inside
   * the UnblockOnload() call.
   */
  virtual void BlockOnload() = 0;
  virtual void UnblockOnload() = 0;

  /**
   * Notification that the page has been shown, for documents which are loaded
   * into a DOM window.  This corresponds to the completion of document load,
   * or to the page's presentation being restored into an existing DOM window.
   * This notification fires applicable DOM events to the content window.  See
   * nsIDOMPageTransitionEvent.idl for a description of the |aPersisted|
   * parameter.
   */
  virtual void OnPageShow(PRBool aPersisted) = 0;

  /**
   * Notification that the page has been hidden, for documents which are loaded
   * into a DOM window.  This corresponds to the unloading of the document, or
   * to the document's presentation being saved but removed from an existing
   * DOM window.  This notification fires applicable DOM events to the content
   * window.  See nsIDOMPageTransitionEvent.idl for a description of the
   * |aPersisted| parameter.
   */
  virtual void OnPageHide(PRBool aPersisted) = 0;
  
  /*
   * We record the set of links in the document that are relevant to
   * style.
   */
  /**
   * Notification that an element is a link with a given URI that is
   * relevant to style.
   */
  virtual void AddStyleRelevantLink(nsIContent* aContent, nsIURI* aURI) = 0;
  /**
   * Notification that an element is a link and its URI might have been
   * changed or the element removed. If the element is still a link relevant
   * to style, then someone must ensure that AddStyleRelevantLink is
   * (eventually) called on it again.
   */
  virtual void ForgetLink(nsIContent* aContent) = 0;
  /**
   * Notification that the visitedness state of a URI has been changed
   * and style related to elements linking to that URI should be updated.
   */
  virtual void NotifyURIVisitednessChanged(nsIURI* aURI) = 0;

protected:
  ~nsIDocument()
  {
    // XXX The cleanup of mNodeInfoManager (calling DropDocumentReference and
    //     releasing it) happens in the nsDocument destructor. We'd prefer to
    //     do it here but nsNodeInfoManager is a concrete class that we don't
    //     want to expose to users of the nsIDocument API outside of Gecko.
  }

  nsString mDocumentTitle;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mDocumentBaseURI;

  nsWeakPtr mDocumentLoadGroup;

  nsWeakPtr mDocumentContainer;

  nsCString mCharacterSet;
  PRInt32 mCharacterSetSource;

  // This is just a weak pointer; the parent document owns its children.
  nsIDocument* mParentDocument;

  // A weak reference to the only child element, or null if no
  // such element exists.
  nsIContent* mRootContent;

  // A content ID counter used to give a monotonically increasing ID
  // to the content objects in the document's content model
  PRInt32 mNextContentID;

  nsCOMPtr<nsIBindingManager> mBindingManager;
  nsNodeInfoManager* mNodeInfoManager; // [STRONG]

  nsICSSLoader* mCSSLoader; // [STRONG; not a COMPtr to avoid
                            // including nsICSSLoader.h; the ownership
                            // is managed by nsDocument]

  // Table of element properties for this document.
  nsPropertyTable mPropertyTable;

  // True if BIDI is enabled.
  PRBool mBidiEnabled;

  nsXPIDLCString mContentLanguage;
  nsCString mContentType;

  // The document's security info
  nsCOMPtr<nsISupports> mSecurityInfo;

  // if this document is part of a multipart document,
  // the ID can be used to distinguish it from the other parts.
  PRUint32 mPartID;
};


/**
 * Helper class to automatically handle batching of document updates.  This
 * class will call BeginUpdate on construction and EndUpdate on destruction on
 * the given document with the given update type.  The document could be null,
 * in which case no updates will be called.  The constructor also takes a
 * boolean that can be set to false to prevent notifications.
 */
class mozAutoDocUpdate
{
public:
  mozAutoDocUpdate(nsIDocument* aDocument, nsUpdateType aUpdateType,
                   PRBool aNotify) :
    mDocument(aNotify ? aDocument : nsnull),
    mUpdateType(aUpdateType)
  {
    if (mDocument) {
      mDocument->BeginUpdate(mUpdateType);
    }
  }

  ~mozAutoDocUpdate()
  {
    if (mDocument) {
      mDocument->EndUpdate(mUpdateType);
    }
  }

private:
  nsCOMPtr<nsIDocument> mDocument;
  nsUpdateType mUpdateType;
};

// XXX These belong somewhere else
nsresult
NS_NewHTMLDocument(nsIDocument** aInstancePtrResult);

nsresult
NS_NewXMLDocument(nsIDocument** aInstancePtrResult);

#ifdef MOZ_SVG
nsresult
NS_NewSVGDocument(nsIDocument** aInstancePtrResult);
#endif

nsresult
NS_NewImageDocument(nsIDocument** aInstancePtrResult);

nsresult
NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
                       nsIDocument* aOwnerDocument);
nsresult
NS_NewDOMDocument(nsIDOMDocument** aInstancePtrResult,
                  const nsAString& aNamespaceURI, 
                  const nsAString& aQualifiedName, 
                  nsIDOMDocumentType* aDoctype,
                  nsIURI* aBaseURI);
nsresult
NS_NewPluginDocument(nsIDocument** aInstancePtrResult);

#endif /* nsIDocument_h___ */
