/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XULDocument_h
#define mozilla_dom_XULDocument_h

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsXULPrototypeDocument.h"
#include "nsTArray.h"

#include "mozilla/dom/XMLDocument.h"
#include "mozilla/StyleSheet.h"
#include "nsForwardReference.h"
#include "nsIContent.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsCOMArray.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "nsIStreamLoader.h"
#include "nsICSSLoaderObserver.h"
#include "nsIXULStore.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/ScriptLoader.h"

#include "js/TracingAPI.h"
#include "js/TypeDecls.h"

class nsPIWindowRoot;
class nsXULPrototypeElement;
#if 0 // XXXbe save me, scc (need NSCAP_FORWARD_DECL(nsXULPrototypeScript))
class nsIObjectInputStream;
class nsIObjectOutputStream;
#else
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsXULElement.h"
#endif
#include "nsURIHashKey.h"
#include "nsInterfaceHashtable.h"

/**
 * The XUL document class
 */

// Factory function.
nsresult NS_NewXULDocument(nsIDocument** result);

namespace mozilla {
namespace dom {

class XULDocument final : public XMLDocument,
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
    virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) override;
    virtual void ResetToURI(nsIURI *aURI, nsILoadGroup* aLoadGroup,
                            nsIPrincipal* aPrincipal) override;

    virtual nsresult StartDocumentLoad(const char* aCommand,
                                       nsIChannel *channel,
                                       nsILoadGroup* aLoadGroup,
                                       nsISupports* aContainer,
                                       nsIStreamListener **aDocListener,
                                       bool aReset = true,
                                       nsIContentSink* aSink = nullptr) override;

    virtual void SetContentType(const nsAString& aContentType) override;

    virtual void EndLoad() override;

    // nsIMutationObserver interface
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

    /**
     * Notify the XUL document that a subtree has been added
     */
    nsresult AddSubtreeToDocument(nsIContent* aContent);
    /**
     * Notify the XUL document that a subtree has been removed
     */
    nsresult RemoveSubtreeFromDocument(nsIContent* aContent);
    /**
     * This is invoked whenever the prototype for this document is loaded
     * and should be walked, regardless of whether the XUL cache is
     * disabled, whether the protototype was loaded, whether the
     * prototype was loaded from the cache or created by parsing the
     * actual XUL source, etc.
     *
     * @param aResumeWalk whether this should also call ResumeWalk().
     * Sometimes the caller of OnPrototypeLoadDone resumes the walk itself
     */
    nsresult OnPrototypeLoadDone(bool aResumeWalk);
    /**
     * Callback notifying when a document could not be parsed properly.
     */
    bool OnDocumentParserError();

    // nsINode interface overrides
    virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                           bool aPreallocateChildren) const override;

    // nsICSSLoaderObserver
    NS_IMETHOD StyleSheetLoaded(mozilla::StyleSheet* aSheet,
                                bool aWasAlternate,
                                nsresult aStatus) override;

    virtual void EndUpdate() override;

    virtual bool IsDocumentRightToLeft() override;

    /**
     * Reset the document direction so that it is recomputed.
     */
    void ResetDocumentDirection();

    virtual nsIDocument::DocumentTheme GetDocumentLWTheme() override;
    virtual nsIDocument::DocumentTheme ThreadSafeGetDocumentLWTheme() const override;

    void ResetDocumentLWTheme() { mDocLWTheme = Doc_Theme_Uninitialized; }

    NS_IMETHOD OnScriptCompileComplete(JSScript* aScript, nsresult aStatus) override;

    static bool
    MatchAttribute(Element* aContent,
                   int32_t aNameSpaceID,
                   nsAtom* aAttrName,
                   void* aData);

    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULDocument, XMLDocument)

    void TraceProtos(JSTracer* aTrc);

    // WebIDL API
    already_AddRefed<nsINode> GetPopupNode();
    void SetPopupNode(nsINode* aNode);
    nsINode* GetPopupRangeParent(ErrorResult& aRv);
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
    void Persist(const nsAString& aId, const nsAString& aAttr,
                 ErrorResult& aRv);
    using nsDocument::GetBoxObjectFor;
    void LoadOverlay(const nsAString& aURL, nsIObserver* aObserver,
                     ErrorResult& aRv);

protected:
    virtual ~XULDocument();

    // Implementation methods
    friend nsresult
    (::NS_NewXULDocument(nsIDocument** aResult));

    nsresult Init(void) override;
    nsresult StartLayout(void);

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
    nsresult ApplyPersistentAttributesToElements(const nsAString &aID,
                                                 nsCOMArray<Element>& aElements);

    nsresult
    AddElementToDocumentPre(Element* aElement);

    nsresult
    AddElementToDocumentPost(Element* aElement);

    nsresult
    ExecuteOnBroadcastHandlerFor(Element* aBroadcaster,
                                 Element* aListener,
                                 nsAtom* aAttr);

    nsresult
    BroadcastAttributeChangeFromOverlay(nsIContent* aNode,
                                        int32_t aNameSpaceID,
                                        nsAtom* aAttribute,
                                        nsAtom* aPrefix,
                                        const nsAString& aValue);

    already_AddRefed<nsPIWindowRoot> GetWindowRoot();

    static void DirectionChanged(const char* aPrefName, void* aData);

    // pseudo constants
    static int32_t gRefCnt;

    static LazyLogModule gXULLog;

    nsresult
    Persist(mozilla::dom::Element* aElement,
            int32_t aNameSpaceID,
            nsAtom* aAttribute);
    // Just like Persist but ignores the return value so we can use it
    // as a runnable method.
    void DoPersist(mozilla::dom::Element* aElement,
                   int32_t aNameSpaceID,
                   nsAtom* aAttribute)
    {
        Persist(aElement, aNameSpaceID, aAttribute);
    }

    virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

    // IMPORTANT: The ownership implicit in the following member
    // variables has been explicitly checked and set using nsCOMPtr
    // for owning pointers and raw COM interface pointers for weak
    // (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
    // NOTE, THIS IS STILL IN PROGRESS, TALK TO PINK OR SCC BEFORE
    // CHANGING

    XULDocument*             mNextSrcLoadWaiter;  // [OWNER] but not COMPtr

    nsCOMPtr<nsIXULStore>       mLocalStore;
    bool                        mApplyingPersistedAttrs;
    bool                        mIsWritingFastLoad;
    bool                        mDocumentLoaded;
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
    nsTArray<RefPtr<StyleSheet>> mOverlaySheets;

    nsCOMPtr<nsIDOMXULCommandDispatcher>     mCommandDispatcher; // [OWNER] of the focus tracker

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
     * containing window object.
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
    nsresult AddAttributes(nsXULPrototypeElement* aPrototype, Element* aElement);

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
    char16_t* mOffThreadCompileStringBuf;
    size_t mOffThreadCompileStringLength;

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
        RefPtr<Element> mObservesElement; // [OWNER]
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

        virtual Phase GetPhase() override { return eHookup; }
        virtual Result Resolve() override;
    };

    friend class BroadcasterHookup;


    /**
     * Used to hook up overlays
     */
    class OverlayForwardReference : public nsForwardReference
    {
    protected:
        XULDocument* mDocument;      // [WEAK]
        nsCOMPtr<Element> mOverlay; // [OWNER]
        bool mResolved;

        nsresult Merge(Element* aTargetNode, Element* aOverlayNode, bool aNotify);

    public:
        OverlayForwardReference(XULDocument* aDocument, Element* aOverlay)
            : mDocument(aDocument), mOverlay(aOverlay), mResolved(false) {}

        virtual ~OverlayForwardReference();

        virtual Phase GetPhase() override { return eConstruction; }
        virtual Result Resolve() override;
    };

    friend class OverlayForwardReference;

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

    // FIXME: This should probably be renamed, there's nothing guaranteeing that
    // aChild is an Element as far as I can tell!
    static
    nsresult
    InsertElement(nsINode* aParent, nsIContent* aChild, bool aNotify);

    /**
     * The current prototype that we are walking to construct the
     * content model.
     */
    RefPtr<nsXULPrototypeDocument> mCurrentPrototype;

    /**
     * The master document (outermost, .xul) prototype, from which
     * all subdocuments get their security principals.
     */
    RefPtr<nsXULPrototypeDocument> mMasterPrototype;

    /**
     * Owning references to all of the prototype documents that were
     * used to construct this document.
     */
    nsTArray< RefPtr<nsXULPrototypeDocument> > mPrototypes;

    /**
     * Prepare to walk the current prototype.
     */
    nsresult PrepareToWalk();

    /**
     * Creates a processing instruction based on aProtoPI and inserts
     * it to the DOM.
     */
    nsresult
    CreateAndInsertPI(const nsXULPrototypePI* aProtoPI,
                      nsINode* aParent, nsINode* aBeforeThis);

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
                          nsINode* aBeforeThis,
                          nsIContent* aPINode);

    /**
     * Inserts the passed <?xul-overlay ?> PI at the specified index.
     * Schedules the referenced overlay URI for further processing.
     */
    nsresult
    InsertXULOverlayPI(const nsXULPrototypePI* aProtoPI,
                       nsINode* aParent,
                       nsINode* aBeforeThis,
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
        RefPtr<XULDocument> mDocument;
        bool mProtoLoaded;

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
        RefPtr<XULDocument> mDocument;
        RefPtr<nsXULPrototypeDocument> mPrototype;
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
                               nsAtom* aAttrName,
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
      RefPtr<nsAtom>       mAttrName;
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

inline mozilla::dom::XULDocument*
nsIDocument::AsXULDocument()
{
  MOZ_ASSERT(IsXULDocument());
  return static_cast<mozilla::dom::XULDocument*>(this);
}

#endif // mozilla_dom_XULDocument_h
