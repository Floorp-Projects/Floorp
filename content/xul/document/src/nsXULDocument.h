/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Contributor(s): 
 */

#ifndef nsXULDocument_h__
#define nsXULDocument_h__

#include "nsCOMPtr.h"
#include "nsElementMap.h"
#include "nsForwardReference.h"
#include "nsIArena.h"
#include "nsICSSLoader.h"
#include "nsIContent.h"
#include "nsIDOMEventCapturer.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsISelection.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIJSScriptObject.h"
#include "nsILineBreakerFactory.h"
#include "nsINameSpaceManager.h"
#include "nsIParser.h"
#include "nsIPrincipal.h"
#include "nsIRDFDataSource.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoadableDocument.h"
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

class nsIAtom;
class nsIElementFactory;
class nsIDOMStyleSheetList;
class nsILoadGroup;
class nsIRDFResource;
class nsIRDFService;
class nsITimer;
class nsIXULContentUtils;
class nsIXULPrototypeCache;
#if 0 // XXXbe save me, scc (need NSCAP_FORWARD_DECL(nsXULPrototypeScript))
class nsIXULPrototypeScript;
#else
#include "nsXULElement.h"
#endif

struct JSObject;
struct PRLogModuleInfo;

/**
 * The XUL document class
 */
class nsXULDocument : public nsIDocument,
                      public nsIXULDocument,
                      public nsIStreamLoadableDocument,
                      public nsIDOMXULDocument,
                      public nsIDOMDocumentEvent,
                      public nsIDOMDocumentView,
                      public nsIDOMDocumentXBL,
                      public nsIDOMNSDocument,
                      public nsIDOMEventCapturer,
                      public nsIJSScriptObject,
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
    virtual nsIArena* GetArena();

    NS_IMETHOD GetContentType(nsAWritableString& aContentType) const;

    NS_IMETHOD StartDocumentLoad(const char* aCommand,
                                 nsIChannel* aChannel,
                                 nsILoadGroup* aLoadGroup,
                                 nsISupports* aContainer,
                                 nsIStreamListener **aDocListener,
                                 PRBool aReset);

    NS_IMETHOD StopDocumentLoad();

    virtual const nsString* GetDocumentTitle() const;

    virtual nsIURI* GetDocumentURL() const;

    NS_IMETHOD GetPrincipal(nsIPrincipal **aPrincipal);

    NS_IMETHOD AddPrincipal(nsIPrincipal *aPrincipal);

    NS_IMETHOD GetDocumentLoadGroup(nsILoadGroup **aGroup) const;

    NS_IMETHOD GetBaseURL(nsIURI*& aURL) const;

    NS_IMETHOD GetDocumentCharacterSet(nsAWritableString& oCharSetID);

    NS_IMETHOD SetDocumentCharacterSet(const nsAReadableString& aCharSetID);

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

    virtual nsIPresShell* GetShellAt(PRInt32 aIndex);

    virtual nsIDocument* GetParentDocument();

    virtual void SetParentDocument(nsIDocument* aParent);

    virtual void AddSubDocument(nsIDocument* aSubDoc);

    virtual PRInt32 GetNumberOfSubDocuments();

    virtual nsIDocument* GetSubDocumentAt(PRInt32 aIndex);

    virtual nsIContent* GetRootContent();

    virtual void SetRootContent(nsIContent* aRoot);

    NS_IMETHOD AppendToProlog(nsIContent* aContent);
    NS_IMETHOD AppendToEpilog(nsIContent* aContent);
    NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
    NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const;
    NS_IMETHOD GetChildCount(PRInt32& aCount);

    virtual PRInt32 GetNumberOfStyleSheets();

    virtual nsIStyleSheet* GetStyleSheetAt(PRInt32 aIndex);

    virtual PRInt32 GetIndexOfStyleSheet(nsIStyleSheet* aSheet);

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

    NS_IMETHOD CreateXIF(nsAWritableString & aBuffer, nsISelection* aSelection);

    NS_IMETHOD ToXIF(nsIXIFConverter *aConverter, nsIDOMNode* aNode);

    NS_IMETHOD FlushPendingNotifications();

    NS_IMETHOD GetAndIncrementContentID(PRInt32* aID);

    NS_IMETHOD GetBindingManager(nsIBindingManager** aResult);

    NS_IMETHOD GetNodeInfoManager(class nsINodeInfoManager *&aNodeInfoManager);

    virtual void BeginConvertToXIF(nsIXIFConverter* aConverter, nsIDOMNode* aNode);

    virtual void ConvertChildrenToXIF(nsIXIFConverter* aConverter, nsIDOMNode* aNode);

    virtual void FinishConvertToXIF(nsIXIFConverter* aConverter, nsIDOMNode* aNode);

    virtual PRBool IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const;

    virtual PRBool IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const;

    virtual PRBool IsInSelection(nsISelection* aSelection, const nsIContent *aContent) const;

    virtual nsIContent* GetPrevContent(const nsIContent *aContent) const;

    virtual nsIContent* GetNextContent(const nsIContent *aContent) const;

    virtual void SetDisplaySelection(PRInt8 aToggle);

    virtual PRInt8 GetDisplaySelection() const;

    NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus);

    // nsIXMLDocument interface
    NS_IMETHOD SetDefaultStylesheets(nsIURI* aUrl);

    // nsIXULDocument interface
    NS_IMETHOD AddElementForID(const nsAReadableString& aID, nsIContent* aElement);
    NS_IMETHOD RemoveElementForID(const nsAReadableString& aID, nsIContent* aElement);
    NS_IMETHOD GetElementsForID(const nsAReadableString& aID, nsISupportsArray* aElements);
    NS_IMETHOD CreateContents(nsIContent* aElement);
    NS_IMETHOD AddContentModelBuilder(nsIRDFContentModelBuilder* aBuilder);
    NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
    NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
    NS_IMETHOD AddForwardReference(nsForwardReference* aRef);
    NS_IMETHOD ResolveForwardReferences();
    NS_IMETHOD SetMasterPrototype(nsIXULPrototypeDocument* aDocument);
    NS_IMETHOD GetMasterPrototype(nsIXULPrototypeDocument** aDocument);
    NS_IMETHOD SetCurrentPrototype(nsIXULPrototypeDocument* aDocument);
    NS_IMETHOD SetDocumentURL(nsIURI* anURL);
    NS_IMETHOD PrepareStyleSheets(nsIURI* anURL);
    
    // nsIStreamLoadableDocument interface
    NS_IMETHOD LoadFromStream(nsIInputStream& xulStream,
                              nsISupports* aContainer,
                              const char* aCommand );

    // nsIDOMEventCapturer interface
    NS_IMETHOD    CaptureEvent(const nsAReadableString& aType);
    NS_IMETHOD    ReleaseEvent(const nsAReadableString& aType);

    // nsIDOMEventReceiver interface (yuck. inherited from nsIDOMEventCapturer)
    NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
    NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);
    NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

    // nsIDOMEventTarget interface
    NS_IMETHOD AddEventListener(const nsAReadableString& aType,
                                nsIDOMEventListener* aListener,
                                PRBool aUseCapture);
    NS_IMETHOD RemoveEventListener(const nsAReadableString& aType,
                                   nsIDOMEventListener* aListener,
                                   PRBool aUseCapture);
    NS_IMETHOD DispatchEvent(nsIDOMEvent* aEvent);

    // nsIDOMDocument interface
    NS_DECL_IDOMDOCUMENT

    // nsIDOMDocumentEvent interface
    NS_DECL_IDOMDOCUMENTEVENT

    // nsIDOMDocumentView interface
    NS_DECL_IDOMDOCUMENTVIEW

    // nsIDOMDocumentXBL interface
    NS_DECL_IDOMDOCUMENTXBL

    // nsIDOMNSDocument interface
    NS_DECL_IDOMNSDOCUMENT

    // nsIDOMXULDocument interface
    NS_DECL_IDOMXULDOCUMENT

    // nsIDOMNode interface
    NS_DECL_IDOMNODE

    // nsIJSScriptObject interface
    virtual PRBool AddProperty(JSContext *aContext, JSObject *aObj, 
                            jsval aID, jsval *aVp);
    virtual PRBool DeleteProperty(JSContext *aContext, JSObject *aObj, 
                            jsval aID, jsval *aVp);
    virtual PRBool GetProperty(JSContext *aContext, JSObject *aObj, 
                            jsval aID, jsval *aVp);
    virtual PRBool SetProperty(JSContext *aContext, JSObject *aObj, 
                            jsval aID, jsval *aVp);
    virtual PRBool EnumerateProperty(JSContext *aContext, JSObject *aObj);
    virtual PRBool Resolve(JSContext *aContext, JSObject *aObj, jsval aID);
    virtual PRBool Convert(JSContext *aContext, JSObject *aObj, jsval aID);
    virtual void   Finalize(JSContext *aContext, JSObject *aObj);

    // nsIScriptObjectOwner interface
    NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

    // nsIHTMLContentContainer interface
    NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult);
    NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult);

    static nsresult
    GetElementsByTagName(nsIContent* aContent,
                         const nsAReadableString& aTagName,
                         PRInt32 aNamespaceID,
                         nsRDFDOMNodeList* aElements);

protected:
    // Implementation methods
    friend nsresult
    NS_NewXULDocument(nsIXULDocument** aResult);

    nsresult Init(void);
    nsresult StartLayout(void);

    nsresult OpenWidgetItem(nsIContent* aElement);
    nsresult CloseWidgetItem(nsIContent* aElement);
    nsresult RebuildWidgetItem(nsIContent* aElement);

    nsresult
    AddSubtreeToDocument(nsIContent* aElement);

    nsresult
    RemoveSubtreeFromDocument(nsIContent* aElement);

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

protected:
    // pseudo constants
    static PRInt32 gRefCnt;

    static nsIAtom*  kAttributeAtom;
    static nsIAtom*  kCommandUpdaterAtom;
    static nsIAtom*  kContextAtom;
    static nsIAtom*  kDataSourcesAtom;
    static nsIAtom*  kElementAtom;
    static nsIAtom*  kIdAtom;
    static nsIAtom*  kKeysetAtom;
    static nsIAtom*  kObservesAtom;
    static nsIAtom*  kOpenAtom;
    static nsIAtom*  kOverlayAtom;
    static nsIAtom*  kPersistAtom;
    static nsIAtom*  kPopupAtom;
    static nsIAtom*  kPositionAtom;
    static nsIAtom*  kInsertAfterAtom;
    static nsIAtom*  kInsertBeforeAtom;
    static nsIAtom*  kRefAtom;
    static nsIAtom*  kRuleAtom;
    static nsIAtom*  kStyleAtom;
    static nsIAtom*  kTemplateAtom;
    static nsIAtom*  kTooltipAtom;

    static nsIAtom*  kCoalesceAtom;
    static nsIAtom*  kAllowNegativesAtom;

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
    static nsIScriptSecurityManager* gScriptSecurityManager;
    static nsIPrincipal* gSystemPrincipal;

    static PRLogModuleInfo* gXULLog;

    static void GetElementFactory(PRInt32 aNameSpaceID, nsIElementFactory** aResult);

    nsIContent*
    FindContent(const nsIContent* aStartNode,
                const nsIContent* aTest1,
                const nsIContent* aTest2) const;

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
    nsVoidArray                mObservers;
    nsAutoString               mDocumentTitle;
    nsCOMPtr<nsIURI>           mDocumentURL;        // [OWNER] ??? compare with loader
    nsWeakPtr                  mDocumentLoadGroup;  // [WEAK] leads to loader
    nsCOMPtr<nsIPrincipal>     mDocumentPrincipal;  // [OWNER]
    nsCOMPtr<nsIContent>       mRootContent;        // [OWNER]
    nsIDocument*               mParentDocument;     // [WEAK]
    nsIScriptGlobalObject*     mScriptGlobalObject; // [WEAK]
    void*                      mScriptObject;       // ????
    nsXULDocument*             mNextSrcLoadWaiter;  // [OWNER] but not COMPtr
    nsString                   mCharSetID;
    nsVoidArray                mCharSetObservers;
    nsVoidArray                mStyleSheets;
    nsCOMPtr<nsISelection>  mSelection;          // [OWNER]
    PRInt8                     mDisplaySelection;
    nsVoidArray                mPresShells;
    nsCOMPtr<nsIEventListenerManager> mListenerManager;   // [OWNER]
    nsCOMPtr<nsINameSpaceManager>     mNameSpaceManager;  // [OWNER]
    nsCOMPtr<nsIHTMLStyleSheet>       mAttrStyleSheet;    // [OWNER]
    nsCOMPtr<nsIHTMLCSSStyleSheet>    mInlineStyleSheet;  // [OWNER]
    nsCOMPtr<nsICSSLoader>            mCSSLoader;         // [OWNER]
    nsElementMap               mElementMap;
    nsCOMPtr<nsISupportsArray> mBuilders;        // [OWNER] of array, elements shouldn't own this, but they do
    nsCOMPtr<nsIRDFDataSource>          mLocalStore;
    nsCOMPtr<nsILineBreaker>            mLineBreaker;    // [OWNER] 
    nsCOMPtr<nsIWordBreaker>            mWordBreaker;    // [OWNER] 
    nsString                   mCommand;
    nsVoidArray                mSubDocuments;     // [OWNER] of subelements
    PRBool                     mIsPopup;
    nsCOMPtr<nsIDOMHTMLFormElement>     mHiddenForm;   // [OWNER] of this content element
    nsCOMPtr<nsIDOMXULCommandDispatcher>     mCommandDispatcher; // [OWNER] of the focus tracker

    nsCOMPtr<nsIBindingManager> mBindingManager; // [OWNER] of all bindings
    
    nsVoidArray mForwardReferences;
    nsForwardReference::Phase mResolutionPhase;
    PRInt32 mNextContentID;

    // The following are pointers into the content model which provide access to
    // the objects triggering either a popup or a tooltip. These are marked as
    // [OWNER] only because someone could, through DOM calls, delete the object from the
    // content model while the popup/tooltip was visible. If we didn't have a reference
    // to it, the object would go away and we'd be left pointing to garbage. This
    // does not introduce cycles into the ownership model because this is still
    // parent/child ownership. Just wanted the reader to know hyatt and I had thought about
    // this (pinkerton).
    nsCOMPtr<nsIDOMNode>    mPopupNode;            // [OWNER] element triggering the popup
    nsCOMPtr<nsIDOMNode>    mTooltipNode;          // [OWNER] element triggering the tooltip
    nsCOMPtr<nsINodeInfoManager> mNodeInfoManager; // [OWNER] list of names in the document

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
	nsCOMPtr<nsIChannel> mPlaceholderChannel;
	
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
    PRBool
    IsChromeURI(nsIURI* aURI);

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

        virtual ~CachedChromeStreamListener();

    public:
        CachedChromeStreamListener(nsXULDocument* aDocument);

        NS_DECL_ISUPPORTS
        NS_DECL_NSISTREAMOBSERVER
        NS_DECL_NSISTREAMLISTENER
    };

    friend class CachedChromeStreamListener;


    class ParserObserver : public nsIStreamObserver {
    protected:
        nsXULDocument* mDocument;
        virtual ~ParserObserver();

    public:
        ParserObserver(nsXULDocument* aDocument);

        NS_DECL_ISUPPORTS
        NS_DECL_NSISTREAMOBSERVER
    };

    friend class ParserObserver;
};



#endif // nsXULDocument_h__
