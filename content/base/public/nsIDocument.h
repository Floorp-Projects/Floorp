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

#include "nsINode.h"
#include "nsStringGlue.h"
#include "nsIDocumentObserver.h" // for nsUpdateType
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIURI.h"
#include "nsWeakPtr.h"
#include "nsIWeakReferenceUtils.h"
#include "nsILoadGroup.h"
#include "nsCRT.h"
#include "mozFlushType.h"
#include "nsIAtom.h"
#include "nsCompatibility.h"
#include "nsTObserverArray.h"
#include "nsNodeInfoManager.h"
#include "nsIStreamListener.h"
#include "nsIObserver.h"

class nsIContent;
class nsPresContext;
class nsIPresShell;
class nsIDocShell;
class nsStyleSet;
class nsIStyleSheet;
class nsIStyleRule;
class nsIViewManager;
class nsIScriptGlobalObject;
class nsPIDOMWindow;
class nsIDOMEvent;
class nsIDeviceContext;
class nsIParser;
class nsIDOMNode;
class nsIDOMElement;
class nsIDOMDocumentFragment;
class nsILineBreaker;
class nsIWordBreaker;
class nsISelection;
class nsIChannel;
class nsIPrincipal;
class nsIDOMDocument;
class nsIDOMDocumentType;
class nsScriptLoader;
class nsIContentSink;
class nsIScriptEventManager;
class nsICSSLoader;
class nsHTMLStyleSheet;
class nsIHTMLCSSStyleSheet;
class nsILayoutHistoryState;
class nsIVariant;
class nsIDOMUserDataHandler;
template<class E> class nsCOMArray;
class nsIDocumentObserver;
class nsBindingManager;
class nsIDOMNodeList;
class mozAutoSubtreeModified;
struct JSObject;
class nsFrameLoader;

// IID for the nsIDocument interface
#define NS_IDOCUMENT_IID      \
{ 0x6304ae8e, 0x2634, 0x45ed, \
  { 0x9e, 0x09, 0x83, 0x09, 0x5b, 0x46, 0x72, 0x8b } }

// Flag for AddStyleSheet().
#define NS_STYLESHEET_FROM_CATALOG                (1 << 0)

//----------------------------------------------------------------------

// Document interface.  This is implemented by all document objects in
// Gecko.
class nsIDocument : public nsINode
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_IID)
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

#ifdef MOZILLA_INTERNAL_API
  nsIDocument()
    : nsINode(nsnull),
      mCharacterSet(NS_LITERAL_CSTRING("ISO-8859-1")),
      mNodeInfoManager(nsnull),
      mCompatMode(eCompatibility_FullStandards),
      mIsInitialDocumentInWindow(PR_FALSE),
      mMayStartLayout(PR_TRUE),
      mPartID(0),
      mJSObject(nsnull)
  {
    mParentPtrBits |= PARENT_BIT_INDOCUMENT;
  }
#endif
  
  /**
   * Let the document know that we're starting to load data into it.
   * @param aCommand The parser command
   *                 XXXbz It's odd to have that here.
   * @param aChannel The channel the data will come from
   * @param aLoadGroup The loadgroup this document should use from now on.
   *                   Note that the document might not be the only thing using
   *                   this loadgroup.
   * @param aContainer The container this document is in.  This may be null.
   *                   XXXbz maybe we should make it more explicit (eg make the
   *                   container an nsIWebNavigation or nsIDocShell or
   *                   something)?
   * @param [out] aDocListener the listener to pump data from the channel into.
   *                           Generally this will be the parser this document
   *                           sets up, or some sort of data-handler for media
   *                           documents.
   * @param aReset whether the document should call Reset() on itself.  If this
   *               is false, the document will NOT set its principal to the
   *               channel's owner, will not clear any event listeners that are
   *               already set on it, etc.
   * @param aSink The content sink to use for the data.  If this is null and
   *              the document needs a content sink, it will create one based
   *              on whatever it knows about the data it's going to load.
   *
   * Once this has been called, the document will return false for
   * MayStartLayout() until SetMayStartLayout(PR_TRUE) is called on it.  Making
   * sure this happens is the responsibility of the caller of
   * StartDocumentLoad().
   */  
  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     PRBool aReset,
                                     nsIContentSink* aSink = nsnull) = 0;
  virtual void StopDocumentLoad() = 0;

  /**
   * Signal that the document title may have changed
   * (see nsDocument::GetTitle).
   * @param aBoundTitleElement true if an HTML or SVG <title> element
   * has just been bound to the document.
   */
  virtual void NotifyPossibleTitleChange(PRBool aBoundTitleElement) = 0;
  
  /**
   * Return the URI for the document. May return null.
   */
  nsIURI* GetDocumentURI() const
  {
    return mDocumentURI;
  }

  /**
   * Set the URI for the document.
   */
  virtual void SetDocumentURI(nsIURI* aURI) = 0;

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
   * Return a standard name for the document's character set.
   */
  const nsCString& GetDocumentCharacterSet() const
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

  // This method MUST be called before SetDocumentCharacterSet if
  // you're planning to call both.
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
   * This gets fired when the element that an id refers to changes.
   * This fires at difficult times. It is generally not safe to do anything
   * which could modify the DOM in any way. Use
   * nsContentUtils::AddScriptRunner.
   * @return PR_TRUE to keep the callback in the callback set, PR_FALSE
   * to remove it.
   */
  typedef PRBool (* IDTargetObserver)(nsIContent* aOldContent,
                                      nsIContent* aNewContent, void* aData);

  /**
   * Add an IDTargetObserver for a specific ID. The IDTargetObserver
   * will be fired whenever the content associated with the ID changes
   * in the future. At most one (aObserver, aData) pair can be registered
   * for each ID.
   * @return the content currently associated with the ID.
   */
  virtual nsIContent* AddIDTargetObserver(nsIAtom* aID,
                                          IDTargetObserver aObserver, void* aData) = 0;
  /**
   * Remove the (aObserver, aData) pair for a specific ID, if registered.
   */
  virtual void RemoveIDTargetObserver(nsIAtom* aID,
                                      IDTargetObserver aObserver, void* aData) = 0;

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
    CopyASCIItoUTF16(mContentLanguage, aContentLanguage);
  }

  // The states BidiEnabled and MathMLEnabled should persist across multiple views
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
  void SetBidiEnabled()
  {
    mBidiEnabled = PR_TRUE;
  }
  
  /**
   * Check if the document contains (or has contained) any MathML elements.
   */
  PRBool GetMathMLEnabled() const
  {
    return mMathMLEnabled;
  }
  
  void SetMathMLEnabled()
  {
    mMathMLEnabled = PR_TRUE;
  }

  /**
   * Ask this document whether it's the initial document in its window.
   */
  PRBool IsInitialDocument() const
  {
    return mIsInitialDocumentInWindow;
  }
  
  /**
   * Tell this document that it's the initial document in its window.  See
   * comments on mIsInitialDocumentInWindow for when this should be called.
   */
  void SetIsInitialDocument(PRBool aIsInitialDocument)
  {
    mIsInitialDocumentInWindow = aIsInitialDocument;
  }
  

  /**
   * Get the bidi options for this document.
   * @see nsBidiUtils.h
   */
  PRUint32 GetBidiOptions() const
  {
    return mBidiOptions;
  }

  /**
   * Set the bidi options for this document.  This just sets the bits;
   * callers are expected to take action as needed if they want this
   * change to actually change anything immediately.
   * @see nsBidiUtils.h
   */
  void SetBidiOptions(PRUint32 aBidiOptions)
  {
    mBidiOptions = aBidiOptions;
  }
  
  /**
   * Access HTTP header data (this may also get set from other
   * sources, like HTML META tags).
   */
  virtual void GetHeaderData(nsIAtom* aHeaderField, nsAString& aData) const = 0;
  virtual void SetHeaderData(nsIAtom* aheaderField, const nsAString& aData) = 0;

  /**
   * Create a new presentation shell that will use aContext for its
   * presentation context (presentation contexts <b>must not</b> be
   * shared among multiple presentation shells). The caller of this
   * method is responsible for calling BeginObservingDocument() on the
   * presshell if the presshell should observe document mutations.
   */
  virtual nsresult CreateShell(nsPresContext* aContext,
                               nsIViewManager* aViewManager,
                               nsStyleSet* aStyleSet,
                               nsIPresShell** aInstancePtrResult) = 0;
  virtual PRBool DeleteShell(nsIPresShell* aShell) = 0;
  virtual nsIPresShell *GetPrimaryShell() const = 0;
  void SetShellsHidden(PRBool aHide) { mShellsAreHidden = aHide; }
  PRBool ShellsAreHidden() const { return mShellsAreHidden; }

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
    return (mCachedRootContent &&
            mCachedRootContent->GetNodeParent() == this) ?
           reinterpret_cast<nsIContent*>(mCachedRootContent.get()) :
           GetRootContentInternal();
  }
  virtual nsIContent *GetRootContentInternal() const = 0;

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
   * Get the channel that was passed to StartDocumentLoad or Reset for this
   * document.  Note that this may be null in some cases (eg if
   * StartDocumentLoad or Reset were never called)
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
   * Get/set the object from which the context for the event/script handling can
   * be got. Normally GetScriptHandlingObject() returns the same object as
   * GetScriptGlobalObject(), but if the document is loaded as data,
   * non-null may be returned, even if GetScriptGlobalObject() returns null.
   * aHasHadScriptHandlingObject is set PR_TRUE if document has had the object
   * for event/script handling. Do not process any events/script if the method
   * returns null, but aHasHadScriptHandlingObject is true.
   */
  virtual nsIScriptGlobalObject*
    GetScriptHandlingObject(PRBool& aHasHadScriptHandlingObject) const = 0;
  virtual void SetScriptHandlingObject(nsIScriptGlobalObject* aScriptObject) = 0;

  /**
   * Get the object that is used as the scope for all of the content
   * wrappers whose owner document is this document. Unlike the script global
   * object, this will only return null when the global object for this
   * document is truly gone. Use this object when you're trying to find a
   * content wrapper in XPConnect.
   */
  virtual nsIScriptGlobalObject* GetScopeObject() = 0;

  /**
   * Return the window containing the document (the outer window).
   */
  virtual nsPIDOMWindow *GetWindow() = 0;

  /**
   * Return the inner window used as the script compilation scope for
   * this document. If you're not absolutely sure you need this, use
   * GetWindow().
   */
  virtual nsPIDOMWindow *GetInnerWindow() = 0;

  /**
   * Get the script loader for this document
   */ 
  virtual nsScriptLoader* ScriptLoader() = 0;

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
  // notify that one or two content nodes changed state
  // either may be nsnull, but not both
  virtual void ContentStatesChanged(nsIContent* aContent1,
                                    nsIContent* aContent2,
                                    PRInt32 aStateMask) = 0;

  // Observation hooks for style data to propagate notifications
  // to document observers
  virtual void StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aOldStyleRule,
                                nsIStyleRule* aNewStyleRule) = 0;
  virtual void StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) = 0;
  virtual void StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule) = 0;

  /**
   * Notify document of pending attribute change
   */
  virtual void AttributeWillChange(nsIContent* aChild,
                                   PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute) = 0;

  /**
   * Flush notifications for this document and its parent documents
   * (since those may affect the layout of this one).
   */
  virtual void FlushPendingNotifications(mozFlushType aType) = 0;

  nsBindingManager* BindingManager() const
  {
    return mNodeInfoManager->GetBindingManager();
  }

  /**
   * Only to be used inside Gecko, you can't really do anything with the
   * pointer outside Gecko anyway.
   */
  nsNodeInfoManager* NodeInfoManager() const
  {
    return mNodeInfoManager;
  }

  /**
   * Reset the document using the given channel and loadgroup.  This works
   * like ResetToURI, but also sets the document's channel to aChannel.
   * The principal of the document will be set from the channel.
   */
  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) = 0;

  /**
   * Reset this document to aURI, aLoadGroup, and aPrincipal.  aURI must not be
   * null.  If aPrincipal is null, a codebase principal based on aURI will be
   * used.
   */
  virtual void ResetToURI(nsIURI *aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal) = 0;

  virtual void AddReference(void *aKey, nsISupports *aReference) = 0;
  virtual nsISupports *GetReference(void *aKey) = 0;
  virtual void RemoveReference(void *aKey) = 0;

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
   * PR_FALSE to stop.  This will never get passed a null aDocument.
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
   * Notify the document that its associated ContentViewer is no longer
   * the current viewer for the docshell. The document might still
   * be rendered in "zombie state" until the next document is ready.
   * The document should save form control state.
   */
  virtual void RemovedFromDocShell() = 0;
  
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
   * pressing the Stop button, say).
   */
  virtual void BlockOnload() = 0;
  /**
   * @param aFireSync whether to fire onload synchronously.  If false,
   * onload will fire asynchronously after all onload blocks have been
   * removed.  It will NOT fire from inside UnblockOnload.  If true,
   * onload may fire from inside UnblockOnload.
   */
  virtual void UnblockOnload(PRBool aFireSync) = 0;

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

  /**
   * Resets and removes a box object from the document's box object cache
   *
   * @param aElement canonical nsIContent pointer of the box object's element
   */
  virtual void ClearBoxObjectFor(nsIContent *aContent) = 0;

  /**
   * Get the compatibility mode for this document
   */
  nsCompatibility GetCompatibilityMode() const {
    return mCompatMode;
  }
  
  /**
   * Check whether we've ever fired a DOMTitleChanged event for this
   * document.
   */
  PRBool HaveFiredDOMTitleChange() const {
    return mHaveFiredTitleChange;
  }

  /**
   * See GetXBLChildNodesFor on nsBindingManager
   */
  virtual nsresult GetXBLChildNodesFor(nsIContent* aContent,
                                       nsIDOMNodeList** aResult) = 0;

  /**
   * See GetContentListFor on nsBindingManager
   */
  virtual nsresult GetContentListFor(nsIContent* aContent,
                                     nsIDOMNodeList** aResult) = 0;

  /**
   * Helper for nsIDOMNSDocument::elementFromPoint implementation that allows
   * ignoring the scroll frame and/or avoiding layout flushes.
   *
   * @see nsIDOMWindowUtils::elementFromPoint
   */
  virtual nsresult ElementFromPointHelper(PRInt32 aX, PRInt32 aY,
                                          PRBool aIgnoreRootScrollFrame,
                                          PRBool aFlushLayout,
                                          nsIDOMElement** aReturn) = 0;

  /**
   * See FlushSkinBindings on nsBindingManager
   */
  virtual void FlushSkinBindings() = 0;

  /**
   * To batch DOMSubtreeModified, document needs to be informed when
   * a mutation event might be dispatched, even if the event isn't actually
   * created because there are no listeners for it.
   *
   * @param aTarget is the target for the mutation event.
   */
  void MayDispatchMutationEvent(nsINode* aTarget)
  {
    if (mSubtreeModifiedDepth > 0) {
      mSubtreeModifiedTargets.AppendObject(aTarget);
    }
  }

  /**
   * Marks as not-going-to-be-collected for the given generation of
   * cycle collection.
   */
  void MarkUncollectableForCCGeneration(PRUint32 aGeneration)
  {
    mMarkedCCGeneration = aGeneration;
  }

  /**
   * Gets the cycle collector generation this document is marked for.
   */
  PRUint32 GetMarkedCCGeneration()
  {
    return mMarkedCCGeneration;
  }

  PRBool IsLoadedAsData()
  {
    return mLoadedAsData;
  }

  PRBool MayStartLayout()
  {
    return mMayStartLayout;
  }

  void SetMayStartLayout(PRBool aMayStartLayout)
  {
    mMayStartLayout = aMayStartLayout;
  }

  JSObject* GetJSObject() const
  {
    return mJSObject;
  }

  void SetJSObject(JSObject *aJSObject)
  {
    mJSObject = aJSObject;
  }

  // This method should return an addrefed nsIParser* or nsnull. Implementations
  // should transfer ownership of the parser to the caller.
  virtual already_AddRefed<nsIParser> GetFragmentParser() {
    return nsnull;
  }

  virtual void SetFragmentParser(nsIParser* aParser) {
    // Do nothing.
  }

  // In case of failure, the document really can't initialize the frame loader.
  virtual nsresult InitializeFrameLoader(nsFrameLoader* aLoader) = 0;
  // In case of failure, the caller must handle the error, for example by
  // finalizing frame loader asynchronously.
  virtual nsresult FinalizeFrameLoader(nsFrameLoader* aLoader) = 0;
  // Removes the frame loader of aShell from the initialization list.
  virtual void TryCancelFrameLoaderInitialization(nsIDocShell* aShell) = 0;
  //  Returns true if the frame loader of aShell is in the finalization list.
  virtual PRBool FrameLoaderScheduledToBeFinalized(nsIDocShell* aShell) = 0;

  /**
   * Check whether this document is a root document that is not an
   * external resource.
   */
  PRBool IsRootDisplayDocument() const
  {
    return !mParentDocument && !mDisplayDocument;
  }

  /**
   * Get the document for which this document is an external resource.  This
   * will be null if this document is not an external resource.  Otherwise,
   * GetDisplayDocument() will return a non-null document, and
   * GetDisplayDocument()->GetDisplayDocument() is guaranteed to be null.
   */
  nsIDocument* GetDisplayDocument() const
  {
    return mDisplayDocument;
  }
  
  /**
   * Set the display document for this document.  aDisplayDocument must not be
   * null.
   */
  void SetDisplayDocument(nsIDocument* aDisplayDocument)
  {
    NS_PRECONDITION(!GetPrimaryShell() &&
                    !nsCOMPtr<nsISupports>(GetContainer()) &&
                    !GetWindow() &&
                    !GetScriptGlobalObject(),
                    "Shouldn't set mDisplayDocument on documents that already "
                    "have a presentation or a docshell or a window");
    NS_PRECONDITION(aDisplayDocument != this, "Should be different document");
    NS_PRECONDITION(!aDisplayDocument->GetDisplayDocument(),
                    "Display documents should not nest");
    mDisplayDocument = aDisplayDocument;
  }

  /**
   * A class that represents an external resource load that has begun but
   * doesn't have a document yet.  Observers can be registered on this object,
   * and will be notified after the document is created.  Observers registered
   * after the document has been created will NOT be notified.  When observers
   * are notified, the subject will be the newly-created document, the topic
   * will be "external-resource-document-created", and the data will be null.
   * If document creation fails for some reason, observers will still be
   * notified, with a null document pointer.
   */
  class ExternalResourceLoad : public nsISupports
  {
  public:
    virtual ~ExternalResourceLoad() {}

    void AddObserver(nsIObserver* aObserver) {
      NS_PRECONDITION(aObserver, "Must have observer");
      mObservers.AppendElement(aObserver);
    }

    const nsTArray< nsCOMPtr<nsIObserver> > & Observers() {
      return mObservers;
    }
  protected:
    nsAutoTArray< nsCOMPtr<nsIObserver>, 8 > mObservers;    
  };

  /**
   * Request an external resource document for aURI.  This will return the
   * resource document if available.  If one is not available yet, it will
   * start loading as needed, and the pending load object will be returned in
   * aPendingLoad so that the caller can register an observer to wait for the
   * load.  If this function returns null and doesn't return a pending load,
   * that means that there is no resource document for this URI and won't be
   * one in the future.
   *
   * @param aURI the URI to get
   * @param aRequestingNode the node making the request
   * @param aPendingLoad the pending load for this request, if any
   */
  virtual nsIDocument*
    RequestExternalResource(nsIURI* aURI,
                            nsINode* aRequestingNode,
                            ExternalResourceLoad** aPendingLoad) = 0;

  /**
   * Enumerate the external resource documents associated with this document.
   * The enumerator callback should return PR_TRUE to continue enumerating, or
   * PR_FALSE to stop.  This callback will never get passed a null aDocument.
   */
  virtual void EnumerateExternalResources(nsSubDocEnumFunc aCallback,
                                          void* aData) = 0;
  
protected:
  ~nsIDocument()
  {
    // XXX The cleanup of mNodeInfoManager (calling DropDocumentReference and
    //     releasing it) happens in the nsDocument destructor. We'd prefer to
    //     do it here but nsNodeInfoManager is a concrete class that we don't
    //     want to expose to users of the nsIDocument API outside of Gecko.
  }

  /**
   * These methods should be called before and after dispatching
   * a mutation event.
   * To make this easy and painless, use the mozAutoSubtreeModified helper class.
   */
  virtual void WillDispatchMutationEvent(nsINode* aTarget) = 0;
  virtual void MutationEventDispatched(nsINode* aTarget) = 0;
  friend class mozAutoSubtreeModified;
  friend class nsPresShellIterator;

  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mDocumentBaseURI;

  nsWeakPtr mDocumentLoadGroup;

  nsWeakPtr mDocumentContainer;

  nsCString mCharacterSet;
  PRInt32 mCharacterSetSource;

  // This is just a weak pointer; the parent document owns its children.
  nsIDocument* mParentDocument;

  // A reference to the content last returned from GetRootContent().
  // This should be an nsIContent, but that would force us to pull in
  // nsIContent.h
  nsCOMPtr<nsINode> mCachedRootContent;

  // We'd like these to be nsRefPtrs, but that'd require us to include
  // additional headers that we don't want to expose.
  // The cleanup is handled by the nsDocument destructor.
  nsNodeInfoManager* mNodeInfoManager; // [STRONG]
  nsICSSLoader* mCSSLoader; // [STRONG]

  // Table of element properties for this document.
  nsPropertyTable mPropertyTable;

  // Compatibility mode
  nsCompatibility mCompatMode;

  // True if BIDI is enabled.
  PRPackedBool mBidiEnabled;
  // True if a MathML element has ever been owned by this document.
  PRPackedBool mMathMLEnabled;

  // True if this document is the initial document for a window.  This should
  // basically be true only for documents that exist in newly-opened windows or
  // documents created to satisfy a GetDocument() on a window when there's no
  // document in it.
  PRPackedBool mIsInitialDocumentInWindow;

  PRPackedBool mShellsAreHidden;

  // True if we're loaded as data and therefor has any dangerous stuff, such
  // as scripts and plugins, disabled.
  PRPackedBool mLoadedAsData;

  // If true, whoever is creating the document has gotten it to the
  // point where it's safe to start layout on it.
  PRPackedBool mMayStartLayout;
  
  // True iff we've ever fired a DOMTitleChanged event for this document
  PRPackedBool mHaveFiredTitleChange;

  // The bidi options for this document.  What this bitfield means is
  // defined in nsBidiUtils.h
  PRUint32 mBidiOptions;

  nsCString mContentLanguage;
  nsCString mContentType;

  // The document's security info
  nsCOMPtr<nsISupports> mSecurityInfo;

  // if this document is part of a multipart document,
  // the ID can be used to distinguish it from the other parts.
  PRUint32 mPartID;
  
  // Cycle collector generation in which we're certain that this document
  // won't be collected
  PRUint32 mMarkedCCGeneration;

  nsTObserverArray<nsIPresShell*> mPresShells;

  nsCOMArray<nsINode> mSubtreeModifiedTargets;
  PRUint32            mSubtreeModifiedDepth;

  // If we're an external resource document, this will be non-null and will
  // point to our "display document": the one that all resource lookups should
  // go to.
  nsCOMPtr<nsIDocument> mDisplayDocument;

private:
  // JSObject cache. Only to be used for performance
  // optimizations. This will be set once this document is touched
  // from JS, and it will be unset once the JSObject is finalized.
  JSObject *mJSObject;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocument, NS_IDOCUMENT_IID)

/**
 * mozAutoSubtreeModified batches DOM mutations so that a DOMSubtreeModified
 * event is dispatched, if necessary, when the outermost mozAutoSubtreeModified
 * object is deleted.
 */
class NS_STACK_CLASS mozAutoSubtreeModified
{
public:
  /**
   * @param aSubTreeOwner The document in which a subtree will be modified.
   * @param aTarget       The target of the possible DOMSubtreeModified event.
   *                      Can be nsnull, in which case mozAutoSubtreeModified
   *                      is just used to batch DOM mutations.
   */
  mozAutoSubtreeModified(nsIDocument* aSubtreeOwner, nsINode* aTarget)
  {
    UpdateTarget(aSubtreeOwner, aTarget);
  }

  ~mozAutoSubtreeModified()
  {
    UpdateTarget(nsnull, nsnull);
  }

  void UpdateTarget(nsIDocument* aSubtreeOwner, nsINode* aTarget)
  {
    if (mSubtreeOwner) {
      mSubtreeOwner->MutationEventDispatched(mTarget);
    }

    mTarget = aTarget;
    mSubtreeOwner = aSubtreeOwner;
    if (mSubtreeOwner) {
      mSubtreeOwner->WillDispatchMutationEvent(mTarget);
    }
  }

private:
  nsCOMPtr<nsINode>     mTarget;
  nsCOMPtr<nsIDocument> mSubtreeOwner;
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

#ifdef MOZ_MEDIA
nsresult
NS_NewVideoDocument(nsIDocument** aInstancePtrResult);
#endif

nsresult
NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
                       nsNodeInfoManager *aNodeInfoManager);

// Note: it's the caller's responsibility to create or get aPrincipal as needed
// -- this method will not attempt to get a principal based on aDocumentURI.
// Also, both aDocumentURI and aBaseURI must not be null.
nsresult
NS_NewDOMDocument(nsIDOMDocument** aInstancePtrResult,
                  const nsAString& aNamespaceURI, 
                  const nsAString& aQualifiedName, 
                  nsIDOMDocumentType* aDoctype,
                  nsIURI* aDocumentURI,
                  nsIURI* aBaseURI,
                  nsIPrincipal* aPrincipal,
                  PRBool aLoadedAsData);
nsresult
NS_NewPluginDocument(nsIDocument** aInstancePtrResult);

#endif /* nsIDocument_h___ */
