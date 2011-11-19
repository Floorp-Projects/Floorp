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
 *   Ms2ger <ms2ger@gmail.com>
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
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsNodeInfoManager.h"
#include "nsIStreamListener.h"
#include "nsIVariant.h"
#include "nsIObserver.h"
#include "nsGkAtoms.h"
#include "nsAutoPtr.h"
#include "nsPIDOMWindow.h"
#include "nsSMILAnimationController.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocumentEncoder.h"
#include "nsIAnimationFrameListener.h"
#include "nsEventStates.h"
#include "nsIStructuredCloneContainer.h"
#include "nsIBFCacheEntry.h"
#include "nsDOMMemoryReporter.h"

class nsIContent;
class nsPresContext;
class nsIPresShell;
class nsIDocShell;
class nsStyleSet;
class nsIStyleSheet;
class nsIStyleRule;
class nsCSSStyleSheet;
class nsIViewManager;
class nsIDOMEvent;
class nsIDOMEventTarget;
class nsDeviceContext;
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
class nsHTMLStyleSheet;
class nsHTMLCSSStyleSheet;
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
class nsIBoxObject;
class imgIRequest;
class nsISHEntry;
class nsDOMNavigationTiming;

namespace mozilla {
namespace css {
class Loader;
} // namespace css

namespace dom {
class Link;
class Element;
} // namespace dom
} // namespace mozilla

#define NS_IDOCUMENT_IID \
{ 0xc3e40e8e, 0x8b91, 0x424c, \
  { 0xbe, 0x9c, 0x9c, 0xc1, 0x76, 0xa7, 0xf7, 0x24 } }

// Flag for AddStyleSheet().
#define NS_STYLESHEET_FROM_CATALOG                (1 << 0)

// Document states

// RTL locale: specific to the XUL localedir attribute
#define NS_DOCUMENT_STATE_RTL_LOCALE              NS_DEFINE_EVENT_STATE_MACRO(0)
// Window activation status
#define NS_DOCUMENT_STATE_WINDOW_INACTIVE         NS_DEFINE_EVENT_STATE_MACRO(1)

//----------------------------------------------------------------------

// Document interface.  This is implemented by all document objects in
// Gecko.
class nsIDocument : public nsINode
{
public:
  typedef mozilla::dom::Element Element;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_IID)
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
  NS_DECL_DOM_MEMORY_REPORTER_SIZEOF

#ifdef MOZILLA_INTERNAL_API
  nsIDocument()
    : nsINode(nsnull),
      mCharacterSet(NS_LITERAL_CSTRING("ISO-8859-1")),
      mNodeInfoManager(nsnull),
      mCompatMode(eCompatibility_FullStandards),
      mIsInitialDocumentInWindow(false),
      mMayStartLayout(true),
      mVisible(true),
      mRemovedFromDocShell(false),
      // mAllowDNSPrefetch starts true, so that we can always reliably && it
      // with various values that might disable it.  Since we never prefetch
      // unless we get a window, and in that case the docshell value will get
      // &&-ed in, this is safe.
      mAllowDNSPrefetch(true),
      mIsBeingUsedAsImage(false),
      mHasLinksToUpdate(false),
      mPartID(0)
  {
    SetInDocument();
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
   * MayStartLayout() until SetMayStartLayout(true) is called on it.  Making
   * sure this happens is the responsibility of the caller of
   * StartDocumentLoad().
   */  
  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     bool aReset,
                                     nsIContentSink* aSink = nsnull) = 0;
  virtual void StopDocumentLoad() = 0;

  /**
   * Signal that the document title may have changed
   * (see nsDocument::GetTitle).
   * @param aBoundTitleElement true if an HTML or SVG <title> element
   * has just been bound to the document.
   */
  virtual void NotifyPossibleTitleChange(bool aBoundTitleElement) = 0;

  /**
   * Return the URI for the document. May return null.
   *
   * The value returned corresponds to the "document's current address" in
   * HTML5.  As such, it may change over the lifetime of the document, for
   * instance as a result of a call to pushState() or replaceState().
   */
  nsIURI* GetDocumentURI() const
  {
    return mDocumentURI;
  }

  /**
   * Return the original URI of the document.  This is the same as the
   * document's URI unless history.pushState() or replaceState() is invoked on
   * the document.
   *
   * This method corresponds to the "document's address" in HTML5 and, once
   * set, doesn't change over the lifetime of the document.
   */
  nsIURI* GetOriginalURI() const
  {
    return mOriginalURI;
  }

  /**
   * Set the URI for the document.  This also sets the document's original URI,
   * if it's null.
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
  nsIURI* GetDocBaseURI() const
  {
    return mDocumentBaseURI ? mDocumentBaseURI : mDocumentURI;
  }
  virtual already_AddRefed<nsIURI> GetBaseURI() const
  {
    nsCOMPtr<nsIURI> uri = GetDocBaseURI();

    return uri.forget();
  }

  virtual nsresult SetBaseURI(nsIURI* aURI) = 0;

  /**
   * Get/Set the base target of a link in a document.
   */
  virtual void GetBaseTarget(nsAString &aBaseTarget) = 0;
  void SetBaseTarget(const nsString& aBaseTarget) {
    mBaseTarget = aBaseTarget;
  }

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
   * @return true to keep the callback in the callback set, false
   * to remove it.
   */
  typedef bool (* IDTargetObserver)(Element* aOldElement,
                                      Element* aNewelement, void* aData);

  /**
   * Add an IDTargetObserver for a specific ID. The IDTargetObserver
   * will be fired whenever the content associated with the ID changes
   * in the future. If aForImage is true, mozSetImageElement can override
   * what content is associated with the ID. In that case the IDTargetObserver
   * will be notified at those times when the result of LookupImageElement
   * changes.
   * At most one (aObserver, aData, aForImage) triple can be
   * registered for each ID.
   * @return the content currently associated with the ID.
   */
  virtual Element* AddIDTargetObserver(nsIAtom* aID, IDTargetObserver aObserver,
                                       void* aData, bool aForImage) = 0;
  /**
   * Remove the (aObserver, aData, aForImage) triple for a specific ID, if
   * registered.
   */
  virtual void RemoveIDTargetObserver(nsIAtom* aID, IDTargetObserver aObserver,
                                      void* aData, bool aForImage) = 0;

  /**
   * Get the Content-Type of this document.
   * (This will always return NS_OK, but has this signature to be compatible
   *  with nsIDOMDocument::GetContentType())
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
  bool GetBidiEnabled() const
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
    mBidiEnabled = true;
  }
  
  /**
   * Check if the document contains (or has contained) any MathML elements.
   */
  bool GetMathMLEnabled() const
  {
    return mMathMLEnabled;
  }
  
  void SetMathMLEnabled()
  {
    mMathMLEnabled = true;
  }

  /**
   * Ask this document whether it's the initial document in its window.
   */
  bool IsInitialDocument() const
  {
    return mIsInitialDocumentInWindow;
  }
  
  /**
   * Tell this document that it's the initial document in its window.  See
   * comments on mIsInitialDocumentInWindow for when this should be called.
   */
  void SetIsInitialDocument(bool aIsInitialDocument)
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
  virtual void DeleteShell() = 0;

  nsIPresShell* GetShell() const
  {
    return GetBFCacheEntry() ? nsnull : mPresShell;
  }

  void SetBFCacheEntry(nsIBFCacheEntry* aEntry)
  {
    mBFCacheEntry = aEntry;
  }

  nsIBFCacheEntry* GetBFCacheEntry() const
  {
    return mBFCacheEntry;
  }

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
  virtual nsresult SetSubDocumentFor(Element* aContent,
                                     nsIDocument* aSubDoc) = 0;

  /**
   * Get the sub document for aContent
   */
  virtual nsIDocument *GetSubDocumentFor(nsIContent *aContent) const = 0;

  /**
   * Find the content node for which aDocument is a sub document.
   */
  virtual Element* FindContentForSubDocument(nsIDocument* aDocument) const = 0;

  /**
   * Return the root element for this document.
   */
  Element *GetRootElement() const;

protected:
  virtual Element *GetRootElementInternal() const = 0;

public:
  // Get the root <html> element, or return null if there isn't one (e.g.
  // if the root isn't <html>)
  Element* GetHtmlElement();
  // Returns the first child of GetHtmlContent which has the given tag,
  // or nsnull if that doesn't exist.
  Element* GetHtmlChildElement(nsIAtom* aTag);
  // Get the canonical <body> element, or return null if there isn't one (e.g.
  // if the root isn't <html> or if the <body> isn't there)
  Element* GetBodyElement() {
    return GetHtmlChildElement(nsGkAtoms::body);
  }
  // Get the canonical <head> element, or return null if there isn't one (e.g.
  // if the root isn't <html> or if the <head> isn't there)
  Element* GetHeadElement() {
    return GetHtmlChildElement(nsGkAtoms::head);
  }
  
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
                                            bool aApplicable) = 0;  

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
  mozilla::css::Loader* CSSLoader() const {
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
  virtual nsHTMLCSSStyleSheet* GetInlineStyleSheet() const = 0;

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
   * aHasHadScriptHandlingObject is set true if document has had the object
   * for event/script handling. Do not process any events/script if the method
   * returns null, but aHasHadScriptHandlingObject is true.
   */
  nsIScriptGlobalObject*
    GetScriptHandlingObject(bool& aHasHadScriptHandlingObject) const
  {
    aHasHadScriptHandlingObject = mHasHadScriptHandlingObject;
    return mScriptGlobalObject ? mScriptGlobalObject.get() :
                                 GetScriptHandlingObjectInternal();
  }
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
  nsPIDOMWindow *GetWindow() const
  {
    return mWindow ? mWindow->GetOuterWindow() : GetWindowInternal();
  }

  /**
   * Return the inner window used as the script compilation scope for
   * this document. If you're not absolutely sure you need this, use
   * GetWindow().
   */
  nsPIDOMWindow* GetInnerWindow()
  {
    return mRemovedFromDocShell ? GetInnerWindowInternal() : mWindow;
  }

  /**
   * Return the outer window ID.
   */
  PRUint64 OuterWindowID() const
  {
    nsPIDOMWindow *window = GetWindow();
    return window ? window->WindowID() : 0;
  }

  /**
   * Return the inner window ID.
   */
  PRUint64 InnerWindowID()
  {
    nsPIDOMWindow *window = GetInnerWindow();
    return window ? window->WindowID() : 0;
  }

  /**
   * Get the script loader for this document
   */ 
  virtual nsScriptLoader* ScriptLoader() = 0;

  /**
   * Add/Remove an element to the document's id and name hashes
   */
  virtual void AddToIdTable(Element* aElement, nsIAtom* aId) = 0;
  virtual void RemoveFromIdTable(Element* aElement, nsIAtom* aId) = 0;
  virtual void AddToNameTable(Element* aElement, nsIAtom* aName) = 0;
  virtual void RemoveFromNameTable(Element* aElement, nsIAtom* aName) = 0;

  /**
   * Returns the element which either requested DOM full-screen mode, or
   * contains the element which requested DOM full-screen mode if the
   * requestee is in a subdocument. Note this element must be *in*
   * this document.
   */
  virtual Element* GetFullScreenElement() = 0;

  /**
   * Asynchronously requests that the document make aElement the full-screen
   * element, and move into full-screen mode.
   */
  virtual void AsyncRequestFullScreen(Element* aElement) = 0;

  /**
   * Requests that the document, and all documents in its hierarchy exit
   * from DOM full-screen mode.
   */
  virtual void CancelFullScreen() = 0;

  /**
   * Returns true if this document is in full-screen mode.
   */
  virtual bool IsFullScreenDoc() = 0;

  //----------------------------------------------------------------------

  // Document notification API's

  /**
   * Add a new observer of document change notifications. Whenever
   * content is changed, appended, inserted or removed the observers are
   * informed.  An observer that is already observing the document must
   * not be added without being removed first.
   */
  virtual void AddObserver(nsIDocumentObserver* aObserver) = 0;

  /**
   * Remove an observer of document change notifications. This will
   * return false if the observer cannot be found.
   */
  virtual bool RemoveObserver(nsIDocumentObserver* aObserver) = 0;

  // Observation hooks used to propagate notifications to document observers.
  // BeginUpdate must be called before any batch of modifications of the
  // content model or of style data, EndUpdate must be called afterward.
  // To make this easy and painless, use the mozAutoDocUpdate helper class.
  virtual void BeginUpdate(nsUpdateType aUpdateType) = 0;
  virtual void EndUpdate(nsUpdateType aUpdateType) = 0;
  virtual void BeginLoad() = 0;
  virtual void EndLoad() = 0;

  enum ReadyState { READYSTATE_UNINITIALIZED = 0, READYSTATE_LOADING = 1, READYSTATE_INTERACTIVE = 3, READYSTATE_COMPLETE = 4};
  virtual void SetReadyStateInternal(ReadyState rs) = 0;
  virtual ReadyState GetReadyStateEnum() = 0;

  // notify that a content node changed state.  This must happen under
  // a scriptblocker but NOT within a begin/end update.
  virtual void ContentStateChanged(nsIContent* aContent,
                                   nsEventStates aStateMask) = 0;

  // Notify that a document state has changed.
  // This should only be called by callers whose state is also reflected in the
  // implementation of nsDocument::GetDocumentState.
  virtual void DocumentStatesChanged(nsEventStates aStateMask) = 0;

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
   * Flush notifications for this document and its parent documents
   * (since those may affect the layout of this one).
   */
  virtual void FlushPendingNotifications(mozFlushType aType) = 0;

  /**
   * Calls FlushPendingNotifications on any external resources this document
   * has. If this document has no external resources or is an external resource
   * itself this does nothing. This should only be called with
   * aType >= Flush_Style.
   */
  virtual void FlushExternalResources(mozFlushType aType) = 0;

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

  /**
   * Set the container (docshell) for this document. Virtual so that
   * docshell can call it.
   */
  virtual void SetContainer(nsISupports *aContainer);

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

  bool IsHTML() const
  {
    return mIsRegularHTML;
  }
  bool IsXUL() const
  {
    return mIsXUL;
  }

  virtual bool IsScriptEnabled() = 0;

  virtual void AddXMLEventsContent(nsIContent * aXMLEventsElement) = 0;

  /**
   * Create an element with the specified name, prefix and namespace ID.
   */
  virtual nsresult CreateElem(const nsAString& aName, nsIAtom *aPrefix,
                              PRInt32 aNamespaceID,
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
  PRInt32 GetDefaultNamespaceID() const
  {
    return mDefaultElementType;
  }

  void DeleteAllProperties();
  void DeleteAllPropertiesFor(nsINode* aNode);

  nsPropertyTable* PropertyTable(PRUint16 aCategory) {
    if (aCategory == 0)
      return &mPropertyTable;
    return GetExtraPropertyTable(aCategory);
  }
  PRUint32 GetPropertyTableCount()
  { return mExtraPropertyTables.Length() + 1; }

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
   * The enumerator callback should return true to continue enumerating, or
   * false to stop.  This will never get passed a null aDocument.
   */
  typedef bool (*nsSubDocEnumFunc)(nsIDocument *aDocument, void *aData);
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
  virtual bool CanSavePresentation(nsIRequest *aNewRequest) = 0;

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
  virtual void UnblockOnload(bool aFireSync) = 0;

  /**
   * Notification that the page has been shown, for documents which are loaded
   * into a DOM window.  This corresponds to the completion of document load,
   * or to the page's presentation being restored into an existing DOM window.
   * This notification fires applicable DOM events to the content window.  See
   * nsIDOMPageTransitionEvent.idl for a description of the |aPersisted|
   * parameter. If aDispatchStartTarget is null, the pageshow event is
   * dispatched on the ScriptGlobalObject for this document, otherwise it's
   * dispatched on aDispatchStartTarget.
   * Note: if aDispatchStartTarget isn't null, the showing state of the
   * document won't be altered.
   */
  virtual void OnPageShow(bool aPersisted,
                          nsIDOMEventTarget* aDispatchStartTarget) = 0;

  /**
   * Notification that the page has been hidden, for documents which are loaded
   * into a DOM window.  This corresponds to the unloading of the document, or
   * to the document's presentation being saved but removed from an existing
   * DOM window.  This notification fires applicable DOM events to the content
   * window.  See nsIDOMPageTransitionEvent.idl for a description of the
   * |aPersisted| parameter. If aDispatchStartTarget is null, the pagehide
   * event is dispatched on the ScriptGlobalObject for this document,
   * otherwise it's dispatched on aDispatchStartTarget.
   * Note: if aDispatchStartTarget isn't null, the showing state of the
   * document won't be altered.
   */
  virtual void OnPageHide(bool aPersisted,
                          nsIDOMEventTarget* aDispatchStartTarget) = 0;
  
  /*
   * We record the set of links in the document that are relevant to
   * style.
   */
  /**
   * Notification that an element is a link that is relevant to style.
   */
  virtual void AddStyleRelevantLink(mozilla::dom::Link* aLink) = 0;
  /**
   * Notification that an element is a link and its URI might have been
   * changed or the element removed. If the element is still a link relevant
   * to style, then someone must ensure that AddStyleRelevantLink is
   * (eventually) called on it again.
   */
  virtual void ForgetLink(mozilla::dom::Link* aLink) = 0;

  /**
   * Resets and removes a box object from the document's box object cache
   *
   * @param aElement canonical nsIContent pointer of the box object's element
   */
  virtual void ClearBoxObjectFor(nsIContent *aContent) = 0;

  /**
   * Get the box object for an element. This is not exposed through a
   * scriptable interface except for XUL documents.
   */
  NS_IMETHOD GetBoxObjectFor(nsIDOMElement* aElement, nsIBoxObject** aResult) = 0;

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
  bool HaveFiredDOMTitleChange() const {
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
   * Helper for nsIDOMDocument::elementFromPoint implementation that allows
   * ignoring the scroll frame and/or avoiding layout flushes.
   *
   * @see nsIDOMWindowUtils::elementFromPoint
   */
  virtual nsresult ElementFromPointHelper(float aX, float aY,
                                          bool aIgnoreRootScrollFrame,
                                          bool aFlushLayout,
                                          nsIDOMElement** aReturn) = 0;

  virtual nsresult NodesFromRectHelper(float aX, float aY,
                                       float aTopSize, float aRightSize,
                                       float aBottomSize, float aLeftSize,
                                       bool aIgnoreRootScrollFrame,
                                       bool aFlushLayout,
                                       nsIDOMNodeList** aReturn) = 0;

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

  bool IsLoadedAsData()
  {
    return mLoadedAsData;
  }

  bool MayStartLayout()
  {
    return mMayStartLayout;
  }

  void SetMayStartLayout(bool aMayStartLayout)
  {
    mMayStartLayout = aMayStartLayout;
  }

  already_AddRefed<nsIDocumentEncoder> GetCachedEncoder()
  {
    return mCachedEncoder.forget();
  }

  void SetCachedEncoder(already_AddRefed<nsIDocumentEncoder> aEncoder)
  {
    mCachedEncoder = aEncoder;
  }

  // In case of failure, the document really can't initialize the frame loader.
  virtual nsresult InitializeFrameLoader(nsFrameLoader* aLoader) = 0;
  // In case of failure, the caller must handle the error, for example by
  // finalizing frame loader asynchronously.
  virtual nsresult FinalizeFrameLoader(nsFrameLoader* aLoader) = 0;
  // Removes the frame loader of aShell from the initialization list.
  virtual void TryCancelFrameLoaderInitialization(nsIDocShell* aShell) = 0;
  //  Returns true if the frame loader of aShell is in the finalization list.
  virtual bool FrameLoaderScheduledToBeFinalized(nsIDocShell* aShell) = 0;

  /**
   * Check whether this document is a root document that is not an
   * external resource.
   */
  bool IsRootDisplayDocument() const
  {
    return !mParentDocument && !mDisplayDocument;
  }

  bool IsBeingUsedAsImage() const {
    return mIsBeingUsedAsImage;
  }

  void SetIsBeingUsedAsImage() {
    mIsBeingUsedAsImage = true;
  }

  bool IsResourceDoc() const {
    return IsBeingUsedAsImage() || // Are we a helper-doc for an SVG image?
      !!mDisplayDocument;          // Are we an external resource doc?
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
    NS_PRECONDITION(!GetShell() &&
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
   * The enumerator callback should return true to continue enumerating, or
   * false to stop.  This callback will never get passed a null aDocument.
   */
  virtual void EnumerateExternalResources(nsSubDocEnumFunc aCallback,
                                          void* aData) = 0;

  /**
   * Return whether the document is currently showing (in the sense of
   * OnPageShow() having been called already and OnPageHide() not having been
   * called yet.
   */
  bool IsShowing() const { return mIsShowing; }
  /**
   * Return whether the document is currently visible (in the sense of
   * OnPageHide having been called and OnPageShow not yet having been called)
   */
  bool IsVisible() const { return mVisible; }
  /**
   * Return true when this document is active, i.e., the active document
   * in a content viewer.
   */
  bool IsActive() const { return mDocumentContainer && !mRemovedFromDocShell; }

  void RegisterFreezableElement(nsIContent* aContent);
  bool UnregisterFreezableElement(nsIContent* aContent);
  typedef void (* FreezableElementEnumerator)(nsIContent*, void*);
  void EnumerateFreezableElements(FreezableElementEnumerator aEnumerator,
                                  void* aData);

  // Indicates whether mAnimationController has been (lazily) initialized.
  // If this returns true, we're promising that GetAnimationController()
  // will have a non-null return value.
  bool HasAnimationController()  { return !!mAnimationController; }

  // Getter for this document's SMIL Animation Controller. Performs lazy
  // initialization, if this document supports animation and if
  // mAnimationController isn't yet initialized.
  virtual nsSMILAnimationController* GetAnimationController() = 0;

  // Makes the images on this document capable of having their animation
  // active or suspended. An Image will animate as long as at least one of its
  // owning Documents needs it to animate; otherwise it can suspend.
  virtual void SetImagesNeedAnimating(bool aAnimating) = 0;

  /**
   * Prevents user initiated events from being dispatched to the document and
   * subdocuments.
   */
  virtual void SuppressEventHandling(PRUint32 aIncrease = 1) = 0;

  /**
   * Unsuppress event handling.
   * @param aFireEvents If true, delayed events (focus/blur) will be fired
   *                    asynchronously.
   */
  virtual void UnsuppressEventHandlingAndFireEvents(bool aFireEvents) = 0;

  PRUint32 EventHandlingSuppressed() const { return mEventsSuppressed; }

  bool IsEventHandlingEnabled() {
    return !EventHandlingSuppressed() && mScriptGlobalObject;
  }

  /**
   * Increment the number of external scripts being evaluated.
   */
  void BeginEvaluatingExternalScript() { ++mExternalScriptsBeingEvaluated; }

  /**
   * Decrement the number of external scripts being evaluated.
   */
  void EndEvaluatingExternalScript() { --mExternalScriptsBeingEvaluated; }

  bool IsDNSPrefetchAllowed() const { return mAllowDNSPrefetch; }

  /**
   * Returns true if this document is allowed to contain XUL element and
   * use non-builtin XBL bindings.
   */
  bool AllowXULXBL() {
    return mAllowXULXBL == eTriTrue ? true :
           mAllowXULXBL == eTriFalse ? false :
           InternalAllowXULXBL();
  }

  void ForceEnableXULXBL() {
    mAllowXULXBL = eTriTrue;
  }

  /**
   * true when this document is a static clone of a normal document.
   * For example print preview and printing use static documents.
   */
  bool IsStaticDocument() { return mIsStaticDocument; }

  /**
   * Clones the document and subdocuments and stylesheet etc.
   * @param aCloneContainer The container for the clone document.
   */
  virtual already_AddRefed<nsIDocument>
  CreateStaticClone(nsISupports* aCloneContainer);

  /**
   * If this document is a static clone, this returns the original
   * document.
   */
  nsIDocument* GetOriginalDocument() { return mOriginalDocument; }

  /**
   * Called by nsParser to preload images. Can be removed and code moved
   * to nsPreloadURIs::PreloadURIs() in file nsParser.cpp whenever the
   * parser-module is linked with gklayout-module.
   */
  virtual void MaybePreLoadImage(nsIURI* uri,
                                 const nsAString& aCrossOriginAttr) = 0;

  /**
   * Called by nsParser to preload style sheets.  Can also be merged into
   * the parser if and when the parser is merged with libgklayout.
   */
  virtual void PreloadStyle(nsIURI* aURI, const nsAString& aCharset) = 0;

  /**
   * Called by the chrome registry to load style sheets.  Can be put
   * back there if and when when that module is merged with libgklayout.
   *
   * This always does a synchronous load.  If aIsAgentSheet is true,
   * it also uses the system principal and enables unsafe rules.
   * DO NOT USE FOR UNTRUSTED CONTENT.
   */
  virtual nsresult LoadChromeSheetSync(nsIURI* aURI, bool aIsAgentSheet,
                                       nsCSSStyleSheet** aSheet) = 0;

  /**
   * Returns true if the locale used for the document specifies a direction of
   * right to left. For chrome documents, this comes from the chrome registry.
   * This is used to determine the current state for the :-moz-locale-dir pseudoclass
   * so once can know whether a document is expected to be rendered left-to-right
   * or right-to-left.
   */
  virtual bool IsDocumentRightToLeft() { return false; }

  enum DocumentTheme {
    Doc_Theme_Uninitialized, // not determined yet
    Doc_Theme_None,
    Doc_Theme_Neutral,
    Doc_Theme_Dark,
    Doc_Theme_Bright
  };

  /**
   * Set the document's pending state object (as serialized using structured
   * clone).
   */
  void SetStateObject(nsIStructuredCloneContainer *scContainer)
  {
    mStateObjectContainer = scContainer;
    mStateObjectCached = nsnull;
  }

  /**
   * Returns Doc_Theme_None if there is no lightweight theme specified,
   * Doc_Theme_Dark for a dark theme, Doc_Theme_Bright for a light theme, and
   * Doc_Theme_Neutral for any other theme. This is used to determine the state
   * of the pseudoclasses :-moz-lwtheme and :-moz-lwtheme-text.
   */
  virtual int GetDocumentLWTheme() { return Doc_Theme_None; }

  /**
   * Returns the document state.
   * Document state bits have the form NS_DOCUMENT_STATE_* and are declared in
   * nsIDocument.h.
   */
  virtual nsEventStates GetDocumentState() = 0;

  virtual nsISupports* GetCurrentContentSink() = 0;

  /**
   * Register/Unregister a filedata uri as being "owned" by this document. 
   * I.e. that its lifetime is connected with this document. When the document
   * goes away it should "kill" the uri by calling
   * nsFileDataProtocolHandler::RemoveFileDataEntry
   */
  virtual void RegisterFileDataUri(const nsACString& aUri) = 0;
  virtual void UnregisterFileDataUri(const nsACString& aUri) = 0;

  virtual void SetScrollToRef(nsIURI *aDocumentURI) = 0;
  virtual void ScrollToRef() = 0;
  virtual void ResetScrolledToRefAlready() = 0;
  virtual void SetChangeScrollPosWhenScrollingToRef(bool aValue) = 0;

  /**
   * This method is similar to GetElementById() from nsIDOMDocument but it
   * returns a mozilla::dom::Element instead of a nsIDOMElement.
   * It prevents converting nsIDOMElement to mozilla::dom::Element which is
   * already converted from mozilla::dom::Element.
   */
  virtual Element* GetElementById(const nsAString& aElementId) = 0;

  /**
   * This method returns _all_ the elements in this document which
   * have id aElementId, if there are any.  Otherwise it returns null.
   * The entries of the nsSmallVoidArray are Element*
   */
  virtual const nsSmallVoidArray* GetAllElementsForId(const nsAString& aElementId) const = 0;

  /**
   * Lookup an image element using its associated ID, which is usually provided
   * by |-moz-element()|. Similar to GetElementById, with the difference that
   * elements set using mozSetImageElement have higher priority.
   * @param aId the ID associated the element we want to lookup
   * @return the element associated with |aId|
   */
  virtual Element* LookupImageElement(const nsAString& aElementId) = 0;

  void ScheduleBeforePaintEvent(nsIAnimationFrameListener* aListener);
  void BeforePaintEventFiring()
  {
    mHavePendingPaint = false;
  }

  typedef nsTArray< nsCOMPtr<nsIAnimationFrameListener> > AnimationListenerList;
  /**
   * Put this documents animation frame listeners into the provided
   * list, and forget about them.
   */
  void TakeAnimationFrameListeners(AnimationListenerList& aListeners);

  // This returns true when the document tree is being teared down.
  bool InUnlinkOrDeletion() { return mInUnlinkOrDeletion; }

  /*
   * Image Tracking
   *
   * Style and content images register their imgIRequests with their document
   * so that the document can efficiently tell all descendant images when they
   * are and are not visible. When an image is on-screen, we want to call
   * LockImage() on it so that it doesn't do things like discarding frame data
   * to save memory. The PresShell informs the document whether its images
   * should be locked or not via SetImageLockingState().
   *
   * See bug 512260.
   */

  // Add/Remove images from the document image tracker
  virtual nsresult AddImage(imgIRequest* aImage) = 0;
  virtual nsresult RemoveImage(imgIRequest* aImage) = 0;

  // Makes the images on this document locked/unlocked. By default, the locking
  // state is unlocked/false.
  virtual nsresult SetImageLockingState(bool aLocked) = 0;

  virtual nsresult GetStateObject(nsIVariant** aResult) = 0;

  virtual nsDOMNavigationTiming* GetNavigationTiming() const = 0;

  virtual nsresult SetNavigationTiming(nsDOMNavigationTiming* aTiming) = 0;

  virtual Element* FindImageMap(const nsAString& aNormalizedMapName) = 0;
  
  // Add aLink to the set of links that need their status resolved. 
  void RegisterPendingLinkUpdate(mozilla::dom::Link* aLink);
  
  // Remove aLink from the set of links that need their status resolved.
  // This function must be called when links are removed from the document.
  void UnregisterPendingLinkUpdate(mozilla::dom::Link* aElement);

  // Update state on links in mLinksToUpdate.  This function must
  // be called prior to selector matching.
  void FlushPendingLinkUpdates();

#define DEPRECATED_OPERATION(_op) e##_op,
  enum DeprecatedOperations {
#include "nsDeprecatedOperationList.h"
    eDeprecatedOperationCount
  };
#undef DEPRECATED_OPERATION
  void WarnOnceAbout(DeprecatedOperations aOperation);

  virtual void PostVisibilityUpdateEvent() = 0;

private:
  PRUint64 mWarnedAbout;

protected:
  ~nsIDocument()
  {
    // XXX The cleanup of mNodeInfoManager (calling DropDocumentReference and
    //     releasing it) happens in the nsDocument destructor. We'd prefer to
    //     do it here but nsNodeInfoManager is a concrete class that we don't
    //     want to expose to users of the nsIDocument API outside of Gecko.
  }

  nsPropertyTable* GetExtraPropertyTable(PRUint16 aCategory);

  // Never ever call this. Only call GetWindow!
  virtual nsPIDOMWindow *GetWindowInternal() const = 0;

  // Never ever call this. Only call GetInnerWindow!
  virtual nsPIDOMWindow *GetInnerWindowInternal() = 0;

  // Never ever call this. Only call GetScriptHandlingObject!
  virtual nsIScriptGlobalObject* GetScriptHandlingObjectInternal() const = 0;

  // Never ever call this. Only call AllowXULXBL!
  virtual bool InternalAllowXULXBL() = 0;

  /**
   * These methods should be called before and after dispatching
   * a mutation event.
   * To make this easy and painless, use the mozAutoSubtreeModified helper class.
   */
  virtual void WillDispatchMutationEvent(nsINode* aTarget) = 0;
  virtual void MutationEventDispatched(nsINode* aTarget) = 0;
  friend class mozAutoSubtreeModified;

  virtual Element* GetNameSpaceElement()
  {
    return GetRootElement();
  }

  void SetContentTypeInternal(const nsACString& aType)
  {
    mCachedEncoder = nsnull;
    mContentType = aType;
  }

  nsCString GetContentTypeInternal() const
  {
    return mContentType;
  }

  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mDocumentBaseURI;

  nsWeakPtr mDocumentLoadGroup;

  nsWeakPtr mDocumentContainer;

  nsCString mCharacterSet;
  PRInt32 mCharacterSetSource;

  // This is just a weak pointer; the parent document owns its children.
  nsIDocument* mParentDocument;

  // A reference to the element last returned from GetRootElement().
  mozilla::dom::Element* mCachedRootElement;

  // We'd like these to be nsRefPtrs, but that'd require us to include
  // additional headers that we don't want to expose.
  // The cleanup is handled by the nsDocument destructor.
  nsNodeInfoManager* mNodeInfoManager; // [STRONG]
  mozilla::css::Loader* mCSSLoader; // [STRONG]

  // The set of all object, embed, applet, video and audio elements for
  // which this is the owner document. (They might not be in the document.)
  // These are non-owning pointers, the elements are responsible for removing
  // themselves when they go away.
  nsAutoPtr<nsTHashtable<nsPtrHashKey<nsIContent> > > mFreezableElements;
  
  // The set of all links that need their status resolved.  Links must add themselves
  // to this set by calling RegisterPendingLinkUpdate when added to a document and must
  // remove themselves by calling UnregisterPendingLinkUpdate when removed from a document.
  nsTHashtable<nsPtrHashKey<mozilla::dom::Link> > mLinksToUpdate;

  // SMIL Animation Controller, lazily-initialized in GetAnimationController
  nsRefPtr<nsSMILAnimationController> mAnimationController;

  // Table of element properties for this document.
  nsPropertyTable mPropertyTable;
  nsTArray<nsAutoPtr<nsPropertyTable> > mExtraPropertyTables;

  // Compatibility mode
  nsCompatibility mCompatMode;

  // True if BIDI is enabled.
  bool mBidiEnabled;
  // True if a MathML element has ever been owned by this document.
  bool mMathMLEnabled;

  // True if this document is the initial document for a window.  This should
  // basically be true only for documents that exist in newly-opened windows or
  // documents created to satisfy a GetDocument() on a window when there's no
  // document in it.
  bool mIsInitialDocumentInWindow;

  bool mIsRegularHTML;
  bool mIsXUL;

  enum {
    eTriUnset = 0,
    eTriFalse,
    eTriTrue
  } mAllowXULXBL;

  // True if we're loaded as data and therefor has any dangerous stuff, such
  // as scripts and plugins, disabled.
  bool mLoadedAsData;

  // If true, whoever is creating the document has gotten it to the
  // point where it's safe to start layout on it.
  bool mMayStartLayout;
  
  // True iff we've ever fired a DOMTitleChanged event for this document
  bool mHaveFiredTitleChange;

  // True iff IsShowing() should be returning true
  bool mIsShowing;

  // True iff the document "page" is not hidden (i.e. currently in the
  // bfcache)
  bool mVisible;

  // True if our content viewer has been removed from the docshell
  // (it may still be displayed, but in zombie state). Form control data
  // has been saved.
  bool mRemovedFromDocShell;

  // True iff DNS prefetch is allowed for this document.  Note that if the
  // document has no window, DNS prefetch won't be performed no matter what.
  bool mAllowDNSPrefetch;
  
  // True when this document is a static clone of a normal document
  bool mIsStaticDocument;

  // True while this document is being cloned to a static document.
  bool mCreatingStaticClone;

  // True iff the document is being unlinked or deleted.
  bool mInUnlinkOrDeletion;

  // True if document has ever had script handling object.
  bool mHasHadScriptHandlingObject;

  // True if we're waiting for a before-paint event.
  bool mHavePendingPaint;

  // True if we're an SVG document being used as an image.
  bool mIsBeingUsedAsImage;

  // True is this document is synthetic : stand alone image, video, audio
  // file, etc.
  bool mIsSyntheticDocument;

  // True if this document has links whose state needs updating
  bool mHasLinksToUpdate;

  // The document's script global object, the object from which the
  // document can get its script context and scope. This is the
  // *inner* window object.
  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobalObject;

  // If mIsStaticDocument is true, mOriginalDocument points to the original
  // document.
  nsCOMPtr<nsIDocument> mOriginalDocument;

  // The bidi options for this document.  What this bitfield means is
  // defined in nsBidiUtils.h
  PRUint32 mBidiOptions;

  nsCString mContentLanguage;
private:
  nsCString mContentType;
protected:

  // The document's security info
  nsCOMPtr<nsISupports> mSecurityInfo;

  // if this document is part of a multipart document,
  // the ID can be used to distinguish it from the other parts.
  PRUint32 mPartID;
  
  // Cycle collector generation in which we're certain that this document
  // won't be collected
  PRUint32 mMarkedCCGeneration;

  nsIPresShell* mPresShell;

  nsCOMArray<nsINode> mSubtreeModifiedTargets;
  PRUint32            mSubtreeModifiedDepth;

  // If we're an external resource document, this will be non-null and will
  // point to our "display document": the one that all resource lookups should
  // go to.
  nsCOMPtr<nsIDocument> mDisplayDocument;

  PRUint32 mEventsSuppressed;

  /**
   * The number number of external scripts (ones with the src attribute) that
   * have this document as their owner and that are being evaluated right now.
   */
  PRUint32 mExternalScriptsBeingEvaluated;

  // Weak reference to mScriptGlobalObject QI:d to nsPIDOMWindow,
  // updated on every set of mSecriptGlobalObject.
  nsPIDOMWindow *mWindow;

  nsCOMPtr<nsIDocumentEncoder> mCachedEncoder;

  AnimationListenerList mAnimationFrameListeners;

  // This object allows us to evict ourself from the back/forward cache.  The
  // pointer is non-null iff we're currently in the bfcache.
  nsIBFCacheEntry *mBFCacheEntry;

  // Our base target.
  nsString mBaseTarget;

  nsCOMPtr<nsIStructuredCloneContainer> mStateObjectContainer;
  nsCOMPtr<nsIVariant> mStateObjectCached;

  PRUint8 mDefaultElementType;
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

nsresult
NS_NewSVGDocument(nsIDocument** aInstancePtrResult);

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
                  bool aLoadedAsData,
                  nsIScriptGlobalObject* aEventObject,
                  bool aSVGDocument);

// This is used only for xbl documents created from the startup cache.
// Non-cached documents are created in the same manner as xml documents.
nsresult
NS_NewXBLDocument(nsIDOMDocument** aInstancePtrResult,
                  nsIURI* aDocumentURI,
                  nsIURI* aBaseURI,
                  nsIPrincipal* aPrincipal);

nsresult
NS_NewPluginDocument(nsIDocument** aInstancePtrResult);

inline nsIDocument*
nsINode::GetOwnerDocument() const
{
  nsIDocument* ownerDoc = OwnerDoc();

  return ownerDoc != this ? ownerDoc : nsnull;
}

#endif /* nsIDocument_h___ */
