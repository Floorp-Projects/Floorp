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

  An implementation for an NGLayout-style content sink that knows how
  to build an RDF content model from XUL.

  For each container tag, an RDF Sequence object is created that
  contains the order set of children of that container.

  For more information on XUL, see http://www.mozilla.org/xpfe

  TO DO
  -----

  1. Factor merging code out into a subroutine so that the tricky pos
     junk isn't typed in two places.

  2. Make sure that forward references add their children to the
     document-to-element map.

*/

#include "nsCOMPtr.h"
#include "nsForwardReference.h"
#include "nsIChromeRegistry.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIContent.h"
#include "nsIContentSink.h"
#include "nsIContentViewerContainer.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDocument.h"
#include "nsIDocumentLoader.h"
#include "nsIFormControl.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsIHTMLElementFactory.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIParser.h"
#include "nsIPresShell.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFService.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIServiceManager.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIViewManager.h"
#include "nsIWebShell.h"
#include "nsIXMLContent.h"
#include "nsIXMLElementFactory.h"
#include "nsIXULChildDocument.h"
#include "nsIXULContentSink.h"
#include "nsIXULContentUtils.h"
#include "nsIXULDocument.h"
#include "nsIXULDocumentInfo.h"
#include "nsIXULKeyListener.h"
#include "nsIXULParentDocument.h"
#include "nsLayoutCID.h"
#include "nsNeckoUtil.h"
#include "nsRDFCID.h"
#include "nsRDFParserUtils.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsXULElement.h"
#include "prlog.h"
#include "prmem.h"
#include "rdfutil.h"

#include "nsHTMLTokens.h" // XXX so we can use nsIParserNode::GetTokenType()

static const char kNameSpaceSeparator = ':';
static const char kNameSpaceDef[] = "xmlns";
static const char kXULID[] = "id";

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;


static NS_DEFINE_CID(kHTMLElementFactoryCID,    NS_HTML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kXMLElementFactoryCID,     NS_XML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kTextNodeCID,              NS_TEXTNODE_CID);
static NS_DEFINE_CID(kXULContentUtilsCID,       NS_XULCONTENTUTILS_CID);
static NS_DEFINE_CID(kXULDocumentInfoCID,       NS_XULDOCUMENTINFO_CID);
static NS_DEFINE_CID(kXULKeyListenerCID,        NS_XULKEYLISTENER_CID);
static NS_DEFINE_CID(kXULTemplateBuilderCID,    NS_XULTEMPLATEBUILDER_CID);
static NS_DEFINE_CID(kChromeRegistryCID,         NS_CHROMEREGISTRY_CID);

////////////////////////////////////////////////////////////////////////

class XULContentSinkImpl : public nsIXULContentSink
{
public:
    XULContentSinkImpl(nsresult& aRV);
    virtual ~XULContentSinkImpl();

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIContentSink
    NS_IMETHOD WillBuildModel(void);
    NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
    NS_IMETHOD WillInterrupt(void);
    NS_IMETHOD WillResume(void);
    NS_IMETHOD SetParser(nsIParser* aParser);  
    NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
    NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
    NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
    NS_IMETHOD AddComment(const nsIParserNode& aNode);
    NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
    NS_IMETHOD NotifyError(const nsParserError* aError);
    NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode=0);

    // nsIXMLContentSink
    NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode);    
    NS_IMETHOD AddCharacterData(const nsIParserNode& aNode);
    NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode);
    NS_IMETHOD AddNotation(const nsIParserNode& aNode);
    NS_IMETHOD AddEntityReference(const nsIParserNode& aNode);

    // nsIXULContentSink
    NS_IMETHOD Init(nsIDocument* aDocument);
    NS_IMETHOD UnblockNextOverlay();
    NS_IMETHOD UpdateOverlayCounters(PRInt32 aDelta);

protected:
    // pseudo-constants
    static nsrefcnt               gRefCnt;
    static nsIRDFService*         gRDFService;
    static nsIHTMLElementFactory* gHTMLElementFactory;
    static nsIXMLElementFactory*  gXMLElementFactory;
    static nsINameSpaceManager*   gNameSpaceManager;
    static nsIXULContentUtils*    gXULUtils;

    static nsIAtom* kCommandUpdaterAtom;
    static nsIAtom* kDataSourcesAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kKeysetAtom;
    static nsIAtom* kOverlayAtom;
    static nsIAtom* kPositionAtom;
    static nsIAtom* kRefAtom;
    static nsIAtom* kScriptAtom;
    static nsIAtom* kTemplateAtom;

    static PRInt32 kNameSpaceID_XUL;

    // Text management
    nsresult FlushText(PRBool aCreateTextNode=PR_TRUE);

    PRUnichar* mText;
    PRInt32 mTextLength;
    PRInt32 mTextSize;
    PRBool mConstrainSize;

    // namespace management -- XXX combine with ContextStack?
    void PushNameSpacesFrom(const nsIParserNode& aNode);
    void PopNameSpaces(void);
    nsresult GetTopNameSpace(nsCOMPtr<nsINameSpace>* aNameSpace);
    NS_IMETHOD GetChromeOverlays();

    nsVoidArray mNameSpaceStack;
    
    // RDF-specific parsing
    nsresult GetXULIDAttribute(const nsIParserNode& aNode, nsString& aID);
    nsresult AddAttributes(const nsIParserNode& aNode, nsIContent* aElement);
    nsresult ParseTag(const nsString& aText, nsIAtom*& aTag, PRInt32& aNameSpaceID);
    nsresult CreateHTMLElement(nsIAtom* aTag, nsCOMPtr<nsIContent>* aResult);
    nsresult CreateXULElement(PRInt32 aNameSpaceID, nsIAtom* aTag, nsCOMPtr<nsIContent>* aResult);

    nsresult CreateTemplateBuilder(nsIContent* aElement,
                                   const nsString& aDataSources,
                                   nsCOMPtr<nsIRDFContentModelBuilder>* aResult);

    nsresult OpenRoot(const nsIParserNode& aNode, PRInt32 aNameSpaceID, nsIAtom* aTag);
    nsresult OpenOverlayRoot(const nsIParserNode& aNode, PRInt32 aNameSpaceID, nsIAtom* aTag);
    nsresult OpenTag(const nsIParserNode& aNode, PRInt32 aNameSpaceID, nsIAtom* aTag);
    nsresult OpenOverlayTag(const nsIParserNode& aNode, PRInt32 aNameSpaceID, nsIAtom* aTag);

    static
    nsresult
    Merge(nsIContent* aOriginalNode, nsIContent* aOverlayNode);

    static
    nsresult
    InsertElement(nsIContent* aParent, nsIContent* aChild);

    nsresult
    AddElementToMap(nsIContent* aElement);

    // Script tag handling
    nsresult OpenScript(const nsIParserNode& aNode);

    static void DoneLoadingScript(nsIUnicharStreamLoader* aLoader,
                                  nsString& aData,
                                  void* aRef,
                                  nsresult aStatus);

    nsresult EvaluateScript(nsString& aScript, nsIURI* aURI, PRUint32 aLineNo);

    nsresult CloseScript(const nsIParserNode& aNode);

    nsCOMPtr<nsIURI> mCurrentScriptURL;
    PRBool mInScript;
    PRUint32 mScriptLineNo;

    // Overlays
    nsresult ProcessOverlay(const nsString& aHref);

    // Style sheets
    nsresult ProcessStyleLink(nsIContent* aElement,
                              const nsString& aHref, PRBool aAlternate,
                              const nsString& aTitle, const nsString& aType,
                              const nsString& aMedia);
    

    public:
    enum State { eInProlog, eInDocumentElement, eInOverlayElement, eInScript, eInEpilog };
    protected:

    State mState;

    // content stack management
    class ContextStack {
    protected:
        struct Entry {
            nsIContent* mElement;
            State       mState;
            Entry*      mNext;
        };

        Entry* mTop;
        PRInt32 mDepth;

    public:
        ContextStack();
        ~ContextStack();

        PRInt32 Depth() { return mDepth; }

        nsresult Push(nsIContent* aElement, State aState);
        nsresult Pop(nsIContent** aElement, State* aState);

        nsresult GetTopElement(nsIContent** aElement);

        PRBool IsInsideXULTemplate();
    };

    friend class ContextStack;
    ContextStack mContextStack;


    class OverlayForwardReference : public nsForwardReference
    {
    protected:
        nsCOMPtr<nsIContent> mContent;

    public:
        OverlayForwardReference(nsIContent* aElement) : mContent(aElement) {}

        virtual ~OverlayForwardReference() {}

        virtual Result Resolve();
    };

    friend class OverlayForwardReference;

    nsCOMPtr<nsIURI> mDocumentURL;         // [OWNER]
    nsCOMPtr<nsIURI> mDocumentBaseURL;     // [OWNER]

    // Overlays
    nsCOMPtr<nsIDocument> mDocument;       // [OWNER]
    nsCOMPtr<nsIDocument> mActualDocument;
    nsIParser*            mParser;         // [OWNER] We use regular pointer b/c of funky exports on nsIParser
    
    PRInt32            mUnprocessedOverlayCount;
    nsVoidArray        mOverlayArray;
    PRInt32            mCurrentOverlay;
    nsCOMPtr<nsIXULContentSink> mParentContentSink; // [OWNER]????
  
    nsString               mPreferredStyle;
    PRInt32                mStyleSheetCount;
    nsCOMPtr<nsICSSLoader> mCSSLoader;     // [OWNER]
};

nsrefcnt XULContentSinkImpl::gRefCnt;
nsIRDFService* XULContentSinkImpl::gRDFService;
nsIHTMLElementFactory* XULContentSinkImpl::gHTMLElementFactory;
nsIXMLElementFactory* XULContentSinkImpl::gXMLElementFactory;
nsINameSpaceManager* XULContentSinkImpl::gNameSpaceManager;
nsIXULContentUtils* XULContentSinkImpl::gXULUtils;

nsIAtom* XULContentSinkImpl::kCommandUpdaterAtom;
nsIAtom* XULContentSinkImpl::kDataSourcesAtom;
nsIAtom* XULContentSinkImpl::kIdAtom;
nsIAtom* XULContentSinkImpl::kKeysetAtom;
nsIAtom* XULContentSinkImpl::kOverlayAtom;
nsIAtom* XULContentSinkImpl::kPositionAtom;
nsIAtom* XULContentSinkImpl::kRefAtom;
nsIAtom* XULContentSinkImpl::kScriptAtom;
nsIAtom* XULContentSinkImpl::kTemplateAtom;

PRInt32 XULContentSinkImpl::kNameSpaceID_XUL;

////////////////////////////////////////////////////////////////////////

XULContentSinkImpl::ContextStack::ContextStack()
    : mTop(nsnull), mDepth(0)
{
}

XULContentSinkImpl::ContextStack::~ContextStack()
{
    while (mTop) {
        Entry* doomed = mTop;
        mTop = mTop->mNext;
        NS_IF_RELEASE(doomed->mElement);
        delete doomed;
    }
}

nsresult
XULContentSinkImpl::ContextStack::Push(nsIContent* aElement, State aState)
{
    Entry* entry = new Entry;
    if (! entry)
        return NS_ERROR_OUT_OF_MEMORY;

    entry->mElement = aElement;
    NS_IF_ADDREF(entry->mElement);
    entry->mState = aState;

    entry->mNext = mTop;
    mTop = entry;

    ++mDepth;
    return NS_OK;
}

nsresult
XULContentSinkImpl::ContextStack::Pop(nsIContent** aElement, State* aState)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    Entry* entry = mTop;
    mTop = mTop->mNext;
    --mDepth;

    *aElement = entry->mElement; // "transfer" refcnt
    *aState   = entry->mState;
    delete entry;
    return NS_OK;
}

nsresult
XULContentSinkImpl::ContextStack::GetTopElement(nsIContent** aElement)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aElement = mTop->mElement;
    NS_IF_ADDREF(*aElement);
    return NS_OK;
}


PRBool
XULContentSinkImpl::ContextStack::IsInsideXULTemplate()
{
    if (mDepth) {
        nsCOMPtr<nsIContent> element = dont_QueryInterface(mTop->mElement);
        while (element) {
            PRInt32 nameSpaceID;
            element->GetNameSpaceID(nameSpaceID);
            if (nameSpaceID == kNameSpaceID_XUL) {
                nsCOMPtr<nsIAtom> tag;
                element->GetTag(*getter_AddRefs(tag));
                if (tag.get() == kTemplateAtom) {
                    return PR_TRUE;
                }
            }

            nsCOMPtr<nsIContent> parent;
            element->GetParent(*getter_AddRefs(parent));
            element = parent;
        }
    }
    return PR_FALSE;
}


////////////////////////////////////////////////////////////////////////


nsForwardReference::Result
XULContentSinkImpl::OverlayForwardReference::Resolve()
{
    nsresult rv;

    nsCOMPtr<nsIDocument> doc;
    rv = mContent->GetDocument(*getter_AddRefs(doc));
    if (NS_FAILED(rv)) return eResolveError;

    nsCOMPtr<nsIDOMXULDocument> xuldoc = do_QueryInterface(doc);
    if (! xuldoc)
        return eResolveError;

    nsAutoString id;
    rv = mContent->GetAttribute(kNameSpaceID_XUL, kIdAtom, id);
    if (NS_FAILED(rv)) return eResolveError;

    nsCOMPtr<nsIDOMElement> domoverlay;
    rv = xuldoc->GetElementById(id, getter_AddRefs(domoverlay));
    if (NS_FAILED(rv)) return eResolveError;

    if (! domoverlay)
        return eResolveLater;

    nsCOMPtr<nsIContent> overlay = do_QueryInterface(domoverlay);
    if (! overlay)
        return eResolveError;

    rv = Merge(mContent, overlay);
    if (NS_FAILED(rv)) return eResolveError;

    return eResolveSucceeded;
}




////////////////////////////////////////////////////////////////////////


XULContentSinkImpl::XULContentSinkImpl(nsresult& rv)
    : mText(nsnull),
      mTextLength(0),
      mTextSize(0),
      mConstrainSize(PR_TRUE),
      mInScript(PR_FALSE),
      mScriptLineNo(0),
      mState(eInProlog),
      mParser(nsnull),
      mUnprocessedOverlayCount(0),
      mCurrentOverlay(0),
      mStyleSheetCount(0)
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**) &gRDFService);
        if (NS_FAILED(rv)) return;

        rv = nsComponentManager::CreateInstance(kHTMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIHTMLElementFactory),
                                                (void**) &gHTMLElementFactory);
        if (NS_FAILED(rv)) return;

        rv = nsComponentManager::CreateInstance(kXMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIXMLElementFactory),
                                                (void**) &gXMLElementFactory);
        if (NS_FAILED(rv)) return;

        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                nsnull,
                                                NS_GET_IID(nsINameSpaceManager),
                                                (void**) &gNameSpaceManager);

        if (NS_FAILED(rv)) return;

        rv = nsServiceManager::GetService(kXULContentUtilsCID,
                                          NS_GET_IID(nsIXULContentUtils),
                                          (nsISupports**) &gXULUtils);

        if (NS_FAILED(rv)) return;

        rv = gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");
        if (NS_FAILED(rv)) return;

        kCommandUpdaterAtom = NS_NewAtom("commandupdater");
        kDataSourcesAtom    = NS_NewAtom("datasources");
        kIdAtom             = NS_NewAtom("id");
        kKeysetAtom         = NS_NewAtom("keyset");
        kOverlayAtom        = NS_NewAtom("overlay");
        kPositionAtom       = NS_NewAtom("position");
        kRefAtom            = NS_NewAtom("ref");
        kScriptAtom         = NS_NewAtom("script");
        kTemplateAtom       = NS_NewAtom("template");
    }

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsXULContentSink");
#endif

    rv = NS_OK;
}


XULContentSinkImpl::~XULContentSinkImpl()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: XULContentSinkImpl\n", gInstanceCount);
#endif

    NS_IF_RELEASE(mParser); // XXX should've been released by now, unless error.

    {
        // There shouldn't be any here except in an error condition
        PRInt32 i = mNameSpaceStack.Count();

        while (0 < i--) {
            nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack[i];

#ifdef PR_LOGGING
            if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
                nsAutoString uri;
                nsCOMPtr<nsIAtom> prefixAtom;

                nameSpace->GetNameSpaceURI(uri);
                nameSpace->GetNameSpacePrefix(*getter_AddRefs(prefixAtom));

                nsAutoString prefix;
                if (prefixAtom)
                    {
                        const PRUnichar *unicodeString;
                        prefixAtom->GetUnicode(&unicodeString);
                        prefix = unicodeString;
                    }
                else
                    {
                        prefix = "<default>";
                    }

                char* prefixStr = prefix.ToNewCString();
                char* uriStr = uri.ToNewCString();

                PR_LOG(gLog, PR_LOG_ALWAYS,
                       ("xul: warning: unclosed namespace '%s' (%s)",
                        prefixStr, uriStr));

                nsCRT::free(prefixStr);
                nsCRT::free(uriStr);
            }
#endif

            NS_RELEASE(nameSpace);
        }
    }

    PR_FREEIF(mText);

    // Delete all the elements from our overlay array
    {
        PRInt32 count = mOverlayArray.Count();
        for (PRInt32 i = 0; i < count; i++) {
            nsString* element = (nsString*)(mOverlayArray[i]);
            delete element;
        }
    }

    if (--gRefCnt == 0) {
        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        NS_IF_RELEASE(gHTMLElementFactory);
        NS_IF_RELEASE(gNameSpaceManager);

        if (gXULUtils) {
            nsServiceManager::ReleaseService(kXULContentUtilsCID, gXULUtils);
            gXULUtils = nsnull;
        }

        NS_IF_RELEASE(kCommandUpdaterAtom);
        NS_IF_RELEASE(kDataSourcesAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kKeysetAtom);
        NS_IF_RELEASE(kOverlayAtom);
        NS_IF_RELEASE(kPositionAtom);
        NS_IF_RELEASE(kRefAtom);
        NS_IF_RELEASE(kScriptAtom);
        NS_IF_RELEASE(kTemplateAtom);
    }
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(XULContentSinkImpl);
NS_IMPL_RELEASE(XULContentSinkImpl);

NS_IMETHODIMP
XULContentSinkImpl::QueryInterface(REFNSIID iid, void** result)
{
    NS_PRECONDITION(result, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(NS_GET_IID(nsIXULContentSink)) ||
        iid.Equals(NS_GET_IID(nsIXMLContentSink)) ||
        iid.Equals(NS_GET_IID(nsIContentSink)) ||
        iid.Equals(NS_GET_IID(nsISupports))) {
        *result = NS_STATIC_CAST(nsIXMLContentSink*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


////////////////////////////////////////////////////////////////////////
// nsIContentSink interface

NS_IMETHODIMP 
XULContentSinkImpl::WillBuildModel(void)
{
    if (! mParentContentSink) {
        // If we're _not_ an overlay, then notify the document that
        // the load is beginning.
        mDocument->BeginLoad();
    }

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::DidBuildModel(PRInt32 aQualityLevel)
{
    // XXX this is silly; who cares?
    PRInt32 i, ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
        nsIPresShell* shell = mDocument->GetShellAt(i);
        if (nsnull != shell) {
            nsIViewManager* vm;
            shell->GetViewManager(&vm);
            if(vm) {
                vm->SetQuality(nsContentQuality(aQualityLevel));
            }
            NS_RELEASE(vm);
            NS_RELEASE(shell);
        }
    }

    if (! mParentContentSink) {
        // If we're _not_ an overlay, then notify the document that
        // load has ended.
        mDocument->EndLoad();
    }

    // Drop our reference to the parser to get rid of a circular
    // reference.
    NS_RELEASE(mParser);
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::WillInterrupt(void)
{
    // XXX Notify the webshell, if necessary
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::WillResume(void)
{
    // XXX Notify the webshell, if necessary
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::SetParser(nsIParser* aParser)
{
    NS_IF_RELEASE(mParser);
    mParser = aParser;
    NS_IF_ADDREF(mParser);
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::OpenContainer(const nsIParserNode& aNode)
{
    // XXX Hopefully the parser will flag this before we get here. If
    // we're in the epilog, there should be no new elements
    NS_PRECONDITION(mState != eInEpilog, "tag in XUL doc epilog");
    if (mState == eInEpilog)
        return NS_ERROR_UNEXPECTED;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        const nsString& text = aNode.GetText();

        nsCAutoString extraWhiteSpace;
        PRInt32 count = mContextStack.Depth();
        while (--count >= 0)
            extraWhiteSpace += "  ";

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("xul: %.5d. %s<%s>",
                aNode.GetSourceLineNumber(),
                NS_STATIC_CAST(const char*, extraWhiteSpace),
                NS_STATIC_CAST(const char*, nsCAutoString(text))));
    }
#endif

    if (mState != eInScript) {
        FlushText();
    }

    // We must register namespace declarations found in the attribute
    // list of an element before creating the element. This is because
    // the namespace prefix for an element might be declared within
    // the attribute list.
    PushNameSpacesFrom(aNode);

    nsresult rv;

    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tag;
    rv = ParseTag(aNode.GetText(), *getter_AddRefs(tag), nameSpaceID);
    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: unrecognized namespace on '%s' at line %d",
                NS_STATIC_CAST(const char*, nsCAutoString(aNode.GetText())),
                aNode.GetSourceLineNumber()));
#endif

        return rv;
    }

    switch (mState) {
    case eInProlog:
        if (! mParentContentSink) {
            // We're the root document element
            rv = OpenRoot(aNode, nameSpaceID, tag);
        }
        else {
            rv = OpenOverlayRoot(aNode, nameSpaceID, tag);
        }
        break;

    case eInOverlayElement:
        NS_ASSERTION(mParentContentSink != nsnull, "how'd we get there");

        // We're in an overlay and are looking at a first-ply
        // node, which should be mergeable
        rv = OpenOverlayTag(aNode, nameSpaceID, tag);
        break;

    case eInDocumentElement:
        rv = OpenTag(aNode, nameSpaceID, tag);
        break;

    case eInEpilog:
    case eInScript:
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: warning: unexpected tags in epilog at line %d",
                aNode.GetSourceLineNumber()));

        rv = NS_ERROR_UNEXPECTED; // XXX
        break;
    }

    return rv;
}

NS_IMETHODIMP 
XULContentSinkImpl::CloseContainer(const nsIParserNode& aNode)
{
    // Never EVER return anything but NS_OK or
    // NS_ERROR_HTMLPARSER_BLOCK from this method. Doing so will blow
    // the parser's little mind all over the planet.
    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        const nsString& text = aNode.GetText();

        nsCAutoString extraWhiteSpace;
        PRInt32 count = mContextStack.Depth();
        while (--count > 0)
            extraWhiteSpace += "  ";

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("xul: %.5d. %s</%s>",
                aNode.GetSourceLineNumber(),
                NS_STATIC_CAST(const char*, extraWhiteSpace),
                NS_STATIC_CAST(const char*, nsCAutoString(text))));
    }
#endif

    if (mState == eInScript) {
        rv = CloseScript(aNode);

        if (NS_FAILED(rv))
            return NS_OK;
    }
    else {
        FlushText();
    }


    nsCOMPtr<nsIContent> element;
    rv = mContextStack.Pop(getter_AddRefs(element), &mState);
    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
            char* tagStr = aNode.GetText().ToNewCString();
            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("xul: extra close tag '</%s>' at line %d\n",
                    tagStr, aNode.GetSourceLineNumber()));
            nsCRT::free(tagStr);
        }
#endif
        // Failure to return NS_OK causes stuff to freak out. See Bug 4433.
        return NS_OK;
    }

    // Check for a 'datasources' tag, in which case we'll create a
    // template builder.
    if (element) {
        nsAutoString dataSources;
        rv = element->GetAttribute(kNameSpaceID_None, kDataSourcesAtom, dataSources);
        if (NS_FAILED(rv)) return NS_OK;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            nsCOMPtr<nsIRDFContentModelBuilder> builder;
            rv = CreateTemplateBuilder(element, dataSources, &builder);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add datasources");
            if (NS_SUCCEEDED(rv)) {
                // Force construction of immediate template sub-content _now_.
                rv = builder->CreateContents(element);
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create template contents");
            }
        }

        rv = NS_OK;
    }

    PopNameSpaces();

    if (mContextStack.Depth() == 0) {
        mState = eInEpilog;
      
        // We're about to finish parsing. Now we want to kick off the processing
        // of our child overlays.

        GetChromeOverlays();

        PRInt32 count;
        if ((count = mOverlayArray.Count()) != 0) {
            // Block the parser. It will only be unblocked after all
            // of our child overlays have finished parsing.
            rv = NS_ERROR_HTMLPARSER_BLOCK;

            // All content sinks in a chain back to the root have 
            // +count added to them.  Until all overlays are
            // processed, they won't unblock.
            UpdateOverlayCounters(count);
        }
        else if (mParentContentSink) {
            // We had no overlays.  If we have a parent content sink,
            // we need to notify them that this overlay completed.
            mParentContentSink->UpdateOverlayCounters(-1);
        }
    }

    return rv;
}


NS_IMETHODIMP 
XULContentSinkImpl::AddLeaf(const nsIParserNode& aNode)
{
    // XXX For now, all leaf content is character data
    AddCharacterData(aNode);
    return NS_OK;
}

NS_IMETHODIMP
XULContentSinkImpl::NotifyError(const nsParserError* aError)
{
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("xul: parser error '%s'", aError));

    return NS_OK;
}

// nsIXMLContentSink
NS_IMETHODIMP 
XULContentSinkImpl::AddXMLDecl(const nsIParserNode& aNode)
{
    // XXX We'll ignore it for now
    PR_LOG(gLog, PR_LOG_WARNING,
           ("xul: ignoring XML decl at line %d", aNode.GetSourceLineNumber()));

    return NS_OK;
}


NS_IMETHODIMP 
XULContentSinkImpl::AddComment(const nsIParserNode& aNode)
{
    FlushText();
    nsAutoString text;
    nsresult result = NS_OK;

    text = aNode.GetText();

    // XXX add comment here...

    return result;
}

static void SplitMimeType(const nsString& aValue, nsString& aType, nsString& aParams)
{
  aType.Truncate();
  aParams.Truncate();
  PRInt32 semiIndex = aValue.FindChar(PRUnichar(';'));
  if (-1 != semiIndex) {
    aValue.Left(aType, semiIndex);
    aValue.Right(aParams, (aValue.Length() - semiIndex) - 1);
  }
  else {
    aType = aValue;
  }
}

nsresult
XULContentSinkImpl::ProcessOverlay(const nsString& aHref)
{
    // Synchronously load the overlay.
    nsresult result = NS_OK;
  
    // Kick off the load 
    nsCOMPtr<nsIContentViewerContainer> container;
    nsCOMPtr<nsIXULParentDocument> xulParentDocument;
    xulParentDocument = do_QueryInterface(mDocument);
    if (xulParentDocument == nsnull) {
        NS_ERROR("Unable to turn document into a XUL parent document.");
        return result;
    }

    if (NS_FAILED(result = xulParentDocument->GetContentViewerContainer(getter_AddRefs(container)))) {
        NS_ERROR("Unable to retrieve content viewer container from parent document.");
        return result;
    }

    nsAutoString command;
    if (NS_FAILED(result = xulParentDocument->GetCommand(command))) {
        NS_ERROR("Unable to retrieve the command from parent document.");
        return result;
    }
  
    nsCOMPtr<nsIXULDocumentInfo> docInfo;
    if (NS_FAILED(result = nsComponentManager::CreateInstance(kXULDocumentInfoCID,
                                                              nsnull,
                                                              NS_GET_IID(nsIXULDocumentInfo),
                                                              (void**) getter_AddRefs(docInfo)))) {
        NS_ERROR("unable to create document info object");
        return result;
    }
  
    if (NS_FAILED(result = docInfo->Init(mDocument, this))) {
        NS_ERROR("unable to initialize doc info object.");
        return result;
    }

    // Turn the content viewer into a webshell
    nsCOMPtr<nsIWebShell> webshell;
    webshell = do_QueryInterface(container);
    if (webshell == nsnull) {
        NS_ERROR("this isn't a webshell. we're in trouble.");
        return result;
    }

    nsCOMPtr<nsIDocumentLoader> docLoader;
    if (NS_FAILED(result = webshell->GetDocumentLoader(*getter_AddRefs(docLoader)))) {
        NS_ERROR("unable to obtain the document loader to kick off the load.");
        return result;
    }

    nsCOMPtr<nsIURI> uri;
    result = NS_NewURI(getter_AddRefs(uri), aHref, mDocumentBaseURL);
    if (NS_FAILED(result)) return result;

    return docLoader->LoadSubDocument(uri, docInfo.get());
    return result;
}

nsresult
XULContentSinkImpl::ProcessStyleLink(nsIContent* aElement,
                                     const nsString& aHref, PRBool aAlternate,
                                     const nsString& aTitle, const nsString& aType,
                                     const nsString& aMedia)
{
  static const char kCSSType[] = "text/css";

  nsresult result = NS_OK;

  if (aAlternate) { // if alternate, does it have title?
    if (0 == aTitle.Length()) { // alternates must have title
      return NS_OK; //return without error, for now
    }
  }

  nsAutoString  mimeType;
  nsAutoString  params;
  SplitMimeType(aType, mimeType, params);

  if ((0 == mimeType.Length()) || mimeType.EqualsIgnoreCase(kCSSType)) {
    nsIURI* url = nsnull;
#ifdef NECKO
    result = NS_NewURI(&url, aHref, mDocumentBaseURL);
#else
    nsILoadGroup* LoadGroup = nsnull;
    mDocumentBaseURL->GetLoadGroup(&LoadGroup);
    if (LoadGroup) {
      result = LoadGroup->CreateURL(&url, mDocumentBaseURL, aHref, nsnull);
      NS_RELEASE(LoadGroup);
    }
    else {
      result = NS_NewURL(&url, aHref, mDocumentBaseURL);
    }
#endif
    if (NS_OK != result) {
      return NS_OK; // The URL is bad, move along, don't propagate the error (for now)
    }

    PRBool blockParser = PR_FALSE;
    if (! aAlternate) {
      if (0 < aTitle.Length()) {  // possibly preferred sheet
        if (0 == mPreferredStyle.Length()) {
          mPreferredStyle = aTitle;
          mCSSLoader->SetPreferredSheet(aTitle);
          nsIAtom* defaultStyle = NS_NewAtom("default-style");
          if (defaultStyle) {
            mDocument->SetHeaderData(defaultStyle, aTitle);
            NS_RELEASE(defaultStyle);
          }
        }
      }
      else {  // persistent sheet, block
        blockParser = PR_TRUE;
      }
    }

    PRBool doneLoading;
    result = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia, kNameSpaceID_Unknown,
                                       mStyleSheetCount++, 
                                       ((blockParser) ? mParser : nsnull),
                                       doneLoading);
    NS_RELEASE(url);
    if (NS_SUCCEEDED(result) && blockParser && (! doneLoading)) {
      result = NS_ERROR_HTMLPARSER_BLOCK;
    }
  }
  return result;
}


NS_IMETHODIMP 
XULContentSinkImpl::AddProcessingInstruction(const nsIParserNode& aNode)
{

    static const char kStyleSheetPI[] = "<?xml-stylesheet";
    static const char kOverlayPI[] = "<?xul-overlay";

    nsresult rv;
    FlushText();

    // XXX For now, we don't add the PI to the content model.
    // We just check for a style sheet PI
    const nsString& text = aNode.GetText();

    if (text.Find(kOverlayPI) == 0) {
        // Load a XUL overlay.
        nsAutoString href;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "href", href);
        if (NS_FAILED(rv)) return rv;

        // If there was an error or there's no href, we can't do
        // anything with this PI
        if (0 == href.Length())
            return NS_OK;

        // Add the overlay to our list of overlays that need to be processed.
        nsString* overlayItem = new nsString(href);
        mOverlayArray.AppendElement(overlayItem);
    }
    // If it's a stylesheet PI...
    else if (text.Find(kStyleSheetPI) == 0) {
        nsAutoString href;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "href", href);
        if (NS_FAILED(rv)) return rv;

        // If there was an error or there's no href, we can't do
        // anything with this PI
        if (0 == href.Length())
            return NS_OK;

        nsAutoString type;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "type", type);
        if (NS_FAILED(rv)) return rv;

        nsAutoString title;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "title", title);
        if (NS_FAILED(rv)) return rv;

        title.CompressWhitespace();

        nsAutoString media;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "media", media);
        if (NS_FAILED(rv)) return rv;

        media.ToLowerCase();

        nsAutoString alternate;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "alternate", alternate);
        if (NS_FAILED(rv)) return rv;

        rv = ProcessStyleLink(nsnull /* XXX need a node here */,
                              href, alternate.Equals("yes"),  /* XXX ignore case? */
                              title, type, media);

        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)
{
    PR_LOG(gLog, PR_LOG_WARNING,
           ("xul: ignoring doctype decl at line %d", aNode.GetSourceLineNumber()));

    return NS_OK;
}


NS_IMETHODIMP 
XULContentSinkImpl::AddCharacterData(const nsIParserNode& aNode)
{
    nsAutoString text = aNode.GetText();

    if (aNode.GetTokenType() == eToken_entity) {
        char buf[12];
        text.ToCString(buf, sizeof(buf));
        text.Truncate();
        text.Append(nsRDFParserUtils::EntityToUnicode(buf));
    }

    PRInt32 addLen = text.Length();
    if (0 == addLen) {
        return NS_OK;
    }

    // Create buffer when we first need it
    if (0 == mTextSize) {
        mText = (PRUnichar *) PR_MALLOC(sizeof(PRUnichar) * 4096);
        if (nsnull == mText) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mTextSize = 4096;
    }

    // Copy data from string into our buffer; flush buffer when it fills up
    PRInt32 offset = 0;
    while (0 != addLen) {
        PRInt32 amount = mTextSize - mTextLength;
        if (amount > addLen) {
            amount = addLen;
        }
        if (0 == amount) {
            if (mConstrainSize) {
                nsresult rv = FlushText();
                if (NS_OK != rv) {
                    return rv;
                }
            }
            else {
                mTextSize += addLen;
                mText = (PRUnichar *) PR_REALLOC(mText, sizeof(PRUnichar) * mTextSize);
                if (nsnull == mText) {
                    return NS_ERROR_OUT_OF_MEMORY;
                }
            }
        }
        memcpy(&mText[mTextLength], text.GetUnicode() + offset,
               sizeof(PRUnichar) * amount);
        mTextLength += amount;
        offset += amount;
        addLen -= amount;
    }

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::AddUnparsedEntity(const nsIParserNode& aNode)
{
    PR_LOG(gLog, PR_LOG_WARNING,
           ("xul: ignoring unparsed entity at line %d", aNode.GetSourceLineNumber()));

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::AddNotation(const nsIParserNode& aNode)
{
    PR_LOG(gLog, PR_LOG_WARNING,
           ("xul: ignoring notation at line %d", aNode.GetSourceLineNumber()));

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::AddEntityReference(const nsIParserNode& aNode)
{
    PR_LOG(gLog, PR_LOG_WARNING,
           ("xul: ignoring entity reference at line %d", aNode.GetSourceLineNumber()));

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFContentSink interface

NS_IMETHODIMP
XULContentSinkImpl::Init(nsIDocument* aDocument)
{
    NS_PRECONDITION(aDocument != nsnull, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;
    
    nsresult rv;

    mActualDocument = aDocument;

    // We could potentially be an overlay; if we are, then our
    // nsIXULChildDocument interface will have a content sink set.
    nsCOMPtr<nsIXULChildDocument> overlay = do_QueryInterface(aDocument);
    NS_ASSERTION(overlay != nsnull, "not an nsIXULChildDocument");
    if (! overlay)
        return NS_ERROR_UNEXPECTED;

    rv = overlay->GetContentSink(getter_AddRefs(mParentContentSink));
    if (NS_FAILED(rv)) return rv;

    if (! mParentContentSink) {
        // Nope. We're the root document.
        mDocument = aDocument;
    }
    else {
        // We _are_ an overlay. Find the parent document's data source
        // and make assertions there.
      
        // First of all, find the root document.
        nsCOMPtr<nsIDocument> root = dont_QueryInterface(aDocument);
        while (1) {
            nsCOMPtr<nsIDocument> parent = dont_AddRef(root->GetParentDocument());

            // root will have no parent.
            if (! parent)
                break;

            root = parent;
        }

        mDocument = root;
    }

    mDocumentURL = dont_AddRef(mDocument->GetDocumentURL());
    mDocumentBaseURL = mDocumentURL;

    // XXX this presumes HTTP header info is already set in document
    // XXX if it isn't we need to set it here...
    nsCOMPtr<nsIAtom> defaultStyle = dont_AddRef( NS_NewAtom("default-style") );
    if (! defaultStyle)
        return NS_ERROR_OUT_OF_MEMORY;

    mDocument->GetHeaderData(defaultStyle, mPreferredStyle);

    nsCOMPtr<nsIHTMLContentContainer> htmlContainer = do_QueryInterface(mDocument);
    NS_ASSERTION(htmlContainer != nsnull, "not an HTML container");
    if (! htmlContainer)
        return NS_ERROR_UNEXPECTED;

    htmlContainer->GetCSSLoader(*getter_AddRefs(mCSSLoader));

    mState = eInProlog;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Text buffering

static PRBool
rdf_IsDataInBuffer(PRUnichar* buffer, PRInt32 length)
{
    for (PRInt32 i = 0; i < length; ++i) {
        if (buffer[i] == ' ' ||
            buffer[i] == '\t' ||
            buffer[i] == '\n' ||
            buffer[i] == '\r')
            continue;

        return PR_TRUE;
    }
    return PR_FALSE;
}


nsresult
XULContentSinkImpl::FlushText(PRBool aCreateTextNode)
{
    nsresult rv;

    do {
        // Don't do anything if there's no text to create a node from, or
        // if they've told us not to create a text node
        if (! mTextLength)
            break;

        if (! aCreateTextNode)
            break;

        // Don't bother if there's nothing but whitespace.
        // XXX This could cause problems...
        if (! rdf_IsDataInBuffer(mText, mTextLength))
            break;

        // Don't bother if we're not in XUL document body
        if (mState != eInDocumentElement || mContextStack.Depth() == 0)
            break;

        nsCOMPtr<nsIContent> parent;
        rv = mContextStack.GetTopElement(getter_AddRefs(parent));
        if (NS_FAILED(rv)) return rv;

        // Trim the leading and trailing whitespace
        nsAutoString value;
        value.Append(mText, mTextLength);
        value.Trim(" \t\n\r");

        nsCOMPtr<nsITextContent> text;
        rv = nsComponentManager::CreateInstance(kTextNodeCID,
                                                nsnull,
                                                NS_GET_IID(nsITextContent),
                                                getter_AddRefs(text));
        if (NS_FAILED(rv)) return rv;

        rv = text->SetText(mText, mTextLength, PR_FALSE);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIContent> child = do_QueryInterface(text);
        NS_ASSERTION(child != nsnull, "not an nsIContent");
        if (! child)
            return NS_ERROR_UNEXPECTED;

        // hook it up to the child
        rv = parent->AppendChildTo(child, PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to append text node");
        if (NS_FAILED(rv)) return rv;
    } while (0);

    // Reset our text buffer
    mTextLength = 0;
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////

nsresult
XULContentSinkImpl::GetXULIDAttribute(const nsIParserNode& aNode, nsString& aID)
{
    for (PRInt32 i = aNode.GetAttributeCount(); i >= 0; --i) {
        if (aNode.GetKeyAt(i).Equals("id")) {
            aID = aNode.GetValueAt(i);
            nsRDFParserUtils::StripAndConvert(aID);
            return NS_OK;
        }
    }

    // Otherwise, we couldn't find anything, so just gensym one...
    aID.Truncate();
    return NS_OK;
}

nsresult
XULContentSinkImpl::AddAttributes(const nsIParserNode& aNode, nsIContent* aElement)
{
    // Add tag attributes to the element
    nsresult rv;

    PRInt32 count = aNode.GetAttributeCount();
    for (PRInt32 i = 0; i < count; i++) {
        const nsString& qname = aNode.GetKeyAt(i);

        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> attr;
        rv = aElement->ParseAttributeString(qname, *getter_AddRefs(attr), nameSpaceID);

        if (NS_FAILED(rv)) {
            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("xul: unable to parse attribute '%s' at line %d",
                    (const char*) nsCAutoString(qname), aNode.GetSourceLineNumber()));

            // Bring it.
            continue;
        }

        nsAutoString valueStr(aNode.GetValueAt(i));
        nsRDFParserUtils::StripAndConvert(valueStr);

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
            nsCAutoString extraWhiteSpace;
            PRInt32 cnt = mContextStack.Depth();
            while (--cnt >= 0)
                extraWhiteSpace += "  ";

            PR_LOG(gLog, PR_LOG_DEBUG,
                   ("xul: %.5d. %s    %s=%s",
                    aNode.GetSourceLineNumber(),
                    NS_STATIC_CAST(const char*, extraWhiteSpace),
                    NS_STATIC_CAST(const char*, nsCAutoString(qname)),
                    NS_STATIC_CAST(const char*, nsCAutoString(valueStr))));
        }
#endif

        aElement->SetAttribute(nameSpaceID, attr, valueStr, PR_FALSE);
    }

    return NS_OK;
}



nsresult
XULContentSinkImpl::ParseTag(const nsString& aText, nsIAtom*& aTag, PRInt32& aNameSpaceID)
{
    nsresult rv;

    // Split the tag into prefix and tag substrings.
    nsAutoString prefixStr;
    nsAutoString tagStr = aText;
    PRInt32 nsoffset = tagStr.FindChar(kNameSpaceSeparator);
    if (nsoffset >= 0) {
        tagStr.Left(prefixStr, nsoffset);
        tagStr.Cut(0, nsoffset + 1);
    }

    nsCOMPtr<nsIAtom> prefix;
    if (prefixStr.Length() > 0) {
        prefix = dont_AddRef( NS_NewAtom(prefixStr) );
    }

    nsCOMPtr<nsINameSpace> ns;
    rv = GetTopNameSpace(&ns);
    if (NS_FAILED(rv)) return rv;

    rv = ns->FindNameSpaceID(prefix, aNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    aTag = NS_NewAtom(tagStr);
    if (! aTag)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

nsresult
XULContentSinkImpl::CreateHTMLElement(nsIAtom* aTag, nsCOMPtr<nsIContent>* aResult)
{
    nsresult rv;

    const PRUnichar* tagStr;
    rv = aTag->GetUnicode(&tagStr);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIHTMLContent> htmlelement;
    rv = gHTMLElementFactory->CreateInstanceByTag(tagStr, getter_AddRefs(htmlelement));
    if (NS_FAILED(rv)) return rv;

    rv = htmlelement->SetDocument(mDocument, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFormControl> htmlformctrl = do_QueryInterface(htmlelement);
    if (htmlformctrl) {
        nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
        NS_ASSERTION(xuldoc != nsnull, "no nsIXULDocument interface");
        if (! xuldoc)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIDOMHTMLFormElement> docform;
        rv = xuldoc->GetForm(getter_AddRefs(docform));
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(docform != nsnull, "no document form");
        if (! docform)
            return NS_ERROR_UNEXPECTED;

        htmlformctrl->SetForm(docform);
    }

    *aResult = do_QueryInterface(htmlelement);
    if (! *aResult)
        return NS_ERROR_UNEXPECTED;

    return rv;
}

nsresult
XULContentSinkImpl::CreateXULElement(PRInt32 aNameSpaceID, nsIAtom* aTag, nsCOMPtr<nsIContent>* aResult)
{
    nsresult rv;

    nsCOMPtr<nsIContent> element;
    rv = nsXULElement::Create(aNameSpaceID, aTag, getter_AddRefs(element));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new content element");
    if (NS_FAILED(rv)) return rv;

    rv = element->SetDocument(mDocument, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // Initialize the element's containing namespace to that of its
    // parent.
    nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(element) );
    NS_ASSERTION(xml != nsnull, "not an XML element");
    if (! xml)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsINameSpace> ns;
    rv = GetTopNameSpace(&ns);
    if (NS_FAILED(rv)) return rv;

    rv = xml->SetContainingNameSpace(ns);
    if (NS_FAILED(rv)) return rv;

    // Set the XML tag info: namespace prefix and ID. We do this
    // _after_ processing all the attributes so that we can extract an
    // appropriate prefix based on whatever namespace decls are
    // active.
    rv = xml->SetNameSpaceID(aNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    if (aNameSpaceID != kNameSpaceID_None) {
        nsCOMPtr<nsINameSpace> nameSpace;
        rv = xml->GetContainingNameSpace(*getter_AddRefs(nameSpace));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIAtom> prefix;
        rv = nameSpace->FindNameSpacePrefix(aNameSpaceID, *getter_AddRefs(prefix));
        if (NS_FAILED(rv)) return rv;

        rv = xml->SetNameSpacePrefix(prefix);
        if (NS_FAILED(rv)) return rv;
    }

    // We also need to pay special attention to the keyset tag to set up a listener
    //
    // XXX shouldn't this be done in SetAttribute() or something?
    if((aNameSpaceID == kNameSpaceID_XUL) && (aTag == kKeysetAtom)) {
        // Create our nsXULKeyListener and hook it up.
        nsCOMPtr<nsIXULKeyListener> keyListener;
        rv = nsComponentManager::CreateInstance(kXULKeyListenerCID,
                                                nsnull,
                                                NS_GET_IID(nsIXULKeyListener),
                                                getter_AddRefs(keyListener));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a key listener");
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsIDOMEventListener> domEventListener = do_QueryInterface(keyListener);
        if (domEventListener) {
            // Take the key listener and add it as an event listener for keypress events.
            nsCOMPtr<nsIDOMDocument> domDocument = do_QueryInterface(mDocument);

            // Init the listener with the keyset node
            nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(element);
            keyListener->Init(domElement, domDocument);
            
            nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(mDocument);
            if(receiver) {
                receiver->AddEventListener("keypress", domEventListener, PR_TRUE); 
                receiver->AddEventListener("keydown",  domEventListener, PR_TRUE);  
                receiver->AddEventListener("keyup",    domEventListener, PR_TRUE);   
            }
        }
    }

    *aResult = element;
    return NS_OK;
}


nsresult
XULContentSinkImpl::CreateTemplateBuilder(nsIContent* aElement,
                                          const nsString& aDataSources,
                                          nsCOMPtr<nsIRDFContentModelBuilder>* aResult)
{
    nsresult rv;

    // construct a new builder
    nsCOMPtr<nsIRDFContentModelBuilder> builder;
    rv = nsComponentManager::CreateInstance(kXULTemplateBuilderCID,
                                            nsnull,
                                            NS_GET_IID(nsIRDFContentModelBuilder),
                                            getter_AddRefs(builder));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create tree content model builder");
    if (NS_FAILED(rv)) return rv;

    rv = builder->SetRootContent(aElement);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set builder's root content element");
    if (NS_FAILED(rv)) return rv;

    // create a database for the builder
    nsCOMPtr<nsIRDFCompositeDataSource> db;
    rv = nsComponentManager::CreateInstance(kRDFCompositeDataSourceCID,
                                            nsnull,
                                            NS_GET_IID(nsIRDFCompositeDataSource),
                                            getter_AddRefs(db));

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to construct new composite data source");
    if (NS_FAILED(rv)) return rv;

    // Add the local store as the first data source in the db. Note
    // that we _might_ not be able to get a local store if we haven't
    // got a profile to read from yet.
    nsCOMPtr<nsIRDFDataSource> localstore;
    rv = gRDFService->GetDataSource("rdf:local-store", getter_AddRefs(localstore));
    if (NS_SUCCEEDED(rv)) {
        rv = db->AddDataSource(localstore);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add local store to db");
        if (NS_FAILED(rv)) return rv;
    }

    // Parse datasources: they are assumed to be a whitespace
    // separated list of URIs; e.g.,
    //
    //     rdf:bookmarks rdf:history http://foo.bar.com/blah.cgi?baz=9
    //
    PRInt32 first = 0;

    while(1) {
        while (first < aDataSources.Length() && nsString::IsSpace(aDataSources.CharAt(first)))
            ++first;

        if (first >= aDataSources.Length())
            break;

        PRInt32 last = first;
        while (last < aDataSources.Length() && !nsString::IsSpace(aDataSources.CharAt(last)))
            ++last;

        nsAutoString uri;
        aDataSources.Mid(uri, first, last - first);
        first = last + 1;

        // A special 'dummy' datasource
        if (uri.Equals("rdf:null"))
            continue;

        rv = rdf_MakeAbsoluteURI(mDocumentURL, uri);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFDataSource> ds;
        rv = gRDFService->GetDataSource(nsCAutoString(uri), getter_AddRefs(ds));

        if (NS_FAILED(rv)) {
            // This is only a warning because the data source may not
            // be accessable for any number of reasons, including
            // security, a bad URL, etc.
#ifdef DEBUG
            nsCAutoString msg;
            msg += "unable to load datasource '";
            msg += nsCAutoString(uri);
            msg += '\'';
            NS_WARNING((const char*) msg);
#endif
            continue;
        }

        rv = db->AddDataSource(ds);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add datasource to composite data source");
        if (NS_FAILED(rv)) return rv;
    }

    // add it to the set of builders in use by the document
    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
    if (! xuldoc)
        return NS_ERROR_UNEXPECTED;

    rv = xuldoc->AddContentModelBuilder(builder);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add builder to the document");
    if (NS_FAILED(rv)) return rv;

    rv = builder->SetDataBase(db);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set builder's database");
    if (NS_FAILED(rv)) return rv;

    *aResult = builder;
    return NS_OK;
}


nsresult
XULContentSinkImpl::OpenRoot(const nsIParserNode& aNode, PRInt32 aNameSpaceID, nsIAtom* aTag)
{
    nsresult rv;
    if ((aNameSpaceID == kNameSpaceID_HTML) && (aTag == kScriptAtom)) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: script tag not allowed as root content element"));

        return NS_ERROR_UNEXPECTED;
    }

    // Create the element
    nsCOMPtr<nsIContent> element;
    if (aNameSpaceID == kNameSpaceID_HTML) {
        rv = CreateHTMLElement(aTag, &element);
    }
    else {
        rv = CreateXULElement(aNameSpaceID, aTag, &element);
    }

    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: unable to create element '%s' at line %d",
                NS_STATIC_CAST(const char*, nsCAutoString(aNode.GetText())),
                aNode.GetSourceLineNumber()));
#endif

        return rv;
    }

    // Okay, this is the root element. Do all the juju we need to
    // do to get stuff hooked up.
    NS_ASSERTION(mState == eInProlog, "how'd we get here?");
    if (mState != eInProlog)
        return NS_ERROR_UNEXPECTED;

    mDocument->SetRootContent(element);

    // Add the attributes
    rv = AddAttributes(aNode, element);
    if (NS_FAILED(rv)) return rv;

    // Create the document's "hidden form" element which will wrap all
    // HTML form elements that turn up.
    nsCOMPtr<nsIHTMLContent> form;
    rv = gHTMLElementFactory->CreateInstanceByTag(nsAutoString("form"),
                                                  getter_AddRefs(form));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create form element");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMHTMLFormElement> htmlFormElement = do_QueryInterface(form);
    NS_ASSERTION(htmlFormElement != nsnull, "not an nSIDOMHTMLFormElement");
    if (! htmlFormElement)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
    if (! xuldoc)
        return NS_ERROR_UNEXPECTED;

    rv = xuldoc->SetForm(htmlFormElement);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set form element");
    if (NS_FAILED(rv)) return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIContent> content = do_QueryInterface(form);
    NS_ASSERTION(content != nsnull, "not an nsIContent");
    if (! content)
        return NS_ERROR_UNEXPECTED;

    // XXX Would like to make this anonymous, but still need the
    // form's frame to get built. For now make it explicit.
    rv = element->InsertChildAt(content, 0, PR_FALSE); 
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add anonymous form element");
    if (NS_FAILED(rv)) return rv;

    // Add the element to the XUL document's ID-to-element map.
    rv = AddElementToMap(element);
    if (NS_FAILED(rv)) return rv;

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;
    return NS_OK;
}


nsresult
XULContentSinkImpl::OpenOverlayRoot(const nsIParserNode& aNode, PRInt32 aNameSpaceID, nsIAtom* aTag)
{
    nsresult rv;

    if (aNameSpaceID != kNameSpaceID_XUL || aTag != kOverlayAtom) {
#ifdef PR_LOGGING
        nsXPIDLCString urlstr;
        mDocumentURL->GetSpec(getter_Copies(urlstr));
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: expected 'overlay' tag to open %s", (const char*) urlstr));
#endif

        return NS_ERROR_UNEXPECTED;
    }

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(nsnull, mState);
    if (NS_FAILED(rv)) return rv;

    mState = eInOverlayElement;
    return NS_OK;
}

nsresult
XULContentSinkImpl::OpenTag(const nsIParserNode& aNode, PRInt32 aNameSpaceID, nsIAtom* aTag)
{
    nsresult rv;
    if ((aNameSpaceID == kNameSpaceID_HTML) && (aTag == kScriptAtom)) {
        // Oops, it's a script!
        return OpenScript(aNode);
    }

    // Create the element
    nsCOMPtr<nsIContent> element;
    if (aNameSpaceID == kNameSpaceID_HTML) {
        rv = CreateHTMLElement(aTag, &element);
    }
    else {
        rv = CreateXULElement(aNameSpaceID, aTag, &element);
    }

    if (NS_FAILED(rv)) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: unable to create element '%s' at line %d",
                NS_STATIC_CAST(const char*, nsCAutoString(aNode.GetText())),
                aNode.GetSourceLineNumber()));

        return rv;
    }

    // Add the attributes
    rv = AddAttributes(aNode, element);
    if (NS_FAILED(rv)) return rv;

    // Insert the element into the content model
    nsCOMPtr<nsIContent> parent;
    rv = mContextStack.GetTopElement(getter_AddRefs(parent));
    if (NS_FAILED(rv)) return rv;

    rv = InsertElement(parent, element);
    if (NS_FAILED(rv)) return rv;

    // Check for an 'id' attribute.  If we're inside a XUL template,
    // then _everything_ needs to have an ID for the 'template'
    // attribute hookup.
    nsAutoString id;
    rv = element->GetAttribute(kNameSpaceID_None, kIdAtom, id);
    if (NS_FAILED(rv)) return rv;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE && mContextStack.IsInsideXULTemplate()) {
        id = "$";
        id.Append(PRInt32(element.get()), 16);
        rv = element->SetAttribute(kNameSpaceID_None, kIdAtom, id, PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    // Add the element to the XUL document's ID-to-element map.
    rv = AddElementToMap(element);
    if (NS_FAILED(rv)) return rv;

    // Check for a 'commandupdater' attribute, in which case we'll
    // hook the node up as a command updater.
    nsAutoString value;
    rv = element->GetAttribute(kNameSpaceID_None, kCommandUpdaterAtom, value);
    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && value.Equals("true")) {
        rv = gXULUtils->SetCommandUpdater(mDocument, element);
        if (NS_FAILED(rv)) return rv;
    }

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;

    return NS_OK;
}


nsresult
XULContentSinkImpl::OpenOverlayTag(const nsIParserNode& aNode, PRInt32 aNameSpaceID, nsIAtom* aTag)
{
    nsresult rv;
    if ((aNameSpaceID == kNameSpaceID_HTML) && (aTag == kScriptAtom)) {
        // Oops, it's a script!
        return OpenScript(aNode);
    }

    // First, see if the element is already in the tree. If so, we can
    // do _immediate_ hookup.
    nsAutoString id;
    rv = GetXULIDAttribute(aNode, id);
    if (NS_FAILED(rv)) return rv;

    if (id.Length() == 0) {
        // XXX We will probably get our context stack screwed up
        // because we haven't pushed something here...

        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: overlay element at line %d has no 'id' attribute",
                aNode.GetSourceLineNumber()));

        return NS_OK;
    }

    nsCOMPtr<nsIDOMXULDocument> domxuldoc = do_QueryInterface(mDocument);
    if (! domxuldoc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDOMElement> domparent;
    rv = domxuldoc->GetElementById(id, getter_AddRefs(domparent));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIContent> element;

    if (domparent) {
        // Great, the element was already in the content model. We'll
        // just hook it up.
        element = do_QueryInterface(domparent);
        NS_ASSERTION(element != nsnull, "element is not an nsIContent");
        if (! element)
            return NS_ERROR_UNEXPECTED;
    }
    else {
        // Create a dummy element that we can throw some attributes
        // and children onto.
        nsCOMPtr<nsIXMLContent> xml;
        rv = gXMLElementFactory->CreateInstanceByTag(nsAutoString("overlay"), getter_AddRefs(xml));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create overlay element");
        if (NS_FAILED(rv)) return rv;

        element = do_QueryInterface(xml);
        NS_ASSERTION(element != nsnull, "xml element is not an nsIXMLContent");
        if (! xml)
            return NS_ERROR_UNEXPECTED;

        // Initialize the overlay element
        rv = element->SetDocument(mDocument, PR_FALSE);
        if (NS_FAILED(rv)) return rv;
            
        // Set up namespace junk so that subsequent parsing won't
        // freak out.
        nsCOMPtr<nsINameSpace> ns;
        rv = GetTopNameSpace(&ns);
        if (NS_FAILED(rv)) return rv;
            
        rv = xml->SetContainingNameSpace(ns);
        if (NS_FAILED(rv)) return rv;

        OverlayForwardReference* fwdref = new OverlayForwardReference(element);
        if (! fwdref)
            return NS_ERROR_OUT_OF_MEMORY;

        nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
        if (! xuldoc)
            return NS_ERROR_UNEXPECTED;

        // transferring ownership to ya...
        rv = xuldoc->AddForwardReference(fwdref);
        if (NS_FAILED(rv)) return rv;

        // No match, just push the overlay element onto the context
        // stack so we have somewhere to hang its child nodes off'n.
    }

    // Add the attributes
    rv = AddAttributes(aNode, element);
    if (NS_FAILED(rv)) return rv;

    rv = AddElementToMap(element);
    if (NS_FAILED(rv)) return rv;

    if (domparent) {
        // Check for a 'commandupdater' attribute, in which case we'll
        // hook the node up as a command updater. We only do this in
        // the case that the element was already in the document.
        nsAutoString value;
        rv = element->GetAttribute(kNameSpaceID_None, kCommandUpdaterAtom, value);
        if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && value.Equals("true")) {
            rv = gXULUtils->SetCommandUpdater(mDocument, element);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;

    return NS_OK;
}


nsresult
XULContentSinkImpl::Merge(nsIContent* aOriginalNode, nsIContent* aOverlayNode)
{
    nsresult rv;

    {
        // Whack the attributes from aOverlayNode onto aOriginalNode
        PRInt32 count;
        rv = aOverlayNode->GetAttributeCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = 0; i < count; ++i) {
            PRInt32 nameSpaceID;
            nsCOMPtr<nsIAtom> tag;
            rv = aOverlayNode->GetAttributeNameAt(i, nameSpaceID, *getter_AddRefs(tag));
            if (NS_FAILED(rv)) return rv;

            nameSpaceID = kNameSpaceID_None;

            if (nameSpaceID == kNameSpaceID_None && tag.get() == kIdAtom)
                continue;

            nsAutoString value;
            rv = aOverlayNode->GetAttribute(nameSpaceID, tag, value);
            if (NS_FAILED(rv)) return rv;

            rv = aOriginalNode->SetAttribute(nameSpaceID, tag, value, PR_FALSE);
            if (NS_FAILED(rv)) return rv;
        }

        // Check for a 'commandupdater' attribute, in which case we'll
        // hook the node up as a command updater. We need to do this
        // _here_, because it's possible that an overlay has added the
        // 'commandupdater' attribute, or even altered the 'targets'
        // or 'events' attributes.
        nsAutoString value;
        rv = aOriginalNode->GetAttribute(kNameSpaceID_None, kCommandUpdaterAtom, value);
        if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && value.Equals("true")) {
            nsCOMPtr<nsIDocument> doc;
            rv = aOriginalNode->GetDocument(*getter_AddRefs(doc));
            if (NS_FAILED(rv)) return rv;

            rv = gXULUtils->SetCommandUpdater(doc, aOriginalNode);
            if (NS_FAILED(rv)) return rv;
        }
    }

    {
        // Now move any kids
        PRInt32 count;
        rv = aOverlayNode->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIContent> child;
            rv = aOverlayNode->ChildAt(i, *getter_AddRefs(child));
            if (NS_FAILED(rv)) return rv;

            rv = InsertElement(aOriginalNode, child);
            if (NS_FAILED(rv)) return rv;
        }

        // Ok, now we _don't_ need to add these to the
        // document-to-element map because normal construction of the
        // nodes in OpenTag() will have done this.
    }

    return NS_OK;
}


nsresult
XULContentSinkImpl::InsertElement(nsIContent* aParent, nsIContent* aChild)
{
    // Insert aChild appropriately into aParent, accountinf for a
    // 'pos' attribute set on aChild.
    nsresult rv;

    nsAutoString posStr;
    rv = aChild->GetAttribute(kNameSpaceID_None, kPositionAtom, posStr);
    if (NS_FAILED(rv)) return rv;

    PRBool wasInserted = PR_FALSE;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        PRInt32 pos = posStr.ToInteger(NS_REINTERPRET_CAST(PRInt32*, &rv));
        if (NS_SUCCEEDED(rv)) {
            rv = aParent->InsertChildAt(aChild, pos, PR_FALSE);
            if (NS_FAILED(rv)) return rv;

            wasInserted = PR_TRUE;
        }
    }

    if (! wasInserted) {
        rv = aParent->AppendChildTo(aChild, PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
XULContentSinkImpl::AddElementToMap(nsIContent* aElement)
{
    // Add the specified element to the document's ID-to-element map
    nsresult rv;

    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
    if (! xuldoc)
        return NS_ERROR_UNEXPECTED;

    nsAutoString id;
    rv = aElement->GetAttribute(kNameSpaceID_None, kIdAtom, id);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        rv = xuldoc->AddElementForID(id, aElement);
        if (NS_FAILED(rv)) return rv;
    }

    rv = aElement->GetAttribute(kNameSpaceID_None, kRefAtom, id);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        rv = xuldoc->AddElementForID(id, aElement);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
XULContentSinkImpl::OpenScript(const nsIParserNode& aNode)
{
    nsresult rv = NS_OK;
    PRBool   isJavaScript = PR_TRUE;
    PRInt32 i, ac = aNode.GetAttributeCount();

    // Look for SRC attribute and look for a LANGUAGE attribute
    nsAutoString src;
    for (i = 0; i < ac; i++) {
        const nsString& key = aNode.GetKeyAt(i);
        if (key.EqualsIgnoreCase("src")) {
            src = aNode.GetValueAt(i);
            nsRDFParserUtils::StripAndConvert(src);
        }
        else if (key.EqualsIgnoreCase("type")) {
            nsAutoString  type(aNode.GetValueAt(i));
            nsRDFParserUtils::StripAndConvert(type);
            nsAutoString  mimeType;
            nsAutoString  params;
            SplitMimeType(type, mimeType, params);

            isJavaScript = mimeType.EqualsIgnoreCase("text/javascript");
        }
        else if (key.EqualsIgnoreCase("language")) {
            nsAutoString  lang(aNode.GetValueAt(i));
            nsRDFParserUtils::StripAndConvert(lang);
            isJavaScript = nsRDFParserUtils::IsJavaScriptLanguage(lang);
        }
    }
  
    // Don't process scripts that aren't JavaScript
    if (isJavaScript) {
        nsAutoString script;

        // If there is a SRC attribute...
        if (src.Length() > 0) {
            // Use the SRC attribute value to load the URL
            nsIURI* url = nsnull;
            nsAutoString absURL;
#ifdef NECKO
            rv = NS_NewURI(&url, src, mDocumentBaseURL);
#else
            nsILoadGroup* LoadGroup;

            rv = mDocumentBaseURL->GetLoadGroup(&LoadGroup);
      
            if ((NS_OK == rv) && LoadGroup) {
                rv = LoadGroup->CreateURL(&url, mDocumentBaseURL, src, nsnull);
                NS_RELEASE(LoadGroup);
            }
            else {
                rv = NS_NewURL(&url, absURL);
            }
#endif
            if (NS_OK != rv) {
                return rv;
            }

            // Add a reference to this since the url loader is holding
            // onto it as opaque data.
            NS_ADDREF(this);

            // Set the current script URL so that the
            // DoneLoadingScript() call can get report the right file
            // if there are errors in the script.
            mCurrentScriptURL = url;

            nsIUnicharStreamLoader* loader;
            nsCOMPtr<nsILoadGroup> group;

            mDocument->GetDocumentLoadGroup(getter_AddRefs(group));
            rv = NS_NewUnicharStreamLoader(&loader,
                                           url, 
                                           group,
                                           (nsStreamCompleteFunc)DoneLoadingScript, 
                                           (void *)this);
            NS_RELEASE(url);
            if (NS_OK == rv) {
                rv = NS_ERROR_HTMLPARSER_BLOCK;
            }
        }

        mInScript = PR_TRUE;
        mConstrainSize = PR_FALSE;
        mScriptLineNo = (PRUint32)aNode.GetSourceLineNumber();

        mContextStack.Push(nsnull, mState);
        mState = eInScript;
    }

    return rv;
}

void
XULContentSinkImpl::DoneLoadingScript(nsIUnicharStreamLoader* aLoader,
                                      nsString& aData,
                                      void* aRef,
                                      nsresult aStatus)
{
    XULContentSinkImpl* sink = (XULContentSinkImpl*)aRef;

    if (NS_OK == aStatus) {
        // XXX We have no way of indicating failure. Silently fail?
        sink->EvaluateScript(aData, sink->mCurrentScriptURL, 0);
    }
    else {
        NS_ERROR("error loading script");
    }

    if (sink->mParser) {
        sink->mParser->EnableParser(PR_TRUE);
    }

    // The url loader held a reference to the sink
    NS_RELEASE(sink);

    // We added a reference when the loader was created. This
    // release should destroy it.
    NS_RELEASE(aLoader);
}


nsresult
XULContentSinkImpl::EvaluateScript(nsString& aScript, nsIURI* aURI, PRUint32 aLineNo)
{
    nsresult rv = NS_OK;

    if (0 < aScript.Length()) {
        nsIScriptContextOwner *owner;
        nsIScriptContext *context;
        owner = mDocument->GetScriptContextOwner();
        if (nsnull != owner) {
      
            rv = owner->GetScriptContext(&context);
            if (rv != NS_OK) {
                NS_RELEASE(owner);
                return rv;
            }

            nsXPIDLCString url;
            if (aURI) {
                (void)aURI->GetSpec(getter_Copies(url));
            }

            nsAutoString val;
            PRBool isUndefined;

            context->EvaluateString(aScript, url, aLineNo, 
                                    val, &isUndefined);
      
            NS_RELEASE(context);
            NS_RELEASE(owner);
        }
    }

    return NS_OK;
}

nsresult
XULContentSinkImpl::CloseScript(const nsIParserNode& aNode)
{
    nsresult result = NS_OK;
    if (mInScript) {
        nsAutoString script;
        script.SetString(mText, mTextLength);
        nsCOMPtr<nsIURI> docURL = dont_AddRef(mDocument->GetDocumentURL());
        result = EvaluateScript(script, docURL, mScriptLineNo);
        FlushText(PR_FALSE);
        mInScript = PR_FALSE;
    }

    return result;
}



////////////////////////////////////////////////////////////////////////
// Namespace management

void
XULContentSinkImpl::PushNameSpacesFrom(const nsIParserNode& aNode)
{
    nsINameSpace* nameSpace = nsnull;
    if (0 < mNameSpaceStack.Count()) {
        nameSpace = (nsINameSpace*)mNameSpaceStack[mNameSpaceStack.Count() - 1];
        NS_ADDREF(nameSpace);
    }
    else {
        gNameSpaceManager->CreateRootNameSpace(nameSpace);
    }

    NS_ASSERTION(nameSpace != nsnull, "no parent namespace");
    if (! nameSpace)
        return;

    PRInt32 ac = aNode.GetAttributeCount();
    for (PRInt32 i = 0; i < ac; i++) {
        nsAutoString k(aNode.GetKeyAt(i));

        // Look for "xmlns" at the start of the attribute name
        PRInt32 offset = k.Find(kNameSpaceDef);
        if (0 != offset)
            continue;

        nsAutoString prefix;
        if (k.Length() >= PRInt32(sizeof kNameSpaceDef)) {
            // If the next character is a :, there is a namespace prefix
            PRUnichar next = k.CharAt(sizeof(kNameSpaceDef)-1);
            if (':' == next) {
                k.Right(prefix, k.Length()-sizeof(kNameSpaceDef));
            }
            else {
                continue; // it's not "xmlns:"
            }
        }

        // Get the attribute value (the URI for the namespace)
        nsAutoString uri(aNode.GetValueAt(i));
        nsRDFParserUtils::StripAndConvert(uri);

        // Open a local namespace
        nsIAtom* prefixAtom = ((0 < prefix.Length()) ? NS_NewAtom(prefix) : nsnull);
        nsINameSpace* child = nsnull;
        nameSpace->CreateChildNameSpace(prefixAtom, uri, child);
        if (nsnull != child) {
            NS_RELEASE(nameSpace);
            nameSpace = child;
        }

        NS_IF_RELEASE(prefixAtom);
    }

    // Now push the *last* namespace that we discovered on to the stack.
    mNameSpaceStack.AppendElement(nameSpace);
}

void
XULContentSinkImpl::PopNameSpaces(void)
{
    if (0 < mNameSpaceStack.Count()) {
        PRInt32 i = mNameSpaceStack.Count() - 1;
        nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack[i];
        mNameSpaceStack.RemoveElementAt(i);

        // Releasing the most deeply nested namespace will recursively
        // release intermediate parent namespaces until the next
        // reference is held on the namespace stack.
        NS_RELEASE(nameSpace);
    }
}

nsresult
XULContentSinkImpl::GetTopNameSpace(nsCOMPtr<nsINameSpace>* aNameSpace)
{
    PRInt32 count = mNameSpaceStack.Count();
    if (count == 0)
        return NS_ERROR_UNEXPECTED;

    *aNameSpace = dont_QueryInterface(NS_REINTERPRET_CAST(nsINameSpace*, mNameSpaceStack[count - 1]));
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
XULContentSinkImpl::UnblockNextOverlay() 
{
	PRInt32 count = mOverlayArray.Count();
    if (count) {
        // Process the next overlay.
        nsString* href = (nsString*)mOverlayArray.ElementAt(mCurrentOverlay);
        ProcessOverlay(*href);
	
        mCurrentOverlay++;
    }

	return NS_OK;
}

NS_IMETHODIMP
XULContentSinkImpl::UpdateOverlayCounters(PRInt32 aDelta)
{
    nsresult rv;

    PRInt32 count = mOverlayArray.Count();
    if (count) {
        PRInt32 remaining = count - mCurrentOverlay;
  
        mUnprocessedOverlayCount += aDelta;
  
        if (remaining && (remaining == mUnprocessedOverlayCount)) {
            // The only overlays we have left are our own.
            // Unblock the next overlay.
            UnblockNextOverlay();
        }

        if (mParentContentSink)
            mParentContentSink->UpdateOverlayCounters(aDelta);

        if (count && mUnprocessedOverlayCount == 0) {
            // We're done now, so we need to do a special
            // extra notification.
            if (mParentContentSink)
                mParentContentSink->UpdateOverlayCounters(-1);

            // We can be unblocked and allowed to finish.
            // XXX THIS CAUSES US TO BE DELETED. Why??
            rv = mParser->EnableParser(PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP XULContentSinkImpl::GetChromeOverlays()
{
  nsresult rv;
  NS_WITH_SERVICE(nsIChromeRegistry, reg, kChromeRegistryCID, &rv);

  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mActualDocument, &rv);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIChannel> channel;
  if (NS_FAILED(xuldoc->GetChannel(getter_AddRefs(channel))))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(channel->GetOriginalURI(getter_AddRefs(uri))))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISimpleEnumerator> oe;
  reg->GetOverlays(uri, getter_AddRefs(oe));

  if (!oe)
    return NS_OK;

  PRBool moreElements;
  oe->HasMoreElements(&moreElements);

  while (moreElements)
  {
    nsCOMPtr<nsISupports> next;
    oe->GetNext(getter_AddRefs(next));
    if (!next)
      return NS_OK;

    nsCOMPtr<nsIURI> uri = do_QueryInterface(next);
    if (!uri)
      return NS_OK;

    char *spec;
    uri->GetSpec(&spec);

    nsString *str = new nsString(spec);

    mOverlayArray.AppendElement(str);

    oe->HasMoreElements(&moreElements);
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXULContentSink(nsIXULContentSink** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    XULContentSinkImpl* sink = new XULContentSinkImpl(rv);
    if (! sink)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv)) {
        delete sink;
        return rv;
    }

    NS_ADDREF(sink);
    *aResult = sink;
    return NS_OK;
}
