/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Dan Rosen <dr@netscape.com>
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

#ifndef nsXULDocument_h__
#define nsXULDocument_h__

#include "nsCOMPtr.h"
#include "nsElementMap.h"
#include "nsForwardReference.h"
#include "nsIArena.h"
#include "nsICSSLoader.h"
#include "nsIContent.h"
#include "nsIDOMEventCapturer.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentStyle.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMStyleSheetList.h"
#include "nsISelection.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIHTMLStyleSheet.h"
#include "nsILineBreakerFactory.h"
#include "nsINameSpaceManager.h"
#include "nsIParser.h"
#include "nsIPrincipal.h"
#include "nsIRDFDataSource.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsArray.h"
#include "nsIURI.h"
#include "nsIWordBreakerFactory.h"
#include "nsIXULDocument.h"
#include "nsIXULPrototypeDocument.h"
#include "nsRDFDOMNodeList.h"
#include "nsTime.h"
#include "nsVoidArray.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"
#include "nsIStreamLoader.h"
#include "nsIBindingManager.h"
#include "nsINodeInfo.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIFocusController.h"
#include "nsScriptLoader.h"
#include "pldhash.h"

class nsIAtom;
class nsIElementFactory;
class nsIFile;
class nsILoadGroup;
class nsIRDFResource;
class nsIRDFService;
class nsITimer;
class nsIXULContentUtils;
class nsIXULPrototypeCache;
#if 0 // XXXbe save me, scc (need NSCAP_FORWARD_DECL(nsXULPrototypeScript))
class nsIObjectInputStream;
class nsIObjectOutputStream;
class nsIXULPrototypeScript;
#else
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsXULElement.h"
#endif

struct JSObject;
struct PRLogModuleInfo;

#include "nsIFastLoadService.h"         // XXXbe temporary?

/**
 * The XUL document class
 */
class nsXULDocument : public nsIDocument,
                      public nsIXULDocument,
                      public nsIDOMXULDocument,
                      public nsIDOMDocumentEvent,
                      public nsIDOMDocumentView,
                      public nsIDOMDocumentXBL,
                      public nsIDOMDocumentRange,
                      public nsIDOMDocumentTraversal,
                      public nsIDOMNSDocument,
                      public nsIDOM3Node,
                      public nsIDOMDocumentStyle,
                      public nsIDOMEventCapturer,
                      public nsIHTMLContentContainer,
                      public nsIStreamLoaderObserver,
                      public nsSupportsWeakReference
{
public:
    nsXULDocument();
    virtual ~nsXULDocument();

    // nsISupports interface
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLOADEROBSERVER

    // nsIDocument interface
    NS_IMETHOD GetArena(nsIArena** aArena);

    NS_IMETHOD Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);

    NS_IMETHOD StartDocumentLoad(const char* aCommand,
                                 nsIChannel *channel,
                                 nsILoadGroup* aLoadGroup,
                                 nsISupports* aContainer,
                                 nsIStreamListener **aDocListener,
                                 PRBool aReset = PR_TRUE,
                                 nsIContentSink* aSink = nsnull);

    NS_IMETHOD StopDocumentLoad();

    virtual const nsString* GetDocumentTitle() const;

    NS_IMETHOD GetDocumentURL(nsIURI** aURI) const;

    NS_IMETHOD GetPrincipal(nsIPrincipal **aPrincipal);

    NS_IMETHOD AddPrincipal(nsIPrincipal *aPrincipal);

    NS_IMETHOD GetDocumentLoadGroup(nsILoadGroup **aGroup) const;

    NS_IMETHOD GetBaseURL(nsIURI*& aURL) const;

    NS_IMETHOD SetBaseURL(nsIURI *aURI);
    
    NS_IMETHOD GetBaseTarget(nsAWritableString &aBaseTarget);

    NS_IMETHOD SetBaseTarget(const nsAReadableString &aBaseTarget);

    NS_IMETHOD GetStyleSheets(nsIDOMStyleSheetList** aStyleSheets);

    NS_IMETHOD GetDocumentCharacterSet(nsAWritableString& oCharSetID);

    NS_IMETHOD SetDocumentCharacterSet(const nsAReadableString& aCharSetID);

    NS_IMETHOD GetContentLanguage(nsAWritableString& aContentLanguage) const;

#ifdef IBMBIDI
    /**
     *  Retrieve and get bidi state of the document 
     *  (set depending on presence of bidi data).
     */
    NS_IMETHOD GetBidiEnabled(PRBool* aBidiEnabled) const;
    NS_IMETHOD SetBidiEnabled(PRBool aBidiEnabled);
#endif // IBMBIDI

    NS_IMETHOD AddCharSetObserver(nsIObserver* aObserver);
    NS_IMETHOD RemoveCharSetObserver(nsIObserver* aObserver);

    NS_IMETHOD GetLineBreaker(nsILineBreaker** aResult) ;
    NS_IMETHOD SetLineBreaker(nsILineBreaker* aLineBreaker) ;
    NS_IMETHOD GetWordBreaker(nsIWordBreaker** aResult) ;
    NS_IMETHOD SetWordBreaker(nsIWordBreaker* aWordBreaker) ;

    NS_IMETHOD GetHeaderData(nsIAtom* aHeaderField,
                             nsAWritableString& aData) const;
    NS_IMETHOD SetHeaderData(nsIAtom* aheaderField,
                             const nsAReadableString& aData);

    NS_IMETHOD CreateShell(nsIPresContext* aContext,
                           nsIViewManager* aViewManager,
                           nsIStyleSet* aStyleSet,
                           nsIPresShell** aInstancePtrResult);

    virtual PRBool DeleteShell(nsIPresShell* aShell);

    virtual PRInt32 GetNumberOfShells();

    NS_IMETHOD GetShellAt(PRInt32 aIndex, nsIPresShell** aShell);

    NS_IMETHOD GetParentDocument(nsIDocument** aParent);

    NS_IMETHOD SetParentDocument(nsIDocument* aParent);

    NS_IMETHOD AddSubDocument(nsIDocument* aSubDoc);

    NS_IMETHOD GetNumberOfSubDocuments(PRInt32* aCount);

    NS_IMETHOD GetSubDocumentAt(PRInt32 aIndex, nsIDocument** aSubDoc);

    NS_IMETHOD GetRootContent(nsIContent** aRoot);

    NS_IMETHOD SetRootContent(nsIContent* aRoot);

    NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
    NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const;
    NS_IMETHOD GetChildCount(PRInt32& aCount);

    NS_IMETHOD GetNumberOfStyleSheets(PRInt32* aCount);
    NS_IMETHOD GetStyleSheetAt(PRInt32 aIndex, nsIStyleSheet** aSheet);
    NS_IMETHOD GetIndexOfStyleSheet(nsIStyleSheet* aSheet, PRInt32* aIndex);

    virtual void AddStyleSheet(nsIStyleSheet* aSheet);
    virtual void RemoveStyleSheet(nsIStyleSheet* aSheet);
    NS_IMETHOD UpdateStyleSheets(nsISupportsArray* aOldSheets, nsISupportsArray* aNewSheets);
    void AddStyleSheetToStyleSets(nsIStyleSheet* aSheet);
    void RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet);

    NS_IMETHOD InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex, PRBool aNotify);

    virtual void SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                            PRBool mDisabled);

    NS_IMETHOD GetCSSLoader(nsICSSLoader*& aLoader);

    NS_IMETHOD GetScriptGlobalObject(nsIScriptGlobalObject** aScriptGlobalObject);

    NS_IMETHOD SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject);

    NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager);

    NS_IMETHOD GetScriptLoader(nsIScriptLoader** aScriptLoader);

    NS_IMETHOD GetFocusController(nsIFocusController** aFocusController);

    virtual void AddObserver(nsIDocumentObserver* aObserver);

    virtual PRBool RemoveObserver(nsIDocumentObserver* aObserver);

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
                                PRInt32 aModType, 
                                PRInt32 aHint); // See nsStyleConsts fot hint values

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
    NS_IMETHOD AttributeWillChange(nsIContent* aChild,
                                   PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute);

    NS_IMETHOD StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule,
                                PRInt32 aHint); // See nsStyleConsts fot hint values

    NS_IMETHOD StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);

    NS_IMETHOD StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule);

    NS_IMETHOD GetSelection(nsISelection** aSelection);

    NS_IMETHOD SelectAll();

    NS_IMETHOD FindNext(const nsAReadableString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

    NS_IMETHOD FlushPendingNotifications(PRBool aFlushReflows = PR_TRUE, PRBool aUpdateViews = PR_FALSE);

    NS_IMETHOD GetAndIncrementContentID(PRInt32* aID);

    NS_IMETHOD GetBindingManager(nsIBindingManager** aResult);

    NS_IMETHOD GetNodeInfoManager(class nsINodeInfoManager *&aNodeInfoManager);

    NS_IMETHOD AddReference(void *aKey, nsISupports *aReference);
    NS_IMETHOD RemoveReference(void *aKey, nsISupports **aOldReference);

    virtual void SetDisplaySelection(PRInt8 aToggle);

    virtual PRInt8 GetDisplaySelection() const;

    NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus);
    NS_IMETHOD_(PRBool) EventCaptureRegistration(PRInt32 aCapturerIncrement);

    // nsIXMLDocument interface
    NS_IMETHOD SetDefaultStylesheets(nsIURI* aUrl);
    NS_IMETHOD SetTitle(const PRUnichar *aTitle);

    // nsIXULDocument interface
    NS_IMETHOD AddElementForID(const nsAReadableString& aID, nsIContent* aElement);
    NS_IMETHOD RemoveElementForID(const nsAReadableString& aID, nsIContent* aElement);
    NS_IMETHOD GetElementsForID(const nsAReadableString& aID, nsISupportsArray* aElements);
    NS_IMETHOD AddForwardReference(nsForwardReference* aRef);
    NS_IMETHOD ResolveForwardReferences();
    NS_IMETHOD SetMasterPrototype(nsIXULPrototypeDocument* aDocument);
    NS_IMETHOD GetMasterPrototype(nsIXULPrototypeDocument** aDocument);
    NS_IMETHOD SetCurrentPrototype(nsIXULPrototypeDocument* aDocument);
    NS_IMETHOD SetDocumentURL(nsIURI* anURL);
    NS_IMETHOD PrepareStyleSheets(nsIURI* anURL);
    NS_IMETHOD AddSubtreeToDocument(nsIContent* aElement);
    NS_IMETHOD RemoveSubtreeFromDocument(nsIContent* aElement);
    NS_IMETHOD SetTemplateBuilderFor(nsIContent* aContent, nsIXULTemplateBuilder* aBuilder);
    NS_IMETHOD GetTemplateBuilderFor(nsIContent* aContent, nsIXULTemplateBuilder** aResult);
    NS_IMETHOD OnPrototypeLoadDone();
    NS_IMETHOD OnResumeContentSink();
    
    // nsIDOMEventCapturer interface
    NS_IMETHOD    CaptureEvent(const nsAReadableString& aType);
    NS_IMETHOD    ReleaseEvent(const nsAReadableString& aType);

    // nsIDOMEventReceiver interface (yuck. inherited from nsIDOMEventCapturer)
    NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
    NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

    // nsIDOMEventTarget interface
    NS_IMETHOD AddEventListener(const nsAReadableString& aType,
                                nsIDOMEventListener* aListener,
                                PRBool aUseCapture);
    NS_IMETHOD RemoveEventListener(const nsAReadableString& aType,
                                   nsIDOMEventListener* aListener,
                                   PRBool aUseCapture);
    NS_IMETHOD DispatchEvent(nsIDOMEvent* aEvent, PRBool *_retval);

    // nsIDOMDocument interface
    NS_DECL_NSIDOMDOCUMENT

    // nsIDOMDocumentEvent interface
    NS_DECL_NSIDOMDOCUMENTEVENT

    // nsIDOMDocumentView interface
    NS_DECL_NSIDOMDOCUMENTVIEW

    // nsIDOMDocumentXBL interface
    NS_DECL_NSIDOMDOCUMENTXBL

    // nsIDOMDocumentRange interface
    NS_DECL_NSIDOMDOCUMENTRANGE

    // nsIDOMDocumentTraversal interface
    NS_DECL_NSIDOMDOCUMENTTRAVERSAL

    // nsIDOMNSDocument interface
    NS_DECL_NSIDOMNSDOCUMENT

    // nsIDOMXULDocument interface
    NS_DECL_NSIDOMXULDOCUMENT

    // nsIDOMNode interface
    NS_DECL_NSIDOMNODE

    // nsIDOM3Node interface
    NS_DECL_NSIDOM3NODE

    // nsIHTMLContentContainer interface
    NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult);
    NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult);

    static nsresult
    GetElementsByTagName(nsIContent* aContent,
                         const nsAReadableString& aTagName,
                         PRInt32 aNamespaceID,
                         nsRDFDOMNodeList* aElements);

    static nsresult
    GetFastLoadService(nsIFastLoadService** aResult)
    {
        NS_IF_ADDREF(*aResult = gFastLoadService);
        return NS_OK;
    }

    static nsresult
    AbortFastLoads();

protected:
    // Implementation methods
    friend nsresult
    NS_NewXULDocument(nsIXULDocument** aResult);

    nsresult Init(void);
    nsresult StartLayout(void);

    nsresult
    AddElementToMap(nsIContent* aElement);

    nsresult
    RemoveElementFromMap(nsIContent* aElement);

    nsresult GetPixelDimensions(nsIPresShell* aShell, PRInt32* aWidth,
                                PRInt32* aHeight);

    static PRIntn
    RemoveElementsFromMapByContent(const PRUnichar* aID,
                                   nsIContent* aElement,
                                   void* aClosure);

    static nsresult
    GetElementsByAttribute(nsIDOMNode* aNode,
                           const nsAReadableString& aAttribute,
                           const nsAReadableString& aValue,
                           nsRDFDOMNodeList* aElements);

    nsresult
    ParseTagString(const nsAReadableString& aTagName, nsIAtom*& aName,
                   nsIAtom*& aPrefix);

    void SetIsPopup(PRBool isPopup) { mIsPopup = isPopup; };

    nsresult CreateElement(nsINodeInfo *aNodeInfo, nsIContent** aResult);

    nsresult StartFastLoad();
    nsresult EndFastLoad();

    static nsIFastLoadService*  gFastLoadService;
    static nsIFile*             gFastLoadFile;
    static PRBool               gFastLoadDone;
    static nsXULDocument*       gFastLoadList;

    void RemoveFromFastLoadList() {
        nsXULDocument** docp = &gFastLoadList;
        nsXULDocument* doc;
        while ((doc = *docp) != nsnull) {
            if (doc == this) {
                *docp = doc->mNextFastLoad;
                doc->mNextFastLoad = nsnull;
                break;
            }
            docp = &doc->mNextFastLoad;
        }
    }

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
    nsresult ApplyPersistentAttributesToElements(nsIRDFResource* aResource, nsISupportsArray* aElements);

    nsresult
    AddElementToDocumentPre(nsIContent* aElement);

    nsresult
    AddElementToDocumentPost(nsIContent* aElement);

    nsresult
    ExecuteOnBroadcastHandlerFor(nsIContent* aBroadcaster,
                                 nsIDOMElement* aListener,
                                 nsIAtom* aAttr);

protected:
    // pseudo constants
    static PRInt32 gRefCnt;

    static nsIAtom** kIdentityAttrs[];

    static nsIRDFService* gRDFService;
    static nsIRDFResource* kNC_persist;
    static nsIRDFResource* kNC_attribute;
    static nsIRDFResource* kNC_value;

    static nsIElementFactory* gHTMLElementFactory;
    static nsIElementFactory* gXMLElementFactory;

    static nsINameSpaceManager* gNameSpaceManager;
    static PRInt32 kNameSpaceID_XUL;

    static nsIXULContentUtils* gXULUtils;
    static nsIXULPrototypeCache* gXULCache;

    static PRLogModuleInfo* gXULLog;

    static void GetElementFactory(PRInt32 aNameSpaceID, nsIElementFactory** aResult);

    nsresult
    Persist(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aAttribute);

    nsresult
    DestroyForwardReferences();

    // IMPORTANT: The ownership implicit in the following member variables has been
    // explicitly checked and set using nsCOMPtr for owning pointers and raw COM interface
    // pointers for weak (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
    // NOTE, THIS IS STILL IN PROGRESS, TALK TO PINK OR SCC BEFORE CHANGING

    nsCOMPtr<nsIArena>         mArena;
    // This always has at least one observer
    nsAutoVoidArray            mObservers;
    nsString                   mDocumentTitle;
    nsCOMPtr<nsIURI>           mDocumentURL;        // [OWNER] ??? compare with loader
    nsCOMPtr<nsIURI>           mDocumentBaseURL;
    nsWeakPtr                  mDocumentLoadGroup;  // [WEAK] leads to loader
    nsCOMPtr<nsIPrincipal>     mDocumentPrincipal;  // [OWNER]
    nsCOMPtr<nsIContent>       mRootContent;        // [OWNER]
    nsIDocument*               mParentDocument;     // [WEAK]
    nsCOMPtr<nsIDOMStyleSheetList>          mDOMStyleSheets;      // [OWNER]
    nsIScriptGlobalObject*     mScriptGlobalObject; // [WEAK]
    nsXULDocument*             mNextSrcLoadWaiter;  // [OWNER] but not COMPtr
    nsString                   mCharSetID;
    // This is set in nsPresContext::Init, which calls SetShell.
    // Since I think this is almost always done, take the 32-byte hit for
    // an nsAutoVoidArray instead of having it be a separate allocation.
    nsAutoVoidArray            mCharSetObservers;
    nsVoidArray                mStyleSheets;
    nsCOMPtr<nsISelection>  mSelection;          // [OWNER]
    PRInt8                     mDisplaySelection;
    // if we're attached to a DocumentViewImpl, we have a presshell
    nsAutoVoidArray            mPresShells;
    nsCOMPtr<nsIEventListenerManager> mListenerManager;   // [OWNER]
    nsCOMPtr<nsINameSpaceManager>     mNameSpaceManager;  // [OWNER]
    nsCOMPtr<nsIHTMLStyleSheet>       mAttrStyleSheet;    // [OWNER]
    nsCOMPtr<nsIHTMLCSSStyleSheet>    mInlineStyleSheet;  // [OWNER]
    nsCOMPtr<nsICSSLoader>            mCSSLoader;         // [OWNER]
    nsCOMPtr<nsIScriptLoader>         mScriptLoader;      // [OWNER]
    nsElementMap               mElementMap;
    nsCOMPtr<nsIRDFDataSource>          mLocalStore;
    nsCOMPtr<nsILineBreaker>            mLineBreaker;    // [OWNER] 
    nsCOMPtr<nsIWordBreaker>            mWordBreaker;    // [OWNER] 
    nsVoidArray                mSubDocuments;     // [OWNER] of subelements
    PRPackedBool               mIsPopup;
    PRPackedBool               mIsFastLoad;
    PRPackedBool               mApplyingPersitedAttrs;
    nsXULDocument*             mNextFastLoad;
    nsCOMPtr<nsIDOMXULCommandDispatcher>     mCommandDispatcher; // [OWNER] of the focus tracker

    nsCOMPtr<nsIBindingManager> mBindingManager; // [OWNER] of all bindings
    nsSupportsHashtable* mBoxObjectTable; // Box objects for content nodes. 

    // Maintains the template builders that have been attached to
    // content elements
    nsSupportsHashtable* mTemplateBuilderTable;
    
    nsVoidArray mForwardReferences;
    nsForwardReference::Phase mResolutionPhase;
    PRInt32 mNextContentID;
    PRInt32 mNumCapturers; //Number of capturing event handlers in doc.  Used to optimize event delivery.

#ifdef IBMBIDI
    PRBool mBidiEnabled;
#endif // IBMBIDI

    /*
     * XXX dr
     * ------
     * We used to have two pointers into the content model: mPopupNode and
     * mTooltipNode, which were used to retrieve the objects triggering a
     * popup or tooltip. You need that access because your reference has
     * disappeared by the time you click on a popup item or do whatever
     * with a tooltip. These were owning references (no cycles, as pinkerton
     * pointed out, since we're still parent-child).
     *
     * We still have mTooltipNode, but mPopupNode has moved to the
     * FocusController. The APIs (IDL attributes popupNode and tooltipNode)
     * are still here for compatibility and ease of use, but we should
     * probably move the mTooltipNode over to FocusController at some point
     * as well, for consistency.
     */

    nsCOMPtr<nsIDOMNode>    mTooltipNode;          // [OWNER] element triggering the tooltip
    nsCOMPtr<nsINodeInfoManager> mNodeInfoManager; // [OWNER] list of names in the document

    nsWeakPtr mFocusController;

    /**
     * Context stack, which maintains the state of the Builder and allows
     * it to be interrupted.
     */
    class ContextStack {
    protected:
        struct Entry {
            nsXULPrototypeElement* mPrototype;
            nsIContent*            mElement;
            PRInt32                mIndex;
            Entry*                 mNext;
        };

        Entry* mTop;
        PRInt32 mDepth;

    public:
        ContextStack();
        ~ContextStack();

        PRInt32 Depth() { return mDepth; }

        nsresult Push(nsXULPrototypeElement* aPrototype, nsIContent* aElement);
        nsresult Pop();
        nsresult Peek(nsXULPrototypeElement** aPrototype, nsIContent** aElement, PRInt32* aIndex);

        nsresult SetTopIndex(PRInt32 aIndex);

        PRBool IsInsideXULTemplate();
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
     */
    nsCOMPtr<nsISupportsArray> mUnloadedOverlays;

    /**
     * Load the transcluded script at the specified URI. If the
     * prototype construction must 'block' until the load has
     * completed, aBlock will be set to true.
     */
    nsresult LoadScript(nsXULPrototypeScript *aScriptProto, PRBool* aBlock);

    /**
     * Execute the precompiled script object scoped by this XUL document's
     * containing window object, and using its associated script context.
     */
    nsresult ExecuteScript(JSObject* aScriptObject);

    /**
     * Create a delegate content model element from a prototype.
     */
    nsresult CreateElement(nsXULPrototypeElement* aPrototype, nsIContent** aResult);

    /**
     * Create a temporary 'overlay' element to which content nodes
     * can be attached for later resolution.
     */
    nsresult CreateOverlayElement(nsXULPrototypeElement* aPrototype, nsIContent** aResult);

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
     * A "dummy" channel that is used as a placeholder to signal document load
     * completion.
     */
    nsCOMPtr<nsIRequest> mPlaceHolderRequest;
        
    /**
     * Create a XUL template builder on the specified node if a 'datasources'
     * attribute is present.
     */
    static nsresult
    CheckTemplateBuilder(nsIContent* aElement);

    /**
     * Do hookup for <xul:observes> tag
     */
    nsresult HookupObserver(nsIContent* aElement);

    /**
     * Add the current prototype's style sheets to the document.
     */
    nsresult AddPrototypeSheets();

    /**
     * Used to resolve broadcaster references
     */
    class BroadcasterHookup : public nsForwardReference
    {
    protected:
        nsXULDocument* mDocument;              // [WEAK]
        nsCOMPtr<nsIContent> mObservesElement; // [OWNER]
        PRBool mResolved;

    public:
        BroadcasterHookup(nsXULDocument* aDocument, nsIContent* aObservesElement) :
            mDocument(aDocument),
            mObservesElement(aObservesElement),
            mResolved(PR_FALSE) {}

        virtual ~BroadcasterHookup();

        virtual Phase GetPhase() { return eHookup; }
        virtual Result Resolve();
    };

    friend class BroadcasterHookup;


    /**
     * Used to hook up overlays
     */
    class OverlayForwardReference : public nsForwardReference
    {
    protected:
        nsXULDocument* mDocument;      // [WEAK]
        nsCOMPtr<nsIContent> mOverlay; // [OWNER]
        PRBool mResolved;

        nsresult Merge(nsIContent* aTargetNode, nsIContent* aOverlayNode);

    public:
        OverlayForwardReference(nsXULDocument* aDocument, nsIContent* aOverlay)
            : mDocument(aDocument), mOverlay(aOverlay), mResolved(PR_FALSE) {}

        virtual ~OverlayForwardReference();

        virtual Phase GetPhase() { return eConstruction; }
        virtual Result Resolve();
    };

    friend class OverlayForwardReference;


    static
    nsresult
    CheckBroadcasterHookup(nsXULDocument* aDocument,
                           nsIContent* aElement,
                           PRBool* aNeedsHookup,
                           PRBool* aDidResolve);

    static
    nsresult
    InsertElement(nsIContent* aParent, nsIContent* aChild);

    static 
    nsresult
    RemoveElement(nsIContent* aParent, nsIContent* aChild);

    /**
     * The current prototype that we are walking to construct the
     * content model.
     */
    nsCOMPtr<nsIXULPrototypeDocument> mCurrentPrototype;

    /**
     * The master document (outermost, .xul) prototype, from which
     * all subdocuments get their security principals.
     */
    nsCOMPtr<nsIXULPrototypeDocument> mMasterPrototype;

    /**
     * Owning references to all of the prototype documents that were
     * used to construct this document.
     */
    nsCOMPtr<nsISupportsArray> mPrototypes;

    /**
     * Prepare to walk the current prototype.
     */
    nsresult PrepareToWalk();

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

#if defined(DEBUG_waterson) || defined(DEBUG_hyatt)
    // timing
    nsTime mLoadStart;
#endif

    class CachedChromeStreamListener : public nsIStreamListener {
    protected:
        nsXULDocument* mDocument;
        PRPackedBool   mProtoLoaded;

        virtual ~CachedChromeStreamListener();

    public:
        CachedChromeStreamListener(nsXULDocument* aDocument, PRBool aProtoLoaded);

        NS_DECL_ISUPPORTS
        NS_DECL_NSIREQUESTOBSERVER
        NS_DECL_NSISTREAMLISTENER
    };

    friend class CachedChromeStreamListener;


    class ParserObserver : public nsIRequestObserver {
    protected:
        nsXULDocument* mDocument;
        virtual ~ParserObserver();

    public:
        ParserObserver(nsXULDocument* aDocument);

        NS_DECL_ISUPPORTS
        NS_DECL_NSIREQUESTOBSERVER
    };

    friend class ParserObserver;

    nsSupportsHashtable mContentWrapperHash;

    /**
     * A map from a broadcaster element to a list of listener elements.
     */
    PLDHashTable* mBroadcasterMap;

private:
    // helpers

};



#endif // nsXULDocument_h__
