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
#include "nsIContent.h"
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
    void AddSubtreeToDocument(nsIContent* aContent);
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

    // nsINode interface overrides
    virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

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

    NS_IMETHOD OnScriptCompileComplete(JSScript* aScript, nsresult aStatus) override;

    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULDocument, XMLDocument)

    void TraceProtos(JSTracer* aTrc);

protected:
    virtual ~XULDocument();

    // Implementation methods
    friend nsresult
    (::NS_NewXULDocument(nsIDocument** aResult));

    nsresult Init(void) override;
    nsresult StartLayout(void);

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

    nsresult ApplyPersistentAttributes();
    nsresult ApplyPersistentAttributesInternal();
    nsresult ApplyPersistentAttributesToElements(const nsAString &aID,
                                                 nsCOMArray<Element>& aElements);

    void AddElementToDocumentPost(Element* aElement);

    static void DirectionChanged(const char* aPrefName, XULDocument* aData);

    // pseudo constants
    static int32_t gRefCnt;

    static LazyLogModule gXULLog;

    void
    Persist(mozilla::dom::Element* aElement,
            int32_t aNameSpaceID,
            nsAtom* aAttribute);

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

    uint32_t mPendingSheets;

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


protected:
    /**
     * The current prototype that we are walking to construct the
     * content model.
     */
    RefPtr<nsXULPrototypeDocument> mCurrentPrototype;

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

    bool mInitialLayoutComplete;

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
