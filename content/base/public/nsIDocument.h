/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocument_h___
#define nsIDocument_h___

#include "mozFlushType.h"                // for enum
#include "nsAutoPtr.h"                   // for member
#include "nsCOMArray.h"                  // for member
#include "nsCRT.h"                       // for NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
#include "nsCompatibility.h"             // for member
#include "nsCOMPtr.h"                    // for member
#include "nsGkAtoms.h"                   // for static class members
#include "nsIDocumentObserver.h"         // for typedef (nsUpdateType)
#include "nsILoadGroup.h"                // for member (in nsCOMPtr)
#include "nsINode.h"                     // for base class
#include "nsIScriptGlobalObject.h"       // for member (in nsCOMPtr)
#include "nsPIDOMWindow.h"               // for use in inline functions
#include "nsPropertyTable.h"             // for member
#include "nsTHashtable.h"                // for member
#include "nsWeakReference.h"
#include "mozilla/dom/DocumentBinding.h"
#include "mozilla/WeakPtr.h"
#include "Units.h"
#include "nsExpirationTracker.h"
#include "nsClassHashtable.h"

class imgIRequest;
class nsAString;
class nsBindingManager;
class nsIDocShell;
class nsDocShell;
class nsDOMNavigationTiming;
class nsFrameLoader;
class nsHTMLCSSStyleSheet;
class nsHTMLDocument;
class nsHTMLStyleSheet;
class nsIAtom;
class nsIBFCacheEntry;
class nsIChannel;
class nsIContent;
class nsIContentSink;
class nsIDocShell;
class nsIDocumentEncoder;
class nsIDocumentObserver;
class nsIDOMDocument;
class nsIDOMDocumentFragment;
class nsIDOMDocumentType;
class nsIDOMElement;
class nsIDOMNodeFilter;
class nsIDOMNodeList;
class nsIDOMXPathNSResolver;
class nsIHTMLCollection;
class nsILayoutHistoryState;
class nsILoadContext;
class nsIObjectLoadingContent;
class nsIObserver;
class nsIPresShell;
class nsIPrincipal;
class nsIRequest;
class nsIStreamListener;
class nsIStructuredCloneContainer;
class nsIStyleRule;
class nsIStyleSheet;
class nsIURI;
class nsIVariant;
class nsLocation;
class nsViewManager;
class nsPresContext;
class nsRange;
class nsScriptLoader;
class nsSMILAnimationController;
class nsStyleSet;
class nsTextNode;
class nsWindowSizes;
class nsSmallVoidArray;
class nsDOMCaretPosition;
class nsViewportInfo;
class nsIGlobalObject;
struct nsCSSSelectorList;

namespace mozilla {
class CSSStyleSheet;
class ErrorResult;
class EventStates;
class SVGAttrAnimationRuleProcessor;

namespace css {
class Loader;
class ImageLoader;
} // namespace css

namespace dom {
class AnimationTimeline;
class Attr;
class BoxObject;
class CDATASection;
class Comment;
struct CustomElementDefinition;
class DocumentFragment;
class DocumentType;
class DOMImplementation;
class DOMStringList;
class Element;
struct ElementRegistrationOptions;
class Event;
class EventTarget;
class FontFaceSet;
class FrameRequestCallback;
class ImportManager;
class OverfillCallback;
class HTMLBodyElement;
struct LifecycleCallbackArgs;
class Link;
class GlobalObject;
class NodeFilter;
class NodeIterator;
class ProcessingInstruction;
class StyleSheetList;
class SVGDocument;
class Touch;
class TouchList;
class TreeWalker;
class UndoManager;
class XPathEvaluator;
class XPathExpression;
class XPathResult;
template<typename> class OwningNonNull;
template<typename> class Sequence;

template<typename, typename> class CallbackObjectHolder;
typedef CallbackObjectHolder<NodeFilter, nsIDOMNodeFilter> NodeFilterHolder;
} // namespace dom
} // namespace mozilla

#define NS_IDOCUMENT_IID \
{ 0x42a263db, 0x6ac6, 0x40ff, \
  { 0x89, 0xe2, 0x25, 0x12, 0xe4, 0xbc, 0x2d, 0x2d } }

// Enum for requesting a particular type of document when creating a doc
enum DocumentFlavor {
  DocumentFlavorLegacyGuess, // compat with old code until made HTML5-compliant
  DocumentFlavorHTML, // HTMLDocument with HTMLness bit set to true
  DocumentFlavorSVG, // SVGDocument
  DocumentFlavorPlain, // Just a Document
};

// Document states

// RTL locale: specific to the XUL localedir attribute
#define NS_DOCUMENT_STATE_RTL_LOCALE              NS_DEFINE_EVENT_STATE_MACRO(0)
// Window activation status
#define NS_DOCUMENT_STATE_WINDOW_INACTIVE         NS_DEFINE_EVENT_STATE_MACRO(1)

// Some function forward-declarations
class nsContentList;

already_AddRefed<nsContentList>
NS_GetContentList(nsINode* aRootNode,
                  int32_t aMatchNameSpaceId,
                  const nsAString& aTagname);
//----------------------------------------------------------------------

// Document interface.  This is implemented by all document objects in
// Gecko.
class nsIDocument : public nsINode
{
  typedef mozilla::dom::GlobalObject GlobalObject;
public:
  typedef mozilla::dom::Element Element;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_IID)
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

#ifdef MOZILLA_INTERNAL_API
  nsIDocument();
#endif

  /**
   * Let the document know that we're starting to load data into it.
   * @param aCommand The parser command. Must not be null.
   *                 XXXbz It's odd to have that here.
   * @param aChannel The channel the data will come from. The channel must be
   *                 able to report its Content-Type.
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
   *              This MUST be null if the underlying document is an HTML
   *              document. Even in the XML case, please don't add new calls
   *              with non-null sink.
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
                                     nsIContentSink* aSink = nullptr) = 0;
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
   * Set the URI for the document loaded via XHR, when accessed from
   * chrome privileged script.
   */
  virtual void SetChromeXHRDocURI(nsIURI* aURI) = 0;

  /**
   * Set the base URI for the document loaded via XHR, when accessed from
   * chrome privileged script.
   */
  virtual void SetChromeXHRDocBaseURI(nsIURI* aURI) = 0;

  /**
   * Set the principal responsible for this document.
   */
  virtual void SetPrincipal(nsIPrincipal *aPrincipal) = 0;

  /**
   * Return the LoadGroup for the document. May return null.
   */
  already_AddRefed<nsILoadGroup> GetDocumentLoadGroup() const
  {
    nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
    return group.forget();
  }

  /**
   * Return the base URI for relative URIs in the document (the document uri
   * unless it's overridden by SetBaseURI, HTML <base> tags, etc.).  The
   * returned URI could be null if there is no document URI.  If the document
   * is a srcdoc document, return the parent document's base URL.
   */
  nsIURI* GetDocBaseURI() const
  {
    if (mIsSrcdocDocument && mParentDocument) {
      return mParentDocument->GetDocBaseURI();
    }
    return mDocumentBaseURI ? mDocumentBaseURI : mDocumentURI;
  }
  virtual already_AddRefed<nsIURI> GetBaseURI(bool aTryUseXHRDocBaseURI = false) const MOZ_OVERRIDE;

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

  int32_t GetDocumentCharacterSetSource() const
  {
    return mCharacterSetSource;
  }

  // This method MUST be called before SetDocumentCharacterSet if
  // you're planning to call both.
  void SetDocumentCharacterSetSource(int32_t aCharsetSource)
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
  uint32_t GetBidiOptions() const
  {
    return mBidiOptions;
  }

  /**
   * Set the bidi options for this document.  This just sets the bits;
   * callers are expected to take action as needed if they want this
   * change to actually change anything immediately.
   * @see nsBidiUtils.h
   */
  void SetBidiOptions(uint32_t aBidiOptions)
  {
    mBidiOptions = aBidiOptions;
  }

  /**
   * Get the has mixed active content loaded flag for this document.
   */
  bool GetHasMixedActiveContentLoaded()
  {
    return mHasMixedActiveContentLoaded;
  }

  /**
   * Set the has mixed active content loaded flag for this document.
   */
  void SetHasMixedActiveContentLoaded(bool aHasMixedActiveContentLoaded)
  {
    mHasMixedActiveContentLoaded = aHasMixedActiveContentLoaded;
  }

  /**
   * Get mixed active content blocked flag for this document.
   */
  bool GetHasMixedActiveContentBlocked()
  {
    return mHasMixedActiveContentBlocked;
  }

  /**
   * Set the mixed active content blocked flag for this document.
   */
  void SetHasMixedActiveContentBlocked(bool aHasMixedActiveContentBlocked)
  {
    mHasMixedActiveContentBlocked = aHasMixedActiveContentBlocked;
  }

  /**
   * Get the has mixed display content loaded flag for this document.
   */
  bool GetHasMixedDisplayContentLoaded()
  {
    return mHasMixedDisplayContentLoaded;
  }

  /**
   * Set the has mixed display content loaded flag for this document.
   */
  void SetHasMixedDisplayContentLoaded(bool aHasMixedDisplayContentLoaded)
  {
    mHasMixedDisplayContentLoaded = aHasMixedDisplayContentLoaded;
  }

  /**
   * Get mixed display content blocked flag for this document.
   */
  bool GetHasMixedDisplayContentBlocked()
  {
    return mHasMixedDisplayContentBlocked;
  }

  /**
   * Set the mixed display content blocked flag for this document.
   */
  void SetHasMixedDisplayContentBlocked(bool aHasMixedDisplayContentBlocked)
  {
    mHasMixedDisplayContentBlocked = aHasMixedDisplayContentBlocked;
  }

  /**
   * Get tracking content blocked flag for this document.
   */
  bool GetHasTrackingContentBlocked()
  {
    return mHasTrackingContentBlocked;
  }

  /**
   * Set the tracking content blocked flag for this document.
   */
  void SetHasTrackingContentBlocked(bool aHasTrackingContentBlocked)
  {
    mHasTrackingContentBlocked = aHasTrackingContentBlocked;
  }

  /**
   * Get tracking content loaded flag for this document.
   */
  bool GetHasTrackingContentLoaded()
  {
    return mHasTrackingContentLoaded;
  }

  /**
   * Set the tracking content loaded flag for this document.
   */
  void SetHasTrackingContentLoaded(bool aHasTrackingContentLoaded)
  {
    mHasTrackingContentLoaded = aHasTrackingContentLoaded;
  }

  /**
   * Get the sandbox flags for this document.
   * @see nsSandboxFlags.h for the possible flags
   */
  uint32_t GetSandboxFlags() const
  {
    return mSandboxFlags;
  }

  /**
   * Set the sandbox flags for this document.
   * @see nsSandboxFlags.h for the possible flags
   */
  void SetSandboxFlags(uint32_t sandboxFlags)
  {
    mSandboxFlags = sandboxFlags;
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
  virtual already_AddRefed<nsIPresShell> CreateShell(nsPresContext* aContext,
                                                     nsViewManager* aViewManager,
                                                     nsStyleSet* aStyleSet) = 0;
  virtual void DeleteShell() = 0;

  nsIPresShell* GetShell() const
  {
    return GetBFCacheEntry() ? nullptr : mPresShell;
  }

  void DisallowBFCaching()
  {
    NS_ASSERTION(!mBFCacheEntry, "We're already in the bfcache!");
    mBFCacheDisallowed = true;
  }

  bool IsBFCachingAllowed() const
  {
    return !mBFCacheDisallowed;
  }

  void SetBFCacheEntry(nsIBFCacheEntry* aEntry)
  {
    NS_ASSERTION(IsBFCachingAllowed() || !aEntry,
                 "You should have checked!");

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
   * Are plugins allowed in this document ?
   */
  virtual nsresult GetAllowPlugins (bool* aAllowPlugins) = 0;

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
   * Return the doctype for this document.
   */
  mozilla::dom::DocumentType* GetDoctype() const;

  /**
   * Return the root element for this document.
   */
  Element* GetRootElement() const;

  virtual nsViewportInfo GetViewportInfo(const mozilla::ScreenIntSize& aDisplaySize) = 0;

  /**
   * True iff this doc will ignore manual character encoding overrides.
   */
  virtual bool WillIgnoreCharsetOverride() {
    return true;
  }

  /**
   * Return whether the document was created by a srcdoc iframe.
   */
  bool IsSrcdocDocument() const {
    return mIsSrcdocDocument;
  }

  /**
   * Sets whether the document was created by a srcdoc iframe.
   */
  void SetIsSrcdocDocument(bool aIsSrcdocDocument) {
    mIsSrcdocDocument = aIsSrcdocDocument;
  }

  /*
   * Gets the srcdoc string from within the channel (assuming both exist).
   * Returns a void string if this isn't a srcdoc document or if
   * the channel has not been set.
   */
  nsresult GetSrcdocData(nsAString& aSrcdocData);

  bool DidDocumentOpen() {
    return mDidDocumentOpen;
  }

protected:
  virtual Element *GetRootElementInternal() const = 0;

private:
  class SelectorCacheKey
  {
    public:
      explicit SelectorCacheKey(const nsAString& aString) : mKey(aString)
      {
        MOZ_COUNT_CTOR(SelectorCacheKey);
      }

      nsString mKey;
      nsExpirationState mState;

      nsExpirationState* GetExpirationState() { return &mState; }

      ~SelectorCacheKey()
      {
        MOZ_COUNT_DTOR(SelectorCacheKey);
      }
  };

  class SelectorCacheKeyDeleter;

public:
  class SelectorCache MOZ_FINAL
    : public nsExpirationTracker<SelectorCacheKey, 4>
  {
    public:
      SelectorCache();

      // CacheList takes ownership of aSelectorList.
      void CacheList(const nsAString& aSelector, nsCSSSelectorList* aSelectorList);

      virtual void NotifyExpired(SelectorCacheKey* aSelector) MOZ_OVERRIDE;

      // We do not call MarkUsed because it would just slow down lookups and
      // because we're OK expiring things after a few seconds even if they're
      // being used.  Returns whether we actually had an entry for aSelector.
      // If we have an entry and *aList is null, that indicates that aSelector
      // has already been parsed and is not a syntactically valid selector.
      bool GetList(const nsAString& aSelector, nsCSSSelectorList** aList)
      {
        return mTable.Get(aSelector, aList);
      }

      ~SelectorCache()
      {
        AgeAllGenerations();
      }

    private:
      nsClassHashtable<nsStringHashKey, nsCSSSelectorList> mTable;
  };

  SelectorCache& GetSelectorCache()
  {
    return mSelectorCache;
  }
  // Get the root <html> element, or return null if there isn't one (e.g.
  // if the root isn't <html>)
  Element* GetHtmlElement() const;
  // Returns the first child of GetHtmlContent which has the given tag,
  // or nullptr if that doesn't exist.
  Element* GetHtmlChildElement(nsIAtom* aTag);
  // Get the canonical <body> element, or return null if there isn't one (e.g.
  // if the root isn't <html> or if the <body> isn't there)
  mozilla::dom::HTMLBodyElement* GetBodyElement();
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
   * These exists to allow us to on-demand load user-agent style sheets that
   * would otherwise be loaded by nsDocumentViewer::CreateStyleSet. This allows
   * us to keep the memory used by a document's rule cascade data (the stuff in
   * its nsStyleSet's nsCSSRuleProcessors) - which can be considerable - lower
   * than it would be if we loaded all built-in user-agent style sheets up
   * front.
   *
   * By "built-in" user-agent style sheets we mean the user-agent style sheets
   * that gecko itself supplies (such as html.css and svg.css) as opposed to
   * user-agent level style sheets inserted by add-ons or the like.
   *
   * This function prepends the given style sheet to the document's style set
   * in order to make sure that it does not override user-agent style sheets
   * supplied by add-ons or by the app (Firefox OS or Firefox Mobile, for
   * example), since their sheets should override built-in sheets.
   *
   * TODO We can get rid of the whole concept of delayed loading if we fix
   * bug 77999.
   */
  virtual void EnsureOnDemandBuiltInUASheet(mozilla::CSSStyleSheet* aSheet) = 0;

  /**
   * Get the number of (document) stylesheets
   *
   * @return the number of stylesheets
   * @throws no exceptions
   */
  virtual int32_t GetNumberOfStyleSheets() const = 0;
  
  /**
   * Get a particular stylesheet
   * @param aIndex the index the stylesheet lives at.  This is zero-based
   * @return the stylesheet at aIndex.  Null if aIndex is out of range.
   * @throws no exceptions
   */
  virtual nsIStyleSheet* GetStyleSheetAt(int32_t aIndex) const = 0;
  
  /**
   * Insert a sheet at a particular spot in the stylesheet list (zero-based)
   * @param aSheet the sheet to insert
   * @param aIndex the index to insert at.  This index will be
   *   adjusted for the "special" sheets.
   * @throws no exceptions
   */
  virtual void InsertStyleSheetAt(nsIStyleSheet* aSheet, int32_t aIndex) = 0;

  /**
   * Get the index of a particular stylesheet.  This will _always_
   * consider the "special" sheets as part of the sheet list.
   * @param aSheet the sheet to get the index of
   * @return aIndex the index of the sheet in the full list
   */
  virtual int32_t GetIndexOfStyleSheet(nsIStyleSheet* aSheet) const = 0;

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

  enum additionalSheetType {
    eAgentSheet,
    eUserSheet,
    eAuthorSheet,
    SheetTypeCount
  };

  virtual nsresult LoadAdditionalStyleSheet(additionalSheetType aType, nsIURI* aSheetURI) = 0;
  virtual nsresult AddAdditionalStyleSheet(additionalSheetType aType, nsIStyleSheet* aSheet) = 0;
  virtual void RemoveAdditionalStyleSheet(additionalSheetType aType, nsIURI* sheetURI) = 0;
  virtual nsIStyleSheet* FirstAdditionalAuthorSheet() = 0;

  /**
   * Get this document's CSSLoader.  This is guaranteed to not return null.
   */
  mozilla::css::Loader* CSSLoader() const {
    return mCSSLoader;
  }

  /**
   * Get this document's StyleImageLoader.  This is guaranteed to not return null.
   */
  mozilla::css::ImageLoader* StyleImageLoader() const {
    return mStyleImageLoader;
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
  nsHTMLStyleSheet* GetAttributeStyleSheet() const {
    return mAttrStyleSheet;
  }

  /**
   * Get this document's inline style sheet.  May return null if there
   * isn't one
   */
  nsHTMLCSSStyleSheet* GetInlineStyleSheet() const {
    return mStyleAttrStyleSheet;
  }

  /**
   * Get this document's SVG Animation rule processor.  May return null
   * if there isn't one.
   */
  mozilla::SVGAttrAnimationRuleProcessor*
  GetSVGAttrAnimationRuleProcessor() const
  {
    return mSVGAttrAnimationRuleProcessor;
  }

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
  virtual nsIGlobalObject* GetScopeObject() const = 0;
  virtual void SetScopeObject(nsIGlobalObject* aGlobal) = 0;

  /**
   * Return the window containing the document (the outer window).
   */
  nsPIDOMWindow *GetWindow() const
  {
    return mWindow ? mWindow->GetOuterWindow() : GetWindowInternal();
  }

  bool IsInBackgroundWindow() const
  {
    nsPIDOMWindow* outer = mWindow ? mWindow->GetOuterWindow() : nullptr;
    return outer && outer->IsBackground();
  }
  
  /**
   * Return the inner window used as the script compilation scope for
   * this document. If you're not absolutely sure you need this, use
   * GetWindow().
   */
  nsPIDOMWindow* GetInnerWindow() const
  {
    return mRemovedFromDocShell ? nullptr : mWindow;
  }

  /**
   * Return the outer window ID.
   */
  uint64_t OuterWindowID() const
  {
    nsPIDOMWindow *window = GetWindow();
    return window ? window->WindowID() : 0;
  }

  /**
   * Return the inner window ID.
   */
  uint64_t InnerWindowID()
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
   * Asynchronously requests that the document make aElement the fullscreen
   * element, and move into fullscreen mode. The current fullscreen element
   * (if any) is pushed onto the fullscreen element stack, and it can be
   * returned to fullscreen status by calling RestorePreviousFullScreenState().
   *
   * Note that requesting fullscreen in a document also makes the element which
   * contains this document in this document's parent document fullscreen. i.e.
   * the <iframe> or <browser> that contains this document is also mode
   * fullscreen. This happens recursively in all ancestor documents.
   */
  virtual void AsyncRequestFullScreen(Element* aElement) = 0;

  /**
   * Called when a frame in a child process has entered fullscreen or when a
   * fullscreen frame in a child process changes to another origin.
   * aFrameElement is the frame element which contains the child-process
   * fullscreen document, and aNewOrigin is the origin of the new fullscreen
   * document.
   */
  virtual nsresult RemoteFrameFullscreenChanged(nsIDOMElement* aFrameElement,
                                                const nsAString& aNewOrigin) = 0;

  /**
   * Called when a frame in a remote child document has rolled back fullscreen
   * so that all its fullscreen element stacks are empty; we must continue the
   * rollback in this parent process' doc tree branch which is fullscreen.
   * Note that only one branch of the document tree can have its documents in
   * fullscreen state at one time. We're in inconsistent state if a
   * fullscreen document has a parent and that parent isn't fullscreen. We
   * preserve this property across process boundaries.
   */
   virtual nsresult RemoteFrameFullscreenReverted() = 0;

  /**
   * Restores the previous full-screen element to full-screen status. If there
   * is no former full-screen element, this exits full-screen, moving the
   * top-level browser window out of full-screen mode.
   */
  virtual void RestorePreviousFullScreenState() = 0;

  /**
   * Returns true if this document is in full-screen mode.
   */
  virtual bool IsFullScreenDoc() = 0;

  /**
   * Returns true if this document is a fullscreen leaf document, i.e. it
   * is in fullscreen mode and has no fullscreen children.
   */
  virtual bool IsFullscreenLeaf() = 0;

  /**
   * Returns the document which is at the root of this document's branch
   * in the in-process document tree. Returns nullptr if the document isn't
   * fullscreen.
   */
  virtual nsIDocument* GetFullscreenRoot() = 0;

  /**
   * Sets the fullscreen root to aRoot. This stores a weak reference to aRoot
   * in this document.
   */
  virtual void SetFullscreenRoot(nsIDocument* aRoot) = 0;

  /**
   * Sets whether this document is approved for fullscreen mode.
   * Documents aren't approved for fullscreen until chrome has sent a
   * "fullscreen-approved" notification with a subject which is a pointer
   * to the approved document.
   */
  virtual void SetApprovedForFullscreen(bool aIsApproved) = 0;

  /**
   * Exits documents out of DOM fullscreen mode.
   *
   * If aDocument is null, all fullscreen documents in all browser windows
   * exit fullscreen.
   *
   * If aDocument is non null, all documents from aDocument's fullscreen root
   * to the fullscreen leaf exit fullscreen. 
   *
   * Note that the fullscreen leaf is the bottom-most document which is
   * fullscreen, it may have non-fullscreen child documents. The fullscreen
   * root is usually the chrome document, but if fullscreen is content-only,
   * (see the comment in nsContentUtils.h on IsFullscreenApiContentOnly())
   * the fullscreen root will be a direct child of the chrome document, and
   * there may be other branches of the same doctree that are fullscreen.
   *
   * If aRunAsync is true, fullscreen is executed asynchronously.
   *
   * Note if aDocument is not fullscreen this function has no effect, even if
   * aDocument has fullscreen ancestors.
   */
  static void ExitFullscreen(nsIDocument* aDocument, bool aRunAsync);

  virtual void RequestPointerLock(Element* aElement) = 0;

  static void UnlockPointer(nsIDocument* aDoc = nullptr);


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
  ReadyState GetReadyStateEnum()
  {
    return mReadyState;
  }

  // notify that a content node changed state.  This must happen under
  // a scriptblocker but NOT within a begin/end update.
  virtual void ContentStateChanged(nsIContent* aContent,
                                   mozilla::EventStates aStateMask) = 0;

  // Notify that a document state has changed.
  // This should only be called by callers whose state is also reflected in the
  // implementation of nsDocument::GetDocumentState.
  virtual void DocumentStatesChanged(mozilla::EventStates aStateMask) = 0;

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
  virtual void SetContainer(nsDocShell* aContainer);

  /**
   * Get the container (docshell) for this document.
   */
  virtual nsISupports* GetContainer() const;

  /**
   * Get the container's load context for this document.
   */
  nsILoadContext* GetLoadContext() const;

  /**
   * Get docshell the for this document.
   */
  nsIDocShell* GetDocShell() const;

  /**
   * Set and get XML declaration. If aVersion is null there is no declaration.
   * aStandalone takes values -1, 0 and 1 indicating respectively that there
   * was no standalone parameter in the declaration, that it was given as no,
   * or that it was given as yes.
   */
  virtual void SetXMLDeclaration(const char16_t *aVersion,
                                 const char16_t *aEncoding,
                                 const int32_t aStandalone) = 0;
  virtual void GetXMLDeclaration(nsAString& aVersion,
                                 nsAString& aEncoding,
                                 nsAString& Standalone) = 0;

  /**
   * Returns true if this is what HTML 5 calls an "HTML document" (for example
   * regular HTML document with Content-Type "text/html", image documents and
   * media documents).  Returns false for XHTML and any other documents parsed
   * by the XML parser.
   */
  bool IsHTML() const
  {
    return mType == eHTML;
  }
  bool IsHTMLOrXHTML() const
  {
    return mType == eHTML || mType == eXHTML;
  }
  bool IsXML() const
  {
    return !IsHTML();
  }
  bool IsSVG() const
  {
    return mType == eSVG;
  }
  bool IsXUL() const
  {
    return mType == eXUL;
  }
  bool IsUnstyledDocument()
  {
    return IsLoadedAsData() || IsLoadedAsInteractiveData();
  }
  bool LoadsFullXULStyleSheetUpFront()
  {
    return IsXUL() || AllowXULXBL();
  }

  virtual bool IsScriptEnabled() = 0;

  /**
   * Create an element with the specified name, prefix and namespace ID.
   */
  virtual nsresult CreateElem(const nsAString& aName, nsIAtom *aPrefix,
                              int32_t aNamespaceID,
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
   * Get the channel that failed to load and resulted in an error page, if it
   * exists. This is only relevant to error pages.
   */
  virtual nsIChannel* GetFailedChannel() const = 0;

  /**
   * Set the channel that failed to load and resulted in an error page.
   * This is only relevant to error pages.
   */
  virtual void SetFailedChannel(nsIChannel* aChannel) = 0;

  /**
   * Returns the default namespace ID used for elements created in this
   * document.
   */
  int32_t GetDefaultNamespaceID() const
  {
    return mDefaultElementType;
  }

  void DeleteAllProperties();
  void DeleteAllPropertiesFor(nsINode* aNode);

  nsPropertyTable* PropertyTable(uint16_t aCategory) {
    if (aCategory == 0)
      return &mPropertyTable;
    return GetExtraPropertyTable(aCategory);
  }
  uint32_t GetPropertyTableCount()
  { return mExtraPropertyTables.Length() + 1; }

  /**
   * Sets the ID used to identify this part of the multipart document
   */
  void SetPartID(uint32_t aID) {
    mPartID = aID;
  }

  /**
   * Return the ID used to identify this part of the multipart document
   */
  uint32_t GetPartID() const {
    return mPartID;
  }

  /**
   * Sanitize the document by resetting all input elements and forms that have
   * autocomplete=off to their default values.
   */
  virtual void Sanitize() = 0;

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

  void BlockDOMContentLoaded()
  {
    ++mBlockDOMContentLoaded;
  }

  virtual void UnblockDOMContentLoaded() = 0;

  /**
   * Notification that the page has been shown, for documents which are loaded
   * into a DOM window.  This corresponds to the completion of document load,
   * or to the page's presentation being restored into an existing DOM window.
   * This notification fires applicable DOM events to the content window.  See
   * PageTransitionEvent.webidl for a description of the |aPersisted|
   * parameter. If aDispatchStartTarget is null, the pageshow event is
   * dispatched on the ScriptGlobalObject for this document, otherwise it's
   * dispatched on aDispatchStartTarget.
   * Note: if aDispatchStartTarget isn't null, the showing state of the
   * document won't be altered.
   */
  virtual void OnPageShow(bool aPersisted,
                          mozilla::dom::EventTarget* aDispatchStartTarget) = 0;

  /**
   * Notification that the page has been hidden, for documents which are loaded
   * into a DOM window.  This corresponds to the unloading of the document, or
   * to the document's presentation being saved but removed from an existing
   * DOM window.  This notification fires applicable DOM events to the content
   * window.  See PageTransitionEvent.webidl for a description of the
   * |aPersisted| parameter. If aDispatchStartTarget is null, the pagehide
   * event is dispatched on the ScriptGlobalObject for this document,
   * otherwise it's dispatched on aDispatchStartTarget.
   * Note: if aDispatchStartTarget isn't null, the showing state of the
   * document won't be altered.
   */
  virtual void OnPageHide(bool aPersisted,
                          mozilla::dom::EventTarget* aDispatchStartTarget) = 0;

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
  virtual already_AddRefed<mozilla::dom::BoxObject>
    GetBoxObjectFor(mozilla::dom::Element* aElement,
                    mozilla::ErrorResult& aRv) = 0;

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
   * See GetAnonymousElementByAttribute on nsIDOMDocumentXBL.
   */
  virtual Element*
    GetAnonymousElementByAttribute(nsIContent* aElement,
                                   nsIAtom* aAttrName,
                                   const nsAString& aAttrValue) const = 0;

  /**
   * Helper for nsIDOMDocument::elementFromPoint implementation that allows
   * ignoring the scroll frame and/or avoiding layout flushes.
   *
   * @see nsIDOMWindowUtils::elementFromPoint
   */
  virtual Element* ElementFromPointHelper(float aX, float aY,
                                          bool aIgnoreRootScrollFrame,
                                          bool aFlushLayout) = 0;

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
  void MarkUncollectableForCCGeneration(uint32_t aGeneration)
  {
    mMarkedCCGeneration = aGeneration;
  }

  /**
   * Gets the cycle collector generation this document is marked for.
   */
  uint32_t GetMarkedCCGeneration()
  {
    return mMarkedCCGeneration;
  }

  bool IsLoadedAsData()
  {
    return mLoadedAsData;
  }

  bool IsLoadedAsInteractiveData()
  {
    return mLoadedAsInteractiveData;
  }

  bool MayStartLayout()
  {
    return mMayStartLayout;
  }

  void SetMayStartLayout(bool aMayStartLayout)
  {
    mMayStartLayout = aMayStartLayout;
  }

  already_AddRefed<nsIDocumentEncoder> GetCachedEncoder();

  void SetCachedEncoder(already_AddRefed<nsIDocumentEncoder> aEncoder);

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
                    !GetContainer() &&
                    !GetWindow(),
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
   * Return whether the document and all its ancestors are visible in the sense of
   * pageshow / hide.
   */
  bool IsVisibleConsideringAncestors() const;

  /**
   * Return true when this document is active, i.e., the active document
   * in a content viewer.  Note that this will return true for bfcached
   * documents, so this does NOT match the "active document" concept in
   * the WHATWG spec.  That would correspond to GetInnerWindow() &&
   * GetInnerWindow()->IsCurrentInnerWindow().
   */
  bool IsActive() const { return mDocumentContainer && !mRemovedFromDocShell; }

  /**
   * Register/Unregister the ActivityObserver into mActivityObservers to listen
   * the document's activity changes such as OnPageHide, visibility, activity.
   * The ActivityObserver objects can be nsIObjectLoadingContent or
   * nsIDocumentActivity or HTMLMEdiaElement.
   */
  void RegisterActivityObserver(nsISupports* aSupports);
  bool UnregisterActivityObserver(nsISupports* aSupports);
  // Enumerate all the observers in mActivityObservers by the aEnumerator.
  typedef void (* ActivityObserverEnumerator)(nsISupports*, void*);
  void EnumerateActivityObservers(ActivityObserverEnumerator aEnumerator,
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

  enum SuppressionType {
    eAnimationsOnly = 0x1,

    // Note that suppressing events also suppresses animation frames, so
    // there's no need to split out events in its own bitmask.
    eEvents = 0x3,
  };

  /**
   * Prevents user initiated events from being dispatched to the document and
   * subdocuments.
   */
  virtual void SuppressEventHandling(SuppressionType aWhat,
                                     uint32_t aIncrease = 1) = 0;

  /**
   * Unsuppress event handling.
   * @param aFireEvents If true, delayed events (focus/blur) will be fired
   *                    asynchronously.
   */
  virtual void UnsuppressEventHandlingAndFireEvents(SuppressionType aWhat,
                                                    bool aFireEvents) = 0;

  uint32_t EventHandlingSuppressed() const { return mEventsSuppressed; }
  uint32_t AnimationsPaused() const { return mAnimationsPaused; }

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
   * Returns the template content owner document that owns the content of
   * HTMLTemplateElement.
   */
  virtual nsIDocument* GetTemplateContentsOwner() = 0;

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
  CreateStaticClone(nsIDocShell* aCloneContainer);

  /**
   * If this document is a static clone, this returns the original
   * document.
   */
  nsIDocument* GetOriginalDocument()
  {
    MOZ_ASSERT(!mOriginalDocument || !mOriginalDocument->GetOriginalDocument());
    return mOriginalDocument;
  }

  /**
   * Called by nsParser to preload images. Can be removed and code moved
   * to nsPreloadURIs::PreloadURIs() in file nsParser.cpp whenever the
   * parser-module is linked with gklayout-module.  aCrossOriginAttr should
   * be a void string if the attr is not present.
   */
  virtual void MaybePreLoadImage(nsIURI* uri,
                                 const nsAString& aCrossOriginAttr) = 0;

  /**
   * Called by nsParser to preload style sheets.  Can also be merged into the
   * parser if and when the parser is merged with libgklayout.  aCrossOriginAttr
   * should be a void string if the attr is not present.
   */
  virtual void PreloadStyle(nsIURI* aURI, const nsAString& aCharset,
                            const nsAString& aCrossOriginAttr) = 0;

  /**
   * Called by the chrome registry to load style sheets.  Can be put
   * back there if and when when that module is merged with libgklayout.
   *
   * This always does a synchronous load.  If aIsAgentSheet is true,
   * it also uses the system principal and enables unsafe rules.
   * DO NOT USE FOR UNTRUSTED CONTENT.
   */
  virtual nsresult LoadChromeSheetSync(nsIURI* aURI, bool aIsAgentSheet,
                                       mozilla::CSSStyleSheet** aSheet) = 0;

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
  void SetStateObject(nsIStructuredCloneContainer *scContainer);

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
  virtual mozilla::EventStates GetDocumentState() = 0;

  virtual nsISupports* GetCurrentContentSink() = 0;

  /**
   * Register/Unregister a hostobject uri as being "owned" by this document.
   * I.e. that its lifetime is connected with this document. When the document
   * goes away it should "kill" the uri by calling
   * nsHostObjectProtocolHandler::RemoveDataEntry
   */
  virtual void RegisterHostObjectUri(const nsACString& aUri) = 0;
  virtual void UnregisterHostObjectUri(const nsACString& aUri) = 0;

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

  virtual already_AddRefed<mozilla::dom::UndoManager> GetUndoManager() = 0;

  virtual mozilla::dom::AnimationTimeline* Timeline() = 0;

  typedef mozilla::dom::CallbackObjectHolder<
    mozilla::dom::FrameRequestCallback,
    nsIFrameRequestCallback> FrameRequestCallbackHolder;
  nsresult ScheduleFrameRequestCallback(const FrameRequestCallbackHolder& aCallback,
                                        int32_t *aHandle);
  void CancelFrameRequestCallback(int32_t aHandle);

  typedef nsTArray<FrameRequestCallbackHolder> FrameRequestCallbackList;
  /**
   * Put this document's frame request callbacks into the provided
   * list, and forget about them.
   */
  void TakeFrameRequestCallbacks(FrameRequestCallbackList& aCallbacks);

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
  // If the REQUEST_DISCARD flag is passed then if the lock count is zero we
  // will request the image be discarded now (instead of waiting).
  enum { REQUEST_DISCARD = 0x1 };
  virtual nsresult RemoveImage(imgIRequest* aImage, uint32_t aFlags = 0) = 0;

  // Makes the images on this document locked/unlocked. By default, the locking
  // state is unlocked/false.
  virtual nsresult SetImageLockingState(bool aLocked) = 0;

  virtual nsresult AddPlugin(nsIObjectLoadingContent* aPlugin) = 0;
  virtual void RemovePlugin(nsIObjectLoadingContent* aPlugin) = 0;
  virtual void GetPlugins(nsTArray<nsIObjectLoadingContent*>& aPlugins) = 0;

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
  bool HasWarnedAbout(DeprecatedOperations aOperation);
  void WarnOnceAbout(DeprecatedOperations aOperation, bool asError = false);

  virtual void PostVisibilityUpdateEvent() = 0;
  
  bool IsSyntheticDocument() const { return mIsSyntheticDocument; }

  void SetNeedLayoutFlush() {
    mNeedLayoutFlush = true;
    if (mDisplayDocument) {
      mDisplayDocument->SetNeedLayoutFlush();
    }
  }

  void SetNeedStyleFlush() {
    mNeedStyleFlush = true;
    if (mDisplayDocument) {
      mDisplayDocument->SetNeedStyleFlush();
    }
  }

  // Note: nsIDocument is a sub-class of nsINode, which has a
  // SizeOfExcludingThis function.  However, because nsIDocument objects can
  // only appear at the top of the DOM tree, we have a specialized measurement
  // function which returns multiple sizes.
  virtual void DocAddSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const;
  // DocAddSizeOfIncludingThis doesn't need to be overridden by sub-classes
  // because nsIDocument inherits from nsINode;  see the comment above the
  // declaration of nsINode::SizeOfIncludingThis.
  virtual void DocAddSizeOfIncludingThis(nsWindowSizes* aWindowSizes) const;

  bool MayHaveDOMMutationObservers()
  {
    return mMayHaveDOMMutationObservers;
  }

  void SetMayHaveDOMMutationObservers()
  {
    mMayHaveDOMMutationObservers = true;
  }

  bool IsInSyncOperation()
  {
    return mInSyncOperationCount != 0;
  }

  void SetIsInSyncOperation(bool aSync)
  {
    if (aSync) {
      ++mInSyncOperationCount;
    } else {
      --mInSyncOperationCount;
    }
  }

  bool CreatingStaticClone() const
  {
    return mCreatingStaticClone;
  }

  /**
   * Creates a new element in the HTML namespace with a local name given by
   * aTag.
   */
  already_AddRefed<Element> CreateHTMLElement(nsIAtom* aTag);

  // WebIDL API
  nsIGlobalObject* GetParentObject() const
  {
    return GetScopeObject();
  }
  static already_AddRefed<nsIDocument>
    Constructor(const GlobalObject& aGlobal,
                mozilla::ErrorResult& rv);
  virtual mozilla::dom::DOMImplementation*
    GetImplementation(mozilla::ErrorResult& rv) = 0;
  void GetURL(nsString& retval) const;
  void GetDocumentURI(nsString& retval) const;
  // Return the URI for the document.
  // The returned value may differ if the document is loaded via XHR, and
  // when accessed from chrome privileged script and
  // from content privileged script for compatibility.
  void GetDocumentURIFromJS(nsString& aDocumentURI) const;
  void GetCompatMode(nsString& retval) const;
  void GetCharacterSet(nsAString& retval) const;
  // Skip GetContentType, because our NS_IMETHOD version above works fine here.
  // GetDoctype defined above
  Element* GetDocumentElement() const
  {
    return GetRootElement();
  }

  enum ElementCallbackType {
    eCreated,
    eAttached,
    eDetached,
    eAttributeChanged
  };

  /**
   * Registers an unresolved custom element that is a candidate for
   * upgrade when the definition is registered via registerElement.
   * |aTypeName| is the name of the custom element type, if it is not
   * provided, then element name is used. |aTypeName| should be provided
   * when registering a custom element that extends an existing
   * element. e.g. <button is="x-button">.
   */
  virtual nsresult RegisterUnresolvedElement(Element* aElement,
                                             nsIAtom* aTypeName = nullptr) = 0;
  virtual void EnqueueLifecycleCallback(ElementCallbackType aType,
                                        Element* aCustomElement,
                                        mozilla::dom::LifecycleCallbackArgs* aArgs = nullptr,
                                        mozilla::dom::CustomElementDefinition* aDefinition = nullptr) = 0;
  virtual void SwizzleCustomElement(Element* aElement,
                                    const nsAString& aTypeExtension,
                                    uint32_t aNamespaceID,
                                    mozilla::ErrorResult& rv) = 0;
  virtual void
    RegisterElement(JSContext* aCx, const nsAString& aName,
                    const mozilla::dom::ElementRegistrationOptions& aOptions,
                    JS::MutableHandle<JSObject*> aRetval,
                    mozilla::ErrorResult& rv) = 0;

  /**
   * In some cases, new document instances must be associated with
   * an existing web components custom element registry as specified.
   */
  virtual void UseRegistryFromDocument(nsIDocument* aDocument) = 0;

  already_AddRefed<nsContentList>
  GetElementsByTagName(const nsAString& aTagName)
  {
    return NS_GetContentList(this, kNameSpaceID_Unknown, aTagName);
  }
  already_AddRefed<nsContentList>
    GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                           const nsAString& aLocalName,
                           mozilla::ErrorResult& aResult);
  already_AddRefed<nsContentList>
    GetElementsByClassName(const nsAString& aClasses);
  // GetElementById defined above
  already_AddRefed<Element> CreateElement(const nsAString& aTagName,
                                          mozilla::ErrorResult& rv);
  already_AddRefed<Element> CreateElementNS(const nsAString& aNamespaceURI,
                                            const nsAString& aQualifiedName,
                                            mozilla::ErrorResult& rv);
  virtual already_AddRefed<Element> CreateElement(const nsAString& aTagName,
                                                  const nsAString& aTypeExtension,
                                                  mozilla::ErrorResult& rv) = 0;
  virtual already_AddRefed<Element> CreateElementNS(const nsAString& aNamespaceURI,
                                                    const nsAString& aQualifiedName,
                                                    const nsAString& aTypeExtension,
                                                    mozilla::ErrorResult& rv) = 0;
  already_AddRefed<mozilla::dom::DocumentFragment>
    CreateDocumentFragment() const;
  already_AddRefed<nsTextNode> CreateTextNode(const nsAString& aData) const;
  already_AddRefed<mozilla::dom::Comment>
    CreateComment(const nsAString& aData) const;
  already_AddRefed<mozilla::dom::ProcessingInstruction>
    CreateProcessingInstruction(const nsAString& target, const nsAString& data,
                                mozilla::ErrorResult& rv) const;
  already_AddRefed<nsINode>
    ImportNode(nsINode& aNode, bool aDeep, mozilla::ErrorResult& rv) const;
  nsINode* AdoptNode(nsINode& aNode, mozilla::ErrorResult& rv);
  already_AddRefed<mozilla::dom::Event>
    CreateEvent(const nsAString& aEventType, mozilla::ErrorResult& rv) const;
  already_AddRefed<nsRange> CreateRange(mozilla::ErrorResult& rv);
  already_AddRefed<mozilla::dom::NodeIterator>
    CreateNodeIterator(nsINode& aRoot, uint32_t aWhatToShow,
                       mozilla::dom::NodeFilter* aFilter,
                       mozilla::ErrorResult& rv) const;
  already_AddRefed<mozilla::dom::NodeIterator>
    CreateNodeIterator(nsINode& aRoot, uint32_t aWhatToShow,
                       const mozilla::dom::NodeFilterHolder& aFilter,
                       mozilla::ErrorResult& rv) const;
  already_AddRefed<mozilla::dom::TreeWalker>
    CreateTreeWalker(nsINode& aRoot, uint32_t aWhatToShow,
                     mozilla::dom::NodeFilter* aFilter, mozilla::ErrorResult& rv) const;
  already_AddRefed<mozilla::dom::TreeWalker>
    CreateTreeWalker(nsINode& aRoot, uint32_t aWhatToShow,
                     const mozilla::dom::NodeFilterHolder& aFilter,
                     mozilla::ErrorResult& rv) const;

  // Deprecated WebIDL bits
  already_AddRefed<mozilla::dom::CDATASection>
    CreateCDATASection(const nsAString& aData, mozilla::ErrorResult& rv);
  already_AddRefed<mozilla::dom::Attr>
    CreateAttribute(const nsAString& aName, mozilla::ErrorResult& rv);
  already_AddRefed<mozilla::dom::Attr>
    CreateAttributeNS(const nsAString& aNamespaceURI,
                      const nsAString& aQualifiedName,
                      mozilla::ErrorResult& rv);
  void GetInputEncoding(nsAString& aInputEncoding);
  already_AddRefed<nsLocation> GetLocation() const;
  void GetReferrer(nsAString& aReferrer) const;
  void GetLastModified(nsAString& aLastModified) const;
  void GetReadyState(nsAString& aReadyState) const;
  // Not const because otherwise the compiler can't figure out whether to call
  // this GetTitle or the nsAString version from non-const methods, since
  // neither is an exact match.
  virtual void GetTitle(nsString& aTitle) = 0;
  virtual void SetTitle(const nsAString& aTitle, mozilla::ErrorResult& rv) = 0;
  void GetDir(nsAString& aDirection) const;
  void SetDir(const nsAString& aDirection);
  nsIDOMWindow* GetDefaultView() const
  {
    return GetWindow();
  }
  Element* GetActiveElement();
  bool HasFocus(mozilla::ErrorResult& rv) const;
  // Event handlers are all on nsINode already
  bool MozSyntheticDocument() const
  {
    return IsSyntheticDocument();
  }
  Element* GetCurrentScript();
  void ReleaseCapture() const;
  virtual void MozSetImageElement(const nsAString& aImageElementId,
                                  Element* aElement) = 0;
  nsIURI* GetDocumentURIObject() const;
  // Not const because all the full-screen goop is not const
  virtual bool MozFullScreenEnabled() = 0;
  virtual Element* GetMozFullScreenElement(mozilla::ErrorResult& rv) = 0;
  bool MozFullScreen()
  {
    return IsFullScreenDoc();
  }
  void MozCancelFullScreen();
  Element* GetMozPointerLockElement();
  void MozExitPointerLock()
  {
    UnlockPointer(this);
  }
  bool Hidden() const
  {
    return mVisibilityState != mozilla::dom::VisibilityState::Visible;
  }
  bool MozHidden() // Not const because of WarnOnceAbout
  {
    WarnOnceAbout(ePrefixedVisibilityAPI);
    return Hidden();
  }
  mozilla::dom::VisibilityState VisibilityState()
  {
    return mVisibilityState;
  }
  mozilla::dom::VisibilityState MozVisibilityState()
  {
    WarnOnceAbout(ePrefixedVisibilityAPI);
    return VisibilityState();
  }
  virtual mozilla::dom::StyleSheetList* StyleSheets() = 0;
  void GetSelectedStyleSheetSet(nsAString& aSheetSet);
  virtual void SetSelectedStyleSheetSet(const nsAString& aSheetSet) = 0;
  virtual void GetLastStyleSheetSet(nsString& aSheetSet) = 0;
  void GetPreferredStyleSheetSet(nsAString& aSheetSet);
  virtual mozilla::dom::DOMStringList* StyleSheetSets() = 0;
  virtual void EnableStyleSheetsForSet(const nsAString& aSheetSet) = 0;
  Element* ElementFromPoint(float aX, float aY);

  /**
   * Retrieve the location of the caret position (DOM node and character
   * offset within that node), given a point.
   *
   * @param aX Horizontal point at which to determine the caret position, in
   *           page coordinates.
   * @param aY Vertical point at which to determine the caret position, in
   *           page coordinates.
   */
  already_AddRefed<nsDOMCaretPosition>
    CaretPositionFromPoint(float aX, float aY);

  // QuerySelector and QuerySelectorAll already defined on nsINode
  nsINodeList* GetAnonymousNodes(Element& aElement);
  Element* GetAnonymousElementByAttribute(Element& aElement,
                                          const nsAString& aAttrName,
                                          const nsAString& aAttrValue);
  Element* GetBindingParent(nsINode& aNode);
  void LoadBindingDocument(const nsAString& aURI, mozilla::ErrorResult& rv);
  mozilla::dom::XPathExpression*
    CreateExpression(const nsAString& aExpression,
                     nsIDOMXPathNSResolver* aResolver,
                     mozilla::ErrorResult& rv);
  already_AddRefed<nsIDOMXPathNSResolver>
    CreateNSResolver(nsINode* aNodeResolver, mozilla::ErrorResult& rv);
  already_AddRefed<mozilla::dom::XPathResult>
    Evaluate(JSContext* aCx, const nsAString& aExpression, nsINode* aContextNode,
             nsIDOMXPathNSResolver* aResolver, uint16_t aType,
             JS::Handle<JSObject*> aResult, mozilla::ErrorResult& rv);
  // Touch event handlers already on nsINode
  already_AddRefed<mozilla::dom::Touch>
    CreateTouch(nsIDOMWindow* aView, mozilla::dom::EventTarget* aTarget,
                int32_t aIdentifier, int32_t aPageX, int32_t aPageY,
                int32_t aScreenX, int32_t aScreenY, int32_t aClientX,
                int32_t aClientY, int32_t aRadiusX, int32_t aRadiusY,
                float aRotationAngle, float aForce);
  already_AddRefed<mozilla::dom::TouchList> CreateTouchList();
  already_AddRefed<mozilla::dom::TouchList>
    CreateTouchList(mozilla::dom::Touch& aTouch,
                    const mozilla::dom::Sequence<mozilla::dom::OwningNonNull<mozilla::dom::Touch> >& aTouches);
  already_AddRefed<mozilla::dom::TouchList>
    CreateTouchList(const mozilla::dom::Sequence<mozilla::dom::OwningNonNull<mozilla::dom::Touch> >& aTouches);

  void SetStyleSheetChangeEventsEnabled(bool aValue)
  {
    mStyleSheetChangeEventsEnabled = aValue;
  }

  bool StyleSheetChangeEventsEnabled() const
  {
    return mStyleSheetChangeEventsEnabled;
  }

  void ObsoleteSheet(nsIURI *aSheetURI, mozilla::ErrorResult& rv);

  void ObsoleteSheet(const nsAString& aSheetURI, mozilla::ErrorResult& rv);

  // ParentNode
  nsIHTMLCollection* Children();
  uint32_t ChildElementCount();

  virtual nsHTMLDocument* AsHTMLDocument() { return nullptr; }
  virtual mozilla::dom::SVGDocument* AsSVGDocument() { return nullptr; }

  // The root document of the import tree. If this document is not an import
  // this will return the document itself.
  virtual nsIDocument* MasterDocument() = 0;
  virtual void SetMasterDocument(nsIDocument* master) = 0;
  virtual bool IsMasterDocument() = 0;
  virtual mozilla::dom::ImportManager* ImportManager() = 0;
  // We keep track of the order of sub imports were added to the document.
  virtual bool HasSubImportLink(nsINode* aLink) = 0;
  virtual uint32_t IndexOfSubImportLink(nsINode* aLink) = 0;
  virtual void AddSubImportLink(nsINode* aLink) = 0;
  virtual nsINode* GetSubImportLink(uint32_t aIdx) = 0;

  /*
   * Given a node, get a weak reference to it and append that reference to
   * mBlockedTrackingNodes. Can be used later on to look up a node in it.
   * (e.g., by the UI)
   */
  void AddBlockedTrackingNode(nsINode *node)
  {
    if (!node) {
      return;
    }

    nsWeakPtr weakNode = do_GetWeakReference(node);

    if (weakNode) {
      mBlockedTrackingNodes.AppendElement(weakNode);
    }
  }

  // FontFaceSource
  mozilla::dom::FontFaceSet* GetFonts(mozilla::ErrorResult& aRv);

  bool DidFireDOMContentLoaded() const { return mDidFireDOMContentLoaded; }

private:
  uint64_t mWarnedAbout;
  SelectorCache mSelectorCache;

protected:
  ~nsIDocument();
  nsPropertyTable* GetExtraPropertyTable(uint16_t aCategory);

  // Never ever call this. Only call GetWindow!
  virtual nsPIDOMWindow *GetWindowInternal() const = 0;

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

  void SetContentTypeInternal(const nsACString& aType);

  nsCString GetContentTypeInternal() const
  {
    return mContentType;
  }

  mozilla::dom::XPathEvaluator* XPathEvaluator();

  nsCString mReferrer;
  nsString mLastModified;

  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mChromeXHRDocURI;
  nsCOMPtr<nsIURI> mDocumentBaseURI;
  nsCOMPtr<nsIURI> mChromeXHRDocBaseURI;

  nsWeakPtr mDocumentLoadGroup;

  mozilla::WeakPtr<nsDocShell> mDocumentContainer;

  nsCString mCharacterSet;
  int32_t mCharacterSetSource;

  // This is just a weak pointer; the parent document owns its children.
  nsIDocument* mParentDocument;

  // A reference to the element last returned from GetRootElement().
  mozilla::dom::Element* mCachedRootElement;

  // This is a weak reference, but we hold a strong reference to mNodeInfo,
  // which in turn holds a strong reference to this mNodeInfoManager.
  nsNodeInfoManager* mNodeInfoManager;
  nsRefPtr<mozilla::css::Loader> mCSSLoader;
  nsRefPtr<mozilla::css::ImageLoader> mStyleImageLoader;
  nsRefPtr<nsHTMLStyleSheet> mAttrStyleSheet;
  nsRefPtr<nsHTMLCSSStyleSheet> mStyleAttrStyleSheet;
  nsRefPtr<mozilla::SVGAttrAnimationRuleProcessor> mSVGAttrAnimationRuleProcessor;

  // The set of all object, embed, applet, video/audio elements or
  // nsIObjectLoadingContent or nsIDocumentActivity for which this is the
  // owner document. (They might not be in the document.)
  // These are non-owning pointers, the elements are responsible for removing
  // themselves when they go away.
  nsAutoPtr<nsTHashtable<nsPtrHashKey<nsISupports> > > mActivityObservers;

  // The set of all links that need their status resolved.  Links must add themselves
  // to this set by calling RegisterPendingLinkUpdate when added to a document and must
  // remove themselves by calling UnregisterPendingLinkUpdate when removed from a document.
  nsTHashtable<nsPtrHashKey<mozilla::dom::Link> > mLinksToUpdate;

  // SMIL Animation Controller, lazily-initialized in GetAnimationController
  nsRefPtr<nsSMILAnimationController> mAnimationController;

  // Table of element properties for this document.
  nsPropertyTable mPropertyTable;
  nsTArray<nsAutoPtr<nsPropertyTable> > mExtraPropertyTables;

  // Our cached .children collection
  nsCOMPtr<nsIHTMLCollection> mChildrenCollection;

  // Compatibility mode
  nsCompatibility mCompatMode;

  // Our readyState
  ReadyState mReadyState;

  // Our visibility state
  mozilla::dom::VisibilityState mVisibilityState;

  // True if BIDI is enabled.
  bool mBidiEnabled;
  // True if a MathML element has ever been owned by this document.
  bool mMathMLEnabled;

  // True if this document is the initial document for a window.  This should
  // basically be true only for documents that exist in newly-opened windows or
  // documents created to satisfy a GetDocument() on a window when there's no
  // document in it.
  bool mIsInitialDocumentInWindow;

  // True if we're loaded as data and therefor has any dangerous stuff, such
  // as scripts and plugins, disabled.
  bool mLoadedAsData;

  // This flag is only set in XMLDocument, for e.g. documents used in XBL. We
  // don't want animations to play in such documents, so we need to store the
  // flag here so that we can check it in nsDocument::GetAnimationController.
  bool mLoadedAsInteractiveData;

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

  // True if we're an SVG document being used as an image.
  bool mIsBeingUsedAsImage;

  // True is this document is synthetic : stand alone image, video, audio
  // file, etc.
  bool mIsSyntheticDocument;

  // True if this document has links whose state needs updating
  bool mHasLinksToUpdate;

  // True if a layout flush might not be a no-op
  bool mNeedLayoutFlush;

  // True if a style flush might not be a no-op
  bool mNeedStyleFlush;

  // True if a DOMMutationObserver is perhaps attached to a node in the document.
  bool mMayHaveDOMMutationObservers;

  // True if a document has loaded Mixed Active Script (see nsMixedContentBlocker.cpp)
  bool mHasMixedActiveContentLoaded;

  // True if a document has blocked Mixed Active Script (see nsMixedContentBlocker.cpp)
  bool mHasMixedActiveContentBlocked;

  // True if a document has loaded Mixed Display/Passive Content (see nsMixedContentBlocker.cpp)
  bool mHasMixedDisplayContentLoaded;

  // True if a document has blocked Mixed Display/Passive Content (see nsMixedContentBlocker.cpp)
  bool mHasMixedDisplayContentBlocked;

  // True if a document has blocked Tracking Content
  bool mHasTrackingContentBlocked;

  // True if a document has loaded Tracking Content
  bool mHasTrackingContentLoaded;

  // True if DisallowBFCaching has been called on this document.
  bool mBFCacheDisallowed;

  // If true, we have an input encoding.  If this is false, then the
  // document was created entirely in memory
  bool mHaveInputEncoding;

  bool mHasHadDefaultView;

  // Whether style sheet change events will be dispatched for this document
  bool mStyleSheetChangeEventsEnabled;

  // Whether the document was created by a srcdoc iframe.
  bool mIsSrcdocDocument;

  // Records whether we've done a document.open. If this is true, it's possible
  // for nodes from this document to have outdated wrappers in their wrapper
  // caches.
  bool mDidDocumentOpen;

#ifdef DEBUG
  /**
   * This is true while FlushPendingLinkUpdates executes.  Calls to
   * [Un]RegisterPendingLinkUpdate will assert when this is true.
   */
  bool mIsLinkUpdateRegistrationsForbidden;
#endif

  enum Type {
    eUnknown, // should never be used
    eHTML,
    eXHTML,
    eGenericXML,
    eSVG,
    eXUL
  };

  uint8_t mType;

  uint8_t mDefaultElementType;

  enum {
    eTriUnset = 0,
    eTriFalse,
    eTriTrue
  };

  uint8_t mAllowXULXBL;

  // The document's script global object, the object from which the
  // document can get its script context and scope. This is the
  // *inner* window object.
  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobalObject;

  // If mIsStaticDocument is true, mOriginalDocument points to the original
  // document.
  nsCOMPtr<nsIDocument> mOriginalDocument;

  // The bidi options for this document.  What this bitfield means is
  // defined in nsBidiUtils.h
  uint32_t mBidiOptions;

  // The sandbox flags on the document. These reflect the value of the sandbox attribute of the
  // associated IFRAME or CSP-protectable content, if existent. These are set at load time and
  // are immutable - see nsSandboxFlags.h for the possible flags.
  uint32_t mSandboxFlags;

  nsCString mContentLanguage;

  // The channel that got passed to nsDocument::StartDocumentLoad(), if any.
  nsCOMPtr<nsIChannel> mChannel;
private:
  nsCString mContentType;
protected:

  // The document's security info
  nsCOMPtr<nsISupports> mSecurityInfo;

  // The channel that failed to load and resulted in an error page.
  // This only applies to error pages. Might be null.
  nsCOMPtr<nsIChannel> mFailedChannel;

  // if this document is part of a multipart document,
  // the ID can be used to distinguish it from the other parts.
  uint32_t mPartID;
  
  // Cycle collector generation in which we're certain that this document
  // won't be collected
  uint32_t mMarkedCCGeneration;

  nsIPresShell* mPresShell;

  nsCOMArray<nsINode> mSubtreeModifiedTargets;
  uint32_t            mSubtreeModifiedDepth;

  // If we're an external resource document, this will be non-null and will
  // point to our "display document": the one that all resource lookups should
  // go to.
  nsCOMPtr<nsIDocument> mDisplayDocument;

  uint32_t mEventsSuppressed;

  uint32_t mAnimationsPaused;

  /**
   * The number number of external scripts (ones with the src attribute) that
   * have this document as their owner and that are being evaluated right now.
   */
  uint32_t mExternalScriptsBeingEvaluated;

  /**
   * The current frame request callback handle
   */
  int32_t mFrameRequestCallbackCounter;

  // Array of nodes that have been blocked to prevent user tracking.
  // They most likely have had their nsIChannel canceled by the URL
  // classifier. (Safebrowsing)
  //
  // Weak nsINode pointers are used to allow nodes to disappear.
  nsTArray<nsWeakPtr> mBlockedTrackingNodes;

  // Weak reference to mScriptGlobalObject QI:d to nsPIDOMWindow,
  // updated on every set of mSecriptGlobalObject.
  nsPIDOMWindow *mWindow;

  nsCOMPtr<nsIDocumentEncoder> mCachedEncoder;

  struct FrameRequest;

  nsTArray<FrameRequest> mFrameRequestCallbacks;

  // This object allows us to evict ourself from the back/forward cache.  The
  // pointer is non-null iff we're currently in the bfcache.
  nsIBFCacheEntry *mBFCacheEntry;

  // Our base target.
  nsString mBaseTarget;

  nsCOMPtr<nsIStructuredCloneContainer> mStateObjectContainer;
  nsCOMPtr<nsIVariant> mStateObjectCached;

  uint32_t mInSyncOperationCount;

  nsRefPtr<mozilla::dom::XPathEvaluator> mXPathEvaluator;

  uint32_t mBlockDOMContentLoaded;
  bool mDidFireDOMContentLoaded:1;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocument, NS_IDOCUMENT_IID)

/**
 * mozAutoSubtreeModified batches DOM mutations so that a DOMSubtreeModified
 * event is dispatched, if necessary, when the outermost mozAutoSubtreeModified
 * object is deleted.
 */
class MOZ_STACK_CLASS mozAutoSubtreeModified
{
public:
  /**
   * @param aSubTreeOwner The document in which a subtree will be modified.
   * @param aTarget       The target of the possible DOMSubtreeModified event.
   *                      Can be nullptr, in which case mozAutoSubtreeModified
   *                      is just used to batch DOM mutations.
   */
  mozAutoSubtreeModified(nsIDocument* aSubtreeOwner, nsINode* aTarget)
  {
    UpdateTarget(aSubtreeOwner, aTarget);
  }

  ~mozAutoSubtreeModified()
  {
    UpdateTarget(nullptr, nullptr);
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

class MOZ_STACK_CLASS nsAutoSyncOperation
{
public:
  explicit nsAutoSyncOperation(nsIDocument* aDocument);
  ~nsAutoSyncOperation();
private:
  nsCOMArray<nsIDocument> mDocuments;
  uint32_t                mMicroTaskLevel;
};

// XXX These belong somewhere else
nsresult
NS_NewHTMLDocument(nsIDocument** aInstancePtrResult, bool aLoadedAsData = false);

nsresult
NS_NewXMLDocument(nsIDocument** aInstancePtrResult, bool aLoadedAsData = false,
                  bool aIsPlainDocument = false);

nsresult
NS_NewSVGDocument(nsIDocument** aInstancePtrResult);

nsresult
NS_NewImageDocument(nsIDocument** aInstancePtrResult);

nsresult
NS_NewVideoDocument(nsIDocument** aInstancePtrResult);

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
                  nsIGlobalObject* aEventObject,
                  DocumentFlavor aFlavor);

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

  return ownerDoc != this ? ownerDoc : nullptr;
}

inline nsINode*
nsINode::OwnerDocAsNode() const
{
  return OwnerDoc();
}

inline mozilla::dom::ParentObject
nsINode::GetParentObject() const
{
  return GetParentObjectInternal(OwnerDoc());
}

#endif /* nsIDocument_h___ */
