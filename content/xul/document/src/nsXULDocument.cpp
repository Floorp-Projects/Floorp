/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

  An implementation for the nsIRDFDocument interface. This
  implementation serves as the basis for generating an NGLayout
  content model.

  To Do
  -----

  1. Implement DOM range constructors.

  2. Implement XIF conversion (this is really low priority).

  Notes
  -----

  1. We do some monkey business in the document observer methods to
     keep the element map in sync for HTML elements. Why don't we just
     do it for _all_ elements? Well, in the case of XUL elements,
     which may be lazily created during frame construction, the
     document observer methods will never be called because we'll be
     adding the XUL nodes into the content model "quietly".

  2. The "element map" maps an RDF resource to the elements whose 'id'
     or 'ref' attributes refer to that resource. We re-use the element
     map to support the HTML-like 'getElementById()' method.

*/

// Note the ALPHABETICAL ORDERING
#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsIArena.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsIContent.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventCapturer.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMSelection.h"
#include "nsIDOMStyleSheetCollection.h"
#include "nsIDOMText.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDTD.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsIHTMLStyleSheet.h"
#include "nsICSSLoader.h"
#include "nsIJSScriptObject.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIParser.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIContentViewer.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIEventListenerManager.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStreamLoadableDocument.h"
#include "nsIStyleContext.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsISupportsArray.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsNeckoUtil.h"
#include "nsILoadGroup.h"
#else
#include "nsIURLGroup.h"
#endif // NECKO
#include "nsIWebShell.h"
#include "nsIXMLContent.h"
#include "nsIXULChildDocument.h"
#include "nsIXULContentSink.h"
#include "nsIXULParentDocument.h"
#include "nsLayoutCID.h"
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsRDFDOMNodeList.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h" // XXX should go away
#include "plhash.h"
#include "plstr.h"
#include "prlog.h"
#include "rdfutil.h"
#include "rdf.h"

#include "nsIFrameReflow.h"
#include "nsIBrowserWindow.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIXULCommandDispatcher.h"
#include "nsIXULContent.h"
#include "nsIDOMEventCapturer.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"

#include "nsILineBreakerFactory.h"
#include "nsIWordBreakerFactory.h"
#include "nsLWBrkCIID.h"

#include "nsIInputStream.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kICSSParserIID,              NS_ICSS_PARSER_IID); // XXX grr..
static NS_DEFINE_IID(kIContentIID,                NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDTDIID,                    NS_IDTD_IID);
static NS_DEFINE_IID(kIDOMScriptObjectFactoryIID, NS_IDOM_SCRIPT_OBJECT_FACTORY_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID,        NS_IPRIVATEDOMEVENT_IID);
static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID,   NS_IHTMLCONTENTCONTAINER_IID);
static NS_DEFINE_IID(kIHTMLStyleSheetIID,         NS_IHTML_STYLE_SHEET_IID);
static NS_DEFINE_IID(kIHTMLCSSStyleSheetIID,      NS_IHTML_CSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kICSSLoaderIID,              NS_ICSS_LOADER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID,         NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,       NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIParserIID,                 NS_IPARSER_IID);
static NS_DEFINE_IID(kIPresShellIID,              NS_IPRESSHELL_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,          NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,            NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID,      NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMSelectionIID,           NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kIDOMEventCapturerIID,       NS_IDOMEVENTCAPTURER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,       NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMEventTargetIID,         NS_IDOMEVENTTARGET_IID);
static NS_DEFINE_IID(kIStreamListenerIID,         NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStreamObserverIID,         NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellIID,               NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIXMLDocumentIID,            NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIXULContentSinkIID,         NS_IXULCONTENTSINK_IID);
static NS_DEFINE_IID(kIEventListenerManagerIID,   NS_IEVENTLISTENERMANAGER_IID);

static NS_DEFINE_CID(kEventListenerManagerCID, NS_EVENTLISTENERMANAGER_CID);
static NS_DEFINE_CID(kCSSParserCID,              NS_CSSPARSER_CID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID,         NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kHTMLCSSStyleSheetCID,      NS_HTML_CSS_STYLESHEET_CID);
static NS_DEFINE_CID(kCSSLoaderCID,              NS_CSS_LOADER_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,       NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kParserCID,                 NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kPresShellCID,              NS_PRESSHELL_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kLocalStoreCID,             NS_LOCALSTORE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,       NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFXULBuilderCID,          NS_RDFXULBUILDER_CID);
static NS_DEFINE_CID(kTextNodeCID,               NS_TEXTNODE_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,          NS_WELLFORMEDDTD_CID);
static NS_DEFINE_CID(kXULContentSinkCID,         NS_XULCONTENTSINK_CID);

static NS_DEFINE_CID(kXULCommandDispatcherCID, NS_XULCOMMANDDISPATCHER_CID);
static NS_DEFINE_IID(kIXULCommandDispatcherIID, NS_IXULCOMMANDDISPATCHER_IID);
static NS_DEFINE_IID(kLWBrkCID, NS_LWBRK_CID);
static NS_DEFINE_IID(kILineBreakerFactoryIID, NS_ILINEBREAKERFACTORY_IID);
static NS_DEFINE_IID(kIWordBreakerFactoryIID, NS_IWORDBREAKERFACTORY_IID);

////////////////////////////////////////////////////////////////////////
// Standard vocabulary items

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
#define XUL_NAMESPACE_URI_PREFIX XUL_NAMESPACE_URI "#"
DEFINE_RDF_VOCAB(XUL_NAMESPACE_URI_PREFIX, XUL, element);

static PRLogModuleInfo* gMapLog;
static PRLogModuleInfo* gXULLog;

////////////////////////////////////////////////////////////////////////
// nsElementMap

class nsElementMap
{
private:
    PLHashTable* mResources;

    class ContentListItem {
    public:
        ContentListItem(nsIContent* aContent)
            : mNext(nsnull), mContent(aContent) {}

        ContentListItem* mNext;
        nsIContent*      mContent;
    };

    static PLHashNumber
    HashPointer(const void* key)
    {
        return (PLHashNumber) key;
    }

    static PRIntn
    ReleaseContentList(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure);

public:
    nsElementMap(void);
    virtual ~nsElementMap();

    nsresult
    Add(nsIRDFResource* aResource, nsIContent* aContent);

    nsresult
    Remove(nsIRDFResource* aResource, nsIContent* aContent);

    nsresult
    Find(nsIRDFResource* aResource, nsISupportsArray* aResults);

    typedef PRIntn (*nsElementMapEnumerator)(nsIRDFResource* aResource,
                                             nsIContent* aElement,
                                             void* aClosure);
    nsresult
    Enumerate(nsElementMapEnumerator aEnumerator, void* aClosure);

private:
    struct EnumerateClosure {
        nsElementMapEnumerator mEnumerator;
        void*                  mClosure;
    };
        
    static PRIntn
    EnumerateImpl(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure);
};


nsElementMap::nsElementMap()
{
    // Create a table for mapping RDF resources to elements in the
    // content tree.
    static PRInt32 kInitialResourceTableSize = 1023;
    if ((mResources = PL_NewHashTable(kInitialResourceTableSize,
                                      HashPointer,
                                      PL_CompareValues,
                                      PL_CompareValues,
                                      nsnull,
                                      nsnull)) == nsnull) {
        NS_ERROR("could not create hash table for resources");
    }
}

nsElementMap::~nsElementMap()
{
    if (mResources) {
        PL_HashTableEnumerateEntries(mResources, ReleaseContentList, nsnull);
        PL_HashTableDestroy(mResources);
    }
}


PRIntn
nsElementMap::ReleaseContentList(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure)
{
    nsIRDFResource* resource =
        NS_REINTERPRET_CAST(nsIRDFResource*, NS_CONST_CAST(void*, aHashEntry->key));

    NS_RELEASE(resource);
        
    ContentListItem* head =
        NS_REINTERPRET_CAST(ContentListItem*, aHashEntry->value);

    while (head) {
        ContentListItem* doomed = head;
        head = head->mNext;
        delete doomed;
    }

    return HT_ENUMERATE_NEXT;
}


nsresult
nsElementMap::Add(nsIRDFResource* aResource, nsIContent* aContent)
{
    NS_PRECONDITION(mResources != nsnull, "not initialized");
    if (! mResources)
        return NS_ERROR_NOT_INITIALIZED;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gMapLog, PR_LOG_ALWAYS)) {
        nsXPIDLCString uri;
        aResource->GetValue( getter_Copies(uri) );
        PR_LOG(gMapLog, PR_LOG_ALWAYS,
               ("xulelemap(%p) add    [%p] <-- %s\n",
                this, aContent, (const char*) uri));
    }
#endif

    ContentListItem* head =
        (ContentListItem*) PL_HashTableLookup(mResources, aResource);

    if (! head) {
        head = new ContentListItem(aContent);
        if (! head)
            return NS_ERROR_OUT_OF_MEMORY;

        PL_HashTableAdd(mResources, aResource, head);
        NS_ADDREF(aResource);
        NS_ADDREF(aContent);
    }
    else {
        while (1) {
            if (head->mContent == aContent) {
                // This can happen if an element that was created via
                // frame construction code is then "appended" to the
                // content model with aNotify == PR_TRUE. If you see
                // this warning, it's an indication that you're
                // unnecessarily notifying the frame system, and
                // potentially causing unnecessary reflow.
                NS_WARNING("element was already in the map");
                return NS_OK;
            }
            if (! head->mNext)
                break;

            head = head->mNext;
        }

        head->mNext = new ContentListItem(aContent);
        if (! head->mNext)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(aContent);
    }

    return NS_OK;
}


nsresult
nsElementMap::Remove(nsIRDFResource* aResource, nsIContent* aContent)
{
    NS_PRECONDITION(mResources != nsnull, "not initialized");
    if (! mResources)
        return NS_ERROR_NOT_INITIALIZED;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gMapLog, PR_LOG_ALWAYS)) {
        nsXPIDLCString uri;
        aResource->GetValue( getter_Copies(uri) );
        PR_LOG(gMapLog, PR_LOG_ALWAYS,
               ("xulelemap(%p) remove [%p] <-- %s\n",
                this, aContent, (const char*) uri));
    }
#endif

    ContentListItem* head =
        (ContentListItem*) PL_HashTableLookup(mResources, aResource);

    if (head) {
        if (head->mContent == aContent) {
            NS_RELEASE(aContent);
            ContentListItem* newHead = head->mNext;
            if (newHead) {
                PL_HashTableAdd(mResources, aResource, newHead);
            }
            else {
                // It was the last reference in the table
                PL_HashTableRemove(mResources, aResource);
                NS_RELEASE(aResource);
            }
            delete head;
            return NS_OK;
        }
        else {
            ContentListItem* doomed = head->mNext;
            while (doomed) {
                if (doomed->mContent == aContent) {
                    head->mNext = doomed->mNext;
                    NS_RELEASE(aContent);
                    delete doomed;
                    return NS_OK;
                }
                head = doomed;
                doomed = doomed->mNext;
            }
        }
    }

    // XXX Don't comment out this assert: if you get here,
    // something has gone dreadfully, horribly
    // wrong. Curse. Scream. File a bug against
    // waterson@netscape.com.
    NS_ERROR("attempt to remove an element that was never added");
    return NS_ERROR_ILLEGAL_VALUE;
}



nsresult
nsElementMap::Find(nsIRDFResource* aResource, nsISupportsArray* aResults)
{
    NS_PRECONDITION(mResources != nsnull, "not initialized");
    if (! mResources)
        return NS_ERROR_NOT_INITIALIZED;

    aResults->Clear();
    ContentListItem* head =
        (ContentListItem*) PL_HashTableLookup(mResources, aResource);

    while (head) {
        aResults->AppendElement(head->mContent);
        head = head->mNext;
    }
    return NS_OK;
}

nsresult
nsElementMap::Enumerate(nsElementMapEnumerator aEnumerator, void* aClosure)
{
    EnumerateClosure closure = { aEnumerator, aClosure };
    PL_HashTableEnumerateEntries(mResources, EnumerateImpl, &closure);
    return NS_OK;
}


PRIntn
nsElementMap::EnumerateImpl(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure)
{
    EnumerateClosure* closure = NS_REINTERPRET_CAST(EnumerateClosure*, aClosure);

    nsIRDFResource* resource =
        NS_REINTERPRET_CAST(nsIRDFResource*, NS_CONST_CAST(void*, aHashEntry->key));

    ContentListItem** link = 
        NS_REINTERPRET_CAST(ContentListItem**, &aHashEntry->value);

    ContentListItem* item = *link;

    while (item) {
        PRIntn result = (*closure->mEnumerator)(resource, item->mContent, closure->mClosure);

        if (result == HT_ENUMERATE_REMOVE) {
            NS_RELEASE(item->mContent);
            *link = item->mNext;

            if ((! *link) && (link == NS_REINTERPRET_CAST(ContentListItem**, &aHashEntry->value))) {
                NS_RELEASE(resource);
                return HT_ENUMERATE_REMOVE;
            }
        }
        else {
            link = &item->mNext;
        }

        item = item->mNext;
    }

    return HT_ENUMERATE_NEXT;
}

////////////////////////////////////////////////////////////////////////
// XULDocumentImpl

class XULDocumentImpl : public nsIDocument,
                        public nsIRDFDocument,
                        public nsIStreamLoadableDocument,
                        public nsIDOMXULDocument,
                        public nsIDOMNSDocument,
                        public nsIDOMEventCapturer, 
                        public nsIJSScriptObject,
                        public nsIScriptObjectOwner,
                        public nsIHTMLContentContainer,
                        public nsIXULParentDocument,
                        public nsIXULChildDocument
{
public:
    XULDocumentImpl();
    virtual ~XULDocumentImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDocument interface
    virtual nsIArena* GetArena();

    NS_IMETHOD GetContentType(nsString& aContentType) const;

    NS_IMETHOD StartDocumentLoad(const char* aCommand,
#ifdef NECKO
                                 nsIChannel* aChannel,
                                 nsILoadGroup* aLoadGroup,
#else
                                 nsIURI *aUrl, 
#endif
                                 nsIContentViewerContainer* aContainer,
                                 nsIStreamListener **aDocListener);

    NS_IMETHOD LoadFromStream(nsIInputStream& xulStream,
                              nsIContentViewerContainer* aContainer,
                              const char* aCommand );

    virtual const nsString* GetDocumentTitle() const;

    virtual nsIURI* GetDocumentURL() const;

    virtual nsIPrincipal* GetDocumentPrincipal() const;

    virtual nsILoadGroup* GetDocumentLoadGroup() const;

    NS_IMETHOD GetBaseURL(nsIURI*& aURL) const;

    NS_IMETHOD GetDocumentCharacterSet(nsString& oCharSetID);

    NS_IMETHOD SetDocumentCharacterSet(const nsString& aCharSetID);

    NS_IMETHOD GetLineBreaker(nsILineBreaker** aResult) ;
    NS_IMETHOD SetLineBreaker(nsILineBreaker* aLineBreaker) ;
    NS_IMETHOD GetWordBreaker(nsIWordBreaker** aResult) ;
    NS_IMETHOD SetWordBreaker(nsIWordBreaker* aWordBreaker) ;

    NS_IMETHOD GetHeaderData(nsIAtom* aHeaderField, nsString& aData) const;
    NS_IMETHOD SetHeaderData(nsIAtom* aheaderField, const nsString& aData);

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
    NS_IMETHOD InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex, PRBool aNotify);

    virtual void SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                            PRBool mDisabled);

    NS_IMETHOD GetCSSLoader(nsICSSLoader*& aLoader);

    virtual nsIScriptContextOwner *GetScriptContextOwner();

    virtual void SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner);

    NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager);

    virtual void AddObserver(nsIDocumentObserver* aObserver);

    virtual PRBool RemoveObserver(nsIDocumentObserver* aObserver);

    NS_IMETHOD BeginLoad();

    NS_IMETHOD EndLoad();

    NS_IMETHOD ContentChanged(nsIContent* aContent,
                              nsISupports* aSubContent);

    NS_IMETHOD ContentStatesChanged(nsIContent* aContent1, nsIContent* aContent2);

    NS_IMETHOD AttributeChanged(nsIContent* aChild,
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

    NS_IMETHOD GetSelection(nsIDOMSelection** aSelection);

    NS_IMETHOD SelectAll();

    NS_IMETHOD FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

    virtual void CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection);

    virtual void ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual void FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode);

    virtual PRBool IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const;

    virtual PRBool IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const;

    virtual PRBool IsInSelection(nsIDOMSelection* aSelection, const nsIContent *aContent) const;

    virtual nsIContent* GetPrevContent(const nsIContent *aContent) const;

    virtual nsIContent* GetNextContent(const nsIContent *aContent) const;

    virtual void SetDisplaySelection(PRBool aToggle);

    virtual PRBool GetDisplaySelection() const;

    NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext, 
                              nsEvent* aEvent, 
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus& aEventStatus);


    // nsIXMLDocument interface
    NS_IMETHOD GetContentById(const nsString& aName, nsIContent** aContent);
#ifdef XSL
    NS_IMETHOD SetTransformMediator(nsITransformMediator* aMediator);
#endif

    // nsIRDFDocument interface
    NS_IMETHOD SetRootResource(nsIRDFResource* resource);
    NS_IMETHOD SplitProperty(nsIRDFResource* aResource,
                             PRInt32* aNameSpaceID,
                             nsIAtom** aTag);

    NS_IMETHOD AddElementForResource(nsIRDFResource* aResource, nsIContent* aElement);
    NS_IMETHOD RemoveElementForResource(nsIRDFResource* aResource, nsIContent* aElement);
    NS_IMETHOD GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements);
    NS_IMETHOD CreateContents(nsIContent* aElement);
    NS_IMETHOD AddContentModelBuilder(nsIRDFContentModelBuilder* aBuilder);
    NS_IMETHOD GetDocumentDataSource(nsIRDFDataSource** aDatasource);
    NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
    NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);

    // nsIDOMEventCapturer interface
    NS_IMETHOD    CaptureEvent(const nsString& aType);
    NS_IMETHOD    ReleaseEvent(const nsString& aType);

    // nsIDOMEventReceiver interface (yuck. inherited from nsIDOMEventCapturer)
    NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
    NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);

    // nsIDOMEventTarget interface
    NS_IMETHOD AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                PRBool aUseCapture);
    NS_IMETHOD RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                   PRBool aUseCapture);

    // nsIDOMDocument interface
    NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDoctype);
    NS_IMETHOD    GetImplementation(nsIDOMDOMImplementation** aImplementation);
    NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement);

    NS_IMETHOD    CreateElement(const nsString& aTagName, nsIDOMElement** aReturn);
    NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn);
    NS_IMETHOD    CreateTextNode(const nsString& aData, nsIDOMText** aReturn);
    NS_IMETHOD    CreateComment(const nsString& aData, nsIDOMComment** aReturn);
    NS_IMETHOD    CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn);
    NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn);
    NS_IMETHOD    CreateAttribute(const nsString& aName, nsIDOMAttr** aReturn);
    NS_IMETHOD    CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn);
    NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn);

    // nsIDOMNSDocument interface
    NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets);
    NS_IMETHOD    CreateElementWithNameSpace(const nsString& aTagName, const nsString& aNameSpace, nsIDOMElement** aResult);
    NS_IMETHOD    CreateRange(nsIDOMRange** aRange);

    // nsIDOMXULDocument interface
    NS_DECL_IDOMXULDOCUMENT
                   
    // nsIXULParentDocument interface
    NS_IMETHOD    GetContentViewerContainer(nsIContentViewerContainer** aContainer);
    NS_IMETHOD    GetCommand(nsString& aCommand);
    NS_IMETHOD    CreatePopupDocument(nsIContent* aPopupElement, nsIDocument** aResult); 
    
    // nsIXULChildDocument Interface
    NS_IMETHOD    SetContentSink(nsIXULContentSink* aContentSink);
    NS_IMETHOD    GetContentSink(nsIXULContentSink** aContentSink);
    NS_IMETHOD    LayoutPopupDocument();

    // nsIDOMNode interface
    NS_IMETHOD    GetNodeName(nsString& aNodeName);
    NS_IMETHOD    GetNodeValue(nsString& aNodeValue);
    NS_IMETHOD    SetNodeValue(const nsString& aNodeValue);
    NS_IMETHOD    GetNodeType(PRUint16* aNodeType);
    NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode);
    NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes);
    NS_IMETHOD    HasChildNodes(PRBool* aHasChildNodes);
    NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild);
    NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild);
    NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling);
    NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling);
    NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes);
    NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
    NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn);
    NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
    NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
    NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);
    NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

    // nsIJSScriptObject interface
    virtual PRBool    AddProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool    DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool    GetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool    SetProperty(JSContext *aContext, jsval aID, jsval *aVp);
    virtual PRBool    EnumerateProperty(JSContext *aContext);
    virtual PRBool    Resolve(JSContext *aContext, jsval aID);
    virtual PRBool    Convert(JSContext *aContext, jsval aID);
    virtual void      Finalize(JSContext *aContext);

    // nsIScriptObjectOwner interface
    NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

    // nsIHTMLContentContainer interface
    NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult);
    NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult);

    // Implementation methods
    nsresult Init(void);
    nsresult StartLayout(void);

    nsresult OpenWidgetItem(nsIContent* aElement);
    nsresult CloseWidgetItem(nsIContent* aElement);
    nsresult RebuildWidgetItem(nsIContent* aElement);

    nsresult
    AddElementToMap(nsIContent* aElement, PRBool aDeep);

    nsresult
    RemoveElementFromMap(nsIContent* aElement, PRBool aDeep);

    static PRIntn
    RemoveElementsFromMapByContent(nsIRDFResource* aResource,
                                   nsIContent* aElement,
                                   void* aClosure);

    nsresult SearchForNodeByID(const nsString& anID, nsIContent* anElement, nsIDOMElement** aReturn);

    static nsresult
    GetElementsByTagName(nsIDOMNode* aNode,
                         const nsString& aTagName,
                         nsRDFDOMNodeList* aElements);

    static nsresult
    GetElementsByAttribute(nsIDOMNode* aNode,
                           const nsString& aAttribute,
                           const nsString& aValue,
                           nsRDFDOMNodeList* aElements);

    nsresult
    ParseTagString(const nsString& aTagName, nsIAtom*& aName, PRInt32& aNameSpaceID);

    NS_IMETHOD PrepareStyleSheets(nsIURI* anURL);
    
    void SetDocumentURLAndGroup(nsIURI* anURL);
    void SetIsPopup(PRBool isPopup) { mIsPopup = isPopup; };

protected:
		nsresult PrepareToLoad( nsCOMPtr<nsIParser>* created_parser,
		                        nsIContentViewerContainer* aContainer,
		                        const char* aCommand,
#ifdef NECKO
                            nsIChannel* aChannel, nsILoadGroup* aLoadGroup
#else
                            nsIURI* aOptionalURL = 0
#endif
                            );

protected:
    // pseudo constants
    static PRInt32 gRefCnt;
    static nsIAtom*  kContainerContentsGeneratedAtom;
    static nsIAtom*  kIdAtom;
    static nsIAtom*  kLazyContentAtom;
    static nsIAtom*  kObservesAtom;
    static nsIAtom*  kOpenAtom;
    static nsIAtom*  kRefAtom;
    static nsIAtom*  kRuleAtom;
    static nsIAtom*  kTemplateAtom;
    static nsIAtom*  kTemplateContentsGeneratedAtom;
    static nsIAtom*  kXULContentsGeneratedAtom;

    static nsIAtom** kIdentityAttrs[];

    static nsIRDFService* gRDFService;
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kRDF_type;
    static nsIRDFResource* kXUL_element;

    static nsINameSpaceManager* gNameSpaceManager;
    static PRInt32 kNameSpaceID_XUL;

    nsIContent*
    FindContent(const nsIContent* aStartNode,
                const nsIContent* aTest1,
                const nsIContent* aTest2) const;

    nsresult
    AddNamedDataSource(const char* uri);

    // IMPORTANT: The ownership implicit in the following member variables has been 
    // explicitly checked and set using nsCOMPtr for owning pointers and raw COM interface 
    // pointers for weak (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
    // NOTE, THIS IS STILL IN PROGRESS, TALK TO PINK OR SCC BEFORE CHANGING

    nsCOMPtr<nsIArena>         mArena;
    nsVoidArray                mObservers;
    nsAutoString               mDocumentTitle;
    nsCOMPtr<nsIURI>           mDocumentURL;        // [OWNER] ??? compare with loader
    nsCOMPtr<nsILoadGroup>     mDocumentLoadGroup;  // [OWNER] leads to loader
    nsCOMPtr<nsIPrincipal>     mDocumentPrincipal;  // [OWNER]
    nsCOMPtr<nsIRDFResource>   mRootResource;       // [OWNER]
    nsCOMPtr<nsIContent>       mRootContent;        // [OWNER] 
    nsIDocument*               mParentDocument;     // [WEAK]
    nsIScriptContextOwner*     mScriptContextOwner; // [WEAK] it owns me! (indirectly)
    void*                      mScriptObject;       // ????
    nsString                   mCharSetID;
    nsVoidArray                mStyleSheets;
    nsCOMPtr<nsIDOMSelection>  mSelection;          // [OWNER] 
    PRBool                     mDisplaySelection;
    nsVoidArray                mPresShells;
    nsCOMPtr<nsIEventListenerManager> mListenerManager;   // [OWNER]
    nsCOMPtr<nsINameSpaceManager>     mNameSpaceManager;  // [OWNER] 
    nsCOMPtr<nsIHTMLStyleSheet>       mAttrStyleSheet;    // [OWNER] 
    nsCOMPtr<nsIHTMLCSSStyleSheet>    mInlineStyleSheet;  // [OWNER]
    nsCOMPtr<nsICSSLoader>            mCSSLoader;         // [OWNER]
    nsElementMap               mResources;
    nsCOMPtr<nsISupportsArray> mBuilders;        // [OWNER] of array, elements shouldn't own this, but they do
    nsCOMPtr<nsIRDFContentModelBuilder> mXULBuilder;     // [OWNER] 
    nsCOMPtr<nsIRDFDataSource>          mLocalDataSource;     // [OWNER] 
    nsCOMPtr<nsIRDFDataSource>          mDocumentDataSource;  // [OWNER] 
    nsCOMPtr<nsILineBreaker>            mLineBreaker;    // [OWNER] 
    nsCOMPtr<nsIWordBreaker>            mWordBreaker;    // [OWNER] 
    nsIContentViewerContainer* mContentViewerContainer;  // [WEAK] it owns me! (indirectly)
    nsString                   mCommand;
    nsIXULContentSink*         mParentContentSink;     // [WEAK] 
    nsVoidArray                mSubDocuments;     // [OWNER] of subelements
    PRBool                     mIsPopup; 
    nsCOMPtr<nsIDOMHTMLFormElement>     mHiddenForm;   // [OWNER] of this content element
    nsCOMPtr<nsIDOMXULCommandDispatcher>     mCommandDispatcher; // [OWNER] of the focus tracker

      // The following are pointers into the content model which provide access to
      // the objects triggering either a popup or a tooltip. These are marked as
      // [OWNER] only because someone could, through DOM calls, delete the object from the
      // content model while the popup/tooltip was visible. If we didn't have a reference
      // to it, the object would go away and we'd be left pointing to garbage. This
      // does not introduce cycles into the ownership model because this is still
      // parent/child ownership. Just wanted the reader to know hyatt and I had thought about
      // this (pinkerton).
    nsCOMPtr<nsIDOMElement>    mPopupElement;            // [OWNER] element triggering the popup
    nsCOMPtr<nsIDOMElement>    mTooltipElement;          // [OWNER] element triggering the tooltip
};

PRInt32 XULDocumentImpl::gRefCnt = 0;
nsIAtom* XULDocumentImpl::kContainerContentsGeneratedAtom;
nsIAtom* XULDocumentImpl::kIdAtom;
nsIAtom* XULDocumentImpl::kLazyContentAtom;
nsIAtom* XULDocumentImpl::kObservesAtom;
nsIAtom* XULDocumentImpl::kOpenAtom;
nsIAtom* XULDocumentImpl::kRefAtom;
nsIAtom* XULDocumentImpl::kRuleAtom;
nsIAtom* XULDocumentImpl::kTemplateAtom;
nsIAtom* XULDocumentImpl::kTemplateContentsGeneratedAtom;
nsIAtom* XULDocumentImpl::kXULContentsGeneratedAtom;

nsIRDFService* XULDocumentImpl::gRDFService;
nsIRDFResource* XULDocumentImpl::kRDF_instanceOf;
nsIRDFResource* XULDocumentImpl::kRDF_type;
nsIRDFResource* XULDocumentImpl::kXUL_element;

nsINameSpaceManager* XULDocumentImpl::gNameSpaceManager;
PRInt32 XULDocumentImpl::kNameSpaceID_XUL;

////////////////////////////////////////////////////////////////////////
// ctors & dtors

XULDocumentImpl::XULDocumentImpl(void)
    : mParentDocument(nsnull),
      mScriptContextOwner(nsnull),
      mScriptObject(nsnull),
      mCharSetID("UTF-8"),
      mDisplaySelection(PR_FALSE),
      mContentViewerContainer(nsnull),
      mParentContentSink(nsnull),
      mIsPopup(PR_FALSE)
{
    NS_INIT_REFCNT();

    nsresult rv;

    // construct a selection object
/*    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRangeListCID,
                                                    nsnull,
                                                    kIDOMSelectionIID,
                                                    (void**) &mSelection))) {
        NS_ERROR("unable to create DOM selection");
    }*/

    if (gRefCnt++ == 0) {
        kContainerContentsGeneratedAtom = NS_NewAtom("containercontentsgenerated");
        kIdAtom                         = NS_NewAtom("id");
        kLazyContentAtom                = NS_NewAtom("lazycontent");
        kObservesAtom                   = NS_NewAtom("observes");
        kOpenAtom                       = NS_NewAtom("open");
        kRefAtom                        = NS_NewAtom("ref");
        kRuleAtom                       = NS_NewAtom("rule");
        kTemplateAtom                   = NS_NewAtom("template");
        kTemplateContentsGeneratedAtom  = NS_NewAtom("templatecontentsgenerated");
        kXULContentsGeneratedAtom       = NS_NewAtom("xulcontentsgenerated");

        // Keep the RDF service cached in a member variable to make using
        // it a bit less painful
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**) &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF Service");

        if (gRDFService) {
            gRDFService->GetResource(kURIRDF_instanceOf, &kRDF_instanceOf);
            gRDFService->GetResource(kURIRDF_type,       &kRDF_type);
            gRDFService->GetResource(kURIXUL_element,    &kXUL_element);
        }

        rv = nsServiceManager::GetService(kNameSpaceManagerCID,
                                          nsCOMTypeInfo<nsINameSpaceManager>::GetIID(),
                                          (nsISupports**) &gNameSpaceManager);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get namespace manager");

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;
        gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
    }

#ifdef PR_LOGGING
    if (! gMapLog)
        gMapLog = PR_NewLogModule("nsXULDocumentElementMap");

    if (! gXULLog)
        gXULLog = PR_NewLogModule("nsXULDocument");
#endif
}

XULDocumentImpl::~XULDocumentImpl()
{
    // mParentDocument is never refcounted
    // Delete references to sub-documents
    {
        PRInt32 i = mSubDocuments.Count();
        while (--i >= 0) {
            nsIDocument* subdoc = (nsIDocument*) mSubDocuments.ElementAt(i);
            NS_RELEASE(subdoc);
        }
    }

    // Delete references to style sheets but only if we aren't a popup document.
    if (!mIsPopup) {
        PRInt32 i = mStyleSheets.Count();
        while (--i >= 0) {
            nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(i);
            sheet->SetOwningDocument(nsnull);
            NS_RELEASE(sheet);
        }
    }

    // set all builder references to document to nsnull -- out of band notification
    // to break ownership cycle
    if (mBuilders)
    {
        PRUint32 cnt = 0;
        nsresult rv = mBuilders->Count(&cnt);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");

#ifdef	DEBUG
        printf("# of builders: %lu\n", (unsigned long)cnt);
#endif

        for (PRUint32 i = 0; i < cnt; ++i) {
          nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

          NS_ASSERTION(builder != nsnull, "null ptr");
          if (! builder) continue;

          rv = builder->SetDocument(nsnull);
          NS_ASSERTION(NS_SUCCEEDED(rv), "error unlinking builder from document");
          // XXX ignore error code?

          rv = builder->SetDataBase(nsnull);
          NS_ASSERTION(NS_SUCCEEDED(rv), "error unlinking builder from database");

          NS_RELEASE(builder);
        }
    }

    if (mCSSLoader) {
      mCSSLoader->DropDocumentReference();
    }
    
    if (--gRefCnt == 0) {
        NS_IF_RELEASE(kContainerContentsGeneratedAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kLazyContentAtom);
        NS_IF_RELEASE(kObservesAtom);
        NS_IF_RELEASE(kOpenAtom);
        NS_IF_RELEASE(kRefAtom);
        NS_IF_RELEASE(kRuleAtom);
        NS_IF_RELEASE(kTemplateAtom);
        NS_IF_RELEASE(kTemplateContentsGeneratedAtom);
        NS_IF_RELEASE(kXULContentsGeneratedAtom);

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kXUL_element);
    }
}


nsresult
NS_NewXULDocument(nsIRDFDocument** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    XULDocumentImpl* doc = new XULDocumentImpl();
    if (! doc)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(doc);

    nsresult rv;
    if (NS_FAILED(rv = doc->Init())) {
        NS_RELEASE(doc);
        return rv;
    }

    *result = doc;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMETHODIMP 
XULDocumentImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIDocumentIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIDocument*, this);
    }
    else if (iid.Equals(nsIXULParentDocument::GetIID())) {
        *result = NS_STATIC_CAST(nsIXULParentDocument*, this);
    }
    else if (iid.Equals(nsIXULChildDocument::GetIID())) {
        *result = NS_STATIC_CAST(nsIXULChildDocument*, this);
    }
    else if (iid.Equals(kIRDFDocumentIID) ||
             iid.Equals(kIXMLDocumentIID)) {
        *result = NS_STATIC_CAST(nsIRDFDocument*, this);
    }
    else if (iid.Equals(nsIDOMXULDocument::GetIID()) ||
             iid.Equals(nsIDOMDocument::GetIID()) ||
             iid.Equals(nsIDOMNode::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMXULDocument*, this);
    }
    else if (iid.Equals(nsIDOMNSDocument::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMNSDocument*, this);
    }
    else if (iid.Equals(kIJSScriptObjectIID)) {
        *result = NS_STATIC_CAST(nsIJSScriptObject*, this);
    }
    else if (iid.Equals(kIScriptObjectOwnerIID)) {
        *result = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
    }
    else if (iid.Equals(kIHTMLContentContainerIID)) {
        *result = NS_STATIC_CAST(nsIHTMLContentContainer*, this);
    }
    else if (iid.Equals(kIDOMEventReceiverIID)) {
        *result = NS_STATIC_CAST(nsIDOMEventReceiver*, this);
    }
    else if (iid.Equals(kIDOMEventTargetIID)) {
        *result = NS_STATIC_CAST(nsIDOMEventTarget*, this);
    }
    else if (iid.Equals(kIDOMEventCapturerIID)) {
        *result = NS_STATIC_CAST(nsIDOMEventCapturer*, this);
    }
    else if (iid.Equals(nsIStreamLoadableDocument::GetIID())) {
    		*result = NS_STATIC_CAST(nsIStreamLoadableDocument*, this);
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
    NS_ADDREF(this);
    return NS_OK;
}

NS_IMPL_ADDREF(XULDocumentImpl);
NS_IMPL_RELEASE(XULDocumentImpl);

////////////////////////////////////////////////////////////////////////
// nsIDocument interface

nsIArena*
XULDocumentImpl::GetArena()
{
    nsIArena* result = mArena;
    NS_IF_ADDREF(result);
    return result;
}

NS_IMETHODIMP 
XULDocumentImpl::GetContentType(nsString& aContentType) const
{
    // XXX Is this right, Chris?
    aContentType.SetString("text/xul");
    return NS_OK;
}

static
nsresult
generate_RDF_seed( nsString* result, nsIURI* aOptionalURL )
	{
		nsresult status = NS_OK;

		if ( aOptionalURL )
			{
#ifdef NECKO
                char* s = 0;
#else
				const char* s = 0;
#endif
                status = aOptionalURL->GetSpec(&s);
				if ( NS_SUCCEEDED(status) ) {
					(*result) = s;      // copied by nsString
#ifdef NECKO
                    nsCRT::free(s);
#endif
                }
			}
		else
			{
				static int unique_per_session_index = 0;

				result->Append("x-anonymous-xul://");
				result->Append(PRInt32(++unique_per_session_index), /*base*/ 10);
			}

		return status;
	}

nsresult
XULDocumentImpl::PrepareToLoad( nsCOMPtr<nsIParser>* created_parser,
                                nsIContentViewerContainer* aContainer,
                                const char* aCommand,
#ifdef NECKO
                                nsIChannel* aChannel, nsILoadGroup* aLoadGroup
#else
                                nsIURI* aOptionalURL
#endif
                                )
{
    nsCOMPtr<nsIURI> syntheticURL;
#ifdef NECKO
		if ( aChannel )
      (void)aChannel->GetURI(getter_AddRefs(syntheticURL));
#else
    if ( aOptionalURL )
        syntheticURL = dont_QueryInterface(aOptionalURL);
#endif
    else
        {
            nsAutoString seedString;
            generate_RDF_seed(&seedString, 0);
#ifndef NECKO
            NS_NewURL(getter_AddRefs(syntheticURL), seedString);
#else
            NS_NewURI(getter_AddRefs(syntheticURL), seedString);
#endif // NECKO
        }

#if 0
    NS_ASSERTION(aURL != nsnull, "null ptr");
    if (! aURL)
        return NS_ERROR_NULL_POINTER;
#endif

    if (aContainer && aContainer != mContentViewerContainer)
        mContentViewerContainer = aContainer;

    nsresult rv;

    mDocumentTitle.Truncate();

    mDocumentURL = syntheticURL;

    rv = aChannel->GetPrincipal(getter_AddRefs(mDocumentPrincipal));
    if (NS_FAILED(rv)) return rv;

#ifdef NECKO
    mDocumentLoadGroup = aLoadGroup;
#else
    syntheticURL->GetLoadGroup(getter_AddRefs(mDocumentLoadGroup));
#endif

    SetDocumentURLAndGroup(syntheticURL);

    rv = PrepareStyleSheets(syntheticURL);
    if (NS_FAILED(rv)) return rv;

    // Create the composite data source and builder, but only do this
    // if we're not a XUL fragment. XUL fragments get "imported"
    // directly into the parent document's datasource.
    if (mParentContentSink == nsnull) {
        // Create a "scratch" in-memory data store to associate with the
        // document to be a catch-all for any doc-specific info that we
        // need to store (e.g., current sort order, etc.)
        //
        // XXX This needs to be cloned across windows, and the final
        // instance needs to be flushed to disk. It may be that this is
        // really an RDFXML data source...
        rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
                                                nsnull,
                                                kIRDFDataSourceIID,
                                                getter_AddRefs(mDocumentDataSource));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create XUL datasource");
        if (NS_FAILED(rv)) return rv;
    }

    // Create a XUL content sink, a parser, and kick off the load.
    nsCOMPtr<nsIXULContentSink> sink;
    rv = nsComponentManager::CreateInstance(kXULContentSinkCID,
                                            nsnull,
                                            kIXULContentSinkIID,
                                            getter_AddRefs(sink));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create XUL content sink");
    if (NS_FAILED(rv)) return rv;

    rv = sink->Init(this, mDocumentDataSource);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to initialize datasource sink");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIParser> parser;
    rv = nsComponentManager::CreateInstance(kParserCID,
                                            nsnull,
                                            kIParserIID,
                                            getter_AddRefs(parser));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create parser");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDTD> dtd;
    rv = nsComponentManager::CreateInstance(kWellFormedDTDCID,
                                            nsnull,
                                            kIDTDIID,
                                            getter_AddRefs(dtd));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to construct DTD");
    if (NS_FAILED(rv)) return rv;

    mCommand = aCommand;

    parser->RegisterDTD(dtd);
    parser->SetCommand(aCommand);
    nsAutoString utf8("UTF-8");
    parser->SetDocumentCharset(utf8, kCharsetFromDocTypeDefault);
    parser->SetContentSink(sink); // grabs a reference to the parser

    *created_parser = parser;

    return rv;
}

NS_IMETHODIMP 
XULDocumentImpl::PrepareStyleSheets(nsIURI* anURL)
{
    nsresult rv;
    
    // Delete references to style sheets - this should be done in superclass...
    PRInt32 i = mStyleSheets.Count();
    while (--i >= 0) {
        nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(i);
        sheet->SetOwningDocument(nsnull);
        NS_RELEASE(sheet);
    }
    mStyleSheets.Clear();

    // Create an HTML style sheet for the HTML content.
    nsCOMPtr<nsIHTMLStyleSheet> sheet;
    if (NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kHTMLStyleSheetCID,
                                                       nsnull,
                                                       kIHTMLStyleSheetIID,
                                                       getter_AddRefs(sheet)))) {
        if (NS_SUCCEEDED(rv = sheet->Init(anURL, this))) {
            mAttrStyleSheet = sheet;
            AddStyleSheet(mAttrStyleSheet);
        }
    }

    if (NS_FAILED(rv)) {
        NS_ERROR("unable to add HTML style sheet");
        return rv;
    }

    // Create an inline style sheet for inline content that contains a style 
    // attribute.
    nsIHTMLCSSStyleSheet* inlineSheet;
    if (NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kHTMLCSSStyleSheetCID,
                                                       nsnull,
                                                       kIHTMLCSSStyleSheetIID,
                                                       (void**)&inlineSheet))) {
        if (NS_SUCCEEDED(rv = inlineSheet->Init(anURL, this))) {
            mInlineStyleSheet = dont_QueryInterface(inlineSheet);
            AddStyleSheet(mInlineStyleSheet);
        }
        NS_RELEASE(inlineSheet);
    }

    if (NS_FAILED(rv)) {
        NS_ERROR("unable to add inline style sheet");
        return rv;
    }

    return NS_OK;
}

void
XULDocumentImpl::SetDocumentURLAndGroup(nsIURI* anURL)
{
    mDocumentURL = dont_QueryInterface(anURL);
#ifdef NECKO
    // XXX help
#else
    anURL->GetLoadGroup(getter_AddRefs(mDocumentLoadGroup));
#endif
}

NS_IMETHODIMP 
XULDocumentImpl::StartDocumentLoad(const char* aCommand,
#ifdef NECKO
                                   nsIChannel* aChannel,
                                   nsILoadGroup* aLoadGroup,
#else
                                   nsIURI *aURL,
#endif
                                   nsIContentViewerContainer* aContainer,
                                   nsIStreamListener **aDocListener)
{
		nsresult status;
		nsCOMPtr<nsIParser> parser;
#ifdef NECKO
    nsCOMPtr<nsIURI> aURL;
    status = aChannel->GetURI(getter_AddRefs(aURL));
    if (NS_FAILED(status)) return status;
#endif

		do
			{
#ifdef NECKO
        status = PrepareToLoad(&parser, aContainer, aCommand, aChannel, aLoadGroup);
#else
        status = PrepareToLoad(&parser, aContainer, aCommand, aURL);
#endif
				if ( NS_FAILED(status) )
					break;

				{
					nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser, &status);
					if ( NS_FAILED(status) )
						{
		        	NS_ERROR("parser doesn't support nsIStreamListener");
							break;
						}

					*aDocListener = listener;
					NS_IF_ADDREF(*aDocListener);
				}

				parser->Parse(aURL);
			}
		while(0);
   
    return status;
}

nsresult
XULDocumentImpl::LoadFromStream( nsIInputStream& xulStream,
																 nsIContentViewerContainer* aContainer,
																 const char* aCommand )
	{
		nsresult status;
		nsCOMPtr<nsIParser> parser;
#ifdef NECKO
		if ( NS_SUCCEEDED(status = PrepareToLoad(&parser, aContainer, aCommand, nsnull, nsnull)) )
#else
		if ( NS_SUCCEEDED(status = PrepareToLoad(&parser, aContainer, aCommand, nsnull)) )
#endif
			parser->Parse(xulStream);

		return status;
	}

const nsString*
XULDocumentImpl::GetDocumentTitle() const
{
    return &mDocumentTitle;
}

nsIURI* 
XULDocumentImpl::GetDocumentURL() const
{
    nsIURI* result = mDocumentURL;
    NS_IF_ADDREF(result);
    return result;
}

nsIPrincipal* 
XULDocumentImpl::GetDocumentPrincipal() const
{
    nsIPrincipal* result = mDocumentPrincipal;
    NS_IF_ADDREF(result);
    return result;
}


nsILoadGroup* 
XULDocumentImpl::GetDocumentLoadGroup() const
{
    nsILoadGroup* result = mDocumentLoadGroup;
    NS_IF_ADDREF(result);
    return result;
}

NS_IMETHODIMP 
XULDocumentImpl::GetBaseURL(nsIURI*& aURL) const
{
    aURL = mDocumentURL;
    NS_IF_ADDREF(aURL);
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetDocumentCharacterSet(nsString& oCharSetID) 
{
    oCharSetID = mCharSetID;
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::SetDocumentCharacterSet(const nsString& aCharSetID)
{
    mCharSetID = aCharSetID;
    return NS_OK;
}


NS_IMETHODIMP 
XULDocumentImpl::GetLineBreaker(nsILineBreaker** aResult) 
{
  if(! mLineBreaker) {
     // no line breaker, find a default one
     nsILineBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          kILineBreakerFactoryIID,
                                          (nsISupports **)&lf);
     if (NS_SUCCEEDED(result)) {
      nsILineBreaker *lb = nsnull ;
      nsAutoString lbarg("");
      result = lf->GetBreaker(lbarg, &lb);
      if(NS_SUCCEEDED(result)) {
         mLineBreaker = lb;
      }
      result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
     }
  }
  *aResult = mLineBreaker;
  NS_IF_ADDREF(*aResult);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP 
XULDocumentImpl::SetLineBreaker(nsILineBreaker* aLineBreaker) 
{
  mLineBreaker = dont_QueryInterface(aLineBreaker);
  return NS_OK;
}
NS_IMETHODIMP 
XULDocumentImpl::GetWordBreaker(nsIWordBreaker** aResult) 
{
  if (! mWordBreaker) {
     // no line breaker, find a default one
     nsIWordBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          kIWordBreakerFactoryIID,
                                          (nsISupports **)&lf);
     if (NS_SUCCEEDED(result)) {
      nsIWordBreaker *lb = nsnull ;
      nsAutoString lbarg("");
      result = lf->GetBreaker(lbarg, &lb);
      if(NS_SUCCEEDED(result)) {
         mWordBreaker = lb;
      }
      result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
     }
  }
  *aResult = mWordBreaker;
  NS_IF_ADDREF(*aResult);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP 
XULDocumentImpl::SetWordBreaker(nsIWordBreaker* aWordBreaker) 
{
  mWordBreaker = dont_QueryInterface(aWordBreaker);
  return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetHeaderData(nsIAtom* aHeaderField, nsString& aData) const
{
  return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl:: SetHeaderData(nsIAtom* aheaderField, const nsString& aData)
{
  return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::CreateShell(nsIPresContext* aContext,
                             nsIViewManager* aViewManager,
                             nsIStyleSet* aStyleSet,
                             nsIPresShell** aInstancePtrResult)
{
    NS_PRECONDITION(aInstancePtrResult, "null ptr");
    if (! aInstancePtrResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsIPresShell* shell;
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kPresShellCID,
                                                    nsnull,
                                                    kIPresShellIID,
                                                    (void**) &shell)))
        return rv;

    if (NS_FAILED(rv = shell->Init(this, aContext, aViewManager, aStyleSet))) {
        NS_RELEASE(shell);
        return rv;
    }

    mPresShells.AppendElement(shell);
    *aInstancePtrResult = shell; // addref implicit in CreateInstance()

    return NS_OK;
}

PRBool 
XULDocumentImpl::DeleteShell(nsIPresShell* aShell)
{
    return mPresShells.RemoveElement(aShell);
}

PRInt32 
XULDocumentImpl::GetNumberOfShells()
{
    return mPresShells.Count();
}

nsIPresShell* 
XULDocumentImpl::GetShellAt(PRInt32 aIndex)
{
    nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[aIndex]);
    NS_IF_ADDREF(shell);
    return shell;
}

nsIDocument* 
XULDocumentImpl::GetParentDocument()
{
    NS_IF_ADDREF(mParentDocument);
    return mParentDocument;
}

void 
XULDocumentImpl::SetParentDocument(nsIDocument* aParent)
{
    // Note that we do *not* AddRef our parent because that would
    // create a circular reference.
    mParentDocument = aParent;
}

void 
XULDocumentImpl::AddSubDocument(nsIDocument* aSubDoc)
{
    NS_ADDREF(aSubDoc);
    mSubDocuments.AppendElement(aSubDoc);
}

PRInt32 
XULDocumentImpl::GetNumberOfSubDocuments()
{
    return mSubDocuments.Count();
}

nsIDocument* 
XULDocumentImpl::GetSubDocumentAt(PRInt32 aIndex)
{
    nsIDocument* doc = (nsIDocument*) mSubDocuments.ElementAt(aIndex);
    if (nsnull != doc) {
        NS_ADDREF(doc);
    }
    return doc;
}

nsIContent* 
XULDocumentImpl::GetRootContent()
{
    nsIContent* result = mRootContent;
    NS_IF_ADDREF(result);
    return result;
}

void 
XULDocumentImpl::SetRootContent(nsIContent* aRoot)
{
    if (mRootContent) {
        mRootContent->SetDocument(nsnull, PR_TRUE);
    }
    mRootContent = aRoot;
    if (mRootContent) {
        mRootContent->SetDocument(this, PR_TRUE);
    }
}

NS_IMETHODIMP 
XULDocumentImpl::AppendToProlog(nsIContent* aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
XULDocumentImpl::AppendToEpilog(nsIContent* aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
XULDocumentImpl::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
XULDocumentImpl::IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
XULDocumentImpl::GetChildCount(PRInt32& aCount)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

PRInt32 
XULDocumentImpl::GetNumberOfStyleSheets()
{
    return mStyleSheets.Count();
}

nsIStyleSheet* 
XULDocumentImpl::GetStyleSheetAt(PRInt32 aIndex)
{
    nsIStyleSheet* sheet = NS_STATIC_CAST(nsIStyleSheet*, mStyleSheets[aIndex]);
    NS_IF_ADDREF(sheet);
    return sheet;
}

PRInt32 
XULDocumentImpl::GetIndexOfStyleSheet(nsIStyleSheet* aSheet)
{
  return mStyleSheets.IndexOf(aSheet);
}

void 
XULDocumentImpl::AddStyleSheet(nsIStyleSheet* aSheet)
{
    NS_PRECONDITION(aSheet, "null arg");
    if (!aSheet)
        return;

    if (aSheet == mAttrStyleSheet.get()) {  // always first
      mStyleSheets.InsertElementAt(aSheet, 0);
    }
    else if (aSheet == (nsIHTMLCSSStyleSheet*)mInlineStyleSheet) {  // always last
      mStyleSheets.AppendElement(aSheet);
    }
    else {
      if ((nsIHTMLCSSStyleSheet*)mInlineStyleSheet == mStyleSheets.ElementAt(mStyleSheets.Count() - 1)) {
        // keep attr sheet last
        mStyleSheets.InsertElementAt(aSheet, mStyleSheets.Count() - 1);
      }
      else {
        mStyleSheets.AppendElement(aSheet);
      }
    }
    NS_ADDREF(aSheet);

    aSheet->SetOwningDocument(this);

    PRBool enabled;
    aSheet->GetEnabled(enabled);

    if (enabled) {
        PRInt32 count, i;

        count = mPresShells.Count();
        for (i = 0; i < count; i++) {
            nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);
            nsCOMPtr<nsIStyleSet> set;
            shell->GetStyleSet(getter_AddRefs(set));
            if (set) {
                set->AddDocStyleSheet(aSheet, this);
            }
        }

        // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
        for (i = 0; i < mObservers.Count(); i++) {
            nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
            observer->StyleSheetAdded(this, aSheet);
            if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
              i--;
            }
        }
    }
}

NS_IMETHODIMP
XULDocumentImpl::InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex, PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aSheet, "null ptr");
  mStyleSheets.InsertElementAt(aSheet, aIndex + 1); // offset by one for attribute sheet

  NS_ADDREF(aSheet);
  aSheet->SetOwningDocument(this);

  PRBool enabled = PR_TRUE;
  aSheet->GetEnabled(enabled);

  PRInt32 count;
  PRInt32 i;
  if (enabled) {
    count = mPresShells.Count();
    for (i = 0; i < count; i++) {
      nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(i);
      nsCOMPtr<nsIStyleSet> set;
      shell->GetStyleSet(getter_AddRefs(set));
      if (set) {
        set->AddDocStyleSheet(aSheet, this);
      }
    }
  }
  if (aNotify) {  // notify here even if disabled, there may have been others that weren't notified
    for (i = 0; i < mObservers.Count(); i++) {
      nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
      observer->StyleSheetAdded(this, aSheet);
      if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
        i--;
      }
    }
  }
  return NS_OK;
}

void 
XULDocumentImpl::SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                          PRBool aDisabled)
{
    NS_PRECONDITION(nsnull != aSheet, "null arg");
    PRInt32 count;
    PRInt32 i;

    // If we're actually in the document style sheet list
    if (-1 != mStyleSheets.IndexOf((void *)aSheet)) {
        count = mPresShells.Count();
        for (i = 0; i < count; i++) {
            nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(i);
            nsCOMPtr<nsIStyleSet> set;
            shell->GetStyleSet(getter_AddRefs(set));
            if (set) {
                if (aDisabled) {
                    set->RemoveDocStyleSheet(aSheet);
                }
                else {
                    set->AddDocStyleSheet(aSheet, this);  // put it first
                }
            }
        }
    }  

    for (i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
        observer->StyleSheetDisabledStateChanged(this, aSheet, aDisabled);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
}

NS_IMETHODIMP
XULDocumentImpl::GetCSSLoader(nsICSSLoader*& aLoader)
{
  nsresult result = NS_OK;
  if (! mCSSLoader) {
    result = nsComponentManager::CreateInstance(kCSSLoaderCID,
                                                nsnull,
                                                kICSSLoaderIID,
                                                (void**)&mCSSLoader);
    if (NS_SUCCEEDED(result)) {
      result = mCSSLoader->Init(this);
      mCSSLoader->SetCaseSensitive(PR_TRUE);
      mCSSLoader->SetQuirkMode(PR_FALSE); // no quirks in XUL
    }
  }
  aLoader = mCSSLoader;
  NS_IF_ADDREF(aLoader);
  return result;
}

nsIScriptContextOwner *
XULDocumentImpl::GetScriptContextOwner()
{
    NS_IF_ADDREF(mScriptContextOwner);
    return mScriptContextOwner;
}

void 
XULDocumentImpl::SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner)
{
    // XXX HACK ALERT! If the script context owner is null, the document
    // will soon be going away. So tell our content that to lose its
    // reference to the document. This has to be done before we
    // actually set the script context owner to null so that the
    // content elements can remove references to their script objects.
    if (!aScriptContextOwner && mRootContent)
        mRootContent->SetDocument(nsnull, PR_TRUE);

    mScriptContextOwner = aScriptContextOwner;
}

NS_IMETHODIMP
XULDocumentImpl::GetNameSpaceManager(nsINameSpaceManager*& aManager)
{
  aManager = mNameSpaceManager;
  NS_IF_ADDREF(aManager);
  return NS_OK;
}


// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void 
XULDocumentImpl::AddObserver(nsIDocumentObserver* aObserver)
{
    // XXX Make sure the observer isn't already in the list
    if (mObservers.IndexOf(aObserver) == -1) {
        mObservers.AppendElement(aObserver);
    }
}

PRBool 
XULDocumentImpl::RemoveObserver(nsIDocumentObserver* aObserver)
{
    return mObservers.RemoveElement(aObserver);
}

NS_IMETHODIMP 
XULDocumentImpl::BeginLoad()
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->BeginLoad(this);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::EndLoad()
{
    // Set up the document's composite datasource, which will include
    // the main document datasource and the local store. Only do this
    // if we are a bona-fide top-level XUL document; (mParentContentSink !=
    // nsnull) implies we are a XUL overlay.
    if (mParentContentSink == nsnull) {
        //NS_PRECONDITION(mRootResource != nsnull, "no root resource");
        if (! mRootResource)
            return NS_ERROR_UNEXPECTED;

        nsresult rv;

        nsCOMPtr<nsIRDFCompositeDataSource> db;
        rv = nsComponentManager::CreateInstance(kRDFCompositeDataSourceCID,
                                                nsnull,
                                                kIRDFCompositeDataSourceIID,
                                                (void**) getter_AddRefs(db));

        NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't create composite datasource");
        if (NS_FAILED(rv)) return rv;

        // Create a XUL content model builder
        rv = nsComponentManager::CreateInstance(kRDFXULBuilderCID,
                                                nsnull,
                                                kIRDFContentModelBuilderIID,
                                                getter_AddRefs(mXULBuilder));

        NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't create XUL builder");
        if (NS_FAILED(rv)) return rv;

        rv = mXULBuilder->SetDataBase(db);
        NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't set builder's db");
        if (NS_FAILED(rv)) return rv;

        mBuilders = nsnull; // release content model builders.
        rv = AddContentModelBuilder(mXULBuilder);
        NS_ASSERTION(NS_SUCCEEDED(rv), "could't add XUL builder");
        if (NS_FAILED(rv)) return rv;

        // Add the main document datasource to the content model builder
        rv = db->AddDataSource(mDocumentDataSource);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add XUL datasource to db");
        if (NS_FAILED(rv)) return rv;

#ifdef USE_LOCAL_STORE
        // Add the local store to the composite datasource.
        rv = gRDFService->GetDataSource("rdf:local-store", &mLocalDataSource);

        if (NS_FAILED(rv)) {
            NS_ERROR("couldn't create local data source");
            return rv;
        }

        rv = db->AddDataSource(mLocalDataSource);
        NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't add local data source to db");
        if (NS_FAILED(rv)) return rv;
#endif

        // Now create the root content for the document.
        rv = mXULBuilder->CreateRootContent(mRootResource);
        if (NS_FAILED(rv)) return rv;

        NS_POSTCONDITION(mRootContent != nsnull, "unable to create root content");
        if (! mRootContent)
            return NS_ERROR_UNEXPECTED;
    }

    StartLayout();

    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->EndLoad(this);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }

    return NS_OK;
}


NS_IMETHODIMP 
XULDocumentImpl::ContentChanged(nsIContent* aContent,
                              nsISupports* aSubContent)
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentChanged(this, aContent, aSubContent);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentStatesChanged(nsIContent* aContent1, nsIContent* aContent2)
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentStatesChanged(this, aContent1, aContent2);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::AttributeChanged(nsIContent* aElement,
                                  nsIAtom* aAttribute,
                                  PRInt32 aHint)
{
    nsresult rv;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    // First see if we need to update our element map.
    if (nameSpaceID == kNameSpaceID_HTML) {
        if ((aAttribute == kIdAtom) || (aAttribute == kRefAtom)) {

            rv = mResources.Enumerate(RemoveElementsFromMapByContent, nsnull);
            if (NS_FAILED(rv)) return rv;

            // That'll have removed _both_ the 'ref' and 'id' entries from
            // the map. So add 'em back now.
            rv = AddElementToMap(aElement, PR_FALSE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->AttributeChanged(this, aElement, aAttribute, aHint);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }

    // Handle "special" cases. We do this handling _after_ we've
    // notified the observer to ensure that any frames that are
    // caching information (e.g., the tree widget and the 'open'
    // attribute) will notice things properly.
    if (nameSpaceID == kNameSpaceID_XUL) {
        if (aAttribute == kOpenAtom) {
            nsAutoString open;
            rv = aElement->GetAttribute(kNameSpaceID_None, kOpenAtom, open);
            if (NS_FAILED(rv)) return rv;

            if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (open.Equals("true"))) {
                OpenWidgetItem(aElement);
            }
            else {
                CloseWidgetItem(aElement);
            }
        }
        else if (aAttribute == kRefAtom) {
            RebuildWidgetItem(aElement);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentAppended(nsIContent* aContainer,
                                 PRInt32 aNewIndexInContainer)
{
    // First update our element map
    {
        nsresult rv;

        PRInt32 count;
        rv = aContainer->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = aNewIndexInContainer; i < count; ++i) {
            nsCOMPtr<nsIContent> child;
            rv = aContainer->ChildAt(i, *getter_AddRefs(child));
            if (NS_FAILED(rv)) return rv;

            rv = AddElementToMap(child, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentAppended(this, aContainer, aNewIndexInContainer);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentInserted(nsIContent* aContainer,
                                 nsIContent* aChild,
                                 PRInt32 aIndexInContainer)
{
    // First update our element map
    {
        nsresult rv;
        rv = AddElementToMap(aChild, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentInserted(this, aContainer, aChild, aIndexInContainer);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentReplaced(nsIContent* aContainer,
                                 nsIContent* aOldChild,
                                 nsIContent* aNewChild,
                                 PRInt32 aIndexInContainer)
{
    // First update our element map
    {
        nsresult rv;
        rv = RemoveElementFromMap(aOldChild, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

        rv = AddElementToMap(aNewChild, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentReplaced(this, aContainer, aOldChild, aNewChild,
                                  aIndexInContainer);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::ContentRemoved(nsIContent* aContainer,
                                nsIContent* aChild,
                                PRInt32 aIndexInContainer)
{
    // First update our element map
    {
        nsresult rv;
        rv = RemoveElementFromMap(aChild, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentRemoved(this, aContainer, 
                                 aChild, aIndexInContainer);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                                  nsIStyleRule* aStyleRule,
                                  PRInt32 aHint)
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleChanged(this, aStyleSheet, aStyleRule, aHint);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule)
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleAdded(this, aStyleSheet, aStyleRule);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                                  nsIStyleRule* aStyleRule)
{
    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleRemoved(this, aStyleSheet, aStyleRule);
        if (observer != (nsIDocumentObserver*)mObservers.ElementAt(i)) {
          i--;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::GetSelection(nsIDOMSelection** aSelection)
{
    if (!mSelection) {
        PR_ASSERT(0);
        *aSelection = nsnull;
        return NS_ERROR_NOT_INITIALIZED;
    }
    *aSelection = mSelection;
    NS_ADDREF(*aSelection);
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::SelectAll()
{

    nsIContent * start = nsnull;
    nsIContent * end   = nsnull;
    nsIContent * body  = nsnull;

    nsAutoString bodyStr("BODY");
    PRInt32 i, n;
    mRootContent->ChildCount(n);
    for (i=0;i<n;i++) {
        nsIContent * child;
        mRootContent->ChildAt(i, child);
        PRBool isSynthetic;
        child->IsSynthetic(isSynthetic);
        if (!isSynthetic) {
            nsIAtom * atom;
            child->GetTag(atom);
            if (bodyStr.EqualsIgnoreCase(atom)) {
                body = child;
                break;
            }

        }
        NS_RELEASE(child);
    }

    if (body == nsnull) {
        return NS_ERROR_FAILURE;
    }

    start = body;
    // Find Very first Piece of Content
    for (;;) {
        start->ChildCount(n);
        if (n <= 0) {
            break;
        }
        nsIContent * child = start;
        child->ChildAt(0, start);
        NS_RELEASE(child);
    }

    end = body;
    // Last piece of Content
    for (;;) {
        end->ChildCount(n);
        if (n <= 0) {
            break;
        }
        nsIContent * child = end;
        child->ChildAt(n-1, end);
        NS_RELEASE(child);
    }

    //NS_RELEASE(start);
    //NS_RELEASE(end);

#if 0 // XXX nsSelectionRange is in another DLL
    nsSelectionRange * range    = mSelection->GetRange();
    nsSelectionPoint * startPnt = range->GetStartPoint();
    nsSelectionPoint * endPnt   = range->GetEndPoint();
    startPnt->SetPoint(start, -1, PR_TRUE);
    endPnt->SetPoint(end, -1, PR_FALSE);
#endif
    SetDisplaySelection(PR_TRUE);

    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
    aIsFound = PR_FALSE;
    return NS_ERROR_FAILURE;
}

void 
XULDocumentImpl::CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection)
{
    PR_ASSERT(0);
}

void 
XULDocumentImpl::ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
XULDocumentImpl::BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
XULDocumentImpl::ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
XULDocumentImpl::FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

PRBool 
XULDocumentImpl::IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const
{
    PRBool  result;

    if (aStartContent == aEndContent) {
            return PRBool(aContent == aStartContent);
    }
    else if (aStartContent == aContent || aEndContent == aContent) {
        result = PR_TRUE;
    }
    else {
        result = IsBefore(aStartContent,aContent);
        if (result)
            result = IsBefore(aContent, aEndContent);
    }
    return result;
}

PRBool 
XULDocumentImpl::IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const
{
    PRBool result = PR_FALSE;

    if (nsnull != aNewContent && nsnull != aCurrentContent && aNewContent != aCurrentContent) {
        nsIContent* test = FindContent(mRootContent, aNewContent, aCurrentContent);
        if (test == aNewContent)
            result = PR_TRUE;

        NS_RELEASE(test);
    }
    return result;
}

PRBool 
XULDocumentImpl::IsInSelection(nsIDOMSelection* aSelection, const nsIContent *aContent) const
{
    PRBool  result = PR_FALSE;

    if (mSelection != nsnull) {
#if 0 // XXX can't include this because nsSelectionPoint is in another DLL.
        nsSelectionRange* range = mSelection->GetRange();
        if (range != nsnull) {
            nsSelectionPoint* startPoint = range->GetStartPoint();
            nsSelectionPoint* endPoint = range->GetEndPoint();

            nsIContent* startContent = startPoint->GetContent();
            nsIContent* endContent = endPoint->GetContent();
            result = IsInRange(startContent, endContent, aContent);
            NS_IF_RELEASE(startContent);
            NS_IF_RELEASE(endContent);
        }
#endif
    }
    return result;
}

nsIContent* 
XULDocumentImpl::GetPrevContent(const nsIContent *aContent) const
{
    nsIContent* result = nsnull;
 
    // Look at previous sibling

    if (nsnull != aContent) {
        nsIContent* parent; 
        aContent->GetParent(parent);

        if (parent && parent != mRootContent.get()) {
            PRInt32 i;
            parent->IndexOf((nsIContent*)aContent, i);
            if (i > 0)
                parent->ChildAt(i - 1, result);
            else
                result = GetPrevContent(parent);
        }
        NS_IF_RELEASE(parent);
    }
    return result;
}

nsIContent* 
XULDocumentImpl::GetNextContent(const nsIContent *aContent) const
{
    nsIContent* result = nsnull;
   
    if (nsnull != aContent) {
        // Look at next sibling
        nsIContent* parent;
        aContent->GetParent(parent);

        if (parent != nsnull && parent != mRootContent.get()) {
            PRInt32 i;
            parent->IndexOf((nsIContent*)aContent, i);

            PRInt32 count;
            parent->ChildCount(count);
            if (i + 1 < count) {
                parent->ChildAt(i + 1, result);
                // Get first child down the tree
                for (;;) {
                    PRInt32 n;
                    result->ChildCount(n);
                    if (n <= 0)
                        break;

                    nsIContent * old = result;
                    old->ChildAt(0, result);
                    NS_RELEASE(old);
                    result->ChildCount(n);
                }
            } else {
                result = GetNextContent(parent);
            }
        }
        NS_IF_RELEASE(parent);
    }
    return result;
}

void 
XULDocumentImpl::SetDisplaySelection(PRBool aToggle)
{
    mDisplaySelection = aToggle;
}

PRBool 
XULDocumentImpl::GetDisplaySelection() const
{
    return mDisplaySelection;
}

NS_IMETHODIMP 
XULDocumentImpl::HandleDOMEvent(nsIPresContext& aPresContext, 
                            nsEvent* aEvent, 
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
  nsresult ret = NS_OK;
  nsIDOMEvent* domEvent = nsnull;

  if (NS_EVENT_FLAG_INIT == aFlags) {
    aDOMEvent = &domEvent;
    aEvent->flags = NS_EVENT_FLAG_NONE;
  }
  
  //Capturing stage
  if (NS_EVENT_FLAG_BUBBLE != aFlags && nsnull != mScriptContextOwner) {
    nsIScriptGlobalObject* global;
    if (NS_OK == mScriptContextOwner->GetScriptGlobalObject(&global)) {
      global->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
      NS_RELEASE(global);
    }
  }
  
  //Local handling stage
  if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
    aEvent->flags = aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_CAPTURE != aFlags && nsnull != mScriptContextOwner) {
    nsIScriptGlobalObject* global;
    if (NS_OK == mScriptContextOwner->GetScriptGlobalObject(&global)) {
      global->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_BUBBLE, aEventStatus);
      NS_RELEASE(global);
    }
  }

  if (NS_EVENT_FLAG_INIT == aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event, release here.
    if (nsnull != *aDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
      //Okay, so someone in the DOM loop (a listener, JS object) still has a ref to the DOM Event but
      //the internal data hasn't been malloc'd.  Force a copy of the data here so the DOM Event is still valid.
        nsIPrivateDOMEvent *privateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(kIPrivateDOMEventIID, (void**)&privateEvent)) {
          privateEvent->DuplicatePrivateData();
          NS_RELEASE(privateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  return ret;
}


////////////////////////////////////////////////////////////////////////
// nsIXMLDocument interface
NS_IMETHODIMP 
XULDocumentImpl::GetContentById(const nsString& aName, nsIContent** aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef XSL
NS_IMETHODIMP 
XULDocumentImpl::SetTransformMediator(nsITransformMediator* aMediator)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

////////////////////////////////////////////////////////////////////////
// nsIRDFDocument interface

NS_IMETHODIMP
XULDocumentImpl::SetRootResource(nsIRDFResource* aResource)
{
    mRootResource = dont_QueryInterface(aResource);
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::SplitProperty(nsIRDFResource* aProperty,
                               PRInt32* aNameSpaceID,
                               nsIAtom** aTag)
{
    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    // XXX okay, this is a total hack. for this to work right, we need
    // to either:
    //
    // 1) Remember what the namespace and the tag were when the
    //    property was created, or
    //
    // 2) Iterate the entire set of namespace prefixes to see if the
    //    specified property's URI has any of them as a substring.
    //
    nsresult rv;

    const char* p;
    rv = aProperty->GetValueConst(&p);
    if (NS_FAILED(rv)) return rv;

    if (! p)
        return NS_ERROR_UNEXPECTED;

    nsAutoString uri(p);

    // First try to split the namespace using the rightmost '#' or '/'
    // character.
    PRInt32 i;
    if ((i = uri.RFindChar('#')) < 0) {
        if ((i = uri.RFindChar('/')) < 0) {
            *aNameSpaceID = kNameSpaceID_None;
            *aTag = NS_NewAtom(uri);
            return NS_OK;
        }
    }

    // Using the above info, split the property's URI into a namespace
    // prefix (that includes the trailing '#' or '/' character) and a
    // tag.
    nsAutoString tag;
    PRInt32 count = uri.Length() - (i + 1);
    uri.Right(tag, count);
    uri.Cut(i + 1, count);

    // XXX XUL atoms seem to all be in lower case. HTML atoms are in
    // upper case. This sucks.
    //tag.ToUpperCase();  // All XML is case sensitive even HTML in XML
    PRInt32 nameSpaceID;
    rv = mNameSpaceManager->GetNameSpaceID(uri, nameSpaceID);

    // Did we find one?
    if (NS_SUCCEEDED(rv) && nameSpaceID != kNameSpaceID_Unknown) {
        *aNameSpaceID = nameSpaceID;

        *aTag = NS_NewAtom(tag);
        return NS_OK;
    }

    // Nope: so we'll need to be a bit more creative. Let's see
    // what happens if we remove the rightmost '#' and then try...
    if (uri.Last() == '#') {
        uri.Truncate(uri.Length() - 1);
        rv = mNameSpaceManager->GetNameSpaceID(uri, nameSpaceID);

        if (NS_SUCCEEDED(rv) && nameSpaceID != kNameSpaceID_Unknown) {
            *aNameSpaceID = nameSpaceID;

            *aTag = NS_NewAtom(tag);
            return NS_OK;
        }
    }

    // allright, if we at least have a tag, then we can return the
    // thing with kNameSpaceID_None for now.
    if (tag.Length()) {
        *aNameSpaceID = kNameSpaceID_None;
        *aTag = NS_NewAtom(tag);
        return NS_OK;
    }

    // Okay, somebody make this code even more convoluted.
    NS_ERROR("unable to convert URI to namespace/tag pair");
    return NS_ERROR_FAILURE; // XXX?
}


NS_IMETHODIMP
XULDocumentImpl::AddElementForResource(nsIRDFResource* aResource, nsIContent* aElement)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mResources.Add(aResource, aElement);
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::RemoveElementForResource(nsIRDFResource* aResource, nsIContent* aElement)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mResources.Remove(aResource, aElement);
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements)
{
    NS_PRECONDITION(aElements != nsnull, "null ptr");
    if (! aElements)
        return NS_ERROR_NULL_POINTER;

    mResources.Find(aResource, aElements);
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::CreateContents(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    if (! mBuilders)
        return NS_ERROR_NOT_INITIALIZED;

    PRUint32 cnt = 0;
    nsresult rv = mBuilders->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    for (PRUint32 i = 0; i < cnt; ++i) {
        // XXX we should QueryInterface() here
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        NS_ASSERTION(builder != nsnull, "null ptr");
        if (! builder)
            continue;

        rv = builder->CreateContents(aElement);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error creating content");
        // XXX ignore error code?

        NS_RELEASE(builder);
    }

    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::AddContentModelBuilder(nsIRDFContentModelBuilder* aBuilder)
{
    NS_PRECONDITION(aBuilder != nsnull, "null ptr");
    if (! aBuilder)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    if (! mBuilders) {
        rv = NS_NewISupportsArray(getter_AddRefs(mBuilders));
        if (NS_FAILED(rv)) return rv;
    }

    rv = aBuilder->SetDocument(this);
    if (NS_FAILED(rv)) return rv;

    return mBuilders->AppendElement(aBuilder) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
XULDocumentImpl::GetDocumentDataSource(nsIRDFDataSource** aDataSource)
{
    *aDataSource = mDocumentDataSource;
    NS_IF_ADDREF(*aDataSource);
    return NS_OK;
}



NS_IMETHODIMP
XULDocumentImpl::GetForm(nsIDOMHTMLFormElement** aForm)
{
  *aForm = mHiddenForm;
  NS_IF_ADDREF(*aForm);
  return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::SetForm(nsIDOMHTMLFormElement* aForm)
{
    mHiddenForm = dont_QueryInterface(aForm);

    // Set the document.
    nsCOMPtr<nsIContent> formContent = do_QueryInterface(aForm);
    formContent->SetDocument(this, PR_TRUE);

    // Forms are containers, and as such take up a bit of space.
    // Set a style attribute to keep the hidden form from showing up.
    mHiddenForm->SetAttribute("style", "margin:0em");
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////
// nsIDOMDocument interface
NS_IMETHODIMP
XULDocumentImpl::GetDoctype(nsIDOMDocumentType** aDoctype)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetImplementation(nsIDOMDOMImplementation** aImplementation)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetDocumentElement(nsIDOMElement** aDocumentElement)
{
    NS_PRECONDITION(aDocumentElement != nsnull, "null ptr");
    if (! aDocumentElement)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(nsIDOMElement::GetIID(), (void**)aDocumentElement);
    }
    else {
        *aDocumentElement = nsnull;
        return NS_OK;
    }
}



NS_IMETHODIMP
XULDocumentImpl::CreateElement(const nsString& aTagName, nsIDOMElement** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
      char* tagCStr = aTagName.ToNewCString();

      PR_LOG(gXULLog, PR_LOG_DEBUG,
             ("xul[CreateElement] %s", tagCStr));

      delete[] tagCStr;
    }
#endif

    nsCOMPtr<nsIAtom> name;
    PRInt32 nameSpaceID;

    *aReturn = nsnull;

    // parse the user-provided string into a tag name and a namespace ID
    rv = ParseTagString(aTagName, *getter_AddRefs(name), nameSpaceID);
    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        char* tagNameStr = aTagName.ToNewCString();
        PR_LOG(gXULLog, PR_LOG_ERROR,
               ("xul[CreateElement] unable to parse tag '%s'; no such namespace.", tagNameStr));
        delete[] tagNameStr;
#endif
        return rv;
    }

    nsCOMPtr<nsIContent> result;
    rv = mXULBuilder->CreateElement(nameSpaceID, name, nsnull, getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;

    // get the DOM interface
    rv = result->QueryInterface(nsIDOMElement::GetIID(), (void**) aReturn);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateTextNode(const nsString& aData, nsIDOMText** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsITextContent> text;
    rv = nsComponentManager::CreateInstance(kTextNodeCID, nsnull, nsITextContent::GetIID(), getter_AddRefs(text));
    if (NS_FAILED(rv)) return rv;

    rv = text->SetText(aData.GetUnicode(), aData.Length(), PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    rv = text->QueryInterface(nsIDOMText::GetIID(), (void**) aReturn);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOMText");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::CreateComment(const nsString& aData, nsIDOMComment** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateAttribute(const nsString& aName, nsIDOMAttr** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::GetElementsByTagName(const nsString& aTagName, nsIDOMNodeList** aReturn)
{
    nsresult rv;
    nsRDFDOMNodeList* elements;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
        NS_ERROR("unable to create node list");
        return rv;
    }

    nsIContent* root = GetRootContent();
    NS_ASSERTION(root != nsnull, "no doc root");

    if (root != nsnull) {
        nsIDOMNode* domRoot;
        if (NS_SUCCEEDED(rv = root->QueryInterface(nsIDOMNode::GetIID(), (void**) &domRoot))) {
            rv = GetElementsByTagName(domRoot, aTagName, elements);
            NS_RELEASE(domRoot);
        }
        NS_RELEASE(root);
    }

    *aReturn = elements;
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetElementsByAttribute(const nsString& aAttribute, const nsString& aValue, 
                                        nsIDOMNodeList** aReturn)
{
    nsresult rv;
    nsRDFDOMNodeList* elements;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
        NS_ERROR("unable to create node list");
        return rv;
    }

    nsIContent* root = GetRootContent();
    NS_ASSERTION(root != nsnull, "no doc root");

    if (root != nsnull) {
        nsIDOMNode* domRoot;
        if (NS_SUCCEEDED(rv = root->QueryInterface(nsIDOMNode::GetIID(), (void**) &domRoot))) {
            rv = GetElementsByAttribute(domRoot, aAttribute, aValue, elements);
            NS_RELEASE(domRoot);
        }
        NS_RELEASE(root);
    }

    *aReturn = elements;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMNSDocument interface

NS_IMETHODIMP
XULDocumentImpl::GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CreateElementWithNameSpace(const nsString& aTagName,
                                            const nsString& aNameSpace,
                                            nsIDOMElement** aResult)
{
    // Create a DOM element given a namespace URI and a tag name.
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
      char* namespaceCStr = aNameSpace.ToNewCString();
      char* tagCStr = aTagName.ToNewCString();

      PR_LOG(gXULLog, PR_LOG_DEBUG,
             ("xul[CreateElementWithNameSpace] [%s]:%s", namespaceCStr, tagCStr));

      delete[] tagCStr;
      delete[] namespaceCStr;
    }
#endif

    nsCOMPtr<nsIAtom> name = dont_AddRef(NS_NewAtom(aTagName.GetUnicode()));
    if (! name)
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 nameSpaceID;
    rv = mNameSpaceManager->GetNameSpaceID(aNameSpace, nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIContent> result;
    rv = mXULBuilder->CreateElement(nameSpaceID, name, nsnull, getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;

    // get the DOM interface
    rv = result->QueryInterface(nsIDOMElement::GetIID(), (void**) aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::CreateRange(nsIDOMRange** aRange)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMXULDocument interface

NS_IMETHODIMP
XULDocumentImpl::GetPopupElement(nsIDOMElement** anElement)
{
	*anElement = mPopupElement;
	NS_IF_ADDREF(*anElement);
	return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::SetPopupElement(nsIDOMElement* anElement)
{
	mPopupElement = dont_QueryInterface(anElement);
	return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetTooltipElement(nsIDOMElement** anElement)
{
	*anElement = mTooltipElement;
	NS_IF_ADDREF(*anElement);
	return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::SetTooltipElement(nsIDOMElement* anElement)
{
	mTooltipElement = dont_QueryInterface(anElement);
	return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetCommandDispatcher(nsIDOMXULCommandDispatcher** aTracker)
{
  *aTracker = mCommandDispatcher;
  NS_IF_ADDREF(*aTracker);
  return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetElementById(const nsString& aId, nsIDOMElement** aReturn)
{
    NS_PRECONDITION(mRootContent != nsnull, "document contains no content");
    if (! mRootContent)
        return NS_ERROR_NOT_INITIALIZED; // XXX right error code?

    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    rv = nsRDFContentUtils::MakeElementResource(this, aId, getter_AddRefs(resource));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    rv = GetElementsForResource(resource, elements);
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt = 0;
    rv = elements->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    if (cnt > 0) {
        if (cnt > 1) {
            // This is a scary case that our API doesn't deal with well.
            NS_WARNING("more than one element found with specified ID; returning first");
        }

        nsISupports* element = elements->ElementAt(0);
        rv = element->QueryInterface(nsIDOMElement::GetIID(), (void**) aReturn);
        NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
        NS_RELEASE(element);
        return rv;
    }

    // Didn't find it in our element map. Grovel for it.
    *aReturn = nsnull;
    SearchForNodeByID(aId, mRootContent, aReturn);

    return NS_OK;
}

nsIAtom** XULDocumentImpl::kIdentityAttrs[] = { &kIdAtom, &kRefAtom, nsnull };

nsresult
XULDocumentImpl::AddElementToMap(nsIContent* aElement, PRBool aDeep)
{
    nsresult rv;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    if (nameSpaceID == kNameSpaceID_HTML) {
        for (PRInt32 i = 0; kIdentityAttrs[i] != nsnull; ++i) {
            nsAutoString value;
            rv = aElement->GetAttribute(kNameSpaceID_None, *kIdentityAttrs[i], value);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get attribute");
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                nsCOMPtr<nsIRDFResource> resource;
                rv = nsRDFContentUtils::MakeElementResource(this, value, getter_AddRefs(resource));

                rv = mResources.Add(resource, aElement);
                if (NS_FAILED(rv)) return rv;
            }
        }
    }

    if (aDeep) {
        PRInt32 count;
        nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
        rv = xulcontent ? xulcontent->PeekChildCount(count) : aElement->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        while (--count > 0) {
            nsCOMPtr<nsIContent> child;
            rv = aElement->ChildAt(count, *getter_AddRefs(child));
            if (NS_FAILED(rv)) return rv;

            rv = AddElementToMap(child, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


nsresult
XULDocumentImpl::RemoveElementFromMap(nsIContent* aElement, PRBool aDeep)
{
    nsresult rv;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    if (nameSpaceID == kNameSpaceID_HTML) {
        for (PRInt32 i = 0; kIdentityAttrs[i] != nsnull; ++i) {
            nsAutoString value;
            rv = aElement->GetAttribute(kNameSpaceID_None, *kIdentityAttrs[i], value);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get attribute");
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                nsCOMPtr<nsIRDFResource> resource;
                rv = nsRDFContentUtils::MakeElementResource(this, value, getter_AddRefs(resource));

                rv = mResources.Remove(resource, aElement);
                if (NS_FAILED(rv)) return rv;
            }
        }
    }

    if (aDeep) {
        PRInt32 count;
        nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
        rv = xulcontent ? xulcontent->PeekChildCount(count) : aElement->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        while (--count > 0) {
            nsCOMPtr<nsIContent> child;
            rv = aElement->ChildAt(count, *getter_AddRefs(child));
            if (NS_FAILED(rv)) return rv;

            rv = RemoveElementFromMap(child, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


PRIntn
XULDocumentImpl::RemoveElementsFromMapByContent(nsIRDFResource* aResource,
                                                nsIContent* aElement,
                                                void* aClosure)
{
    nsIContent* content = NS_REINTERPRET_CAST(nsIContent*, aClosure);
    return (aElement == content) ? HT_ENUMERATE_REMOVE : HT_ENUMERATE_NEXT;
}


nsresult
XULDocumentImpl::SearchForNodeByID(const nsString& aID, 
                                   nsIContent* aElement,
                                   nsIDOMElement** aReturn)
{
    nsresult rv;

    // See if we match.
    PRInt32 namespaceID;
    rv = aElement->GetNameSpaceID(namespaceID);
    if (NS_FAILED(rv)) return rv;
    
    nsAutoString id;
    rv = aElement->GetAttribute(namespaceID, kIdAtom, id);
    if (NS_FAILED(rv)) return rv;

    if (id == aID) {
        nsCOMPtr<nsIDOMElement> domNode( do_QueryInterface(aElement) );
        NS_ASSERTION(domNode != nsnull, "not a dom node");
        
        if (domNode) {
            *aReturn = domNode;
            NS_ADDREF(*aReturn);
        }
        return NS_OK;
    }

    // Walk children.
    // XXX: Don't descend into closed tree items (or menu items or buttons?).
    PRInt32 childCount;
    aElement->ChildCount(childCount);
    for (PRInt32 i = 0; i < childCount && !(*aReturn); i++) {
        nsCOMPtr<nsIContent> child;
        rv = aElement->ChildAt(i, *getter_AddRefs(child));
        if (NS_FAILED(rv)) return rv;
        
        rv = SearchForNodeByID(aID, child, aReturn);
        if (NS_FAILED(rv)) return rv;

        if (*aReturn)
            return NS_OK;
    }

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIXULParentDocument interface
NS_IMETHODIMP 
XULDocumentImpl::GetContentViewerContainer(nsIContentViewerContainer** aContainer)
{
    NS_PRECONDITION ( aContainer, "Null Parameter into GetContentViewerContainer" );
    
    *aContainer = mContentViewerContainer;
    NS_IF_ADDREF(*aContainer);

    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetCommand(nsString& aCommand)
{
    aCommand = mCommand;
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::LayoutPopupDocument()
{
    StartLayout();
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::CreatePopupDocument(nsIContent* aPopupElement, nsIDocument** aResult)
{
    nsresult rv;

    // Create and addref
    XULDocumentImpl* popupDoc = new XULDocumentImpl();
    if (popupDoc == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(popupDoc);

    // Init our new document
    if (NS_FAILED(rv = popupDoc->Init())) {
      NS_RELEASE(popupDoc);
      return rv;
    }

    // Indicate that we are a popup document
    popupDoc->SetIsPopup(PR_TRUE);

    // Our URL is exactly the same as the parent doc.
    popupDoc->SetDocumentURLAndGroup(mDocumentURL);

    // Set our character set.
    popupDoc->SetDocumentCharacterSet(mCharSetID);

    // Try to share the style sheets (this is evil, but it might just work)
    // First do a prepare to pick up a new inline and attr style sheet
    if (NS_FAILED(rv = popupDoc->PrepareStyleSheets(mDocumentURL))) {
      NS_ERROR("problem initializing style sheets.");
      return rv;
    }
    // Now we need to copy all of the style sheets from the parent document
    PRInt32 count = mStyleSheets.Count();
    for (PRInt32 i = 1; i < count-1; i++) {
      // Don't bother addrefing. We don't live as long as our parent document
        nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(i);
        nsCOMPtr<nsICSSStyleSheet> cssSheet = do_QueryInterface(sheet);
        if (cssSheet) {
          nsCOMPtr<nsICSSStyleSheet> clonedSheet;
          if (NS_SUCCEEDED(cssSheet->Clone(*getter_AddRefs(clonedSheet)))) {
            popupDoc->AddStyleSheet(clonedSheet);
          }
        }
    }

    // We don't share builders, but we do share the DB that the current
    // XUL builder has.
    // Create a XUL content model builder
    rv = nsComponentManager::CreateInstance(kRDFXULBuilderCID,
                                                nsnull,
                                                kIRDFContentModelBuilderIID,
                                                (void**) &(popupDoc->mXULBuilder));

    if (NS_FAILED(rv)) {
        NS_ERROR("couldn't create XUL builder");
        return rv;
    }

    nsCOMPtr<nsIRDFCompositeDataSource> db;
    mXULBuilder->GetDataBase(getter_AddRefs(db));
    if (NS_FAILED(rv = popupDoc->mXULBuilder->SetDataBase(db))) {
        NS_ERROR("couldn't set builder's db");
        return rv;
    }

    if (NS_FAILED(rv = popupDoc->AddContentModelBuilder(popupDoc->mXULBuilder))) {
        NS_ERROR("could't add XUL builder");
        return rv;
    }

    // We share the same data sources
    popupDoc->mLocalDataSource = mLocalDataSource;
    popupDoc->mDocumentDataSource = mDocumentDataSource;

    // We share the same namespace manager
    popupDoc->mNameSpaceManager = mNameSpaceManager;

    // We share the mPopup
    popupDoc->mPopupElement = mPopupElement;

    // Suck all of the root's content into our document.
    // We need to make the XUL builder instantiate this node.
    // Retrieve the resource that corresponds to this node.
    nsAutoString idValue;
    nsCOMPtr<nsIDOMElement> domRoot = do_QueryInterface(aPopupElement);
    domRoot->GetAttribute("id", idValue);

    // Use the absolute URL to retrieve a resource from the RDF
    // service that corresponds to the root content.
    nsCOMPtr<nsIRDFResource> rootResource;
    rv = nsRDFContentUtils::MakeElementResource(this, idValue, getter_AddRefs(rootResource));
    if (NS_FAILED(rv)) return rv;

    // Tell the builder to create its root from this resrouce.
    popupDoc->mXULBuilder->CreateRootContent(rootResource);

    // Now the popup will happily use its own XUL builder to churn out nodal
    // clones of the popup content.  They will get their own event handlers
    // (properly scoped to the new global popup window context), and will
    // belong to the popup document instead.  If this works, it will be
    // more righteous than Star Wars Episode I.

    // Return the doc
    *aResult = popupDoc;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIXULChildDocument interface
NS_IMETHODIMP 
XULDocumentImpl::SetContentSink(nsIXULContentSink* aParentContentSink)
{
    mParentContentSink = aParentContentSink;
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetContentSink(nsIXULContentSink** aParentContentSink)
{
    *aParentContentSink = mParentContentSink;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIDOMNode interface
NS_IMETHODIMP
XULDocumentImpl::GetNodeName(nsString& aNodeName)
{
    aNodeName.SetString("#document");
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetNodeValue(nsString& aNodeValue)
{
    aNodeValue.Truncate();
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::SetNodeValue(const nsString& aNodeValue)
{
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetNodeType(PRUint16* aNodeType)
{
    *aNodeType = nsIDOMNode::DOCUMENT_NODE;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetParentNode(nsIDOMNode** aParentNode)
{
    *aParentNode = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    NS_PRECONDITION(aChildNodes != nsnull, "null ptr");
    if (! aChildNodes)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        nsresult rv;

        *aChildNodes = nsnull;

        nsRDFDOMNodeList* children;
        rv = nsRDFDOMNodeList::Create(&children);

        if (NS_SUCCEEDED(rv)) {
            nsIDOMNode* domNode = nsnull;
            rv = mRootContent->QueryInterface(nsIDOMNode::GetIID(), (void**)&domNode);
            NS_ASSERTION(NS_SUCCEEDED(rv), "root content is not a DOM node");

            if (NS_SUCCEEDED(rv)) {
                rv = children->AppendNode(domNode);
                NS_RELEASE(domNode);

                *aChildNodes = children;
                return NS_OK;
            }
        }

        // If we get here, something bad happened.
        NS_RELEASE(children);
        return rv;
    }
    else {
        *aChildNodes = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
XULDocumentImpl::HasChildNodes(PRBool* aHasChildNodes)
{
    NS_PRECONDITION(aHasChildNodes != nsnull, "null ptr");
    if (! aHasChildNodes)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        *aHasChildNodes = PR_TRUE;
    }
    else {
        *aHasChildNodes = PR_FALSE;
    }
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetFirstChild(nsIDOMNode** aFirstChild)
{
    NS_PRECONDITION(aFirstChild != nsnull, "null ptr");
    if (! aFirstChild)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(nsIDOMNode::GetIID(), (void**) aFirstChild);
    }
    else {
        *aFirstChild = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
XULDocumentImpl::GetLastChild(nsIDOMNode** aLastChild)
{
    NS_PRECONDITION(aLastChild != nsnull, "null ptr");
    if (! aLastChild)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(nsIDOMNode::GetIID(), (void**) aLastChild);
    }
    else {
        *aLastChild = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
XULDocumentImpl::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    NS_PRECONDITION(aPreviousSibling != nsnull, "null ptr");
    if (! aPreviousSibling)
        return NS_ERROR_NULL_POINTER;

    *aPreviousSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetNextSibling(nsIDOMNode** aNextSibling)
{
    NS_PRECONDITION(aNextSibling != nsnull, "null ptr");
    if (! aNextSibling)
        return NS_ERROR_NULL_POINTER;

    *aNextSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    NS_PRECONDITION(aAttributes != nsnull, "null ptr");
    if (! aAttributes)
        return NS_ERROR_NULL_POINTER;

    *aAttributes = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    NS_PRECONDITION(aOwnerDocument != nsnull, "null ptr");
    if (! aOwnerDocument)
        return NS_ERROR_NULL_POINTER;

    *aOwnerDocument = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
XULDocumentImpl::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
XULDocumentImpl::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    // We don't allow cloning of a document
    *aReturn = nsnull;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIJSScriptObject interface
PRBool
XULDocumentImpl::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
XULDocumentImpl::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
XULDocumentImpl::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    PRBool result = PR_TRUE;

    if (JSVAL_IS_STRING(aID) && 
        PL_strcmp("location", JS_GetStringBytes(JS_ValueToString(aContext, aID))) == 0) {
        if (nsnull != mScriptContextOwner) {
            nsIScriptGlobalObject *global;
            mScriptContextOwner->GetScriptGlobalObject(&global);
            if (nsnull != global) {
                nsIJSScriptObject *window;
                if (NS_OK == global->QueryInterface(kIJSScriptObjectIID, (void **)&window)) {
                    result = window->GetProperty(aContext, aID, aVp);
                    NS_RELEASE(window);
                }
                else {
                    result = PR_FALSE;
                }
                NS_RELEASE(global);
            }
        }
    }

    return result;
}


PRBool
XULDocumentImpl::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
    nsresult rv;

    if (JSVAL_IS_STRING(aID)) {
        char* s = JS_GetStringBytes(JS_ValueToString(aContext, aID));
        if (PL_strcmp("title", s) == 0) {
            nsAutoString title("get me out of aVp somehow");
            for (PRInt32 i = mPresShells.Count() - 1; i >= 0; --i) {
                nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);
                nsCOMPtr<nsIPresContext> context;
                rv = shell->GetPresContext(getter_AddRefs(context));
                if (NS_FAILED(rv)) return PR_FALSE;

                nsCOMPtr<nsISupports> container;
                rv = context->GetContainer(getter_AddRefs(container));
                if (NS_FAILED(rv)) return PR_FALSE;

                if (! container) continue;

                nsCOMPtr<nsIWebShell> webshell = do_QueryInterface(container);
                if (! webshell) continue;

                rv = webshell->SetTitle(title.GetUnicode());
                if (NS_FAILED(rv)) return PR_FALSE;
            }
        }
        else if (PL_strcmp("location", s) == 0) {
            NS_NOTYETIMPLEMENTED("write me");
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}


PRBool
XULDocumentImpl::EnumerateProperty(JSContext *aContext)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


PRBool
XULDocumentImpl::Resolve(JSContext *aContext, jsval aID)
{
    return PR_TRUE;
}


PRBool
XULDocumentImpl::Convert(JSContext *aContext, jsval aID)
{
    NS_NOTYETIMPLEMENTED("write me");
    return PR_TRUE;
}


void
XULDocumentImpl::Finalize(JSContext *aContext)
{
    NS_NOTYETIMPLEMENTED("write me");
}



////////////////////////////////////////////////////////////////////////
// nsIScriptObjectOwner interface
NS_IMETHODIMP
XULDocumentImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    nsresult res = NS_OK;
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();

    if (nsnull == mScriptObject) {
        res = NS_NewScriptXULDocument(aContext, (nsISupports *)(nsIDOMXULDocument *)this, global, (void**)&mScriptObject);
    }
    *aScriptObject = mScriptObject;

    NS_RELEASE(global);
    return res;
}


NS_IMETHODIMP
XULDocumentImpl::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIHTMLContentContainer interface

NS_IMETHODIMP 
XULDocumentImpl::GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult)
{
    NS_PRECONDITION(nsnull != aResult, "null ptr");
    if (nsnull == aResult) {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = mAttrStyleSheet;
    if (! mAttrStyleSheet) {
        return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
    }
    else {
        NS_ADDREF(*aResult);
    }
    return NS_OK;
}

NS_IMETHODIMP 
XULDocumentImpl::GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult)
{
    NS_NOTYETIMPLEMENTED("get the inline stylesheet!");

    NS_PRECONDITION(nsnull != aResult, "null ptr");
    if (nsnull == aResult) {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = mInlineStyleSheet;
    if (!mInlineStyleSheet) {
        return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
    }
    else {
        NS_ADDREF(*aResult);
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Implementation methods

nsIContent*
XULDocumentImpl::FindContent(const nsIContent* aStartNode,
                             const nsIContent* aTest1, 
                             const nsIContent* aTest2) const
{
    PRInt32 count;
    aStartNode->ChildCount(count);

    PRInt32 i;
    for(i = 0; i < count; i++) {
        nsIContent* child;
        aStartNode->ChildAt(i, child);
        nsIContent* content = FindContent(child,aTest1,aTest2);
        if (content != nsnull) {
            NS_IF_RELEASE(child);
            return content;
        }
        if (child == aTest1 || child == aTest2) {
            NS_IF_RELEASE(content);
            return child;
        }
        NS_IF_RELEASE(child);
        NS_IF_RELEASE(content);
    }
    return nsnull;
}


nsresult
XULDocumentImpl::AddNamedDataSource(const char* uri)
{
    NS_PRECONDITION(mXULBuilder != nsnull, "not initialized");
    if (! mXULBuilder)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
    nsCOMPtr<nsIRDFDataSource> ds;

    if (NS_FAILED(rv = gRDFService->GetDataSource(uri, getter_AddRefs(ds)))) {
        NS_ERROR("unable to get named datasource");
        return rv;
    }

    nsCOMPtr<nsIRDFCompositeDataSource> db;
    if (NS_FAILED(rv = mXULBuilder->GetDataBase(getter_AddRefs(db)))) {
        NS_ERROR("unable to get XUL db");
        return rv;
    }

    if (NS_FAILED(rv = db->AddDataSource(ds))) {
        NS_ERROR("unable to add named data source to XUL db");
        return rv;
    }

    return NS_OK;
}


nsresult
XULDocumentImpl::Init(void)
{
    nsresult rv;

    if (NS_FAILED(rv = NS_NewHeapArena(getter_AddRefs(mArena), nsnull)))
        return rv;

    // Create a namespace manager so we can manage tags
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                    nsnull,
                                                    kINameSpaceManagerIID,
                                                    getter_AddRefs(mNameSpaceManager))))
        return rv;

    // Create our focus tracker and hook it up.
    nsCOMPtr<nsIXULCommandDispatcher> commandDis;
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kXULCommandDispatcherCID,
                                                    nsnull,
                                                    kIXULCommandDispatcherIID,
                                                    getter_AddRefs(commandDis)))) {
        NS_ERROR("unable to create a focus tracker");
        return rv;
    }

    mCommandDispatcher = do_QueryInterface(commandDis);

    nsCOMPtr<nsIDOMEventListener> CommandDispatcher = do_QueryInterface(mCommandDispatcher);
    if (CommandDispatcher) {
      // Take the focus tracker and add it as an event listener for focus and blur events.
        AddEventListener("focus", CommandDispatcher, PR_TRUE);
        AddEventListener("blur", CommandDispatcher, PR_TRUE);
    }

    return NS_OK;
}



nsresult
XULDocumentImpl::StartLayout(void)
{
	if (mParentContentSink)
      return NS_OK; // Overlays rely on the master document for layout

    NS_ASSERTION(mRootContent != nsnull, "Error in XUL file. Love to tell ya where if only I knew.");
    if (!mRootContent)
      return NS_ERROR_UNEXPECTED;
    
    PRInt32 count = GetNumberOfShells();
    for (PRInt32 i = 0; i < count; i++) {
      nsIPresShell* shell = GetShellAt(i);
      if (nsnull == shell)
          continue;

      // Resize-reflow this time
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));

      PRBool intrinsic = PR_FALSE;
      nsCOMPtr<nsIWebShell> webShell;
      nsCOMPtr<nsIBrowserWindow> browser;

		  if (cx) {
			  nsCOMPtr<nsISupports> container;
			  cx->GetContainer(getter_AddRefs(container));
			  if (container) {
			    webShell = do_QueryInterface(container);
			    if (webShell) {
					  webShell->SetScrolling(NS_STYLE_OVERFLOW_HIDDEN);
            nsCOMPtr<nsIWebShellContainer> webShellContainer;
            webShell->GetContainer(*getter_AddRefs(webShellContainer));
            if (webShellContainer) {
              browser = do_QueryInterface(webShellContainer);
              if (browser)
                browser->IsIntrinsicallySized(intrinsic);
            }
			    }
			  }
		  }
        
		  nsRect r;
      cx->GetVisibleArea(r);
      if (intrinsic) {
        // Flow at an unconstrained width and height
        r.width = NS_UNCONSTRAINEDSIZE;
        r.height = NS_UNCONSTRAINEDSIZE;
      }

      if (browser) {
        // We're top-level.
        // See if we have attributes on our root tag that set the width and height.
        // read "height" attribute// Convert r.width and r.height to twips.
        float p2t;
        cx->GetPixelsToTwips(&p2t);
        
        nsCOMPtr<nsIDOMElement> windowElement = do_QueryInterface(mRootContent);
        nsString sizeString;
        PRInt32 specSize;
        PRInt32 errorCode;
        if (NS_SUCCEEDED(windowElement->GetAttribute("height", sizeString))) {
          specSize = sizeString.ToInteger(&errorCode);
          if (NS_SUCCEEDED(errorCode) && specSize > 0)
            r.height = NSIntPixelsToTwips(specSize, p2t);
        }

        // read "width" attribute
        if (NS_SUCCEEDED(windowElement->GetAttribute("width", sizeString))) {
          specSize = sizeString.ToInteger(&errorCode);
          if (NS_SUCCEEDED(errorCode) || specSize > 0)
            r.width = NSIntPixelsToTwips(specSize, p2t);
        }
      }

      cx->SetVisibleArea(r);
      
      // XXX Copy of the code below. See XXX below for details...
      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsCOMPtr<nsIContentViewer> contentViewer;
        nsresult rv = webShell->GetContentViewer(getter_AddRefs(contentViewer));
        if (NS_SUCCEEDED(rv) && (contentViewer != nsnull)) {
          PRBool enabled;
          contentViewer->GetEnableRendering(&enabled);
          if (enabled) {
            vm->EnableRefresh();
          }
        }
      }

      shell->InitialReflow(r.width, r.height);

      if (browser) {
        // We're top level.
        // Retrieve the answer.
        cx->GetVisibleArea(r);

        // Perform the resize
        PRInt32 chromeX,chromeY,chromeWidth,chromeHeight;
        webShell->GetBounds(chromeX,chromeY,chromeWidth,chromeHeight);

        float t2p;
        cx->GetTwipsToPixels(&t2p);
        PRInt32 width = PRInt32((float)r.width*t2p);
        PRInt32 height = PRInt32((float)r.height*t2p);
      
        PRInt32 widthDelta = width - chromeWidth;
        PRInt32 heightDelta = height - chromeHeight;

        nsRect windowBounds;
        browser->GetWindowBounds(windowBounds);
        browser->SizeWindowTo(windowBounds.width + widthDelta, 
                              windowBounds.height + heightDelta);
      }
      
      // XXX Moving this call up before the call to InitialReflow(), because
      // the view manager's UpdateView() function is dropping dirty rects if
      // refresh is disabled rather than accumulating them until refresh is
      // enabled and then triggering a repaint...
#if 0
      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsCOMPtr<nsIContentViewer> contentViewer;
        nsresult rv = webShell->GetContentViewer(getter_AddRefs(contentViewer));
        if (NS_SUCCEEDED(rv) && (contentViewer != nsnull)) {
          PRBool enabled;
          contentViewer->GetEnableRendering(&enabled);
          if (enabled) {
            vm->EnableRefresh();
          }
        }
      }
#endif
 
      // Start observing the document _after_ we do the initial
      // reflow. Otherwise, we'll get into an trouble trying to
      // create kids before the root frame is established.
      shell->BeginObservingDocument();

      NS_RELEASE(shell);
    }
    return NS_OK;
}


nsresult
XULDocumentImpl::GetElementsByTagName(nsIDOMNode* aNode,
                                      const nsString& aTagName,
                                      nsRDFDOMNodeList* aElements)
{
    nsresult rv;

    nsCOMPtr<nsIDOMElement> element;
    element = do_QueryInterface(aNode);
    if (!element)
      return NS_OK;

    if (aTagName.Equals("*")) {
        if (NS_FAILED(rv = aElements->AppendNode(aNode))) {
            NS_ERROR("unable to append element to node list");
            return rv;
        }
    }
    else {
        nsAutoString name;
        if (NS_FAILED(rv = aNode->GetNodeName(name))) {
            NS_ERROR("unable to get node name");
            return rv;
        }

        if (aTagName.Equals(name)) {
            if (NS_FAILED(rv = aElements->AppendNode(aNode))) {
                NS_ERROR("unable to append element to node list");
                return rv;
            }
        }
    }

    nsCOMPtr<nsIDOMNodeList> children;
    if (NS_FAILED(rv = aNode->GetChildNodes( getter_AddRefs(children) ))) {
        NS_ERROR("unable to get node's children");
        return rv;
    }

    // no kids: terminate the recursion
    if (! children)
        return NS_OK;

    PRUint32 length;
    if (NS_FAILED(children->GetLength(&length))) {
        NS_ERROR("unable to get node list's length");
        return rv;
    }

    for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIDOMNode> child;
        if (NS_FAILED(rv = children->Item(i, getter_AddRefs(child) ))) {
            NS_ERROR("unable to get child from list");
            return rv;
        }

        if (NS_FAILED(rv = GetElementsByTagName(child, aTagName, aElements))) {
            NS_ERROR("unable to recursively get elements by tag name");
            return rv;
        }
    }

    return NS_OK;
}

nsresult
XULDocumentImpl::GetElementsByAttribute(nsIDOMNode* aNode,
                                        const nsString& aAttribute,
                                        const nsString& aValue,
                                        nsRDFDOMNodeList* aElements)
{
    nsresult rv;

    nsCOMPtr<nsIDOMElement> element;
    element = do_QueryInterface(aNode);
    if (!element)
        return NS_OK;

    nsAutoString attrValue;
    if (NS_FAILED(rv = element->GetAttribute(aAttribute, attrValue))) {
        NS_ERROR("unable to get attribute value");
        return rv;
    }

    if ((attrValue == aValue) || (attrValue.Length() > 0 && aValue == "*")) {
        if (NS_FAILED(rv = aElements->AppendNode(aNode))) {
            NS_ERROR("unable to append element to node list");
            return rv;
        }
    }
       
    nsCOMPtr<nsIDOMNodeList> children;
    if (NS_FAILED(rv = aNode->GetChildNodes( getter_AddRefs(children) ))) {
        NS_ERROR("unable to get node's children");
        return rv;
    }

    // no kids: terminate the recursion
    if (! children)
        return NS_OK;

    PRUint32 length;
    if (NS_FAILED(children->GetLength(&length))) {
        NS_ERROR("unable to get node list's length");
        return rv;
    }

    for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIDOMNode> child;
        if (NS_FAILED(rv = children->Item(i, getter_AddRefs(child) ))) {
            NS_ERROR("unable to get child from list");
            return rv;
        }

        if (NS_FAILED(rv = GetElementsByAttribute(child, aAttribute, aValue, aElements))) {
            NS_ERROR("unable to recursively get elements by attribute");
            return rv;
        }
    }

    return NS_OK;
}



nsresult
XULDocumentImpl::ParseTagString(const nsString& aTagName, nsIAtom*& aName, PRInt32& aNameSpaceID)
{
    // Parse the tag into a name and a namespace ID. This is slightly
    // different than nsIContent::ParseAttributeString() because we
    // take the default namespace into account (rather than just
    // assigning "no namespace") in the case that there is no
    // namespace prefix present.

static char kNameSpaceSeparator = ':';

    // XXX this is a gross hack, but it'll carry us for now. We parse
    // the tag name using the root content, which presumably has all
    // the namespace info we need.
    NS_PRECONDITION(mRootContent != nsnull, "not initialized");
    if (! mRootContent)
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(mRootContent) );
    if (! xml) return NS_ERROR_UNEXPECTED;
    
    nsresult rv;
    nsCOMPtr<nsINameSpace> ns;
    rv = xml->GetContainingNameSpace(*getter_AddRefs(ns));
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(ns != nsnull, "expected xml namespace info to be available");
    if (! ns)
        return NS_ERROR_UNEXPECTED;

    nsAutoString prefix;
    nsAutoString name(aTagName);
    PRInt32 nsoffset = name.FindChar(kNameSpaceSeparator);
    if (-1 != nsoffset) {
        name.Left(prefix, nsoffset);
        name.Cut(0, nsoffset+1);
    }

    // Figure out the namespace ID
    nsCOMPtr<nsIAtom> nameSpaceAtom;
    if (0 < prefix.Length())
        nameSpaceAtom = getter_AddRefs(NS_NewAtom(prefix));

    rv = ns->FindNameSpaceID(nameSpaceAtom, aNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    aName = NS_NewAtom(name);
    return NS_OK;
}


// nsIDOMEventCapturer and nsIDOMEventReceiver Interface Implementations

NS_IMETHODIMP
XULDocumentImpl::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    nsIEventListenerManager *manager;

    if (NS_OK == GetListenerManager(&manager)) {
        manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
        NS_RELEASE(manager);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
XULDocumentImpl::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    if (mListenerManager) {
        mListenerManager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
XULDocumentImpl::AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                 PRBool aUseCapture)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
XULDocumentImpl::RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                    PRBool aUseCapture)
{
  if (mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
XULDocumentImpl::GetListenerManager(nsIEventListenerManager** aResult)
{
    if (! mListenerManager) {
        nsresult rv;
        rv = nsComponentManager::CreateInstance(kEventListenerManagerCID,
                                                nsnull,
                                                kIEventListenerManagerIID,
                                                getter_AddRefs(mListenerManager));

        if (NS_FAILED(rv)) return rv;
    }
    *aResult = mListenerManager;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
XULDocumentImpl::GetNewListenerManager(nsIEventListenerManager **aResult)
{
    return nsComponentManager::CreateInstance(kEventListenerManagerCID,
                                        nsnull,
                                        kIEventListenerManagerIID,
                                        (void**) aResult);
}

nsresult
XULDocumentImpl::CaptureEvent(const nsString& aType)
{
  nsIEventListenerManager *mManager;

  if (NS_OK == GetListenerManager(&mManager)) {
    //mManager->CaptureEvent(aListener);
    NS_RELEASE(mManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
XULDocumentImpl::ReleaseEvent(const nsString& aType)
{
  if (mListenerManager) {
    //mListenerManager->ReleaseEvent(aListener);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
XULDocumentImpl::OpenWidgetItem(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    if (! mBuilders)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIAtom> tag;
        rv = aElement->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagStr;
        tag->ToString(tagStr);

        PR_LOG(gXULLog, PR_LOG_DEBUG,
               ("xuldoc open-widget-item %s",
                (const char*) nsCAutoString(tagStr)));
    }
#endif

    PRUint32 cnt = 0;
    rv = mBuilders->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    for (PRUint32 i = 0; i < cnt; ++i) {
        // XXX we should QueryInterface() here
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        NS_ASSERTION(builder != nsnull, "null ptr");
        if (! builder)
            continue;

        rv = builder->OpenContainer(aElement);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error opening container");
        // XXX ignore error code?

        NS_RELEASE(builder);
    }

    return NS_OK;
}

nsresult
XULDocumentImpl::CloseWidgetItem(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    if (! mBuilders)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIAtom> tag;
        rv = aElement->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagStr;
        tag->ToString(tagStr);

        PR_LOG(gXULLog, PR_LOG_DEBUG,
               ("xuldoc close-widget-item %s",
                (const char*) nsCAutoString(tagStr)));
    }
#endif

    PRUint32 cnt = 0;
    rv = mBuilders->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    for (PRUint32 i = 0; i < cnt; ++i) {
        // XXX we should QueryInterface() here
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        NS_ASSERTION(builder != nsnull, "null ptr");
        if (! builder)
            continue;

        rv = builder->CloseContainer(aElement);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error closing container");
        // XXX ignore error code?

        NS_RELEASE(builder);
    }

    return NS_OK;
}


nsresult
XULDocumentImpl::RebuildWidgetItem(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    if (! mBuilders)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIAtom> tag;
        rv = aElement->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagStr;
        tag->ToString(tagStr);

        PR_LOG(gXULLog, PR_LOG_DEBUG,
               ("xuldoc close-widget-item %s",
                (const char*) nsCAutoString(tagStr)));
    }
#endif

    PRUint32 cnt = 0;
    rv = mBuilders->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
    for (PRUint32 i = 0; i < cnt; ++i) {
        // XXX we should QueryInterface() here
        nsIRDFContentModelBuilder* builder
            = (nsIRDFContentModelBuilder*) mBuilders->ElementAt(i);

        NS_ASSERTION(builder != nsnull, "null ptr");
        if (! builder)
            continue;

        rv = builder->RebuildContainer(aElement);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error rebuilding container");
        // XXX ignore error code?

        NS_RELEASE(builder);
    }

    return NS_OK;
}




