/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XULDocument_h
#define mozilla_dom_XULDocument_h

#include "nsCOMPtr.h"
#include "nsXULPrototypeDocument.h"
#include "nsXULPrototypeCache.h"
#include "nsTArray.h"

#include "mozilla/dom/XMLDocument.h"
#include "nsForwardReference.h"
#include "nsIContent.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULDocument.h"
#include "nsCOMArray.h"
#include "nsIURI.h"
#include "nsIXULDocument.h"
#include "nsScriptLoader.h"
#include "nsIStreamListener.h"
#include "nsICSSLoaderObserver.h"

#include "mozilla/Attributes.h"

#include "js/TracingAPI.h"
#include "js/TypeDecls.h"

class nsIRDFResource;
class nsIRDFService;
class nsPIWindowRoot;
#if 0 // XXXbe save me, scc (need NSCAP_FORWARD_DECL(nsXULPrototypeScript))
class nsIObjectInputStream;
class nsIObjectOutputStream;
class nsIXULPrototypeScript;
#else
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsXULElement.h"
#endif
#include "nsURIHashKey.h"
#include "nsInterfaceHashtable.h"

struct PRLogModuleInfo;

class nsRefMapEntry : public nsStringHashKey
{
public:
  nsRefMapEntry(const nsAString& aKey) :
    nsStringHashKey(&aKey)
  {
  }
  nsRefMapEntry(const nsAString *aKey) :
    nsStringHashKey(aKey)
  {
  }
  nsRefMapEntry(const nsRefMapEntry& aOther) :
    nsStringHashKey(&aOther.GetKey())
  {
    NS_ERROR("Should never be called");
  }

  mozilla::dom::Element* GetFirstElement();
  void AppendAll(nsCOMArray<nsIContent>* aElements);
  /**
   * @return true if aElement was added, false if we failed due to OOM
   */
  bool AddElement(mozilla::dom::Element* aElement);
  /**
   * @return true if aElement was removed and it was the last content for
   * this ref, so this entry should be removed from the map
   */
  bool RemoveElement(mozilla::dom::Element* aElement);

private:
  nsSmallVoidArray mRefContentList;
};

/**
 * The XUL document class
 */

namespace mozilla {
namespace dom {

class XULDocument MOZ_FINAL : public XMLDocument,
                              public nsIXULDocument,
                              public nsIDOMXULDocument,
                              public nsIStreamLoaderObserver,
                              public nsICSSLoaderObserver,
                              public nsIOffThreadScriptReceiver
{
public:
    XULDocument();

    // nsISupports interface
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSISTREAMLOADEROBSERVER

    // nsIDocument interface
    virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) MOZ_OVERRIDE;
    virtual void ResetToURI(nsIURI *aURI, nsILoadGroup* aLoadGroup,
                            nsIPrincipal* aPrincipal) MOZ_OVERRIDE;

    virtual nsresult StartDocumentLoad(const char* aCommand,
                                       nsIChannel *channel,
                                       nsILoadGroup* aLoadGroup,
                                       nsISupports* aContainer,
                                       nsIStreamListener **aDocListener,
                                       bool aReset = true,
                                       nsIContentSink* aSink = nullptr) MOZ_OVERRIDE;

    virtual void SetContentType(const nsAString& aContentType) MOZ_OVERRIDE;

    virtual void EndLoad() MOZ_OVERRIDE;

    // nsIMutationObserver interface
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE

    // nsIXULDocument interface
    virtual void GetElementsForID(const nsAString& aID,
                                  nsCOMArray<nsIContent>& aElements) MOZ_OVERRIDE;

    NS_IMETHOD AddSubtreeToDocument(nsIContent* aContent) MOZ_OVERRIDE;
    NS_IMETHOD RemoveSubtreeFromDocument(nsIContent* aContent) MOZ_OVERRIDE;
    NS_IMETHOD SetTemplateBuilderFor(nsIContent* aContent,
                                     nsIXULTemplateBuilder* aBuilder) MOZ_OVERRIDE;
    NS_IMETHOD GetTemplateBuilderFor(nsIContent* aContent,
                                     nsIXULTemplateBuilder** aResult) MOZ_OVERRIDE;
    NS_IMETHOD OnPrototypeLoadDone(bool aResumeWalk) MOZ_OVERRIDE;
    bool OnDocumentParserError() MOZ_OVERRIDE;

    // nsINode interface overrides
    virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

    // nsIDOMNode interface
    NS_FORWARD_NSIDOMNODE_TO_NSINODE

    // nsIDOMDocument interface
    using nsDocument::CreateElement;
    using nsDocument::CreateElementNS;
    NS_FORWARD_NSIDOMDOCUMENT(XMLDocument::)
    // And explicitly import the things from nsDocument that we just shadowed
    using nsDocument::GetImplementation;
    using nsDocument::GetTitle;
    using nsDocument::SetTitle;
    using nsDocument::GetLastStyleSheetSet;
    using nsDocument::MozSetImageElement;
    using nsDocument::GetMozFullScreenElement;
    using nsIDocument::GetLocation;

    // nsDocument interface overrides
    virtual Element* GetElementById(const nsAString & elementId) MOZ_OVERRIDE;

    // nsIDOMXULDocument interface
    NS_DECL_NSIDOMXULDOCUMENT

    // nsICSSLoaderObserver
    NS_IMETHOD StyleSheetLoaded(CSSStyleSheet* aSheet,
                                bool aWasAlternate,
                                nsresult aStatus) MOZ_OVERRIDE;

    virtual void EndUpdate(nsUpdateType aUpdateType) MOZ_OVERRIDE;

    virtual bool IsDocumentRightToLeft() MOZ_OVERRIDE;

    virtual void ResetDocumentDirection() MOZ_OVERRIDE;

    virtual int GetDocumentLWTheme() MOZ_OVERRIDE;

    virtual void ResetDocumentLWTheme() MOZ_OVERRIDE { mDocLWTheme = Doc_Theme_Uninitialized; }

    NS_IMETHOD OnScriptCompileComplete(JSScript* aScript, nsresult aStatus) MOZ_OVERRIDE;

    static bool
    MatchAttribute(nsIContent* aContent,
                   int32_t aNameSpaceID,
                   nsIAtom* aAttrName,
                   void* aData);

    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULDocument, XMLDocument)

    void TraceProtos(JSTracer* aTrc, uint32_t aGCNumber);

    // WebIDL API
    already_AddRefed<nsINode> GetPopupNode();
    void SetPopupNode(nsINode* aNode);
    already_AddRefed<nsINode> GetPopupRangeParent(ErrorResult& aRv);
    int32_t GetPopupRangeOffset(ErrorResult& aRv);
    already_AddRefed<nsINode> GetTooltipNode();
    void SetTooltipNode(nsINode* aNode) { /* do nothing */ }
    nsIDOMXULCommandDispatcher* GetCommandDispatcher() const
    {
        return mCommandDispatcher;
    }
    int32_t GetWidth(ErrorResult& aRv);
    int32_t GetHeight(ErrorResult& aRv);
    already_AddRefed<nsINodeList>
      GetElementsByAttribute(const nsAString& aAttribute,
                             const nsAString& aValue);
    already_AddRefed<nsINodeList>
      GetElementsByAttributeNS(const nsAString& aNamespaceURI,
                               const nsAString& aAttribute,
                               const nsAString& aValue,
                               ErrorResult& aRv);
    void AddBroadcastListenerFor(Element& aBroadcaster, Element& aListener,
                                 const nsAString& aAttr, ErrorResult& aRv);
    void RemoveBroadcastListenerFor(Element& aBroadcaster, Element& aListener,
                                    const nsAString& aAttr);
    void Persist(const nsAString& aId, const nsAString& aAttr, ErrorResult& aRv)
    {
        aRv = Persist(aId, aAttr);
    }
    using nsDocument::GetBoxObjectFor;
    void LoadOverlay(const nsAString& aURL, nsIObserver* aObserver,
                     ErrorResult& aRv)
    {
        aRv = LoadOverlay(aURL, aObserver);
    }

protected:
    virtual ~XULDocument();

    // Implementation methods
    friend nsresult
    (::NS_NewXULDocument(nsIXULDocument** aResult));

    nsresult Init(void) MOZ_OVERRIDE;
    nsresult StartLayout(void);

    nsresult
    AddElementToRefMap(Element* aElement);
    void
    RemoveElementFromRefMap(Element* aElement);

    nsresult GetViewportSize(int32_t* aWidth, int32_t* aHeight);

    nsresult PrepareToLoad(nsISupports* aContainer,
                           const char* aCommand,
                           nsIChannel* aChannel,
                           nsILoadGroup* aLoadGroup,
                           nsIParser** aResult);

    nsresult
    PrepareToLoadPrototype(nsIURI* aURI,
                           const char* aCommand,
                           nsIPrincipal* aDocumentPrincipal,
                           nsIParser** aResult);

    nsresult 
    LoadOverlayInternal(nsIURI* aURI, bool aIsDynamic, bool* aShouldReturn,
                        bool* aFailureFromContent);

    nsresult ApplyPersistentAttributes();
    nsresult ApplyPersistentAttributesInternal();
    nsresult ApplyPersistentAttributesToElements(nsIRDFResource* aResource,
                                                 nsCOMArray<nsIContent>& aElements);

    nsresult
    AddElementToDocumentPre(Element* aElement);

    nsresult
    AddElementToDocumentPost(Element* aElement);

    nsresult
    ExecuteOnBroadcastHandlerFor(Element* aBroadcaster,
                                 Element* aListener,
                                 nsIAtom* aAttr);

    nsresult
    BroadcastAttributeChangeFromOverlay(nsIContent* aNode,
                                        int32_t aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        nsIAtom* aPrefix,
                                        const nsAString& aValue);

    already_AddRefed<nsPIWindowRoot> GetWindowRoot();

    static void DirectionChanged(const char* aPrefName, void* aData);

    // pseudo constants
    static int32_t gRefCnt;

    static nsIAtom** kIdentityAttrs[];

    static nsIRDFService* gRDFService;
    static nsIRDFResource* kNC_persist;
    static nsIRDFResource* kNC_attribute;
    static nsIRDFResource* kNC_value;

    static PRLogModuleInfo* gXULLog;

    nsresult
    Persist(nsIContent* aElement, int32_t aNameSpaceID, nsIAtom* aAttribute);

    virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

    // IMPORTANT: The ownership implicit in the following member
    // variables has been explicitly checked and set using nsCOMPtr
    // for owning pointers and raw COM interface pointers for weak
    // (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
    // NOTE, THIS IS STILL IN PROGRESS, TALK TO PINK OR SCC BEFORE
    // CHANGING

    XULDocument*             mNextSrcLoadWaiter;  // [OWNER] but not COMPtr

    // Tracks elements with a 'ref' attribute, or an 'id' attribute where
    // the element's namespace has no registered ID attribute name.
    nsTHashtable<nsRefMapEntry> mRefMap;
    nsCOMPtr<nsIRDFDataSource> mLocalStore;
    bool                       mApplyingPersistedAttrs;
    bool                       mIsWritingFastLoad;
    bool                       mDocumentLoaded;
    /**
     * Since ResumeWalk is interruptible, it's possible that last
     * stylesheet finishes loading while the PD walk is still in
     * progress (waiting for an overlay to finish loading).
     * mStillWalking prevents DoneLoading (and StartLayout) from being
     * called in this situation.
     */
    bool                       mStillWalking;

    /**
     * These two values control where persistent attributes get applied.
     */
    bool                           mRestrictPersistence;
    nsTHashtable<nsStringHashKey>  mPersistenceIds;

    /**
     * An array of style sheets, that will be added (preserving order) to the
     * document after all of them are loaded (in DoneWalking).
     */
    nsTArray<nsRefPtr<CSSStyleSheet>> mOverlaySheets;

    nsCOMPtr<nsIDOMXULCommandDispatcher>     mCommandDispatcher; // [OWNER] of the focus tracker

    // Maintains the template builders that have been attached to
    // content elements
    typedef nsInterfaceHashtable<nsISupportsHashKey, nsIXULTemplateBuilder>
        BuilderTable;
    BuilderTable* mTemplateBuilderTable;

    uint32_t mPendingSheets;

    /**
     * document lightweight theme for use with :-moz-lwtheme, :-moz-lwtheme-brighttext
     * and :-moz-lwtheme-darktext
     */
    DocumentTheme                         mDocLWTheme;

    /**
     * Context stack, which maintains the state of the Builder and allows
     * it to be interrupted.
     */
    class ContextStack {
    protected:
        struct Entry {
            nsXULPrototypeElement* mPrototype;
            nsIContent*            mElement;
            int32_t                mIndex;
            Entry*                 mNext;
        };

        Entry* mTop;
        int32_t mDepth;

    public:
        ContextStack();
        ~ContextStack();

        int32_t Depth() { return mDepth; }

        nsresult Push(nsXULPrototypeElement* aPrototype, nsIContent* aElement);
        nsresult Pop();
        nsresult Peek(nsXULPrototypeElement** aPrototype, nsIContent** aElement, int32_t* aIndex);

        nsresult SetTopIndex(int32_t aIndex);
    };

    friend class ContextStack;
    ContextStack mContextStack;

    enum State { eState_Master, eState_Overlay };
    State mState;

    /**
     * An array of overlay nsIURIs that have yet to be resolved. The
     * order of the array is significant: overlays at the _end_ of the
     * array are resolved before overlays earlier in the array (i.e.,
     * it is a stack).
     *
     * In the current implementation the order the overlays are loaded
     * in is as follows: first overlays from xul-overlay PIs, in the
     * same order as in the document, then the overlays from the chrome
     * registry.
     */
    nsTArray<nsCOMPtr<nsIURI> > mUnloadedOverlays;

    /**
     * Load the transcluded script at the specified URI. If the
     * prototype construction must 'block' until the load has
     * completed, aBlock will be set to true.
     */
    nsresult LoadScript(nsXULPrototypeScript *aScriptProto, bool* aBlock);

    /**
     * Execute the precompiled script object scoped by this XUL document's
     * containing window object, and using its associated script context.
     */
    nsresult ExecuteScript(nsIScriptContext *aContext,
                           JS::Handle<JSScript*> aScriptObject);

    /**
     * Helper method for the above that uses aScript to find the appropriate
     * script context and object.
     */
    nsresult ExecuteScript(nsXULPrototypeScript *aScript);

    /**
     * Create a delegate content model element from a prototype.
     * Note that the resulting content node is not bound to any tree
     */
    nsresult CreateElementFromPrototype(nsXULPrototypeElement* aPrototype,
                                        Element** aResult,
                                        bool aIsRoot);

    /**
     * Create a hook-up element to which content nodes can be attached for
     * later resolution.
     */
    nsresult CreateOverlayElement(nsXULPrototypeElement* aPrototype,
                                  Element** aResult);

    /**
     * Add attributes from the prototype to the element.
     */
    nsresult AddAttributes(nsXULPrototypeElement* aPrototype, nsIContent* aElement);

    /**
     * The prototype-script of the current transcluded script that is being
     * loaded.  For document.write('<script src="nestedwrite.js"><\/script>')
     * to work, these need to be in a stack element type, and we need to hold
     * the top of stack here.
     */
    nsXULPrototypeScript* mCurrentScriptProto;

    /**
     * Whether the current transcluded script is being compiled off thread.
     * The load event is blocked while this is in progress.
     */
    bool mOffThreadCompiling;

    /**
     * If the current transcluded script is being compiled off thread, the
     * source for that script.
     */
    jschar* mOffThreadCompileStringBuf;
    size_t mOffThreadCompileStringLength;

    /**
     * Check if a XUL template builder has already been hooked up.
     */
    static nsresult
    CheckTemplateBuilderHookup(nsIContent* aElement, bool* aNeedsHookup);

    /**
     * Create a XUL template builder on the specified node.
     */
    static nsresult
    CreateTemplateBuilder(nsIContent* aElement);

    /**
     * Add the current prototype's style sheets (currently it's just
     * style overlays from the chrome registry) to the document.
     */
    nsresult AddPrototypeSheets();


protected:
    /* Declarations related to forward references. 
     *
     * Forward references are declarations which are added to the temporary
     * list (mForwardReferences) during the document (or overlay) load and
     * are resolved later, when the document loading is almost complete.
     */

    /**
     * The list of different types of forward references to resolve. After
     * a reference is resolved, it is removed from this array (and
     * automatically deleted)
     */
    nsTArray<nsAutoPtr<nsForwardReference> > mForwardReferences;

    /** Indicates what kind of forward references are still to be processed. */
    nsForwardReference::Phase mResolutionPhase;

    /**
     * Adds aRef to the mForwardReferences array. Takes the ownership of aRef.
     */
    nsresult AddForwardReference(nsForwardReference* aRef);

    /**
     * Resolve all of the document's forward references.
     */
    nsresult ResolveForwardReferences();

    /**
     * Used to resolve broadcaster references
     */
    class BroadcasterHookup : public nsForwardReference
    {
    protected:
        XULDocument* mDocument;              // [WEAK]
        nsRefPtr<Element> mObservesElement; // [OWNER]
        bool mResolved;

    public:
        BroadcasterHookup(XULDocument* aDocument,
                          Element* aObservesElement)
            : mDocument(aDocument),
              mObservesElement(aObservesElement),
              mResolved(false)
        {
        }

        virtual ~BroadcasterHookup();

        virtual Phase GetPhase() MOZ_OVERRIDE { return eHookup; }
        virtual Result Resolve() MOZ_OVERRIDE;
    };

    friend class BroadcasterHookup;


    /**
     * Used to hook up overlays
     */
    class OverlayForwardReference : public nsForwardReference
    {
    protected:
        XULDocument* mDocument;      // [WEAK]
        nsCOMPtr<nsIContent> mOverlay; // [OWNER]
        bool mResolved;

        nsresult Merge(nsIContent* aTargetNode, nsIContent* aOverlayNode, bool aNotify);

    public:
        OverlayForwardReference(XULDocument* aDocument, nsIContent* aOverlay)
            : mDocument(aDocument), mOverlay(aOverlay), mResolved(false) {}

        virtual ~OverlayForwardReference();

        virtual Phase GetPhase() MOZ_OVERRIDE { return eConstruction; }
        virtual Result Resolve() MOZ_OVERRIDE;
    };

    friend class OverlayForwardReference;

    class TemplateBuilderHookup : public nsForwardReference
    {
    protected:
        nsCOMPtr<nsIContent> mElement; // [OWNER]

    public:
        TemplateBuilderHookup(nsIContent* aElement)
            : mElement(aElement) {}

        virtual Phase GetPhase() MOZ_OVERRIDE { return eHookup; }
        virtual Result Resolve() MOZ_OVERRIDE;
    };

    friend class TemplateBuilderHookup;

    // The out params of FindBroadcaster only have values that make sense when
    // the method returns NS_FINDBROADCASTER_FOUND.  In all other cases, the
    // values of the out params should not be relied on (though *aListener and
    // *aBroadcaster do need to be released if non-null, of course).
    nsresult
    FindBroadcaster(Element* aElement,
                    Element** aListener,
                    nsString& aBroadcasterID,
                    nsString& aAttribute,
                    Element** aBroadcaster);

    nsresult
    CheckBroadcasterHookup(Element* aElement,
                           bool* aNeedsHookup,
                           bool* aDidResolve);

    void
    SynchronizeBroadcastListener(Element *aBroadcaster,
                                 Element *aListener,
                                 const nsAString &aAttr);

    static
    nsresult
    InsertElement(nsINode* aParent, nsIContent* aChild, bool aNotify);

    static 
    nsresult
    RemoveElement(nsINode* aParent, nsINode* aChild);

    /**
     * The current prototype that we are walking to construct the
     * content model.
     */
    nsRefPtr<nsXULPrototypeDocument> mCurrentPrototype;

    /**
     * The master document (outermost, .xul) prototype, from which
     * all subdocuments get their security principals.
     */
    nsRefPtr<nsXULPrototypeDocument> mMasterPrototype;

    /**
     * Owning references to all of the prototype documents that were
     * used to construct this document.
     */
    nsTArray< nsRefPtr<nsXULPrototypeDocument> > mPrototypes;

    /**
     * Prepare to walk the current prototype.
     */
    nsresult PrepareToWalk();

    /**
     * Creates a processing instruction based on aProtoPI and inserts
     * it to the DOM (as the aIndex-th child of aParent).
     */
    nsresult
    CreateAndInsertPI(const nsXULPrototypePI* aProtoPI,
                      nsINode* aParent, uint32_t aIndex);

    /**
     * Inserts the passed <?xml-stylesheet ?> PI at the specified
     * index. Loads and applies the associated stylesheet
     * asynchronously.
     * The prototype document walk can happen before the stylesheets
     * are loaded, but the final steps in the load process (see
     * DoneWalking()) are not run before all the stylesheets are done
     * loading.
     */
    nsresult
    InsertXMLStylesheetPI(const nsXULPrototypePI* aProtoPI,
                          nsINode* aParent,
                          uint32_t aIndex,
                          nsIContent* aPINode);

    /**
     * Inserts the passed <?xul-overlay ?> PI at the specified index.
     * Schedules the referenced overlay URI for further processing.
     */
    nsresult
    InsertXULOverlayPI(const nsXULPrototypePI* aProtoPI,
                       nsINode* aParent,
                       uint32_t aIndex,
                       nsIContent* aPINode);

    /**
     * Add overlays from the chrome registry to the set of unprocessed
     * overlays still to do.
     */
    nsresult AddChromeOverlays();

    /**
     * Resume (or initiate) an interrupted (or newly prepared)
     * prototype walk.
     */
    nsresult ResumeWalk();

    /**
     * Called at the end of ResumeWalk() and from StyleSheetLoaded().
     * Expects that both the prototype document walk is complete and
     * all referenced stylesheets finished loading.
     */
    nsresult DoneWalking();

    /**
     * Report that an overlay failed to load
     * @param aURI the URI of the overlay that failed to load
     */
    void ReportMissingOverlay(nsIURI* aURI);

    class CachedChromeStreamListener : public nsIStreamListener {
    protected:
        XULDocument* mDocument;
        bool         mProtoLoaded;

        virtual ~CachedChromeStreamListener();

    public:
        CachedChromeStreamListener(XULDocument* aDocument,
                                   bool aProtoLoaded);

        NS_DECL_ISUPPORTS
        NS_DECL_NSIREQUESTOBSERVER
        NS_DECL_NSISTREAMLISTENER
    };

    friend class CachedChromeStreamListener;


    class ParserObserver : public nsIRequestObserver {
    protected:
        nsRefPtr<XULDocument> mDocument;
        nsRefPtr<nsXULPrototypeDocument> mPrototype;
        virtual ~ParserObserver();

    public:
        ParserObserver(XULDocument* aDocument,
                       nsXULPrototypeDocument* aPrototype);

        NS_DECL_ISUPPORTS
        NS_DECL_NSIREQUESTOBSERVER
    };

    friend class ParserObserver;

    /**
     * A map from a broadcaster element to a list of listener elements.
     */
    PLDHashTable* mBroadcasterMap;

    nsAutoPtr<nsInterfaceHashtable<nsURIHashKey,nsIObserver> > mOverlayLoadObservers;
    nsAutoPtr<nsInterfaceHashtable<nsURIHashKey,nsIObserver> > mPendingOverlayLoadNotifications;

    bool mInitialLayoutComplete;

    class nsDelayedBroadcastUpdate
    {
    public:
      nsDelayedBroadcastUpdate(Element* aBroadcaster,
                               Element* aListener,
                               const nsAString &aAttr)
      : mBroadcaster(aBroadcaster), mListener(aListener), mAttr(aAttr),
        mSetAttr(false), mNeedsAttrChange(false) {}

      nsDelayedBroadcastUpdate(Element* aBroadcaster,
                               Element* aListener,
                               nsIAtom* aAttrName,
                               const nsAString &aAttr,
                               bool aSetAttr,
                               bool aNeedsAttrChange)
      : mBroadcaster(aBroadcaster), mListener(aListener), mAttr(aAttr),
        mAttrName(aAttrName), mSetAttr(aSetAttr),
        mNeedsAttrChange(aNeedsAttrChange) {}

      nsDelayedBroadcastUpdate(const nsDelayedBroadcastUpdate& aOther)
      : mBroadcaster(aOther.mBroadcaster), mListener(aOther.mListener),
        mAttr(aOther.mAttr), mAttrName(aOther.mAttrName),
        mSetAttr(aOther.mSetAttr), mNeedsAttrChange(aOther.mNeedsAttrChange) {}

      nsCOMPtr<Element>       mBroadcaster;
      nsCOMPtr<Element>       mListener;
      // Note if mAttrName isn't used, this is the name of the attr, otherwise
      // this is the value of the attribute.
      nsString                mAttr;
      nsCOMPtr<nsIAtom>       mAttrName;
      bool                    mSetAttr;
      bool                    mNeedsAttrChange;

      class Comparator {
        public:
          static bool Equals(const nsDelayedBroadcastUpdate& a, const nsDelayedBroadcastUpdate& b) {
            return a.mBroadcaster == b.mBroadcaster && a.mListener == b.mListener && a.mAttrName == b.mAttrName;
          }
      };
    };

    nsTArray<nsDelayedBroadcastUpdate> mDelayedBroadcasters;
    nsTArray<nsDelayedBroadcastUpdate> mDelayedAttrChangeBroadcasts;
    bool                               mHandlingDelayedAttrChange;
    bool                               mHandlingDelayedBroadcasters;

    void MaybeBroadcast();
private:
    // helpers

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_XULDocument_h
