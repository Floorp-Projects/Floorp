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
 *   Ben Goodger <ben@netscape.com>
 *   Pete Collins <petejc@collab.net>
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

/*

  An implementation for the XUL document. This implementation serves
  as the basis for generating an NGLayout content model.

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
#include "nsXULDocument.h"
#include "nsDocument.h"

#include "nsDOMCID.h"
#include "nsDOMError.h"
#include "nsIBoxObject.h"
#include "nsIChannel.h"
#include "nsIChromeRegistry.h"
#include "nsIComponentManager.h"
#include "nsICodebasePrincipal.h"
#include "nsIContentSink.h" // for NS_CONTENT_ID_COUNTER_BASE
#include "nsIScrollableView.h"
#include "nsIContentViewer.h"
#include "nsICSSStyleSheet.h"
#include "nsIDOMComment.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsGUIEvent.h"
#include "nsIDOMRange.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMText.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMAbstractView.h"
#include "nsIDTD.h"
#include "nsIDocumentObserver.h"
#include "nsIFormControl.h"
#include "nsIHTMLContent.h"
#include "nsIElementFactory.h"
#include "nsIEventStateManager.h"
#include "nsIInputStream.h"
#include "nsILoadGroup.h"
#include "nsINameSpace.h"
#include "nsIObserver.h"
#include "nsIParser.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFNode.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStyleContext.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsITextContent.h"
#include "nsITimer.h"
#include "nsIURL.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsXULAtoms.h"
#include "nsIXMLContent.h"
#include "nsIXULContent.h"
#include "nsIXULContentSink.h"
#include "nsXULContentUtils.h"
#include "nsIXULPrototypeCache.h"
#include "nsLWBrkCIID.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsPIBoxObject.h"
#include "nsRDFCID.h"
#include "nsILocalStore.h"
#include "nsRDFDOMNodeList.h"
#include "nsXPIDLString.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsXULCommandDispatcher.h"
#include "nsTreeWalker.h"
#include "nsXULDocument.h"
#include "nsXULElement.h"
#include "plstr.h"
#include "prlog.h"
#include "rdf.h"
#include "nsIFrame.h"
#include "nsIPrivateDOMImplementation.h"
#include "nsIDOMDOMImplementation.h"
#include "nsINodeInfo.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIXBLService.h"
#include "nsReadableUtils.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsISelectionController.h"
#include "nsContentUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFastLoadFileControl.h"
#include "nsIFastLoadService.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPref.h"


//----------------------------------------------------------------------
//
// CIDs
//

static NS_DEFINE_CID(kCSSLoaderCID,              NS_CSS_LOADER_CID);
static NS_DEFINE_CID(kCSSParserCID,              NS_CSSPARSER_CID);
static NS_DEFINE_CID(kChromeRegistryCID,         NS_CHROMEREGISTRY_CID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kEventListenerManagerCID,   NS_EVENTLISTENERMANAGER_CID);
static NS_DEFINE_CID(kHTMLCSSStyleSheetCID,      NS_HTML_CSS_STYLESHEET_CID);
static NS_DEFINE_CID(kHTMLElementFactoryCID,     NS_HTML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kHTMLStyleSheetCID,         NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kLWBrkCID,                  NS_LWBRK_CID);
static NS_DEFINE_CID(kLoadGroupCID,              NS_LOADGROUP_CID);
static NS_DEFINE_CID(kLocalStoreCID,             NS_LOCALSTORE_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,       NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kParserCID,                 NS_PARSER_CID);
static NS_DEFINE_CID(kPresShellCID,              NS_PRESSHELL_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,       NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kTextNodeCID,               NS_TEXTNODE_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,          NS_WELLFORMEDDTD_CID);
static NS_DEFINE_CID(kXMLElementFactoryCID,      NS_XML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kXULContentSinkCID,         NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kXULPrototypeCacheCID,      NS_XULPROTOTYPECACHE_CID);
static NS_DEFINE_CID(kXULTemplateBuilderCID,     NS_XULTEMPLATEBUILDER_CID);
static NS_DEFINE_CID(kXULOutlinerBuilderCID,     NS_XULOUTLINERBUILDER_CID);
static NS_DEFINE_CID(kDOMImplementationCID,      NS_DOM_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kRangeCID,                  NS_RANGE_CID);

static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);

static PRBool IsChromeURI(nsIURI* aURI)
{
    // why is this check a member function of nsXULDocument? -gagan
    PRBool isChrome=PR_FALSE;
    if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome)
        return PR_TRUE;
    return PR_FALSE;
}

//----------------------------------------------------------------------
//
// Miscellaneous Constants
//

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"

const nsForwardReference::Phase nsForwardReference::kPasses[] = {
    nsForwardReference::eConstruction,
    nsForwardReference::eHookup,
    nsForwardReference::eDone
};


//----------------------------------------------------------------------
//
// Statics
//

PRInt32 nsXULDocument::gRefCnt = 0;

nsIRDFService* nsXULDocument::gRDFService;
nsIRDFResource* nsXULDocument::kNC_persist;
nsIRDFResource* nsXULDocument::kNC_attribute;
nsIRDFResource* nsXULDocument::kNC_value;

nsIElementFactory* nsXULDocument::gHTMLElementFactory;
nsIElementFactory*  nsXULDocument::gXMLElementFactory;

nsINameSpaceManager* nsXULDocument::gNameSpaceManager;
PRInt32 nsXULDocument::kNameSpaceID_XUL;

nsIXULPrototypeCache* nsXULDocument::gXULCache;

PRLogModuleInfo* nsXULDocument::gXULLog;

class nsProxyLoadStream : public nsIInputStream
{
private:
  const char* mBuffer;
  PRUint32    mSize;
  PRUint32    mIndex;

public:
  nsProxyLoadStream(void) : mBuffer(nsnull)
  {
      NS_INIT_REFCNT();
  }

  virtual ~nsProxyLoadStream(void) {
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIBaseStream
  NS_IMETHOD Close(void) {
      return NS_OK;
  }

  // nsIInputStream
  NS_IMETHOD Available(PRUint32 *aLength) {
      *aLength = mSize - mIndex;
      return NS_OK;
  }

  NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount) {
      PRUint32 readCount = 0;
      while (mIndex < mSize && aCount > 0) {
          *aBuf = mBuffer[mIndex];
          aBuf++;
          mIndex++;
          readCount++;
          aCount--;
      }
      *aReadCount = readCount;
      return NS_OK;
  }

  NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) {
    NS_NOTREACHED("ReadSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking) {
    NS_NOTREACHED("GetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetObserver(nsIInputStreamObserver * *aObserver) {
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD SetObserver(nsIInputStreamObserver * aObserver) {
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Implementation
  void SetBuffer(const char* aBuffer, PRUint32 aSize) {
      mBuffer = aBuffer;
      mSize = aSize;
      mIndex = 0;
  }
};

NS_IMPL_ISUPPORTS1(nsProxyLoadStream, nsIInputStream);

//----------------------------------------------------------------------
//
// PlaceholderRequest
//
//   This is a dummy request implementation that we add to the load
//   group. It ensures that EndDocumentLoad() in the docshell doesn't
//   fire before we've finished building the complete document content
//   model.
//

class PlaceHolderRequest : public nsIChannel
{
protected:
    PlaceHolderRequest();
    virtual ~PlaceHolderRequest();

    static PRInt32 gRefCnt;
    static nsIURI* gURI;

    nsCOMPtr<nsILoadGroup> mLoadGroup;

public:
    static nsresult
    Create(nsIRequest** aResult);
	
    NS_DECL_ISUPPORTS

	// nsIRequest
    NS_IMETHOD GetName(PRUnichar* *result) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    NS_IMETHOD IsPending(PRBool *_retval) { *_retval = PR_TRUE; return NS_OK; }
    NS_IMETHOD GetStatus(nsresult *status) { *status = NS_OK; return NS_OK; }
    NS_IMETHOD Cancel(nsresult status)  { return NS_OK; }
    NS_IMETHOD Suspend(void) { return NS_OK; }
    NS_IMETHOD Resume(void)  { return NS_OK; }

 	// nsIChannel
    NS_IMETHOD GetOriginalURI(nsIURI* *aOriginalURI) { *aOriginalURI = gURI; NS_ADDREF(*aOriginalURI); return NS_OK; }
    NS_IMETHOD SetOriginalURI(nsIURI* aOriginalURI) { gURI = aOriginalURI; NS_ADDREF(gURI); return NS_OK; }
    NS_IMETHOD GetURI(nsIURI* *aURI) { *aURI = gURI; NS_ADDREF(*aURI); return NS_OK; }
    NS_IMETHOD SetURI(nsIURI* aURI) { gURI = aURI; NS_ADDREF(gURI); return NS_OK; }
    NS_IMETHOD Open(nsIInputStream **_retval) { *_retval = nsnull; return NS_OK; }
    NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt) { return NS_OK; }
    NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags) { *aLoadFlags = nsIRequest::LOAD_NORMAL; return NS_OK; }
   	NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags) { return NS_OK; }
 	NS_IMETHOD GetOwner(nsISupports * *aOwner) { *aOwner = nsnull; return NS_OK; }
 	NS_IMETHOD SetOwner(nsISupports * aOwner) { return NS_OK; }
 	NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup) { *aLoadGroup = mLoadGroup; NS_IF_ADDREF(*aLoadGroup); return NS_OK; }
 	NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup) { mLoadGroup = aLoadGroup; return NS_OK; }
 	NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks) { *aNotificationCallbacks = nsnull; return NS_OK; }
 	NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks) { return NS_OK; }
    NS_IMETHOD GetSecurityInfo(nsISupports * *aSecurityInfo) { *aSecurityInfo = nsnull; return NS_OK; }
    NS_IMETHOD GetContentType(char * *aContentType) { *aContentType = nsnull; return NS_OK; }
    NS_IMETHOD SetContentType(const char * aContentType) { return NS_OK; }
    NS_IMETHOD GetContentLength(PRInt32 *aContentLength) { return NS_OK; }
    NS_IMETHOD SetContentLength(PRInt32 aContentLength) { return NS_OK; }

};

PRInt32 PlaceHolderRequest::gRefCnt;
nsIURI* PlaceHolderRequest::gURI;

NS_IMPL_ADDREF(PlaceHolderRequest);
NS_IMPL_RELEASE(PlaceHolderRequest);
NS_IMPL_QUERY_INTERFACE2(PlaceHolderRequest, nsIRequest, nsIChannel);

nsresult
PlaceHolderRequest::Create(nsIRequest** aResult)
{
    PlaceHolderRequest* request = new PlaceHolderRequest();
    if (! request)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = request;
    NS_ADDREF(*aResult);
    return NS_OK;
}


PlaceHolderRequest::PlaceHolderRequest()
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        nsresult rv;
        rv = NS_NewURI(&gURI, "about:xul-master-placeholder", nsnull);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create about:xul-master-placeholder");
    }
}


PlaceHolderRequest::~PlaceHolderRequest()
{
    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gURI);
    }
}

//----------------------------------------------------------------------

struct BroadcasterMapEntry : public PLDHashEntryHdr {
    nsIDOMElement*   mBroadcaster; // [WEAK]
    nsCheapVoidArray mListeners;   // [OWNING] of BroadcastListener objects
};

struct BroadcastListener {
  nsIDOMElement*    mListener;  // [WEAK] XXXwaterson crash waiting to happen!
  nsCOMPtr<nsIAtom> mAttribute;
};

//----------------------------------------------------------------------
//
// ctors & dtors
//

nsXULDocument::nsXULDocument(void)
    : mParentDocument(nsnull),
      mScriptGlobalObject(nsnull),
      mNextSrcLoadWaiter(nsnull),
      mDisplaySelection(PR_FALSE),
      mIsPopup(PR_FALSE),
      mIsFastLoad(PR_FALSE),
      mNextFastLoad(nsnull),
      mBoxObjectTable(nsnull),
      mTemplateBuilderTable(nsnull),
      mResolutionPhase(nsForwardReference::eStart),
      mNextContentID(NS_CONTENT_ID_COUNTER_BASE),
      mNumCapturers(0),
      mState(eState_Master),
      mCurrentScriptProto(nsnull),
      mBroadcasterMap(nsnull)
{
    NS_INIT_REFCNT();
    mCharSetID.AssignWithConversion("UTF-8");

    // Force initialization.
    mBindingManager = do_CreateInstance("@mozilla.org/xbl/binding-manager;1");
    nsCOMPtr<nsIDocumentObserver> observer(do_QueryInterface(mBindingManager));
    if (observer) // We must always be the first observer of the document.
      mObservers.InsertElementAt(observer, 0);
#ifdef IBMBIDI
    mBidiEnabled = PR_FALSE;
#endif // IBMBIDI
}

nsIFastLoadService* nsXULDocument::gFastLoadService = nsnull;
nsIFile*            nsXULDocument::gFastLoadFile = nsnull;
nsXULDocument*      nsXULDocument::gFastLoadList = nsnull;

nsXULDocument::~nsXULDocument()
{
    NS_ASSERTION(mNextSrcLoadWaiter == nsnull,
        "unreferenced document still waiting for script source to load?");

    // In case we failed somewhere early on and the forward observer
    // decls never got resolved.
    DestroyForwardReferences();

    // Destroy our broadcaster map.
    if (mBroadcasterMap)
        PL_DHashTableDestroy(mBroadcasterMap);

    // Notify observer that we're about to go away
    PRInt32 i;
    for (i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
        observer->DocumentWillBeDestroyed(this);
    }

    // mParentDocument is never refcounted
    // Delete references to sub-documents
    {
        i = mSubDocuments.Count();
        while (--i >= 0) {
            nsIDocument* subdoc = (nsIDocument*) mSubDocuments.ElementAt(i);
            NS_RELEASE(subdoc);
        }
    }

    // Delete references to style sheets but only if we aren't a popup document.
    if (!mIsPopup) {
        i = mStyleSheets.Count();
        while (--i >= 0) {
            nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(i);
            sheet->SetOwningDocument(nsnull);
            NS_RELEASE(sheet);
        }
    }

    if (mLocalStore) {
        nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mLocalStore);
        if (remote)
            remote->Flush();
    }

    if (mCSSLoader) {
        mCSSLoader->DropDocumentReference();
    }

    if (mScriptLoader) {
        mScriptLoader->DropDocumentReference();
    }

    delete mTemplateBuilderTable;
    delete mBoxObjectTable;

    if (mListenerManager)
        mListenerManager->SetListenerTarget(nsnull);

    if (--gRefCnt == 0) {
        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        NS_IF_RELEASE(kNC_persist);
        NS_IF_RELEASE(kNC_attribute);
        NS_IF_RELEASE(kNC_value);

        NS_IF_RELEASE(gHTMLElementFactory);
        NS_IF_RELEASE(gXMLElementFactory);

        if (gNameSpaceManager) {
            nsServiceManager::ReleaseService(kNameSpaceManagerCID, gNameSpaceManager);
            gNameSpaceManager = nsnull;
        }

        if (gXULCache) {
            nsServiceManager::ReleaseService(kXULPrototypeCacheCID, gXULCache);
            gXULCache = nsnull;
        }

        NS_IF_RELEASE(gFastLoadService); // don't need ReleaseService nowadays!
        NS_IF_RELEASE(gFastLoadFile);
    }

    if (mNodeInfoManager) {
        mNodeInfoManager->DropDocumentReference();
    }

    if (mIsFastLoad)
        RemoveFromFastLoadList();
}


nsresult
NS_NewXULDocument(nsIXULDocument** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    nsXULDocument* doc = new nsXULDocument();
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


//----------------------------------------------------------------------
//
// nsISupports interface
//

NS_IMPL_ADDREF(nsXULDocument)
NS_IMPL_RELEASE(nsXULDocument)


// QueryInterface implementation for nsHTMLAnchorElement
NS_INTERFACE_MAP_BEGIN(nsXULDocument)
    NS_INTERFACE_MAP_ENTRY(nsIDocument)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocument)
    NS_INTERFACE_MAP_ENTRY(nsIXULDocument)
    NS_INTERFACE_MAP_ENTRY(nsIXMLDocument)
    NS_INTERFACE_MAP_ENTRY(nsIDOMXULDocument)
    NS_INTERFACE_MAP_ENTRY(nsIDOMDocument)
    NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
    NS_INTERFACE_MAP_ENTRY(nsIDOM3Node)
    NS_INTERFACE_MAP_ENTRY(nsIDOMNSDocument)
    NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentEvent)
    NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentView)
    NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentXBL)
    NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentStyle)
    NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentRange)
    NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentTraversal)
    NS_INTERFACE_MAP_ENTRY(nsIHTMLContentContainer)
    NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
    NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
    NS_INTERFACE_MAP_ENTRY(nsIDOMEventCapturer)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY(nsIStreamLoaderObserver)
    NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XULDocument)
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
//
// nsIDocument interface
//

NS_IMETHODIMP
nsXULDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
    NS_NOTREACHED("Reset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetArena(nsIArena** aArena)
{
    NS_IF_ADDREF(*aArena = mArena);
    return NS_OK;
}

// Override the nsDocument.cpp method to keep from returning the
// "cached XUL" type which is completely internal and may confuse
// people
NS_IMETHODIMP
nsXULDocument::GetContentType(nsAWritableString& aContentType)
{
    aContentType.Assign(NS_LITERAL_STRING("application/vnd.mozilla.xul+xml"));
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetContentLanguage(nsAWritableString& aContentLanguage) const
{
    aContentLanguage.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::PrepareStyleSheets(nsIURI* anURL)
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
                                                       NS_GET_IID(nsIHTMLStyleSheet),
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
                                                       NS_GET_IID(nsIHTMLCSSStyleSheet),
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

NS_IMETHODIMP
nsXULDocument::SetDocumentURL(nsIURI* anURL)
{
    mDocumentURL = dont_QueryInterface(anURL);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::StartDocumentLoad(const char* aCommand,
                                 nsIChannel* aChannel,
                                 nsILoadGroup* aLoadGroup,
                                 nsISupports* aContainer,
                                 nsIStreamListener **aDocListener,
                                 PRBool aReset)
{
    nsresult rv;

    mDocumentLoadGroup = getter_AddRefs(NS_GetWeakReference(aLoadGroup));

    mDocumentTitle.Truncate();

    rv = aChannel->GetOriginalURI(getter_AddRefs(mDocumentURL));
    if (NS_FAILED(rv)) return rv;

    rv = PrepareStyleSheets(mDocumentURL);
    if (NS_FAILED(rv)) return rv;

    // Get the content type, if possible, to see if it's a cached XUL
    // load. We explicitly ignore failure at this point, because
    // certain hacks (cough, the directory viewer) need to be able to
    // StartDocumentLoad() before the channel's content type has been
    // detected.

    nsXPIDLCString contentType;
    aChannel->GetContentType(getter_Copies(contentType));

    if (contentType &&
        PL_strcmp(contentType, "mozilla.application/cached-xul") == 0) {
        // Look in the chrome cache: we've got this puppy loaded
        // already.
        nsCOMPtr<nsIXULPrototypeDocument> proto;
        rv = gXULCache->GetPrototype(mDocumentURL, getter_AddRefs(proto));
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(proto != nsnull, "no prototype on cached load");
        if (! proto)
            return NS_ERROR_UNEXPECTED;

        // If we're racing with another document to load proto, wait till the
        // load has finished loading before trying to add cloned style sheets.
        // nsXULDocument::EndLoad will call proto->NotifyLoadDone, which will
        // find all racing documents and notify them via OnPrototypeLoadDone,
        // which will add style sheet clones to each document.
        PRBool loaded;
        rv = proto->AwaitLoadDone(this, &loaded);
        if (NS_FAILED(rv)) return rv;

        mMasterPrototype = mCurrentPrototype = proto;

        // Add cloned style sheet references only if the prototype has in
        // fact already loaded.  It may still be loading when we hit the XUL
        // prototype cache.
        if (loaded) {
            rv = AddPrototypeSheets();
            if (NS_FAILED(rv)) return rv;
        }

        // We need a listener, even if proto is not yet loaded, in which
        // event the listener's OnStopRequest method does nothing, and all
        // the interesting work happens below nsXULDocument::EndLoad, from
        // the call there to mCurrentPrototype->NotifyLoadDone().
        *aDocListener = new CachedChromeStreamListener(this, loaded);
        if (! *aDocListener)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        PRBool useXULCache;
        gXULCache->GetEnabled(&useXULCache);
        PRBool fillXULCache = (useXULCache && IsChromeURI(mDocumentURL));

        // Try to open a FastLoad file for reading, or create one for writing.
        // If one exists and looks valid, the nsIFastLoadService will purvey
        // a non-null input stream via its GetInputStream method, and we will
        // deserialize saved objects from that stream.  Otherwise, we'll write
        // to the output stream returned by GetOutputStream.
        if (fillXULCache && nsCRT::strcmp(aCommand, "view-source") != 0)
            StartFastLoad();

        // It's just a vanilla document load. Create a parser to deal
        // with the stream n' stuff.
        nsCOMPtr<nsIParser> parser;
        rv = PrepareToLoad(aContainer, aCommand, aChannel, aLoadGroup, getter_AddRefs(parser));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser, &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv), "parser doesn't support nsIStreamListener");
        if (NS_FAILED(rv)) return rv;

        *aDocListener = listener;

        parser->Parse(mDocumentURL);

        // Put the current prototype, created under PrepareToLoad, into the
        // XUL prototype cache now.  We can't do this under PrepareToLoad or
        // overlay loading will break; search for PutPrototype in ResumeWalk
        // and see the comment there.
        if (fillXULCache) {
            rv = gXULCache->PutPrototype(mCurrentPrototype);
            if (NS_FAILED(rv)) return rv;
        }
    }

    NS_IF_ADDREF(*aDocListener);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::StopDocumentLoad()
{
    return NS_OK;
}

const nsString*
nsXULDocument::GetDocumentTitle() const
{
    return &mDocumentTitle;
}

NS_IMETHODIMP
nsXULDocument::GetDocumentURL(nsIURI** aURI) const
{
    NS_IF_ADDREF(*aURI = mDocumentURL);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetPrincipal(nsIPrincipal **aPrincipal)
{
    return mMasterPrototype->GetDocumentPrincipal(aPrincipal);
}

NS_IMETHODIMP
nsXULDocument::AddPrincipal(nsIPrincipal *aPrincipal)
{
    NS_NOTREACHED("AddPrincipal");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetDocumentLoadGroup(nsILoadGroup **aGroup) const
{
    nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

    *aGroup = group;
    NS_IF_ADDREF(*aGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetBaseURL(nsIURI*& aURL) const
{
  if (mDocumentBaseURL) {
    aURL = mDocumentBaseURL.get();
    NS_ADDREF(aURL);
  }
  else {
    GetDocumentURL(&aURL);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetBaseURL(nsIURI* aURL)
{
  nsresult rv;
  nsCOMPtr<nsIScriptSecurityManager> securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = securityManager->CheckLoadURI(mDocumentURL, aURL, nsIScriptSecurityManager::STANDARD);
    if (NS_SUCCEEDED(rv)) {
      mDocumentBaseURL = aURL;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsXULDocument::GetBaseTarget(nsAWritableString &aBaseTarget)
{
  aBaseTarget.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetBaseTarget(const nsAReadableString &aBaseTarget)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetStyleSheets(nsIDOMStyleSheetList** aStyleSheets)
{
  if (!mDOMStyleSheets) {
    mDOMStyleSheets = new nsDOMStyleSheetList(this);
    if (!mDOMStyleSheets) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aStyleSheets = mDOMStyleSheets;
  NS_ADDREF(*aStyleSheets);

  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetDocumentCharacterSet(nsAWritableString& oCharSetID)
{
    oCharSetID.Assign(mCharSetID);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetDocumentCharacterSet(const nsAReadableString& aCharSetID)
{
  if (!mCharSetID.Equals(aCharSetID)) {
    mCharSetID.Assign(aCharSetID);
    PRInt32 n = mCharSetObservers.Count();
    for (PRInt32 i = 0; i < n; i++) {
      nsIObserver* observer = (nsIObserver*) mCharSetObservers.ElementAt(i);
      observer->Observe((nsIDocument*) this, "charset", 
                        PromiseFlatString(aCharSetID).get());
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::AddCharSetObserver(nsIObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_TRUE(mCharSetObservers.AppendElement(aObserver), NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::RemoveCharSetObserver(nsIObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_TRUE(mCharSetObservers.RemoveElement(aObserver), NS_ERROR_FAILURE);
  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetLineBreaker(nsILineBreaker** aResult)
{
  if(! mLineBreaker) {
     // no line breaker, find a default one
     nsILineBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          NS_GET_IID(nsILineBreakerFactory),
                                          (nsISupports **)&lf);
     if (NS_SUCCEEDED(result)) {
      nsILineBreaker *lb = nsnull ;
      nsAutoString lbarg;
      result = lf->GetBreaker(lbarg, &lb);
      if(NS_SUCCEEDED(result)) {
         mLineBreaker = dont_AddRef(lb);
      }
      result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
     }
  }
  *aResult = mLineBreaker;
  NS_IF_ADDREF(*aResult);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP
nsXULDocument::SetLineBreaker(nsILineBreaker* aLineBreaker)
{
  mLineBreaker = dont_QueryInterface(aLineBreaker);
  return NS_OK;
}
NS_IMETHODIMP
nsXULDocument::GetWordBreaker(nsIWordBreaker** aResult)
{
  if (! mWordBreaker) {
     // no line breaker, find a default one
     nsIWordBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          NS_GET_IID(nsIWordBreakerFactory),
                                          (nsISupports **)&lf);
     if (NS_SUCCEEDED(result)) {
      nsIWordBreaker *lb = nsnull ;
      nsAutoString lbarg;
      result = lf->GetBreaker(lbarg, &lb);
      if(NS_SUCCEEDED(result)) {
         mWordBreaker = dont_AddRef(lb);
      }
      result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
     }
  }
  *aResult = mWordBreaker;
  NS_IF_ADDREF(*aResult);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP
nsXULDocument::SetWordBreaker(nsIWordBreaker* aWordBreaker)
{
  mWordBreaker = dont_QueryInterface(aWordBreaker);
  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetHeaderData(nsIAtom* aHeaderField,
                             nsAWritableString& aData) const
{
  aData.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument:: SetHeaderData(nsIAtom* aHeaderField,
                              const nsAReadableString& aData)
{
  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CreateShell(nsIPresContext* aContext,
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
                                                    NS_GET_IID(nsIPresShell),
                                                    (void**) &shell)))
        return rv;

    if (NS_FAILED(rv = shell->Init(this, aContext, aViewManager, aStyleSet))) {
        NS_RELEASE(shell);
        return rv;
    }

    mPresShells.AppendElement(shell);
    *aInstancePtrResult = shell; // addref implicit in CreateInstance()

    // tell the context the mode we want (always standard)
    aContext->SetCompatibilityMode(eCompatibility_Standard);

    return NS_OK;
}

PRBool
nsXULDocument::DeleteShell(nsIPresShell* aShell)
{
    return mPresShells.RemoveElement(aShell);
}

PRInt32
nsXULDocument::GetNumberOfShells()
{
    return mPresShells.Count();
}

NS_IMETHODIMP
nsXULDocument::GetShellAt(PRInt32 aIndex, nsIPresShell** aShell)
{
    *aShell = NS_STATIC_CAST(nsIPresShell*, mPresShells[aIndex]);
    NS_IF_ADDREF(*aShell);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetParentDocument(nsIDocument** aParent)
{
    NS_IF_ADDREF(*aParent = mParentDocument);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetParentDocument(nsIDocument* aParent)
{
    // Note that we do *not* AddRef our parent because that would
    // create a circular reference.
    mParentDocument = aParent;
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::AddSubDocument(nsIDocument* aSubDoc)
{
    NS_ADDREF(aSubDoc);
    mSubDocuments.AppendElement(aSubDoc);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetNumberOfSubDocuments(PRInt32 *aCount)
{
    *aCount = mSubDocuments.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetSubDocumentAt(PRInt32 aIndex, nsIDocument** aSubDoc)
{
    *aSubDoc = (nsIDocument*) mSubDocuments.ElementAt(aIndex);
    NS_IF_ADDREF(*aSubDoc);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetRootContent(nsIContent** aRoot)
{
    NS_IF_ADDREF(*aRoot = mRootContent);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetRootContent(nsIContent* aRoot)
{
    if (mRootContent) {
        mRootContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    }
    mRootContent = aRoot;
    if (mRootContent) {
        mRootContent->SetDocument(this, PR_TRUE, PR_TRUE);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
    NS_NOTREACHED("nsXULDocument::ChildAt");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const
{
    NS_NOTREACHED("nsXULDocument::IndexOf");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetChildCount(PRInt32& aCount)
{
    NS_NOTREACHED("nsXULDocument::GetChildCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetNumberOfStyleSheets(PRInt32 *aCount)
{
    *aCount = mStyleSheets.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetStyleSheetAt(PRInt32 aIndex, nsIStyleSheet** aSheet)
{
    *aSheet = NS_STATIC_CAST(nsIStyleSheet*, mStyleSheets[aIndex]);
    NS_IF_ADDREF(*aSheet);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetIndexOfStyleSheet(nsIStyleSheet* aSheet, PRInt32* aIndex)
{
    *aIndex = mStyleSheets.IndexOf(aSheet);
    return NS_OK;
}

void
nsXULDocument::AddStyleSheetToStyleSets(nsIStyleSheet* aSheet)
{
  PRInt32 count = mPresShells.Count();
  PRInt32 indx;
  for (indx = 0; indx < count; indx++) {
    nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(indx);
    nsCOMPtr<nsIStyleSet> set;
    if (NS_SUCCEEDED(shell->GetStyleSet(getter_AddRefs(set)))) {
      if (set) {
        set->AddDocStyleSheet(aSheet, this);
      }
    }
  }
}

void
nsXULDocument::AddStyleSheet(nsIStyleSheet* aSheet)
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
        AddStyleSheetToStyleSets(aSheet);

        // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
        for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
            nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
            observer->StyleSheetAdded(this, aSheet);
        }
    }
}

NS_IMETHODIMP
nsXULDocument::UpdateStyleSheets(nsISupportsArray* aOldSheets, nsISupportsArray* aNewSheets)
{
  PRUint32 oldCount;
  aOldSheets->Count(&oldCount);
  nsCOMPtr<nsIStyleSheet> sheet;
  PRUint32 i;
  for (i = 0; i < oldCount; i++) {
    aOldSheets->QueryElementAt(i, NS_GET_IID(nsIStyleSheet), getter_AddRefs(sheet));
    if (sheet) {
      mStyleSheets.RemoveElement(sheet);
      PRBool enabled = PR_TRUE;
      sheet->GetEnabled(enabled);
      if (enabled) {
        RemoveStyleSheetFromStyleSets(sheet);
      }

      sheet->SetOwningDocument(nsnull);
      nsIStyleSheet* sheetPtr = sheet.get();
      NS_RELEASE(sheetPtr);
    }
  }

  PRUint32 newCount;
  aNewSheets->Count(&newCount);
  for (i = 0; i < newCount; i++) {
    aNewSheets->QueryElementAt(i, NS_GET_IID(nsIStyleSheet), getter_AddRefs(sheet));
    if (sheet) {
      if (sheet == mAttrStyleSheet.get()) {  // always first
        mStyleSheets.InsertElementAt(sheet, 0);
      }
      else if (sheet == (nsIHTMLCSSStyleSheet*)mInlineStyleSheet) {  // always last
        mStyleSheets.AppendElement(sheet);
      }
      else {
        if ((nsIHTMLCSSStyleSheet*)mInlineStyleSheet == mStyleSheets.ElementAt(mStyleSheets.Count() - 1)) {
          // keep attr sheet last
          mStyleSheets.InsertElementAt(sheet, mStyleSheets.Count() - 1);
        }
        else {
          mStyleSheets.AppendElement(sheet);
        }
      }

      nsIStyleSheet* sheetPtr = sheet;
      NS_ADDREF(sheetPtr);
      sheet->SetOwningDocument(this);

      PRBool enabled = PR_TRUE;
      sheet->GetEnabled(enabled);
      if (enabled) {
        AddStyleSheetToStyleSets(sheet);
        sheet->SetOwningDocument(nsnull);
      }
    }
  }

  for (PRInt32 indx = mObservers.Count() - 1; indx >= 0; --indx) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(indx);
    observer->StyleSheetRemoved(this, sheet);
  }

  return NS_OK;
}

void
nsXULDocument::RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet)
{
  PRInt32 count = mPresShells.Count();
  PRInt32 indx;
  for (indx = 0; indx < count; indx++) {
    nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(indx);
    nsCOMPtr<nsIStyleSet> set;
    if (NS_SUCCEEDED(shell->GetStyleSet(getter_AddRefs(set)))) {
      if (set) {
        set->RemoveDocStyleSheet(aSheet);
      }
    }
  }
}

void
nsXULDocument::RemoveStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  mStyleSheets.RemoveElement(aSheet);

  PRBool enabled = PR_TRUE;
  aSheet->GetEnabled(enabled);

  if (enabled) {
    RemoveStyleSheetFromStyleSets(aSheet);

    // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
    for (PRInt32 indx = mObservers.Count() - 1; indx >= 0; --indx) {
      nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(indx);
      observer->StyleSheetRemoved(this, aSheet);
    }
  }

  aSheet->SetOwningDocument(nsnull);
  NS_RELEASE(aSheet);
}

NS_IMETHODIMP
nsXULDocument::InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex, PRBool aNotify)
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
    for (i = mObservers.Count() - 1; i >= 0; --i) {
      nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
      observer->StyleSheetAdded(this, aSheet);
    }
  }

  return NS_OK;
}

void
nsXULDocument::SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
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

    for (i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(i);
        observer->StyleSheetDisabledStateChanged(this, aSheet, aDisabled);
    }
}

NS_IMETHODIMP
nsXULDocument::GetCSSLoader(nsICSSLoader*& aLoader)
{
  nsresult result = NS_OK;
  if (! mCSSLoader) {
    result = nsComponentManager::CreateInstance(kCSSLoaderCID,
                                                nsnull,
                                                NS_GET_IID(nsICSSLoader),
                                                getter_AddRefs(mCSSLoader));
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

NS_IMETHODIMP
nsXULDocument::GetScriptLoader(nsIScriptLoader** aLoader)
{
    NS_ENSURE_ARG_POINTER(aLoader);
    nsresult result = NS_OK;
    if (!mScriptLoader) {
        nsScriptLoader* loader = new nsScriptLoader();
        if (!loader) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mScriptLoader = loader;
        mScriptLoader->Init(this);
    }

    *aLoader = mScriptLoader;
    NS_IF_ADDREF(*aLoader);

    return result;
}

NS_IMETHODIMP
nsXULDocument::GetScriptGlobalObject(nsIScriptGlobalObject** aScriptGlobalObject)
{
   *aScriptGlobalObject = mScriptGlobalObject;
   NS_IF_ADDREF(*aScriptGlobalObject);
   return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject)
{
    if (!aScriptGlobalObject) {
        // XXX HACK ALERT! If the script context owner is null, the
        // document will soon be going away. So tell our content that
        // to lose its reference to the document. This has to be done
        // before we actually set the script context owner to null so
        // that the content elements can remove references to their
        // script objects.

        if (mRootContent)
            mRootContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);

        // Propagate the out-of-band notification to each PresShell's
        // anonymous content as well. This ensures that there aren't
        // any accidental script references left in anonymous content
        // keeping the document alive. (While not strictly necessary
        // -- the PresShell owns us -- it's tidy.)
        for (PRInt32 count = mPresShells.Count() - 1; count >= 0; --count) {
            nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[count]);
            if (! shell)
                continue;

            shell->ReleaseAnonymousContent();
        }

#ifdef DEBUG_jst
    printf ("Content wrapper hash had %d entries.\n",
            mContentWrapperHash.Count());
#endif

        mContentWrapperHash.Reset();
    } else if (mScriptGlobalObject != aScriptGlobalObject) {
      // Update our weak ref to the focus controller
      nsCOMPtr<nsPIDOMWindow> domPrivate = do_QueryInterface(aScriptGlobalObject);
      if (domPrivate) {
        nsCOMPtr<nsIFocusController> fc;
        domPrivate->GetRootFocusController(getter_AddRefs(fc));
        mFocusController = getter_AddRefs(NS_GetWeakReference(fc));
      }
    }

    mScriptGlobalObject = aScriptGlobalObject;
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetFocusController(nsIFocusController** aFocusController)
{
  NS_ENSURE_ARG_POINTER(aFocusController);

  nsCOMPtr<nsIFocusController> fc = do_QueryReferent(mFocusController);
  *aFocusController = fc;
  NS_IF_ADDREF(*aFocusController);
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetNameSpaceManager(nsINameSpaceManager*& aManager)
{
  aManager = mNameSpaceManager;
  NS_IF_ADDREF(aManager);
  return NS_OK;
}


// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void
nsXULDocument::AddObserver(nsIDocumentObserver* aObserver)
{
    // XXX Make sure the observer isn't already in the list
    if (mObservers.IndexOf(aObserver) == -1) {
        mObservers.AppendElement(aObserver);
    }
}

PRBool
nsXULDocument::RemoveObserver(nsIDocumentObserver* aObserver)
{
    return mObservers.RemoveElement(aObserver);
}

NS_IMETHODIMP
nsXULDocument::BeginUpdate()
{
    // XXX Never called. Does this matter?
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->BeginUpdate(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::EndUpdate()
{
    // XXX Never called. Does this matter?
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->EndUpdate(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::BeginLoad()
{
    // XXX Never called. Does this matter?
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->BeginLoad(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::EndLoad()
{
    nsresult rv;

    // Whack the prototype document into the cache so that the next
    // time somebody asks for it, they don't need to load it by hand.
    nsCOMPtr<nsIURI> uri;
    rv = mCurrentPrototype->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    // Remember if the XUL cache is on
    PRBool useXULCache;
    gXULCache->GetEnabled(&useXULCache);

    nsCOMPtr<nsIChromeRegistry> reg(do_GetService(kChromeRegistryCID, &rv));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsISupportsArray> sheets;
    reg->GetStyleSheets(uri, getter_AddRefs(sheets));

    // Walk the sheets and add them to the prototype. Also put them into the document.
    if (sheets) {
        nsCOMPtr<nsICSSStyleSheet> sheet;
        PRUint32 count;
        sheets->Count(&count);
        for (PRUint32 i = 0; i < count; i++) {
            sheets->QueryElementAt(i, NS_GET_IID(nsICSSStyleSheet),
                                   getter_AddRefs(sheet));
            if (sheet) {
                nsCOMPtr<nsIURI> sheetURL;
                sheet->GetURL(*getter_AddRefs(sheetURL));

                if (useXULCache && IsChromeURI(sheetURL)) {
                    mCurrentPrototype->AddStyleSheetReference(sheetURL);
                }
                AddStyleSheet(sheet);
            }
        }
    }

    if (useXULCache && IsChromeURI(uri)) {
        // If it's a 'chrome:' prototype document, then notify any
        // documents that raced to load the prototype, and awaited
        // its load completion via proto->AwaitLoadDone().
        rv = mCurrentPrototype->NotifyLoadDone();
        if (NS_FAILED(rv)) return rv;

        if (mIsFastLoad) {
            rv = gFastLoadService->EndMuxedDocument(uri);
            if (NS_FAILED(rv))
                AbortFastLoads();
        }
    }

    // Now walk the prototype to build content.
    rv = PrepareToWalk();
    if (NS_FAILED(rv)) return rv;

    return ResumeWalk();
}


// Called back from nsXULPrototypeDocument::NotifyLoadDone for each XUL
// document that raced to start the same prototype document load, lost
// the race, but hit the XUL prototype cache because the winner filled
// the cache with the not-yet-loaded prototype object.
NS_IMETHODIMP
nsXULDocument::OnPrototypeLoadDone()
{
    nsresult rv;

    // Need to clone style sheet references now, as we couldn't do that
    // in StartDocumentLoad, because the prototype may not have finished
    // loading at that point.
    rv = AddPrototypeSheets();
    if (NS_FAILED(rv)) return rv;

    // Now we must do for each secondary or later XUL document (those that
    // lost the race to start the prototype document load) what is done by
    // nsCachedChromeStreamListener::OnStopRequest for the primary document
    // (the one that won the race to start the prototype load).
    rv = PrepareToWalk();
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to prepare for walk");
    if (NS_FAILED(rv)) return rv;

    return ResumeWalk();
}


NS_IMETHODIMP
nsXULDocument::OnResumeContentSink()
{
    if (mIsFastLoad) {
        nsresult rv;
        nsCOMPtr<nsIURI> uri;

        rv = mCurrentPrototype->GetURI(getter_AddRefs(uri));
        if (NS_FAILED(rv)) return rv;

        rv = gFastLoadService->SelectMuxedDocument(uri);
        if (NS_FAILED(rv))
            AbortFastLoads();
    }
    return NS_OK;
}

static void PR_CALLBACK
ClearBroadcasterMapEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
    BroadcasterMapEntry* entry =
        NS_STATIC_CAST(BroadcasterMapEntry*, aEntry);

    // N.B. that we need to manually run the dtor because we
    // constructed the nsCheapVoidArray object in-place.
    entry->mListeners.~nsCheapVoidArray();
}

static void
SynchronizeBroadcastListener(nsIDOMElement* aBroadcaster,
                             nsIDOMElement* aListener,
                             const nsAString& aAttr)
{
    nsCOMPtr<nsIContent> broadcaster = do_QueryInterface(aBroadcaster);
    nsCOMPtr<nsIContent> listener = do_QueryInterface(aListener);

    if (aAttr.Equals(NS_LITERAL_STRING("*"))) {
        PRInt32 count;
        broadcaster->GetAttrCount(count);
        while (--count >= 0) {
            PRInt32 nameSpaceID;
            nsCOMPtr<nsIAtom> name;
            nsCOMPtr<nsIAtom> prefix;
            broadcaster->GetAttrNameAt(count, nameSpaceID,
                                       *getter_AddRefs(name),
                                       *getter_AddRefs(prefix));

            // _Don't_ push the |id| attribute's value!
            if ((nameSpaceID == kNameSpaceID_None) && (name == nsXULAtoms::id))
                continue;

            nsAutoString value;
            broadcaster->GetAttr(nameSpaceID, name, value);
            listener->SetAttr(nameSpaceID, name, value, PR_TRUE);
        }
    }
    else {
        // Find out if the attribute is even present at all.
        nsCOMPtr<nsIAtom> name = dont_AddRef(NS_NewAtom(aAttr));

        nsAutoString value;
        nsresult rv = broadcaster->GetAttr(kNameSpaceID_None, name, value);

        if (rv == NS_CONTENT_ATTR_NO_VALUE ||
            rv == NS_CONTENT_ATTR_HAS_VALUE) {
            listener->SetAttr(kNameSpaceID_None, name, value, PR_TRUE);
        }
        else {
            listener->UnsetAttr(kNameSpaceID_None, name, PR_TRUE);
        }
    }
}

NS_IMETHODIMP
nsXULDocument::AddBroadcastListenerFor(nsIDOMElement* aBroadcaster,
                                       nsIDOMElement* aListener,
                                       const nsAString& aAttr)
{
    static PLDHashTableOps gOps = {
        PL_DHashAllocTable,
        PL_DHashFreeTable,
        PL_DHashGetKeyStub,
        PL_DHashVoidPtrKeyStub,
        PL_DHashMatchEntryStub,
        PL_DHashMoveEntryStub,
        ClearBroadcasterMapEntry,
        PL_DHashFinalizeStub,
        nsnull
    };

    if (! mBroadcasterMap) {
        mBroadcasterMap =
            PL_NewDHashTable(&gOps, nsnull, sizeof(BroadcasterMapEntry),
                             PL_DHASH_MIN_SIZE);

        if (! mBroadcasterMap)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    BroadcasterMapEntry* entry =
        NS_STATIC_CAST(BroadcasterMapEntry*,
                       PL_DHashTableOperate(mBroadcasterMap, aBroadcaster,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_FREE(entry)) {
        entry =
            NS_STATIC_CAST(BroadcasterMapEntry*,
                           PL_DHashTableOperate(mBroadcasterMap, aBroadcaster,
                                                PL_DHASH_ADD));

        if (! entry)
            return NS_ERROR_OUT_OF_MEMORY;

        entry->mBroadcaster = aBroadcaster;

        // N.B. placement new to construct the nsCheapVoidArray object
        // in-place
        new (&entry->mListeners) nsCheapVoidArray();
    }

    // Only add the listener if it's not there already!
    nsCOMPtr<nsIAtom> attr = dont_AddRef(NS_NewAtom(aAttr));

    BroadcastListener* bl;
    for (PRInt32 i = entry->mListeners.Count() - 1; i >= 0; --i) {
        bl = NS_STATIC_CAST(BroadcastListener*, entry->mListeners[i]);

        if ((bl->mListener == aListener) && (bl->mAttribute == attr))
            return NS_OK;
    }

    bl = new BroadcastListener;
    if (! bl)
        return NS_ERROR_OUT_OF_MEMORY;

    bl->mListener  = aListener;
    bl->mAttribute = attr;

    entry->mListeners.AppendElement(bl);

    SynchronizeBroadcastListener(aBroadcaster, aListener, aAttr);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::RemoveBroadcastListenerFor(nsIDOMElement* aBroadcaster,
                                          nsIDOMElement* aListener,
                                          const nsAString& aAttr)
{
    // If we haven't added any broadcast listeners, then there sure
    // aren't any to remove.
    if (! mBroadcasterMap)
        return NS_OK;

    BroadcasterMapEntry* entry =
        NS_STATIC_CAST(BroadcasterMapEntry*,
                       PL_DHashTableOperate(mBroadcasterMap, aBroadcaster,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
        nsCOMPtr<nsIAtom> attr = dont_AddRef(NS_NewAtom(aAttr));
        for (PRInt32 i = entry->mListeners.Count() - 1; i >= 0; --i) {
            BroadcastListener* bl =
                NS_STATIC_CAST(BroadcastListener*, entry->mListeners[i]);

            if ((bl->mListener == aListener) && (bl->mAttribute == attr)) {
                entry->mListeners.RemoveElement(aListener);

                if (entry->mListeners.Count() == 0)
                    PL_DHashTableOperate(mBroadcasterMap, aBroadcaster,
                                         PL_DHASH_REMOVE);

                SynchronizeBroadcastListener(aBroadcaster, aListener, aAttr);

                break;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ContentChanged(nsIContent* aContent,
                              nsISupports* aSubContent)
{
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentChanged(this, aContent, aSubContent);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ContentStatesChanged(nsIContent* aContent1, nsIContent* aContent2)
{
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentStatesChanged(this, aContent1, aContent2);
    }
    return NS_OK;
}

nsresult
nsXULDocument::ExecuteOnBroadcastHandlerFor(nsIContent* aBroadcaster,
                                            nsIDOMElement* aListener,
                                            nsIAtom* aAttr)
{
    // Now we execute the onchange handler in the context of the
    // observer. We need to find the observer in order to
    // execute the handler.
    nsAutoString attrName;
    aAttr->ToString(attrName);

    nsCOMPtr<nsIContent> listener = do_QueryInterface(aListener);
    PRInt32 count;
    listener->ChildCount(count);
    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> child;
        listener->ChildAt(i, *getter_AddRefs(child));

        nsCOMPtr<nsIAtom> tag;
        child->GetTag(*getter_AddRefs(tag));
        if (tag != nsXULAtoms::observes)
            continue;

        // Is this the element that was listening to us?
        nsAutoString listeningToID;
        aBroadcaster->GetAttr(kNameSpaceID_None, nsXULAtoms::element, listeningToID);

        nsAutoString broadcasterID;
        aBroadcaster->GetAttr(kNameSpaceID_None, nsXULAtoms::id, broadcasterID);

        if (listeningToID != broadcasterID)
            continue;

        // We are observing the broadcaster, but is this the right
        // attribute?
        nsAutoString listeningToAttribute;
        listener->GetAttr(kNameSpaceID_None, nsXULAtoms::attribute, listeningToAttribute);

        if (!listeningToAttribute.Equals(attrName) &&
            !listeningToAttribute.Equals(NS_LITERAL_STRING("*"))) {
            continue;
        }

        // This is the right observes node. Execute the onchange
        // event handler
        nsEvent event;
        event.eventStructType = NS_EVENT;
        event.message = NS_XUL_BROADCAST;

        PRInt32 j = mPresShells.Count();
        while (--j >= 0) {
            nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[j]);

            nsCOMPtr<nsIPresContext> aPresContext;
            shell->GetPresContext(getter_AddRefs(aPresContext));

            // Handle the DOM event
            nsEventStatus status = nsEventStatus_eIgnore;
            listener->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
        }
    }

    return NS_OK;
}
NS_IMETHODIMP
nsXULDocument::AttributeChanged(nsIContent* aElement,
                                PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute,
                                PRInt32 aModType, 
                                PRInt32 aHint)
{
    nsresult rv;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    // First see if we need to update our element map.
    if ((aAttribute == nsXULAtoms::id) || (aAttribute == nsXULAtoms::ref)) {

        rv = mElementMap.Enumerate(RemoveElementsFromMapByContent, aElement);
        if (NS_FAILED(rv)) return rv;

        // That'll have removed _both_ the 'ref' and 'id' entries from
        // the map. So add 'em back now.
        rv = AddElementToMap(aElement);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->AttributeChanged(this, aElement, aNameSpaceID, aAttribute, aModType, aHint);
    }

    // See if there is anything we need to persist in the localstore.
    //
    // XXX Namespace handling broken :-(
    nsAutoString persist;
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::persist, persist);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        nsAutoString attr;
        rv = aAttribute->ToString(attr);
        if (NS_FAILED(rv)) return rv;

        if (persist.Find(attr) >= 0) {
            rv = Persist(aElement, kNameSpaceID_None, aAttribute);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Synchronize broadcast listeners
    if (mBroadcasterMap &&
        (aNameSpaceID == kNameSpaceID_None) &&
        (aAttribute != nsXULAtoms::id)) {
        nsCOMPtr<nsIDOMElement> domele = do_QueryInterface(aElement);
        BroadcasterMapEntry* entry =
            NS_STATIC_CAST(BroadcasterMapEntry*,
                           PL_DHashTableOperate(mBroadcasterMap, domele.get(),
                                                PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
            // We've got listeners: push the value.
            nsAutoString value;
            rv = aElement->GetAttr(kNameSpaceID_None, aAttribute,
                                   value);

            for (PRInt32 i = entry->mListeners.Count() - 1; i >= 0; --i) {
                BroadcastListener* bl =
                    NS_STATIC_CAST(BroadcastListener*, entry->mListeners[i]);

                if ((bl->mAttribute == aAttribute) ||
                    (bl->mAttribute == nsXULAtoms::_star)) {
                    nsCOMPtr<nsIContent> listener
                        = do_QueryInterface(bl->mListener);

                    if (rv == NS_CONTENT_ATTR_NO_VALUE ||
                        rv == NS_CONTENT_ATTR_HAS_VALUE) {
                        listener->SetAttr(kNameSpaceID_None, aAttribute,
                                          value, PR_TRUE);
                    }
                    else {
                        listener->UnsetAttr(kNameSpaceID_None, aAttribute,
                                            PR_TRUE);
                    }

                    ExecuteOnBroadcastHandlerFor(aElement, bl->mListener,
                                                 aAttribute);
                }
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ContentAppended(nsIContent* aContainer,
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

            rv = AddSubtreeToDocument(child);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Now notify external observers
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentAppended(this, aContainer, aNewIndexInContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ContentInserted(nsIContent* aContainer,
                                 nsIContent* aChild,
                                 PRInt32 aIndexInContainer)
{
    {
        nsresult rv;
        rv = AddSubtreeToDocument(aChild);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentInserted(this, aContainer, aChild, aIndexInContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ContentReplaced(nsIContent* aContainer,
                                 nsIContent* aOldChild,
                                 nsIContent* aNewChild,
                                 PRInt32 aIndexInContainer)
{
    {
        nsresult rv;
        rv = RemoveSubtreeFromDocument(aOldChild);
        if (NS_FAILED(rv)) return rv;

        rv = AddSubtreeToDocument(aNewChild);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentReplaced(this, aContainer, aOldChild, aNewChild,
                                  aIndexInContainer);

        // XXXdwh hack to avoid crash, since an observer removes itself
        // during ContentReplaced.
        PRInt32 newCount = mObservers.Count();
        if (newCount < count) {
          PRInt32 diff = (count - newCount);
          count -= diff;
          i -= diff;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ContentRemoved(nsIContent* aContainer,
                                nsIContent* aChild,
                                PRInt32 aIndexInContainer)
{
    {
        nsresult rv;
        rv = RemoveSubtreeFromDocument(aChild);
        if (NS_FAILED(rv)) return rv;
    }

    // Now notify external observers
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentRemoved(this, aContainer,
                                 aChild, aIndexInContainer);

        // XXXdwh hack to avoid crash, since an observer removes itself
        // during ContentRemoved.
        PRInt32 newCount = mObservers.Count();
        if (newCount < count) {
          PRInt32 diff = (count - newCount);
          count -= diff;
          i -= diff;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::AttributeWillChange(nsIContent* aChild,
                                   PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                                  nsIStyleRule* aStyleRule,
                                  PRInt32 aHint)
{
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleChanged(this, aStyleSheet, aStyleRule, aHint);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule)
{
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleAdded(this, aStyleSheet, aStyleRule);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                                  nsIStyleRule* aStyleRule)
{
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleRemoved(this, aStyleSheet, aStyleRule);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetSelection(nsISelection** aSelection)
{
    if (!mSelection) {
        NS_ASSERTION(0,"not initialized");
        *aSelection = nsnull;
        return NS_ERROR_NOT_INITIALIZED;
    }
    *aSelection = mSelection;
    NS_ADDREF(*aSelection);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SelectAll()
{

    nsIContent * start = nsnull;
    nsIContent * end   = nsnull;
    nsIContent * body  = nsnull;

    nsAutoString bodyStr; bodyStr.Assign(NS_LITERAL_STRING("BODY"));
    PRInt32 i, n;
    mRootContent->ChildCount(n);
    for (i=0;i<n;i++) {
        nsIContent * child;
        mRootContent->ChildAt(i, child);
        nsCOMPtr<nsIAtom> atom;
        child->GetTag(*getter_AddRefs(atom));
        if (bodyStr.EqualsIgnoreCase(atom)) {
            body = child;
            break;
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
    SetDisplaySelection(nsISelectionController::SELECTION_ON);

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::FindNext(const nsAReadableString &aSearchStr,
                        PRBool aMatchCase, PRBool aSearchDown,
                        PRBool &aIsFound)
{
    aIsFound = PR_FALSE;
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsXULDocument::FlushPendingNotifications(PRBool aFlushReflows, PRBool aUpdateViews)
{
  if (aFlushReflows) {

    PRInt32 i, count = mPresShells.Count();

    for (i = 0; i < count; i++) {
      nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);
      if (shell) {
        shell->FlushPendingNotifications(aUpdateViews);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetAndIncrementContentID(PRInt32* aID)
{
  *aID = mNextContentID++;
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetBindingManager(nsIBindingManager** aResult)
{
  *aResult = mBindingManager;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetNodeInfoManager(class nsINodeInfoManager *&aNodeInfoManager)
{
    NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_NOT_INITIALIZED);

    aNodeInfoManager = mNodeInfoManager;
    NS_ADDREF(aNodeInfoManager);

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::AddReference(void *aKey, nsISupports *aReference)
{
  nsVoidKey key(aKey);

  if (mScriptGlobalObject) {
      mContentWrapperHash.Put(&key, aReference);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::RemoveReference(void *aKey, nsISupports **aOldReference)
{
  nsVoidKey key(aKey);

  mContentWrapperHash.Remove(&key, aOldReference);

  return NS_OK;
}

void
nsXULDocument::SetDisplaySelection(PRInt8 aToggle)
{
    mDisplaySelection = aToggle;
}

PRInt8
nsXULDocument::GetDisplaySelection() const
{
    return mDisplaySelection;
}

NS_IMETHODIMP
nsXULDocument::HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus)
{
  nsresult ret = NS_OK;
  nsIDOMEvent* domEvent = nsnull;

  if (NS_EVENT_FLAG_INIT & aFlags) {
    aDOMEvent = &domEvent;
    aEvent->flags = aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
  }

  //Capturing stage
  if (NS_EVENT_FLAG_BUBBLE != aFlags && mScriptGlobalObject) {
    mScriptGlobalObject->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
  }

  //Local handling stage
  if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
    aEvent->flags |= aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, this, aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_CAPTURE != aFlags && mScriptGlobalObject) {
    mScriptGlobalObject->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_BUBBLE, aEventStatus);
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event, release here.
    if (nsnull != *aDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
      //Okay, so someone in the DOM loop (a listener, JS object) still has a ref to the DOM Event but
      //the internal data hasn't been malloc'd.  Force a copy of the data here so the DOM Event is still valid.
        nsIPrivateDOMEvent *privateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(NS_GET_IID(nsIPrivateDOMEvent), (void**)&privateEvent)) {
          privateEvent->DuplicatePrivateData();
          NS_RELEASE(privateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  return ret;
}

NS_IMETHODIMP_(PRBool)
nsXULDocument::EventCaptureRegistration(PRInt32 aCapturerIncrement)
{
  mNumCapturers += aCapturerIncrement;
  NS_ASSERTION(mNumCapturers >= 0, "Number of capturers has become negative");
  return (mNumCapturers > 0 ? PR_TRUE : PR_FALSE);
}

//----------------------------------------------------------------------
//
// nsIXMLDocument interface
//

NS_IMETHODIMP
nsXULDocument::SetDefaultStylesheets(nsIURI* aUrl)
{
    NS_ASSERTION(0,"not implemented");
    NS_NOTREACHED("nsXULDocument::SetDefaultStylesheets");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::SetTitle(const PRUnichar *aTitle)
{
    return SetTitle(nsDependentString(aTitle));
}

//----------------------------------------------------------------------
//
// nsIXULDocument interface
//

NS_IMETHODIMP
nsXULDocument::AddElementForID(const nsAReadableString& aID, nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Add(aID, aElement);
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::RemoveElementForID(const nsAReadableString& aID, nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Remove(aID, aElement);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetElementsForID(const nsAReadableString& aID, nsISupportsArray* aElements)
{
    NS_PRECONDITION(aElements != nsnull, "null ptr");
    if (! aElements)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Find(aID, aElements);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::AddForwardReference(nsForwardReference* aRef)
{
    if (mResolutionPhase < aRef->GetPhase()) {
        mForwardReferences.AppendElement(aRef);
    }
    else {
        NS_ERROR("forward references have already been resolved");
        delete aRef;
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::ResolveForwardReferences()
{
    if (mResolutionPhase == nsForwardReference::eDone)
        return NS_OK;

    // Resolve each outstanding 'forward' reference. We iterate
    // through the list of forward references until no more forward
    // references can be resolved. This annealing process is
    // guaranteed to converge because we've "closed the gate" to new
    // forward references.

    const nsForwardReference::Phase* pass = nsForwardReference::kPasses;
    while ((mResolutionPhase = *pass) != nsForwardReference::eDone) {
        PRInt32 previous = 0;
        while (mForwardReferences.Count() && mForwardReferences.Count() != previous) {
            previous = mForwardReferences.Count();

            for (PRInt32 i = 0; i < mForwardReferences.Count(); ++i) {
                nsForwardReference* fwdref = NS_REINTERPRET_CAST(nsForwardReference*, mForwardReferences[i]);

                if (fwdref->GetPhase() == *pass) {
                    nsForwardReference::Result result = fwdref->Resolve();

                    switch (result) {
                    case nsForwardReference::eResolve_Succeeded:
                    case nsForwardReference::eResolve_Error:
                        mForwardReferences.RemoveElementAt(i);
                        delete fwdref;

                        // fixup because we removed from list
                        --i;
                        break;

                    case nsForwardReference::eResolve_Later:
                        // do nothing. we'll try again later
                        ;
                    }
                }
            }
        }

        ++pass;
    }

    DestroyForwardReferences();
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetMasterPrototype(nsIXULPrototypeDocument* aDocument)
{
  mMasterPrototype = aDocument;
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetMasterPrototype(nsIXULPrototypeDocument** aDocument)
{
  *aDocument = mMasterPrototype;
  NS_IF_ADDREF(*aDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetCurrentPrototype(nsIXULPrototypeDocument* aDocument)
{
  mCurrentPrototype = aDocument;
  return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIDOMDocument interface
//

NS_IMETHODIMP
nsXULDocument::GetDoctype(nsIDOMDocumentType** aDoctype)
{
    NS_NOTREACHED("nsXULDocument::GetDoctype");
    NS_ENSURE_ARG_POINTER(aDoctype);
    *aDoctype = nsnull;
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::GetImplementation(nsIDOMDOMImplementation** aImplementation)
{
  nsresult rv;
  rv = nsComponentManager::CreateInstance(kDOMImplementationCID,
                                          nsnull,
                                          NS_GET_IID(nsIDOMDOMImplementation),
                                          (void**) aImplementation);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIPrivateDOMImplementation> impl = do_QueryInterface(*aImplementation, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = impl->Init(mDocumentURL);
  return rv;
}

NS_IMETHODIMP
nsXULDocument::GetDocumentElement(nsIDOMElement** aDocumentElement)
{
    NS_PRECONDITION(aDocumentElement != nsnull, "null ptr");
    if (! aDocumentElement)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aDocumentElement);
    }
    else {
        *aDocumentElement = nsnull;
        return NS_OK;
    }
}



NS_IMETHODIMP
nsXULDocument::CreateElement(const nsAReadableString& aTagName,
                             nsIDOMElement** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsIAtom> name, prefix;

    name = dont_AddRef(NS_NewAtom(aTagName));
    NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
      char* tagCStr = ToNewCString(aTagName);

      PR_LOG(gXULLog, PR_LOG_DEBUG,
             ("xul[CreateElement] %s", tagCStr));

      nsCRT::free(tagCStr);
    }
#endif

    *aReturn = nsnull;

    nsCOMPtr<nsINodeInfo> ni;

    // CreateElement in the XUL document defaults to the XUL namespace.
    mNodeInfoManager->GetNodeInfo(name, prefix, kNameSpaceID_XUL,
                                  *getter_AddRefs(ni));

    nsCOMPtr<nsIContent> result;
    rv = CreateElement(ni, getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;

    // get the DOM interface
    rv = result->QueryInterface(NS_GET_IID(nsIDOMElement), (void**) aReturn);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
{
    NS_NOTREACHED("nsXULDocument::CreateDocumentFragment");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::CreateTextNode(const nsAReadableString& aData,
                              nsIDOMText** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsITextContent> text;
    rv = nsComponentManager::CreateInstance(kTextNodeCID, nsnull, NS_GET_IID(nsITextContent), getter_AddRefs(text));
    if (NS_FAILED(rv)) return rv;

    rv = text->SetText(aData, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    rv = text->QueryInterface(NS_GET_IID(nsIDOMText), (void**) aReturn);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOMText");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CreateComment(const nsAReadableString& aData,
                             nsIDOMComment** aReturn)
{
    nsCOMPtr<nsIContent> comment;
    nsresult rv = NS_NewCommentNode(getter_AddRefs(comment));

    if (NS_SUCCEEDED(rv)) {
        rv = comment->QueryInterface(NS_GET_IID(nsIDOMComment),
                                     (void**)aReturn);
        (*aReturn)->AppendData(aData);
    }

    return rv;
}


NS_IMETHODIMP
nsXULDocument::CreateCDATASection(const nsAReadableString& aData,
                                  nsIDOMCDATASection** aReturn)
{
    NS_NOTREACHED("nsXULDocument::CreateCDATASection");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::CreateProcessingInstruction(const nsAReadableString& aTarget,
                                           const nsAReadableString& aData,
                                           nsIDOMProcessingInstruction** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;

    nsCOMPtr<nsIContent> content;
    nsresult rv = NS_NewXMLProcessingInstruction(getter_AddRefs(content),
                                                 aTarget, aData);

    if (NS_FAILED(rv)) {
        return rv;
    }

    return CallQueryInterface(content, aReturn);
}


NS_IMETHODIMP
nsXULDocument::CreateAttribute(const nsAReadableString& aName,
                               nsIDOMAttr** aReturn)
{
    NS_NOTREACHED("nsXULDocument::CreateAttribute");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::CreateEntityReference(const nsAReadableString& aName,
                                     nsIDOMEntityReference** aReturn)
{
    NS_NOTREACHED("nsXULDocument::CreateEntityReference");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::GetElementsByTagName(const nsAReadableString& aTagName,
                                    nsIDOMNodeList** aReturn)
{
    nsresult rv;
    nsRDFDOMNodeList* elements;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
        NS_ERROR("unable to create node list");
        return rv;
    }

    nsIContent* root = nsnull;
    GetRootContent(&root);
    NS_ASSERTION(root != nsnull, "no doc root");

    if (root != nsnull) {
        rv = GetElementsByTagName(root, aTagName, kNameSpaceID_Unknown,
                                  elements);

        NS_RELEASE(root);
    }

    *aReturn = elements;
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetElementsByAttribute(const nsAReadableString& aAttribute,
                                      const nsAReadableString& aValue,
                                      nsIDOMNodeList** aReturn)
{
    nsresult rv;
    nsRDFDOMNodeList* elements;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
        NS_ERROR("unable to create node list");
        return rv;
    }

    nsIContent* root = nsnull;
    GetRootContent(&root);
    NS_ASSERTION(root != nsnull, "no doc root");

    if (root != nsnull) {
        nsIDOMNode* domRoot;
        if (NS_SUCCEEDED(rv = root->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) &domRoot))) {
            rv = GetElementsByAttribute(domRoot, aAttribute, aValue, elements);
            NS_RELEASE(domRoot);
        }
        NS_RELEASE(root);
    }

    *aReturn = elements;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::Persist(const nsAReadableString& aID,
                       const nsAReadableString& aAttr)
{
    nsresult rv;

    nsCOMPtr<nsIDOMElement> domelement;
    rv = GetElementById(aID, getter_AddRefs(domelement));
    if (NS_FAILED(rv)) return rv;

    if (! domelement)
        return NS_OK;

    nsCOMPtr<nsIContent> element = do_QueryInterface(domelement);
    NS_ASSERTION(element != nsnull, "null ptr");
    if (! element)
        return NS_ERROR_UNEXPECTED;

    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tag;
    nsCOMPtr<nsINodeInfo> ni;
    rv = element->NormalizeAttrString(aAttr, *getter_AddRefs(ni));
    if (NS_FAILED(rv)) return rv;

    ni->GetNameAtom(*getter_AddRefs(tag));
    ni->GetNamespaceID(nameSpaceID);

    rv = Persist(element, nameSpaceID, tag);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
nsXULDocument::Persist(nsIContent* aElement, PRInt32 aNameSpaceID,
                       nsIAtom* aAttribute)
{
    // First make sure we _have_ a local store to stuff the persited
    // information into. (We might not have one if profile information
    // hasn't been loaded yet...)
    if (! mLocalStore)
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIRDFResource> element;
    rv = nsXULContentUtils::GetElementResource(aElement, getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    // No ID, so nothing to persist.
    if (! element)
        return NS_OK;

    // Ick. Construct a property from the attribute. Punt on
    // namespaces for now.
    const PRUnichar* attrstr;
    rv = aAttribute->GetUnicode(&attrstr);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> attr;
    nsCAutoString temp_attrstr; temp_attrstr.AssignWithConversion(attrstr);
    rv = gRDFService->GetResource(temp_attrstr, getter_AddRefs(attr));
    if (NS_FAILED(rv)) return rv;

    // Turn the value into a literal
    nsAutoString valuestr;
    rv = aElement->GetAttr(kNameSpaceID_None, aAttribute, valuestr);
    if (NS_FAILED(rv)) return rv;

    PRBool novalue = (rv != NS_CONTENT_ATTR_HAS_VALUE);

    // See if there was an old value...
    nsCOMPtr<nsIRDFNode> oldvalue;
    rv = mLocalStore->GetTarget(element, attr, PR_TRUE, getter_AddRefs(oldvalue));
    if (NS_FAILED(rv)) return rv;

    if (oldvalue && novalue) {
        // ...there was an oldvalue, and they've removed it. XXXThis
        // handling isn't quite right...
        rv = mLocalStore->Unassert(element, attr, oldvalue);
    }
    else {
        // Now either 'change' or 'assert' based on whether there was
        // an old value.
        nsCOMPtr<nsIRDFLiteral> newvalue;
        rv = gRDFService->GetLiteral(valuestr.get(), getter_AddRefs(newvalue));
        if (NS_FAILED(rv)) return rv;

        if (oldvalue) {
            rv = mLocalStore->Change(element, attr, oldvalue, newvalue);
        }
        else {
            rv = mLocalStore->Assert(element, attr, newvalue, PR_TRUE);
        }
    }

    if (NS_FAILED(rv)) return rv;

    // Add it to the persisted set for this document (if it's not
    // there already).
    {
        nsXPIDLCString docurl;
        rv = mDocumentURL->GetSpec(getter_Copies(docurl));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> doc;
        rv = gRDFService->GetResource(docurl, getter_AddRefs(doc));
        if (NS_FAILED(rv)) return rv;

        PRBool hasAssertion;
        rv = mLocalStore->HasAssertion(doc, kNC_persist, element, PR_TRUE, &hasAssertion);
        if (NS_FAILED(rv)) return rv;

        if (! hasAssertion) {
            rv = mLocalStore->Assert(doc, kNC_persist, element, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}



nsresult
nsXULDocument::DestroyForwardReferences()
{
    for (PRInt32 i = mForwardReferences.Count() - 1; i >= 0; --i) {
        nsForwardReference* fwdref = NS_REINTERPRET_CAST(nsForwardReference*, mForwardReferences[i]);
        delete fwdref;
    }

    mForwardReferences.Clear();
    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsIDOMNSDocument interface
//

NS_IMETHODIMP
nsXULDocument::GetCharacterSet(nsAWritableString& aCharacterSet)
{
  return GetDocumentCharacterSet(aCharacterSet);
}

NS_IMETHODIMP
nsXULDocument::CreateRange(nsIDOMRange** aRange)
{
    return nsComponentManager::CreateInstance(kRangeCID, nsnull,
                                              NS_GET_IID(nsIDOMRange),
                                              (void **)aRange);
}

NS_IMETHODIMP    
nsXULDocument::CreateNodeIterator(nsIDOMNode *aRoot,
                                  PRUint32 aWhatToShow,
                                  nsIDOMNodeFilter *aFilter,
                                  PRBool aEntityReferenceExpansion,
                                  nsIDOMNodeIterator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsXULDocument::CreateTreeWalker(nsIDOMNode *aRoot,
                                PRUint32 aWhatToShow,
                                nsIDOMNodeFilter *aFilter,
                                PRBool aEntityReferenceExpansion,
                                nsIDOMTreeWalker **_retval)
{
  return NS_NewTreeWalker(aRoot,
                          aWhatToShow,
                          aFilter,
                          aEntityReferenceExpansion,
                          _retval);
}

NS_IMETHODIMP
nsXULDocument::GetDefaultView(nsIDOMAbstractView** aDefaultView)
{
  NS_ENSURE_ARG_POINTER(aDefaultView);
  *aDefaultView = nsnull;

  nsIPresShell *shell = NS_STATIC_CAST(nsIPresShell *,
                                       mPresShells.ElementAt(0));
  NS_ENSURE_TRUE(shell, NS_OK);

  nsCOMPtr<nsIPresContext> ctx;
  nsresult rv = shell->GetPresContext(getter_AddRefs(ctx));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && ctx, rv);

  nsCOMPtr<nsISupports> container;
  rv = ctx->GetContainer(getter_AddRefs(container));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && container, rv);

  nsCOMPtr<nsIInterfaceRequestor> ifrq(do_QueryInterface(container));
  NS_ENSURE_TRUE(ifrq, NS_OK);

  nsCOMPtr<nsIDOMWindowInternal> window;
  ifrq->GetInterface(NS_GET_IID(nsIDOMWindowInternal), getter_AddRefs(window));
  NS_ENSURE_TRUE(window, NS_OK);

  window->QueryInterface(NS_GET_IID(nsIDOMAbstractView),
                         (void **)aDefaultView);

  return NS_OK;
}

nsresult
nsXULDocument::GetPixelDimensions(nsIPresShell* aShell,
                                  PRInt32* aWidth,
                                  PRInt32* aHeight)
{
    nsresult result = NS_OK;
    nsSize size;
    nsIFrame* frame;

    result = FlushPendingNotifications();
    if (NS_FAILED(result)) {
        return result;
    }

    result = aShell->GetPrimaryFrameFor(mRootContent, &frame);
    if (NS_SUCCEEDED(result) && frame) {
        nsIView*                  view;
        nsCOMPtr<nsIPresContext>  presContext;

        aShell->GetPresContext(getter_AddRefs(presContext));
        result = frame->GetView(presContext, &view);
        if (NS_SUCCEEDED(result)) {
            // If we have a view check if it's scrollable. If not,
            // just use the view size itself
            if (view) {
                nsIScrollableView* scrollableView;

                if (NS_SUCCEEDED(view->QueryInterface(NS_GET_IID(nsIScrollableView),
                                                      (void**)&scrollableView))) {
                    scrollableView->GetScrolledView(view);
                }

                result = view->GetDimensions(&size.width, &size.height);
            }
            // If we don't have a view, use the frame size
            else {
                result = frame->GetSize(size);
            }
        }

        // Convert from twips to pixels
        if (NS_SUCCEEDED(result)) {
            nsCOMPtr<nsIPresContext> context;

            result = aShell->GetPresContext(getter_AddRefs(context));

            if (NS_SUCCEEDED(result)) {
                float scale;
                context->GetTwipsToPixels(&scale);

                *aWidth = NSTwipsToIntPixels(size.width, scale);
                *aHeight = NSTwipsToIntPixels(size.height, scale);
            }
        }
    }
    else {
        *aWidth = 0;
        *aHeight = 0;
    }

    return result;

}

NS_IMETHODIMP
nsXULDocument::GetWidth(PRInt32* aWidth)
{
    NS_ENSURE_ARG_POINTER(aWidth);

    nsCOMPtr<nsIPresShell> shell;
    nsresult result = NS_OK;

    // We make the assumption that the first presentation shell
    // is the one for which we need information.
    GetShellAt(0, getter_AddRefs(shell));
    if (shell) {
        PRInt32 width, height;

        result = GetPixelDimensions(shell, &width, &height);
        *aWidth = width;
    } else
        *aWidth = 0;

    return result;
}

NS_IMETHODIMP
nsXULDocument::GetHeight(PRInt32* aHeight)
{
    NS_ENSURE_ARG_POINTER(aHeight);

    nsCOMPtr<nsIPresShell> shell;
    nsresult result = NS_OK;

    // We make the assumption that the first presentation shell
    // is the one for which we need information.
    GetShellAt(0, getter_AddRefs(shell));
    if (shell) {
        PRInt32 width, height;

        result = GetPixelDimensions(shell, &width, &height);
        *aHeight = height;
    } else
        *aHeight = 0;

    return result;
}

NS_IMETHODIMP
nsXULDocument::AddBinding(nsIDOMElement* aContent,
                          const nsAReadableString& aURL)
{
  nsCOMPtr<nsIBindingManager> bm;
  GetBindingManager(getter_AddRefs(bm));
  nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));

  return bm->AddLayeredBinding(content, aURL);
}

NS_IMETHODIMP
nsXULDocument::RemoveBinding(nsIDOMElement* aContent,
                             const nsAReadableString& aURL)
{
  if (mBindingManager) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));
    return mBindingManager->RemoveLayeredBinding(content, aURL);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULDocument::LoadBindingDocument(const nsAReadableString& aURL, nsIDOMDocument** aResult)
{
  if (mBindingManager) {
    nsCOMPtr<nsIDocument> doc;
    mBindingManager->LoadBindingDocument(this, aURL, getter_AddRefs(doc));
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
    *aResult = domDoc;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULDocument::GetBindingParent(nsIDOMNode* aNode, nsIDOMElement** aResult)
{
  *aResult = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (!content)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> result;
  content->GetBindingParent(getter_AddRefs(result));
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(result));
  *aResult = elt;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

static nsresult
GetElementByAttribute(nsIContent* aContent,
                      nsIAtom* aAttrName,
                      const nsAReadableString& aAttrValue,
                      PRBool aUniversalMatch,
                      nsIDOMElement** aResult)
{
  nsAutoString value;
  nsresult rv = aContent->GetAttr(kNameSpaceID_None, aAttrName, value);
  if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
    if (aUniversalMatch || value.Equals(aAttrValue))
      return aContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aResult);
  }


  PRInt32 childCount;
  aContent->ChildCount(childCount);

  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIContent> current;
    aContent->ChildAt(i, *getter_AddRefs(current));

    GetElementByAttribute(current, aAttrName, aAttrValue, aUniversalMatch, aResult);

    if (*aResult)
      return NS_OK;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetAnonymousElementByAttribute(nsIDOMElement* aElement,
                                              const nsAReadableString& aAttrName,
                                              const nsAReadableString& aAttrValue,
                                              nsIDOMElement** aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIDOMNodeList> nodeList;
  GetAnonymousNodes(aElement, getter_AddRefs(nodeList));

  if (!nodeList)
    return NS_OK;

  nsCOMPtr<nsIAtom> attribute = getter_AddRefs(NS_NewAtom(aAttrName));

  PRUint32 length;
  nodeList->GetLength(&length);

  PRBool universalMatch = aAttrValue.Equals(NS_LITERAL_STRING("*"));

  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMNode> current;
    nodeList->Item(i, getter_AddRefs(current));

    nsCOMPtr<nsIContent> content(do_QueryInterface(current));

    GetElementByAttribute(content, attribute, aAttrValue, universalMatch, aResult);
    if (*aResult)
      return NS_OK;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetAnonymousNodes(nsIDOMElement* aElement,
                                 nsIDOMNodeList** aResult)
{
  *aResult = nsnull;
  if (mBindingManager) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
    return mBindingManager->GetAnonymousNodesFor(content, aResult);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetLocation(nsIDOMLocation** aLocation)
{
  if (mScriptGlobalObject) {
    nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(mScriptGlobalObject));
    if(window) {
      return window->GetLocation(aLocation);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetTitle(nsAWritableString& aTitle)
{
    aTitle.Assign(mDocumentTitle);

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetTitle(const nsAReadableString& aTitle)
{
    for (PRInt32 i = mPresShells.Count() - 1; i >= 0; --i) {
        nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);

        nsCOMPtr<nsIPresContext> context;
        nsresult rv = shell->GetPresContext(getter_AddRefs(context));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsISupports> container;
        rv = context->GetContainer(getter_AddRefs(container));
        NS_ENSURE_SUCCESS(rv, rv);

        if (!container)
            continue;

        nsCOMPtr<nsIBaseWindow> docShellWin = do_QueryInterface(container);
        if(!docShellWin)
            continue;

        rv = docShellWin->SetTitle(PromiseFlatString(aTitle).get());
        NS_ENSURE_SUCCESS(rv, rv);
    }

    mDocumentTitle.Assign(aTitle);

    // Fire a DOM event for the title change.
    nsCOMPtr<nsIDOMEvent> event;
    CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
    if (event) {
      event->InitEvent(NS_LITERAL_STRING("DOMTitleChanged"), PR_TRUE, PR_TRUE);
      PRBool noDefault;
      DispatchEvent(event, &noDefault);
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetPlugins(nsIDOMPluginArray** aPlugins)
{
    NS_NOTREACHED("nsXULDocument::GetPlugins");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetDir(nsAWritableString& aDirection)
{
  aDirection.Assign(NS_LITERAL_STRING("ltr"));
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetDir(const nsAReadableString& aDirection)
{
  return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIDOMXULDocument interface
//

NS_IMETHODIMP
nsXULDocument::GetPopupNode(nsIDOMNode** aNode)
{
#ifdef DEBUG_dr
    printf("dr :: nsXULDocument::GetPopupNode\n");
#endif

    nsresult rv;

    // get focus controller
    nsCOMPtr<nsIFocusController> focusController;
    rv = GetFocusController(getter_AddRefs(focusController));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);
    // get popup node
    rv = focusController->GetPopupNode(aNode); // addref happens here

    return rv;
}

NS_IMETHODIMP
nsXULDocument::SetPopupNode(nsIDOMNode* aNode)
{
#ifdef DEBUG_dr
    printf("dr :: nsXULDocument::SetPopupNode\n");
#endif

    nsresult rv;

    // get focus controller
    nsCOMPtr<nsIFocusController> focusController;
    rv = GetFocusController(getter_AddRefs(focusController));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);
    // set popup node
    rv = focusController->SetPopupNode(aNode);

    return rv;
}

NS_IMETHODIMP
nsXULDocument::GetTooltipNode(nsIDOMNode** aNode)
{
    *aNode = mTooltipNode;
    NS_IF_ADDREF(*aNode);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetTooltipNode(nsIDOMNode* aNode)
{
    mTooltipNode = dont_QueryInterface(aNode);
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetCommandDispatcher(nsIDOMXULCommandDispatcher** aTracker)
{
  *aTracker = mCommandDispatcher;
  NS_IF_ADDREF(*aTracker);
  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ImportNode(nsIDOMNode* aImportedNode,
                          PRBool aDeep,
                          nsIDOMNode** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::CreateElementNS(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aQualifiedName,
                               nsIDOMElement** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
      char* namespaceCStr = ToNewCString(aNamespaceURI);
      char* tagCStr = ToNewCString(aQualifiedName);

      PR_LOG(gXULLog, PR_LOG_DEBUG,
             ("xul[CreateElementNS] [%s]:%s", namespaceCStr, tagCStr));

      nsCRT::free(tagCStr);
      nsCRT::free(namespaceCStr);
    }
#endif

    nsCOMPtr<nsIAtom> name, prefix;

    // parse the user-provided string into a tag name and prefix
    rv = ParseTagString(aQualifiedName, *getter_AddRefs(name),
                        *getter_AddRefs(prefix));
    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        char* tagNameStr = ToNewCString(aQualifiedName);
        PR_LOG(gXULLog, PR_LOG_ERROR,
               ("xul[CreateElement] unable to parse tag '%s'; no such namespace.", tagNameStr));
        nsCRT::free(tagNameStr);
#endif
        return rv;
    }

    // Get The real namespace ID
    PRInt32 nameSpaceID;
    rv = mNameSpaceManager->GetNameSpaceID(aNamespaceURI, nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsINodeInfo> ni;
    // XXX This whole method is depricated!
    mNodeInfoManager->GetNodeInfo(name, prefix, nameSpaceID,
                                  *getter_AddRefs(ni));

    nsCOMPtr<nsIContent> result;
    rv = CreateElement(ni, getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;

    // get the DOM interface
    rv = result->QueryInterface(NS_GET_IID(nsIDOMElement), (void**) aReturn);
    NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM element");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::CreateAttributeNS(const nsAReadableString& aNamespaceURI,
                                 const nsAReadableString& aQualifiedName,
                                 nsIDOMAttr** aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetElementById(const nsAReadableString& aId,
                              nsIDOMElement** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;

    NS_WARN_IF_FALSE(!aId.IsEmpty(),"getElementById(\"\"), fix caller?");
    if (aId.IsEmpty())
      return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIContent> element;
    rv = mElementMap.FindFirst(aId, getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    if (element) {
        rv = element->QueryInterface(NS_GET_IID(nsIDOMElement), (void**) aReturn);
    }
    else {
        rv = NS_OK;
    }

    return rv;
}

NS_IMETHODIMP
nsXULDocument::GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,
                                      const nsAReadableString& aLocalName,
                                      nsIDOMNodeList** aReturn)
{
    nsresult rv;

    nsRDFDOMNodeList* elements;
    if (NS_FAILED(rv = nsRDFDOMNodeList::Create(&elements))) {
        NS_ERROR("unable to create node list");
        return rv;
    }

    *aReturn = elements;

    nsCOMPtr<nsIContent> root;
    GetRootContent(getter_AddRefs(root));
    NS_ASSERTION(root, "no doc root");

    if (root) {
        PRInt32 nsid = kNameSpaceID_Unknown;
        if (!aNamespaceURI.Equals(NS_LITERAL_STRING("*"))) {
            rv = mNameSpaceManager->GetNameSpaceID(aNamespaceURI, nsid);
            NS_ENSURE_SUCCESS(rv, rv);

            if (nsid == kNameSpaceID_Unknown) {
                // Namespace not found, then there can't be any elements to
                // be found.
                return NS_OK;
            }
        }

        rv = GetElementsByTagName(root, aLocalName, nsid,
                                  elements);
    }

    return NS_OK;
}

nsresult
nsXULDocument::AddElementToDocumentPre(nsIContent* aElement)
{
    // Do a bunch of work that's necessary when an element gets added
    // to the XUL Document.
    nsresult rv;

    // 1. Add the element to the resource-to-element map
    rv = AddElementToMap(aElement);
    if (NS_FAILED(rv)) return rv;

    // 2. If the element is a 'command updater' (i.e., has a
    // "commandupdater='true'" attribute), then add the element to the
    // document's command dispatcher
    nsAutoString value;
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::commandupdater, value);
    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && value == NS_LITERAL_STRING("true")) {
        rv = nsXULContentUtils::SetCommandUpdater(this, aElement);
        if (NS_FAILED(rv)) return rv;
    }

    // 3. Check for a broadcaster hookup attribute, in which case
    // we'll hook the node up as a listener on a broadcaster.
    PRBool listener, resolved;
    rv = CheckBroadcasterHookup(this, aElement, &listener, &resolved);
    if (NS_FAILED(rv)) return rv;

    // If it's not there yet, we may be able to defer hookup until
    // later.
    if (listener && !resolved && (mResolutionPhase != nsForwardReference::eDone)) {
        BroadcasterHookup* hookup = new BroadcasterHookup(this, aElement);
        if (! hookup)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = AddForwardReference(hookup);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
nsXULDocument::AddElementToDocumentPost(nsIContent* aElement)
{
    nsresult rv;

    // We need to pay special attention to the keyset tag to set up a listener
    nsCOMPtr<nsIAtom> tag;
    aElement->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::keyset) {
        // Create our XUL key listener and hook it up.
        nsCOMPtr<nsIXBLService> xblService(do_GetService("@mozilla.org/xbl;1", &rv));
        if (xblService) {
            nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(aElement));
            xblService->AttachGlobalKeyHandler(rec);
        }
    }

    // See if we need to attach a XUL template to this node
    return CheckTemplateBuilder(aElement);
}

NS_IMETHODIMP
nsXULDocument::AddSubtreeToDocument(nsIContent* aElement)
{
    nsresult rv;

    // Do pre-order addition magic
    rv = AddElementToDocumentPre(aElement);
    if (NS_FAILED(rv)) return rv;

    // Recurse to children
    PRInt32 count = 0;
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    rv = xulcontent ? xulcontent->PeekChildCount(count) : aElement->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    while (--count >= 0) {
        nsCOMPtr<nsIContent> child;
        rv = aElement->ChildAt(count, *getter_AddRefs(child));
        if (NS_FAILED(rv)) return rv;

        rv = AddSubtreeToDocument(child);
        if (NS_FAILED(rv)) return rv;
    }

    // Do post-order addition magic
    return AddElementToDocumentPost(aElement);
}

NS_IMETHODIMP
nsXULDocument::RemoveSubtreeFromDocument(nsIContent* aElement)
{
    // Do a bunch of cleanup to remove an element from the XUL
    // document.
    nsresult rv;

    // 1. Remove any children from the document.
    PRInt32 count;
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    rv = xulcontent ? xulcontent->PeekChildCount(count) : aElement->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    while (--count >= 0) {
        nsCOMPtr<nsIContent> child;
        rv = aElement->ChildAt(count, *getter_AddRefs(child));
        if (NS_FAILED(rv)) return rv;

        rv = RemoveSubtreeFromDocument(child);
        if (NS_FAILED(rv)) return rv;
    }

    // 2. Remove the element from the resource-to-element map
    rv = RemoveElementFromMap(aElement);
    if (NS_FAILED(rv)) return rv;

    // 3. If the element is a 'command updater', then remove the
    // element from the document's command dispatcher.
    nsAutoString value;
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::commandupdater, value);
    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && value == NS_LITERAL_STRING("true")) {
        nsCOMPtr<nsIDOMElement> domelement = do_QueryInterface(aElement);
        NS_ASSERTION(domelement != nsnull, "not a DOM element");
        if (! domelement)
            return NS_ERROR_UNEXPECTED;

        rv = mCommandDispatcher->RemoveCommandUpdater(domelement);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetTemplateBuilderFor(nsIContent* aContent, nsIXULTemplateBuilder* aBuilder)
{
  if (! mTemplateBuilderTable) {
    mTemplateBuilderTable = new nsSupportsHashtable();
    if (! mTemplateBuilderTable)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  nsISupportsKey key(aContent);

  if (aContent) {
    mTemplateBuilderTable->Put(&key, aBuilder);
  }
  else {
    mTemplateBuilderTable->Remove(&key);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetTemplateBuilderFor(nsIContent* aContent, nsIXULTemplateBuilder** aResult)
{
  if (mTemplateBuilderTable) {
    nsISupportsKey key(aContent);
    *aResult = NS_STATIC_CAST(nsIXULTemplateBuilder*, mTemplateBuilderTable->Get(&key));
  }
  else
    *aResult = nsnull;

  return NS_OK;
}

// Attributes that are used with getElementById() and the
// resource-to-element map.
nsIAtom** nsXULDocument::kIdentityAttrs[] = { &nsXULAtoms::id, &nsXULAtoms::ref, nsnull };

nsresult
nsXULDocument::AddElementToMap(nsIContent* aElement)
{
    // Look at the element's 'id' and 'ref' attributes, and if set,
    // add pointers in the resource-to-element map to the element.
    nsresult rv;

    for (PRInt32 i = 0; kIdentityAttrs[i] != nsnull; ++i) {
        nsAutoString value;
        rv = aElement->GetAttr(kNameSpaceID_None, *kIdentityAttrs[i], value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get attribute");
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            rv = mElementMap.Add(value, aElement);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


nsresult
nsXULDocument::RemoveElementFromMap(nsIContent* aElement)
{
    // Remove the element from the resource-to-element map.
    nsresult rv;

    for (PRInt32 i = 0; kIdentityAttrs[i] != nsnull; ++i) {
        nsAutoString value;
        rv = aElement->GetAttr(kNameSpaceID_None, *kIdentityAttrs[i], value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get attribute");
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            rv = mElementMap.Remove(value, aElement);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


PRIntn
nsXULDocument::RemoveElementsFromMapByContent(const PRUnichar* aID,
                                              nsIContent* aElement,
                                              void* aClosure)
{
    nsIContent* content = NS_REINTERPRET_CAST(nsIContent*, aClosure);
    return (aElement == content) ? HT_ENUMERATE_REMOVE : HT_ENUMERATE_NEXT;
}



//----------------------------------------------------------------------
//
// nsIDOMNode interface
//

NS_IMETHODIMP
nsXULDocument::GetNodeName(nsAWritableString& aNodeName)
{
    aNodeName.Assign(NS_LITERAL_STRING("#document"));
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetNodeValue(nsAWritableString& aNodeValue)
{
    SetDOMStringToNull(aNodeValue);

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::SetNodeValue(const nsAReadableString& aNodeValue)
{
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsXULDocument::GetNodeType(PRUint16* aNodeType)
{
    *aNodeType = nsIDOMNode::DOCUMENT_NODE;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetParentNode(nsIDOMNode** aParentNode)
{
    *aParentNode = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetChildNodes(nsIDOMNodeList** aChildNodes)
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
            rv = mRootContent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)&domNode);
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
nsXULDocument::HasChildNodes(PRBool* aHasChildNodes)
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
nsXULDocument::HasAttributes(PRBool* aHasAttributes)
{
    NS_ENSURE_ARG_POINTER(aHasAttributes);

    *aHasAttributes = PR_FALSE;

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetFirstChild(nsIDOMNode** aFirstChild)
{
    NS_PRECONDITION(aFirstChild != nsnull, "null ptr");
    if (! aFirstChild)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aFirstChild);
    }
    else {
        *aFirstChild = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
nsXULDocument::GetLastChild(nsIDOMNode** aLastChild)
{
    NS_PRECONDITION(aLastChild != nsnull, "null ptr");
    if (! aLastChild)
        return NS_ERROR_NULL_POINTER;

    if (mRootContent) {
        return mRootContent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aLastChild);
    }
    else {
        *aLastChild = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
nsXULDocument::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    NS_PRECONDITION(aPreviousSibling != nsnull, "null ptr");
    if (! aPreviousSibling)
        return NS_ERROR_NULL_POINTER;

    *aPreviousSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetNextSibling(nsIDOMNode** aNextSibling)
{
    NS_PRECONDITION(aNextSibling != nsnull, "null ptr");
    if (! aNextSibling)
        return NS_ERROR_NULL_POINTER;

    *aNextSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    NS_PRECONDITION(aAttributes != nsnull, "null ptr");
    if (! aAttributes)
        return NS_ERROR_NULL_POINTER;

    *aAttributes = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    NS_PRECONDITION(aOwnerDocument != nsnull, "null ptr");
    if (! aOwnerDocument)
        return NS_ERROR_NULL_POINTER;

    *aOwnerDocument = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetNamespaceURI(nsAWritableString& aNamespaceURI)
{
    SetDOMStringToNull(aNamespaceURI);

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetPrefix(nsAWritableString& aPrefix)
{
    SetDOMStringToNull(aPrefix);

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::SetPrefix(const nsAReadableString& aPrefix)
{
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsXULDocument::GetLocalName(nsAWritableString& aLocalName)
{
    SetDOMStringToNull(aLocalName);

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                            nsIDOMNode** aReturn)
{
    NS_NOTREACHED("nsXULDocument::InsertBefore");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                            nsIDOMNode** aReturn)
{
    NS_NOTREACHED("nsXULDocument::ReplaceChild");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    NS_NOTREACHED("nsXULDocument::RemoveChild");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    NS_NOTREACHED("nsXULDocument::AppendChild");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    // We don't allow cloning of a document
    *aReturn = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::Normalize()
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULDocument::IsSupported(const nsAReadableString& aFeature,
                           const nsAReadableString& aVersion,
                           PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::GetBaseURI(nsAWritableString &aURI)
{
  aURI.Truncate();
  if (mDocumentBaseURL) {
    nsXPIDLCString spec;
    mDocumentBaseURL->GetSpec(getter_Copies(spec));
    if (spec) {
      CopyASCIItoUCS2(nsDependentCString(spec), aURI);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::LookupNamespacePrefix(const nsAReadableString& aNamespaceURI,
                                     nsAWritableString& aPrefix)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULDocument::LookupNamespaceURI(const nsAReadableString& aNamespacePrefix,
                                  nsAWritableString& aNamespaceURI) 
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
//
// nsIHTMLContentContainer interface
//

NS_IMETHODIMP
nsXULDocument::GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult)
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
nsXULDocument::GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult)
{
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

//----------------------------------------------------------------------
//
// Implementation methods
//

nsresult
nsXULDocument::Init()
{
    nsresult rv;

    rv = NS_NewHeapArena(getter_AddRefs(mArena), nsnull);
    if (NS_FAILED(rv)) return rv;

    // Create a namespace manager so we can manage tags
    rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                            nsnull,
                                            NS_GET_IID(nsINameSpaceManager),
                                            getter_AddRefs(mNameSpaceManager));
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::CreateInstance(NS_NODEINFOMANAGER_CONTRACTID,
                                            nsnull,
                                            NS_GET_IID(nsINodeInfoManager),
                                            getter_AddRefs(mNodeInfoManager));

    if (NS_FAILED(rv)) return rv;

    mNodeInfoManager->Init(this, mNameSpaceManager);

    // Create our command dispatcher and hook it up.
    rv = nsXULCommandDispatcher::Create(this, getter_AddRefs(mCommandDispatcher));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a focus tracker");
    if (NS_FAILED(rv)) return rv;

    // Get the local store. Yeah, I know. I wish GetService() used a
    // 'void**', too.
    nsIRDFDataSource* localstore;
    rv = nsServiceManager::GetService(kLocalStoreCID,
                                      NS_GET_IID(nsIRDFDataSource),
                                      (nsISupports**) &localstore);

    // this _could_ fail; e.g., if we've tried to grab the local store
    // before profiles have initialized. If so, no big deal; nothing
    // will persist.

    if (NS_SUCCEEDED(rv)) {
        mLocalStore = localstore;
        NS_IF_RELEASE(localstore);
    }

    // Create a new nsISupportsArray for dealing with overlay references
    rv = NS_NewISupportsArray(getter_AddRefs(mUnloadedOverlays));
    if (NS_FAILED(rv)) return rv;

    // Create a new nsISupportsArray that will hold owning references
    // to each of the prototype documents used to construct this
    // document. That will ensure that prototype elements will remain
    // alive.
    rv = NS_NewISupportsArray(getter_AddRefs(mPrototypes));
    if (NS_FAILED(rv)) return rv;

#if 0
    // construct a selection object
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRangeListCID,
                                                    nsnull,
                                                    kIDOMSelectionIID,
                                                    (void**) &mSelection))) {
        NS_ERROR("unable to create DOM selection");
    }
#endif

    if (gRefCnt++ == 0) {
        // Keep the RDF service cached in a member variable to make using
        // it a bit less painful
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**) &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF Service");
        if (NS_FAILED(rv)) return rv;

        gRDFService->GetResource(NC_NAMESPACE_URI "persist",   &kNC_persist);
        gRDFService->GetResource(NC_NAMESPACE_URI "attribute", &kNC_attribute);
        gRDFService->GetResource(NC_NAMESPACE_URI "value",     &kNC_value);

        rv = nsComponentManager::CreateInstance(kHTMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIElementFactory),
                                                (void**) &gHTMLElementFactory);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get HTML element factory");
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kXMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIElementFactory),
                                                (void**) &gXMLElementFactory);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get XML element factory");
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kNameSpaceManagerCID,
                                          NS_GET_IID(nsINameSpaceManager),
                                          (nsISupports**) &gNameSpaceManager);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get namespace manager");
        if (NS_FAILED(rv)) return rv;

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;
        gNameSpaceManager->RegisterNameSpace(NS_ConvertASCIItoUCS2(kXULNameSpaceURI), kNameSpaceID_XUL);


        rv = nsServiceManager::GetService(kXULPrototypeCacheCID,
                                          NS_GET_IID(nsIXULPrototypeCache),
                                          (nsISupports**) &gXULCache);
        if (NS_FAILED(rv)) return rv;
    }

#ifdef PR_LOGGING
    if (! gXULLog)
        gXULLog = PR_NewLogModule("nsXULDocument");
#endif

    return NS_OK;
}


nsresult
nsXULDocument::StartLayout(void)
{
    if (! mRootContent) {
#ifdef PR_LOGGING
        nsXPIDLCString urlspec;
        mDocumentURL->GetSpec(getter_Copies(urlspec));

        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: unable to layout '%s'; no root content", (const char*) urlspec));
#endif
        return NS_OK;
    }

    PRInt32 count = GetNumberOfShells();
    for (PRInt32 i = 0; i < count; i++) {
      nsCOMPtr<nsIPresShell> shell;
      GetShellAt(i, getter_AddRefs(shell));
      if (nsnull == shell)
          continue;

      // Resize-reflow this time
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));
      NS_ASSERTION(cx != nsnull, "no pres context");
      if (! cx)
          return NS_ERROR_UNEXPECTED;


      nsCOMPtr<nsISupports> container;
      cx->GetContainer(getter_AddRefs(container));
      NS_ASSERTION(container != nsnull, "pres context has no container");
      if (! container)
          return NS_ERROR_UNEXPECTED;

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
      NS_ASSERTION(docShell != nsnull, "container is not a docshell");
      if (! docShell)
          return NS_ERROR_UNEXPECTED;

      nsRect r;
      cx->GetVisibleArea(r);

      // Trigger a refresh before the call to InitialReflow(), because
      // the view manager's UpdateView() function is dropping dirty rects if
      // refresh is disabled rather than accumulating them until refresh is
      // enabled and then triggering a repaint...
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsCOMPtr<nsIContentViewer> contentViewer;
        nsresult rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
        if (NS_SUCCEEDED(rv) && (contentViewer != nsnull)) {
          PRBool enabled;
          contentViewer->GetEnableRendering(&enabled);
          if (enabled) {
            vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
          }
        }
      }

      shell->InitialReflow(r.width, r.height);
      FlushPendingNotifications(); // This is done because iframes don't load their subdocs until
                                   // they get reflowed.  If we reflow asynchronously, our onload
                                   // will fire too early. -- hyatt

      // Start observing the document _after_ we do the initial
      // reflow. Otherwise, we'll get into an trouble trying to
      // create kids before the root frame is established.
      shell->BeginObservingDocument();
    }
    return NS_OK;
}


nsresult
nsXULDocument::GetElementsByTagName(nsIContent *aContent,
                                    const nsAReadableString& aName,
                                    PRInt32 aNamespaceID,
                                    nsRDFDOMNodeList* aElements)
{
    NS_ENSURE_ARG_POINTER(aContent);
    NS_ENSURE_ARG_POINTER(aElements);

    nsresult rv = NS_OK;

    nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aContent));
    if (!element)
      return NS_OK;

    nsCOMPtr<nsINodeInfo> ni;
    aContent->GetNodeInfo(*getter_AddRefs(ni));
    NS_ENSURE_TRUE(ni, NS_OK);

    if (aName.Equals(NS_LITERAL_STRING("*"))) {
        if (aNamespaceID == kNameSpaceID_Unknown ||
            ni->NamespaceEquals(aNamespaceID)) {
            if (NS_FAILED(rv = aElements->AppendNode(element))) {
                NS_ERROR("unable to append element to node list");
                return rv;
            }
        }
    }
    else {
        if (ni->Equals(aName) &&
            (aNamespaceID == kNameSpaceID_Unknown ||
             ni->NamespaceEquals(aNamespaceID))) {
            if (NS_FAILED(rv = aElements->AppendNode(element))) {
                NS_ERROR("unable to append element to node list");
                return rv;
            }
        }
    }

    PRInt32 length;
    if (NS_FAILED(rv = aContent->ChildCount(length))) {
        NS_ERROR("unable to get childcount");
        return rv;
    }

    for (PRInt32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIContent> child;
        if (NS_FAILED(rv = aContent->ChildAt(i, *getter_AddRefs(child) ))) {
            NS_ERROR("unable to get child from content");
            return rv;
        }

        if (NS_FAILED(rv = GetElementsByTagName(child, aName, aNamespaceID,
                                                aElements))) {
            NS_ERROR("unable to recursively get elements by tag name");
            return rv;
        }
    }

    return NS_OK;
}

nsresult
nsXULDocument::GetElementsByAttribute(nsIDOMNode* aNode,
                                        const nsAReadableString& aAttribute,
                                        const nsAReadableString& aValue,
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

    if ((attrValue.Equals(aValue)) || (attrValue.Length() > 0 && aValue.Equals(NS_LITERAL_STRING("*")))) {
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
nsXULDocument::ParseTagString(const nsAReadableString& aTagName, nsIAtom*& aName,
                              nsIAtom*& aPrefix)
{
    // Parse the tag into a name and prefix

    static char kNameSpaceSeparator = ':';

    nsAutoString prefix;
    nsAutoString name(aTagName);
    PRInt32 nsoffset = name.FindChar(kNameSpaceSeparator);
    if (-1 != nsoffset) {
        name.Left(prefix, nsoffset);
        name.Cut(0, nsoffset+1);
    }

    if (0 < prefix.Length())
        aPrefix = NS_NewAtom(prefix);

    aName = NS_NewAtom(name);

    return NS_OK;
}


// nsIDOMEventCapturer and nsIDOMEventReceiver Interface Implementations

NS_IMETHODIMP
nsXULDocument::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
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
nsXULDocument::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    if (mListenerManager) {
        mListenerManager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULDocument::AddEventListener(const nsAReadableString& aType,
                                nsIDOMEventListener* aListener,
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
nsXULDocument::RemoveEventListener(const nsAReadableString& aType,
                                   nsIDOMEventListener* aListener,
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
nsXULDocument::DispatchEvent(nsIDOMEvent* aEvent, PRBool *_retval)
{
  // Obtain a presentation context
  PRInt32 count = GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  GetShellAt(0, getter_AddRefs(shell));

  // Retrieve the context
  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_SUCCEEDED(presContext->GetEventStateManager(getter_AddRefs(esm)))) {
    return esm->DispatchNewEvent(NS_STATIC_CAST(nsIDocument*, this), aEvent, _retval);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULDocument::CreateEvent(const nsAReadableString& aEventType,
                           nsIDOMEvent** aReturn)
{
  // Obtain a presentation context
  PRInt32 count = GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  GetShellAt(0, getter_AddRefs(shell));

  // Retrieve the context
  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));

  if (presContext) {
    nsCOMPtr<nsIEventListenerManager> lm;
    if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(lm)))) {
      return lm->CreateEvent(presContext, nsnull, aEventType, aReturn);
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULDocument::GetListenerManager(nsIEventListenerManager** aResult)
{
    if (! mListenerManager) {
        nsresult rv;
        rv = nsComponentManager::CreateInstance(kEventListenerManagerCID,
                                                nsnull,
                                                NS_GET_IID(nsIEventListenerManager),
                                                getter_AddRefs(mListenerManager));

        if (NS_FAILED(rv)) return rv;

        mListenerManager->SetListenerTarget(NS_STATIC_CAST(nsIDocument*,this));
    }
    *aResult = mListenerManager;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::HandleEvent(nsIDOMEvent *aEvent)
{
  PRBool noDefault;
  return DispatchEvent(aEvent, &noDefault);
}

nsresult
nsXULDocument::CaptureEvent(const nsAReadableString& aType)
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
nsXULDocument::ReleaseEvent(const nsAReadableString& aType)
{
  if (mListenerManager) {
    //mListenerManager->ReleaseEvent(aListener);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsXULDocument::CreateElement(nsINodeInfo *aNodeInfo, nsIContent** aResult)
{
    NS_ENSURE_ARG_POINTER(aNodeInfo);
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;
    nsCOMPtr<nsIContent> result;

    if (aNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
        rv = nsXULElement::Create(aNodeInfo, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        PRInt32 namespaceID;
        aNodeInfo->GetNamespaceID(namespaceID);

        nsCOMPtr<nsIElementFactory> elementFactory;
        GetElementFactory(namespaceID, getter_AddRefs(elementFactory));

        rv = elementFactory->CreateInstanceByTag(aNodeInfo,
                                                 getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;
    }

    result->SetContentID(mNextContentID++);

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}


const char XUL_FASTLOAD_FILE_BASENAME[] = "XUL";

// XXXbe move to nsXULPrototypeDocument.cpp in the nsISerializable section.
// We'll increment (or maybe decrement, for easier deciphering) this maigc
// number as we flesh out the FastLoad file to include more and more data
// induced by the master prototype document.

#define XUL_FASTLOAD_FILE_VERSION       (0xfeedbeef - 3)

#define XUL_SERIALIZATION_BUFFER_SIZE   (64 * 1024)
#define XUL_DESERIALIZATION_BUFFER_SIZE (8 * 1024)


class nsXULFastLoadFileIO : public nsIFastLoadFileIO
{
  public:
    nsXULFastLoadFileIO(nsIFile* aFile)
      : mFile(aFile) {
        NS_INIT_REFCNT();
        MOZ_COUNT_CTOR(nsXULFastLoadFileIO);
    }

    virtual ~nsXULFastLoadFileIO() {
        MOZ_COUNT_DTOR(nsXULFastLoadFileIO);
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIFASTLOADFILEIO

    nsCOMPtr<nsIFile>         mFile;
    nsCOMPtr<nsIInputStream>  mInputStream;
    nsCOMPtr<nsIOutputStream> mOutputStream;
};


NS_IMPL_THREADSAFE_ISUPPORTS1(nsXULFastLoadFileIO, nsIFastLoadFileIO)
MOZ_DECL_CTOR_COUNTER(nsXULFastLoadFileIO)


NS_IMETHODIMP
nsXULFastLoadFileIO::GetInputStream(nsIInputStream** aResult)
{
    if (! mInputStream) {
        nsresult rv;
        nsCOMPtr<nsIInputStream> fileInput;
        rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInput), mFile);
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewBufferedInputStream(getter_AddRefs(mInputStream),
                                       fileInput,
                                       XUL_DESERIALIZATION_BUFFER_SIZE);
        if (NS_FAILED(rv)) return rv;
    }

    NS_ADDREF(*aResult = mInputStream);
    return NS_OK;
}


NS_IMETHODIMP
nsXULFastLoadFileIO::GetOutputStream(nsIOutputStream** aResult)
{
    if (! mOutputStream) {
        PRInt32 ioFlags = PR_WRONLY;
        if (! mInputStream)
            ioFlags |= PR_CREATE_FILE | PR_TRUNCATE;

        nsresult rv;
        nsCOMPtr<nsIOutputStream> fileOutput;
        rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOutput), mFile,
                                         ioFlags, 0644);
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewBufferedOutputStream(getter_AddRefs(mOutputStream),
                                        fileOutput,
                                        XUL_SERIALIZATION_BUFFER_SIZE);
        if (NS_FAILED(rv)) return rv;
    }

    NS_ADDREF(*aResult = mOutputStream);
    return NS_OK;
}


static PRBool gDisableXULFastLoad = PR_FALSE;           // enabled by default
static PRBool gChecksumXULFastLoadFile = PR_TRUE;       // XXXbe too paranoid

static const char kDisableXULFastLoadPref[] = "nglayout.debug.disable_xul_fastload";
static const char kChecksumXULFastLoadFilePref[] = "nglayout.debug.checksum_xul_fastload_file";

PR_STATIC_CALLBACK(int)
FastLoadPrefChangedCallback(const char* aPref, void* aClosure)
{
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
    if (prefs) {
        PRBool wasEnabled = !gDisableXULFastLoad;
        prefs->GetBoolPref(kDisableXULFastLoadPref, &gDisableXULFastLoad);

        if (wasEnabled && gDisableXULFastLoad)
            nsXULDocument::AbortFastLoads();

        prefs->GetBoolPref(kChecksumXULFastLoadFilePref, &gChecksumXULFastLoadFile);
    }
    return 0;
}


nsresult
nsXULDocument::StartFastLoad()
{
    nsresult rv;

    // Test gFastLoadList to decide whether this is the first nsXULDocument
    // participating in FastLoad.  If gFastLoadList is non-null, this document
    // must not be first, but it can join the FastLoad process.  Examples of
    // multiple master documents participating include hiddenWindow.xul and
    // navigator.xul on the Mac, and multiple-app-component (e.g., mailnews
    // and browser) startup due to command-line arguments.
    //
    // XXXbe we should attempt to update the FastLoad file after startup!
    //
    // XXXbe we do not yet use nsFastLoadPtrs, but once we do, we must keep
    // the FastLoad input stream open for the life of the app.
    if (gFastLoadList) {
        mIsFastLoad = PR_TRUE;
        mNextFastLoad = gFastLoadList;
        gFastLoadList = this;
        return NS_OK;
    }
    NS_ASSERTION(!gFastLoadService,
                 "gFastLoadList null but gFastLoadService non-null!");

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    if (prefs) {
        prefs->GetBoolPref(kDisableXULFastLoadPref, &gDisableXULFastLoad);
        prefs->GetBoolPref(kChecksumXULFastLoadFilePref, &gChecksumXULFastLoadFile);
        prefs->RegisterCallback(kDisableXULFastLoadPref,
                                FastLoadPrefChangedCallback,
                                nsnull);
        prefs->RegisterCallback(kChecksumXULFastLoadFilePref,
                                FastLoadPrefChangedCallback,
                                nsnull);
        if (gDisableXULFastLoad)
            return NS_ERROR_NOT_AVAILABLE;
    }

    // Get the chrome directory to validate against the one stored in the
    // FastLoad file, or to store there if we're generating a new file.
    nsCOMPtr<nsIFile> chromeDir;
    rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR, getter_AddRefs(chromeDir));
    if (NS_FAILED(rv))
        return rv;
    nsXPIDLCString chromePath;
    rv = chromeDir->GetPath(getter_Copies(chromePath));
    if (NS_FAILED(rv))
        return rv;

    // Use a local to refer to the service till we're sure we succeeded, then
    // commit to gFastLoadService.  Same for gFastLoadFile, which is used to
    // delete the FastLoad file on abort.
    nsCOMPtr<nsIFastLoadService> fastLoadService(do_GetFastLoadService());
    if (! fastLoadService)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIFile> file;
    rv = fastLoadService->NewFastLoadFile(XUL_FASTLOAD_FILE_BASENAME,
                                          getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;

    // Give the FastLoad service an object by which it can get or create a
    // file output stream given an input stream on the same file.
    nsXULFastLoadFileIO* xio = new nsXULFastLoadFileIO(file);
    nsCOMPtr<nsIFastLoadFileIO> io = NS_STATIC_CAST(nsIFastLoadFileIO*, xio);
    if (! io)
        return NS_ERROR_OUT_OF_MEMORY;
    fastLoadService->SetFileIO(io);

    // Try to read an existent FastLoad file.
    PRBool exists = PR_FALSE;
    if (NS_SUCCEEDED(file->Exists(&exists)) && exists) {
        nsCOMPtr<nsIInputStream> input;
        rv = io->GetInputStream(getter_AddRefs(input));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIObjectInputStream> objectInput;
        rv = fastLoadService->NewInputStream(input, getter_AddRefs(objectInput));

        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIFastLoadReadControl>
                readControl(do_QueryInterface(objectInput));
            if (readControl) {
                if (gChecksumXULFastLoadFile) {
                    // Verify checksum, using the fastLoadService's checksum
                    // cache to avoid computing more than once per session.
                    PRUint32 checksum;
                    rv = readControl->GetChecksum(&checksum);
                    if (NS_SUCCEEDED(rv)) {
                        PRUint32 verified;
                        rv = fastLoadService->ComputeChecksum(file,
                                                              readControl,
                                                              &verified);
                        if (NS_SUCCEEDED(rv) && verified != checksum) {
                            NS_WARNING("bad FastLoad file checksum");
                            rv = NS_ERROR_FAILURE;
                        }
                    }
                }

                if (NS_SUCCEEDED(rv)) {
                    // Check dependencies, fail if any is newer than file.
                    PRTime dtime, mtime;
                    rv = fastLoadService->MaxDependencyModifiedTime(readControl,
                                                                    &dtime);
                    if (NS_SUCCEEDED(rv)) {
                        rv = file->GetLastModificationDate(&mtime);
                        if (NS_SUCCEEDED(rv) && LL_CMP(mtime, <, dtime)) {
                            NS_WARNING("FastLoad file out of date");
                            rv = NS_ERROR_FAILURE;
                        }
                    }
                }
            }

            if (NS_SUCCEEDED(rv)) {
                // XXXbe get version number, scripts only for now -- bump
                // version later when rest of prototype document header is
                // serialized
                PRUint32 version;
                rv = objectInput->Read32(&version);
                if (NS_SUCCEEDED(rv)) {
                    if (version != XUL_FASTLOAD_FILE_VERSION) {
                        NS_WARNING("bad FastLoad file version");
                        rv = NS_ERROR_UNEXPECTED;
                    } else {
                        nsXPIDLCString fileChromePath;
                        rv = objectInput->ReadStringZ(
                                                 getter_Copies(fileChromePath));
                        if (NS_SUCCEEDED(rv) &&
                            nsCRT::strcmp(fileChromePath, chromePath) != 0) {
                            rv = NS_ERROR_UNEXPECTED;
                        }
                    }
                }
            }
        }

        if (NS_SUCCEEDED(rv)) {
            fastLoadService->SetInputStream(objectInput);
        } else {
            // NB: we must close before attempting to remove, for non-Unix OSes
            // that can't do open-unlink.
            if (objectInput)
                objectInput->Close();
            else
                input->Close();
            xio->mInputStream = nsnull;

#ifdef DEBUG
            file->MoveTo(nsnull, "Invalid.mfasl");
#else
            file->Remove(PR_FALSE);
#endif
            exists = PR_FALSE;
        }
    }

    // FastLoad file not found, or invalid: write a new one.
    if (! exists) {
        nsCOMPtr<nsIOutputStream> output;
        rv = io->GetOutputStream(getter_AddRefs(output));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIObjectOutputStream> objectOutput;
        rv = fastLoadService->NewOutputStream(output,
                                              getter_AddRefs(objectOutput));
        if (NS_FAILED(rv)) return rv;

        rv = objectOutput->Write32(XUL_FASTLOAD_FILE_VERSION);
        if (NS_FAILED(rv)) return rv;

        rv = objectOutput->WriteStringZ(chromePath);
        if (NS_FAILED(rv)) return rv;

        fastLoadService->SetOutputStream(objectOutput);
    }

    // If this fails, some weird reentrancy or multi-threading has occurred.
    NS_ASSERTION(!gFastLoadService && !gFastLoadFile && !gFastLoadList,
                 "something's very wrong!");

    // Success!  Insert this nsXULDocument into the (empty) gFastLoadList,
    // and commit locals to globals.
    mIsFastLoad = PR_TRUE;
    gFastLoadList = this;
    NS_ADDREF(gFastLoadService = fastLoadService);
    NS_ADDREF(gFastLoadFile = file);
    return NS_OK;
}


nsresult
nsXULDocument::EndFastLoad()
{
    nsresult rv = NS_OK, rv2 = NS_OK;

    // Exclude all non-chrome loads and XUL cache hits right away.
    if (! mIsFastLoad)
        return rv;

    // Remove this document from the global FastLoad list.  We use the list's
    // emptiness instead of a counter, to decide when the FastLoad process has
    // finally completed.  We use a singly-linked list because there won't be
    // more than a handful of master XUL documents racing, worst case.
    mIsFastLoad = PR_FALSE;
    RemoveFromFastLoadList();

    // Fetch the current input (if FastLoad file existed) or output (if we're
    // creating the FastLoad file during this app startup) stream.
    nsCOMPtr<nsIObjectInputStream> objectInput;
    nsCOMPtr<nsIObjectOutputStream> objectOutput;
    gFastLoadService->GetInputStream(getter_AddRefs(objectInput));
    gFastLoadService->GetOutputStream(getter_AddRefs(objectOutput));

    if (objectOutput) {
#if 0
        // XXXbe for now, write scripts as we sink content...
        rv = objectOutput->WriteObject(mMasterPrototype, PR_TRUE);
#endif

        // If this is the last of one or more XUL master documents loaded
        // together at app startup, close the FastLoad service's singleton
        // output stream now.
        //
        // NB: we must close input after output, in case the output stream
        // implementation needs to read from the input stream, to compute a
        // FastLoad file checksum.  In that case, the implementation used
        // nsIFastLoadFileIO to get the corresponding input stream for this
        // output stream.
        if (! gFastLoadList) {
            gFastLoadService->SetOutputStream(nsnull);
            rv = objectOutput->Close();

            if (NS_SUCCEEDED(rv) && gChecksumXULFastLoadFile) {
                rv = gFastLoadService->CacheChecksum(gFastLoadFile,
                                                     objectOutput);
            }
        }
    }

    if (objectInput) {
        // If this is the last of one or more XUL master documents loaded
        // together at app startup, close the FastLoad service's singleton
        // input stream now.
        if (! gFastLoadList) {
            gFastLoadService->SetInputStream(nsnull);
            rv2 = objectInput->Close();
        }
    }

    // If the list is empty now, the FastLoad process is done.
    if (! gFastLoadList) {
        NS_RELEASE(gFastLoadService);
        NS_RELEASE(gFastLoadFile);
    }

    return NS_FAILED(rv) ? rv : rv2;
}


nsresult
nsXULDocument::AbortFastLoads()
{
#ifdef DEBUG_brendan
    NS_BREAK();
#endif

    // Save a strong ref to the FastLoad file, so we can remove it after we
    // close open streams to it.
    nsCOMPtr<nsIFile> file = gFastLoadFile;

    // End all pseudo-concurrent XUL document FastLoads, which will close any
    // i/o streams open on the FastLoad file.
    while (gFastLoadList)
        gFastLoadList->EndFastLoad();

    // Now rename or remove the file.
    if (file) {
#ifdef DEBUG
        file->MoveTo(nsnull, "Aborted.mfasl");
#else
        file->Remove(PR_FALSE);
#endif
    }

    // Flush the XUL cache for good measure, in case we cached a bogus/downrev
    // script, somehow.
    if (gXULCache)
        gXULCache->Flush();
    return NS_OK;
}


nsresult
nsXULDocument::PrepareToLoad(nsISupports* aContainer,
                             const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             nsIParser** aResult)
{
    nsresult rv;

    // Get the document's principal
    nsCOMPtr<nsISupports> owner;
    rv = aChannel->GetOwner(getter_AddRefs(owner));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(owner);

    return PrepareToLoadPrototype(mDocumentURL, aCommand, principal, aResult);
}


nsresult
nsXULDocument::PrepareToLoadPrototype(nsIURI* aURI, const char* aCommand,
                                      nsIPrincipal* aDocumentPrincipal,
                                      nsIParser** aResult)
{
    nsresult rv;

    // Create a new prototype document.
    rv = NS_NewXULPrototypeDocument(nsnull,
                                    NS_GET_IID(nsIXULPrototypeDocument),
                                    getter_AddRefs(mCurrentPrototype));
    if (NS_FAILED(rv)) return rv;

    // Bootstrap the master document prototype.
    if (! mMasterPrototype) {
        mMasterPrototype = mCurrentPrototype;
        mMasterPrototype->SetDocumentPrincipal(aDocumentPrincipal);
    }

    rv = mCurrentPrototype->SetURI(aURI);
    if (NS_FAILED(rv)) return rv;

    // If we're reading or writing a FastLoad file, tell the FastLoad
    // service to start multiplexing data from aURI, associating it in
    // the file with the URL's string.  Each time the parser resumes
    // sinking content, it "selects" the memorized document from the
    // FastLoad multiplexor, using the nsIURI* as a fast identifier.
    if (mIsFastLoad) {
        nsXPIDLCString urlspec;
        rv = aURI->GetSpec(getter_Copies(urlspec));
        if (NS_FAILED(rv)) return rv;

        // If StartMuxedDocument returns NS_ERROR_NOT_AVAILABLE, then
        // we must be reading the file, and urlspec was not associated
        // with any multiplexed stream in it.  The FastLoad service
        // will therefore arrange to update the file, writing new data
        // at the end while old (available) data continues to be read
        // from the pre-existing part of the file.
        rv = gFastLoadService->StartMuxedDocument(aURI, urlspec,
                                          nsIFastLoadService::NS_FASTLOAD_READ |
                                          nsIFastLoadService::NS_FASTLOAD_WRITE);
        NS_ASSERTION(rv != NS_ERROR_NOT_AVAILABLE, "only reading FastLoad?!");
        if (NS_FAILED(rv))
            AbortFastLoads();
    }

    // Create a XUL content sink, a parser, and kick off a load for
    // the overlay.
    nsCOMPtr<nsIXULContentSink> sink;
    rv = nsComponentManager::CreateInstance(kXULContentSinkCID,
                                            nsnull,
                                            NS_GET_IID(nsIXULContentSink),
                                            getter_AddRefs(sink));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create XUL content sink");
    if (NS_FAILED(rv)) return rv;

    rv = sink->Init(this, mCurrentPrototype);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to initialize datasource sink");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIParser> parser;
    rv = nsComponentManager::CreateInstance(kParserCID,
                                            nsnull,
                                            kIParserIID,
                                            getter_AddRefs(parser));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create parser");
    if (NS_FAILED(rv)) return rv;

    parser->SetCommand(nsCRT::strcmp(aCommand, "view-source") ? eViewNormal :
      eViewSource);

    nsAutoString charset(NS_LITERAL_STRING("UTF-8"));
    parser->SetDocumentCharset(charset, kCharsetFromDocTypeDefault);
    parser->SetContentSink(sink); // grabs a reference to the parser

    *aResult = parser;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsresult
nsXULDocument::ApplyPersistentAttributes()
{
    // Add all of the 'persisted' attributes into the content
    // model.
    if (! mLocalStore)
        return NS_OK;

    nsresult rv;
    nsCOMPtr<nsISupportsArray> array;

    nsXPIDLCString docurl;
    rv = mDocumentURL->GetSpec(getter_Copies(docurl));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> doc;
    rv = gRDFService->GetResource(docurl, getter_AddRefs(doc));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> elements;
    rv = mLocalStore->GetTargets(doc, kNC_persist, PR_TRUE, getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasmore;
        rv = elements->HasMoreElements(&hasmore);
        if (NS_FAILED(rv)) return rv;

        if (! hasmore)
            break;

        if (! array) {
            rv = NS_NewISupportsArray(getter_AddRefs(array));
            if (NS_FAILED(rv)) return rv;
        }

        nsCOMPtr<nsISupports> isupports;
        rv = elements->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(isupports);
        if (! resource) {
            NS_WARNING("expected element to be a resource");
            continue;
        }

        const char* uri;
        rv = resource->GetValueConst(&uri);
        if (NS_FAILED(rv)) return rv;

        nsAutoString id;
        rv = nsXULContentUtils::MakeElementID(this, NS_ConvertASCIItoUCS2(uri), id);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to compute element ID");
        if (NS_FAILED(rv)) return rv;

        rv = GetElementsForID(id, array);
        if (NS_FAILED(rv)) return rv;

        PRUint32 cnt;
        rv = array->Count(&cnt);
        if (NS_FAILED(rv)) return rv;

        if (! cnt)
            continue;

        rv = ApplyPersistentAttributesToElements(resource, array);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
nsXULDocument::ApplyPersistentAttributesToElements(nsIRDFResource* aResource, nsISupportsArray* aElements)
{
    nsresult rv;

    nsCOMPtr<nsISimpleEnumerator> attrs;
    rv = mLocalStore->ArcLabelsOut(aResource, getter_AddRefs(attrs));
    if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasmore;
        rv = attrs->HasMoreElements(&hasmore);
        if (NS_FAILED(rv)) return rv;

        if (! hasmore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = attrs->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);
        if (! property) {
            NS_WARNING("expected a resource");
            continue;
        }

        const char* attrname;
        rv = property->GetValueConst(&attrname);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIAtom> attr = dont_AddRef(NS_NewAtom(attrname));
        if (! attr)
            return NS_ERROR_OUT_OF_MEMORY;

        // XXX could hang namespace off here, as well...

        nsCOMPtr<nsIRDFNode> node;
        rv = mLocalStore->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(node));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(node);
        if (! literal) {
            NS_WARNING("expected a literal");
            continue;
        }

        const PRUnichar* value;
        rv = literal->GetValueConst(&value);
        if (NS_FAILED(rv)) return rv;

        PRInt32 len = nsCRT::strlen(value);
        CBufDescriptor wrapper(value, PR_TRUE, len + 1, len);

        PRUint32 cnt;
        rv = aElements->Count(&cnt);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
            nsISupports* isupports2 = aElements->ElementAt(i);
            if (! isupports2)
                continue;

            nsCOMPtr<nsIContent> element = do_QueryInterface(isupports2);
            NS_RELEASE(isupports2);

            rv = element->SetAttr(/* XXX */ kNameSpaceID_None,
                                  attr,
                                  nsAutoString(wrapper),
                                  PR_FALSE);
        }
    }

    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsXULDocument::ContextStack
//

nsXULDocument::ContextStack::ContextStack()
    : mTop(nsnull), mDepth(0)
{
}

nsXULDocument::ContextStack::~ContextStack()
{
    while (mTop) {
        Entry* doomed = mTop;
        mTop = mTop->mNext;
        NS_IF_RELEASE(doomed->mElement);
        delete doomed;
    }
}

nsresult
nsXULDocument::ContextStack::Push(nsXULPrototypeElement* aPrototype, nsIContent* aElement)
{
    Entry* entry = new Entry;
    if (! entry)
        return NS_ERROR_OUT_OF_MEMORY;

    entry->mPrototype = aPrototype;
    entry->mElement   = aElement;
    NS_IF_ADDREF(entry->mElement);
    entry->mIndex     = 0;

    entry->mNext = mTop;
    mTop = entry;

    ++mDepth;
    return NS_OK;
}

nsresult
nsXULDocument::ContextStack::Pop()
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    Entry* doomed = mTop;
    mTop = mTop->mNext;
    --mDepth;

    NS_IF_RELEASE(doomed->mElement);
    delete doomed;
    return NS_OK;
}

nsresult
nsXULDocument::ContextStack::Peek(nsXULPrototypeElement** aPrototype,
                                           nsIContent** aElement,
                                           PRInt32* aIndex)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aPrototype = mTop->mPrototype;
    *aElement   = mTop->mElement;
    NS_IF_ADDREF(*aElement);
    *aIndex     = mTop->mIndex;

    return NS_OK;
}


nsresult
nsXULDocument::ContextStack::SetTopIndex(PRInt32 aIndex)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    mTop->mIndex = aIndex;
    return NS_OK;
}


PRBool
nsXULDocument::ContextStack::IsInsideXULTemplate()
{
    if (mDepth) {
        nsCOMPtr<nsIContent> element = dont_QueryInterface(mTop->mElement);
        while (element) {
            PRInt32 nameSpaceID;
            element->GetNameSpaceID(nameSpaceID);
            if (nameSpaceID == kNameSpaceID_XUL) {
                nsCOMPtr<nsIAtom> tag;
                element->GetTag(*getter_AddRefs(tag));
                if (tag.get() == nsXULAtoms::Template) {
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


//----------------------------------------------------------------------
//
// Content model walking routines
//

nsresult
nsXULDocument::PrepareToWalk()
{
    // Prepare to walk the mCurrentPrototype
    nsresult rv;

    // Keep an owning reference to the prototype document so that its
    // elements aren't yanked from beneath us.
    mPrototypes->AppendElement(mCurrentPrototype);

    // Push the overlay references onto our overlay processing
    // stack. GetOverlayReferences() will return an ordered array of
    // overlay references...
    nsCOMPtr<nsISupportsArray> overlays;
    rv = mCurrentPrototype->GetOverlayReferences(getter_AddRefs(overlays));
    if (NS_FAILED(rv)) return rv;

    // ...and we preserve this ordering by appending to our
    // mUnloadedOverlays array in reverse order
    PRUint32 count;
    overlays->Count(&count);
    for (PRInt32 i = count - 1; i >= 0; --i) {
        nsISupports* isupports = overlays->ElementAt(i);
        mUnloadedOverlays->AppendElement(isupports);
        NS_IF_RELEASE(isupports);
    }


    // Now check the chrome registry for any additional overlays.
    rv = AddChromeOverlays();

    // Get the prototype's root element and initialize the context
    // stack for the prototype walk.
    nsXULPrototypeElement* proto;
    rv = mCurrentPrototype->GetRootElement(&proto);
    if (NS_FAILED(rv)) return rv;


    if (! proto) {
#ifdef PR_LOGGING
        nsCOMPtr<nsIURI> url;
        rv = mCurrentPrototype->GetURI(getter_AddRefs(url));
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString urlspec;
        rv = url->GetSpec(getter_Copies(urlspec));
        if (NS_FAILED(rv)) return rv;

        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: error parsing '%s'", (const char*) urlspec));
#endif

        return NS_OK;
    }

    // Do one-time initialization if we're preparing to walk the
    // master document's prototype.
    nsCOMPtr<nsIContent> root;

    if (mState == eState_Master) {
        rv = CreateElement(proto, getter_AddRefs(root));
        if (NS_FAILED(rv)) return rv;

        SetRootContent(root);

        // Add the root element to the XUL document's ID-to-element map.
        rv = AddElementToMap(root);
        if (NS_FAILED(rv)) return rv;

        // Add a dummy channel to the load group as a placeholder for the document
        // load
        rv = PlaceHolderRequest::Create(getter_AddRefs(mPlaceHolderRequest));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

        if (group) {
            nsCOMPtr<nsIChannel> channel = do_QueryInterface(mPlaceHolderRequest);
            rv = channel->SetLoadGroup(group);
            if (NS_FAILED(rv)) return rv;
            rv = group->AddRequest(mPlaceHolderRequest, nsnull);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // There'd better not be anything on the context stack at this
    // point! This is the basis case for our "induction" in
    // ResumeWalk(), below, which'll assume that there's always a
    // content element on the context stack if either 1) we're in the
    // "master" document, or 2) we're in an overlay, and we've got
    // more than one prototype element (the single, root "overlay"
    // element) on the stack.
    NS_ASSERTION(mContextStack.Depth() == 0, "something's on the context stack already");
    if (mContextStack.Depth() != 0)
        return NS_ERROR_UNEXPECTED;

    rv = mContextStack.Push(proto, root);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
nsXULDocument::AddChromeOverlays()
{
    nsresult rv;
    nsCOMPtr<nsIChromeRegistry> reg(do_GetService(kChromeRegistryCID, &rv));

    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsISimpleEnumerator> oe;

    {
        nsCOMPtr<nsIURI> uri;
        rv = mCurrentPrototype->GetURI(getter_AddRefs(uri));
        if (NS_FAILED(rv)) return rv;

        reg->GetOverlays(uri, getter_AddRefs(oe));
    }

    if (!oe)
        return NS_OK;

    PRBool moreElements;
    oe->HasMoreElements(&moreElements);

    while (moreElements) {
        nsCOMPtr<nsISupports> next;
        oe->GetNext(getter_AddRefs(next));
        if (!next)
            return NS_OK;

        nsCOMPtr<nsIURI> uri = do_QueryInterface(next);
        if (!uri)
            return NS_OK;

        mUnloadedOverlays->AppendElement(uri);

        oe->HasMoreElements(&moreElements);
    }

    return NS_OK;
}

nsresult
nsXULDocument::ResumeWalk()
{
    // Walk the prototype and build the delegate content model. The
    // walk is performed in a top-down, left-to-right fashion. That
    // is, a parent is built before any of its children; a node is
    // only built after all of its siblings to the left are fully
    // constructed.
    //
    // It is interruptable so that transcluded documents (e.g.,
    // <html:script src="..." />) can be properly re-loaded if the
    // cached copy of the document becomes stale.
    nsresult rv;

    while (1) {
        // Begin (or resume) walking the current prototype.

        while (mContextStack.Depth() > 0) {
            // Look at the top of the stack to determine what we're
            // currently working on.
            nsXULPrototypeElement* proto;
            nsCOMPtr<nsIContent> element;
            PRInt32 indx;
            rv = mContextStack.Peek(&proto, getter_AddRefs(element), &indx);
            if (NS_FAILED(rv)) return rv;

            if (indx >= proto->mNumChildren) {
                // We've processed all of the prototype's children. If
                // we're in the master prototype, do post-order
                // document-level hookup. (An overlay will get its
                // document hookup done when it's successfully
                // resolved.)
                if (element && (mState == eState_Master))
                    AddElementToDocumentPost(element);

                // Now pop the context stack back up to the parent
                // element and continue the prototype walk.
                mContextStack.Pop();
                continue;
            }

            // Grab the next child, and advance the current context stack
            // to the next sibling to our right.
            nsXULPrototypeNode* childproto = proto->mChildren[indx];
            mContextStack.SetTopIndex(++indx);

            switch (childproto->mType) {
            case nsXULPrototypeNode::eType_Element: {
                // An 'element', which may contain more content.
                nsXULPrototypeElement* protoele =
                    NS_REINTERPRET_CAST(nsXULPrototypeElement*, childproto);

                nsCOMPtr<nsIContent> child;

                if ((mState == eState_Master) || (mContextStack.Depth() > 1)) {
                    // We're in the master document -or -we're in an
                    // overlay, and far enough down into the overlay's
                    // content that we can simply build the delegates
                    // and attach them to the parent node.
                    NS_ASSERTION(element != nsnull, "no element on context stack");

                    rv = CreateElement(protoele, getter_AddRefs(child));
                    if (NS_FAILED(rv)) return rv;

                    // ...and append it to the content model.
                    rv = element->AppendChildTo(child, PR_FALSE, PR_FALSE);
                    if (NS_FAILED(rv)) return rv;

                    // do pre-order document-level hookup, but only if
                    // we're in the master document. For an overlay,
                    // this will happen when the overlay is
                    // successfully resolved.
                    if (mState == eState_Master)
                        AddElementToDocumentPre(child);
                }
                else {
                    // We're in the "first ply" of an overlay: the
                    // "hookup" nodes. Create an 'overlay' element so
                    // that we can continue to build content, and
                    // enter a forward reference so we can hook it up
                    // later.
                    rv = CreateOverlayElement(protoele, getter_AddRefs(child));
                    if (NS_FAILED(rv)) return rv;
                }

                // If it has children, push the element onto the context
                // stack and begin to process them.
                if (protoele->mNumChildren > 0) {
                    rv = mContextStack.Push(protoele, child);
                    if (NS_FAILED(rv)) return rv;
                }
                else if (mState == eState_Master) {
                    // If there are no children, and we're in the
                    // master document, do post-order document hookup
                    // immediately.
                    AddElementToDocumentPost(child);
                }
            }
            break;

            case nsXULPrototypeNode::eType_Script: {
                // A script reference. Execute the script immediately;
                // this may have side effects in the content model.
                nsXULPrototypeScript* scriptproto =
                    NS_REINTERPRET_CAST(nsXULPrototypeScript*, childproto);

                if (scriptproto->mSrcURI) {
                    // A transcluded script reference; this may
                    // "block" our prototype walk if the script isn't
                    // cached, or the cached copy of the script is
                    // stale and must be reloaded.
                    PRBool blocked;
                    rv = LoadScript(scriptproto, &blocked);
                    if (NS_FAILED(rv)) return rv;

                    if (blocked)
                        return NS_OK;
                }
                else if (scriptproto->mJSObject) {
                    // An inline script
                    rv = ExecuteScript(scriptproto->mJSObject);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            break;

            case nsXULPrototypeNode::eType_Text: {
                // A simple text node.

                if ((mState == eState_Master) || (mContextStack.Depth() > 1)) {
                    // We're in the master document -or -we're in an
                    // overlay, and far enough down into the overlay's
                    // content that we can simply build the delegates
                    // and attach them to the parent node.
                    NS_ASSERTION(element != nsnull, "no element on context stack");

                    nsCOMPtr<nsITextContent> text;
                    rv = nsComponentManager::CreateInstance(kTextNodeCID,
                                                            nsnull,
                                                            NS_GET_IID(nsITextContent),
                                                            getter_AddRefs(text));
                    if (NS_FAILED(rv)) return rv;
                    nsXULPrototypeText* textproto =
                        NS_REINTERPRET_CAST(nsXULPrototypeText*, childproto);
                    rv = text->SetText(textproto->mValue.get(),
                                       textproto->mValue.Length(),
                                       PR_FALSE);

                    if (NS_FAILED(rv)) return rv;

                    nsCOMPtr<nsIContent> child = do_QueryInterface(text);
                    if (! child)
                        return NS_ERROR_UNEXPECTED;

                    rv = element->AppendChildTo(child, PR_FALSE, PR_FALSE);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            break;
        }
        }

        // Once we get here, the context stack will have been
        // depleted. That means that the entire prototype has been
        // walked and content has been constructed.

        // If we're not already, mark us as now processing overlays.
        mState = eState_Overlay;

        PRUint32 count;
        mUnloadedOverlays->Count(&count);

        // If there are no overlay URIs, then we're done.
        if (! count)
            break;

        nsCOMPtr<nsIURI> uri =
            dont_AddRef(NS_REINTERPRET_CAST(nsIURI*, mUnloadedOverlays->ElementAt(count - 1)));

        mUnloadedOverlays->RemoveElementAt(count - 1);

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
            nsXPIDLCString urlspec;
            uri->GetSpec(getter_Copies(urlspec));

            PR_LOG(gXULLog, PR_LOG_DEBUG,
                   ("xul: loading overlay %s", (const char*) urlspec));
        }
#endif
        // Look in the prototype cache for the prototype document with
        // the specified overlay URI.
        rv = gXULCache->GetPrototype(uri, getter_AddRefs(mCurrentPrototype));
        if (NS_FAILED(rv)) return rv;

        PRBool cache;
        gXULCache->GetEnabled(&cache);

        if (cache && mCurrentPrototype) {
            NS_ASSERTION(IsChromeURI(uri), "XUL cache hit on non-chrome URI?");
            PRBool loaded;
            rv = mCurrentPrototype->AwaitLoadDone(this, &loaded);
            if (NS_FAILED(rv)) return rv;

            if (! loaded) {
                // Return to the main event loop and eagerly await the
                // prototype overlay load's completion. When the content
                // sink completes, it will trigger an EndLoad(), which'll
                // wind us back up here, in ResumeWalk().
                return NS_OK;
            }

            // Found the overlay's prototype in the cache, fully loaded.
            rv = AddPrototypeSheets();
            if (NS_FAILED(rv)) return rv;

            // Now prepare to walk the prototype to create its content
            rv = PrepareToWalk();
            if (NS_FAILED(rv)) return rv;

            PR_LOG(gXULLog, PR_LOG_DEBUG, ("xul: overlay was cached"));
        }
        else {
            // Not there. Initiate a load.
            PR_LOG(gXULLog, PR_LOG_DEBUG, ("xul: overlay was not cached"));

            nsCOMPtr<nsIParser> parser;
            rv = PrepareToLoadPrototype(uri, "view", nsnull, getter_AddRefs(parser));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser);
            if (! listener)
                return NS_ERROR_UNEXPECTED;

            // Add an observer to the parser; this'll get called when
            // Necko fires its On[Start|Stop]Request() notifications,
            // and will let us recover from a missing overlay.
            ParserObserver* parserObserver = new ParserObserver(this);
            if (! parserObserver)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(parserObserver);
            parser->Parse(uri, parserObserver);
            NS_RELEASE(parserObserver);

            nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
            rv = NS_OpenURI(listener, nsnull, uri, nsnull, group);
            if (NS_FAILED(rv)) return rv;

            // If it's a 'chrome:' prototype document, then put it into
            // the prototype cache; other XUL documents will be reloaded
            // each time.  We must do this after NS_OpenURI and AsyncOpen,
            // or chrome code will wrongly create a cached chrome channel
            // instead of a real one.
            if (cache && IsChromeURI(uri)) {
                rv = gXULCache->PutPrototype(mCurrentPrototype);
                if (NS_FAILED(rv)) return rv;
            }

            // Return to the main event loop and eagerly await the
            // overlay load's completion. When the content sink
            // completes, it will trigger an EndLoad(), which'll wind
            // us back up here, in ResumeWalk().
            return NS_OK;
        }
    }

    // If we get here, there is nothing left for us to walk. The content
    // model is built and ready for layout.
    rv = ResolveForwardReferences();
    if (NS_FAILED(rv)) return rv;

    rv = ApplyPersistentAttributes();
    if (NS_FAILED(rv)) return rv;

    // Everything after this point we only want to do once we're
    // certain that we've been embedded in a presentation shell.

    StartLayout();

    // Since we've bothered to load and parse all this fancy XUL, let's try to
    // save a condensed serialization of it for faster loading next time.  We
    // do this after StartLayout() in case we want to serialize frames.
    EndFastLoad();

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->EndLoad(this);
    }

    // Remove the placeholder channel; if we're the last channel in the
    // load group, this will fire the OnEndDocumentLoad() method in the
    // docshell, and run the onload handlers, etc.
    nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
    if (group) {
        rv = group->RemoveRequest(mPlaceHolderRequest, nsnull, NS_OK);
        if (NS_FAILED(rv)) return rv;

        mPlaceHolderRequest = nsnull;
    }
    return rv;
}


nsresult
nsXULDocument::LoadScript(nsXULPrototypeScript* aScriptProto, PRBool* aBlock)
{
    // Load a transcluded script
    nsresult rv;

    if (aScriptProto->mJSObject) {
        rv = ExecuteScript(aScriptProto->mJSObject);

        // Ignore return value from execution, and don't block
        *aBlock = PR_FALSE;
        return NS_OK;
    }

    // Try the XUL script cache, in case two XUL documents source the same
    // .js file (e.g., strres.js from navigator.xul and utilityOverlay.xul).
    // XXXbe the cache relies on aScriptProto's GC root!
    PRBool useXULCache;
    gXULCache->GetEnabled(&useXULCache);

    if (useXULCache) {
        gXULCache->GetScript(aScriptProto->mSrcURI,
                             NS_REINTERPRET_CAST(void**, &aScriptProto->mJSObject));

        if (aScriptProto->mJSObject) {
            rv = ExecuteScript(aScriptProto->mJSObject);

            // Ignore return value from execution, and don't block
            *aBlock = PR_FALSE;
            return NS_OK;
        }
    }

    // Set the current script prototype so that OnStreamComplete can report
    // the right file if there are errors in the script.
    NS_ASSERTION(!mCurrentScriptProto,
                 "still loading a script when starting another load?");
    mCurrentScriptProto = aScriptProto;

    if (aScriptProto->mSrcLoading) {
        // Another XULDocument load has started, which is still in progress.
        // Remember to ResumeWalk this document when the load completes.
        mNextSrcLoadWaiter = aScriptProto->mSrcLoadWaiters;
        aScriptProto->mSrcLoadWaiters = this;
        NS_ADDREF_THIS();
    }
    else {
        // Set mSrcLoading *before* calling NS_NewStreamLoader, in case the
        // stream completes (probably due to an error) within the activation
        // of NS_NewStreamLoader.
        aScriptProto->mSrcLoading = PR_TRUE;

        nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

        // N.B., the loader will be released in OnStreamComplete
        nsIStreamLoader* loader;
        rv = NS_NewStreamLoader(&loader, aScriptProto->mSrcURI, this, nsnull, group);
        if (NS_FAILED(rv)) return rv;
    }

    // Block until OnStreamComplete resumes us.
    *aBlock = PR_TRUE;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::OnStreamComplete(nsIStreamLoader* aLoader,
                                nsISupports* context,
                                nsresult aStatus,
                                PRUint32 stringLen,
                                const char* string)
{
    // print a load error on bad status
    if (NS_FAILED(aStatus)) {
        nsCOMPtr<nsIRequest> request;
        aLoader->GetRequest(getter_AddRefs(request));
        nsCOMPtr<nsIChannel> channel;
        channel = do_QueryInterface(request);
        if (channel) {
            nsCOMPtr<nsIURI> uri;
            channel->GetURI(getter_AddRefs(uri));
            if (uri) {
                nsXPIDLCString uriSpec;
                uri->GetSpec(getter_Copies(uriSpec));
                printf("Failed to load %s\n",
                       uriSpec.get() ? (const char*) uriSpec : "");
            }
        }
    }

    // This is the completion routine that will be called when a
    // transcluded script completes. Compile and execute the script
    // if the load was successful, then continue building content
    // from the prototype.
    nsresult rv;

    NS_ASSERTION(mCurrentScriptProto && mCurrentScriptProto->mSrcLoading,
                 "script source not loading on unichar stream complete?");

    // Clear mCurrentScriptProto now, but save it first for use below in
    // the compile/execute code, and in the while loop that resumes walks
    // of other documents that raced to load this script
    nsXULPrototypeScript* scriptProto = mCurrentScriptProto;
    mCurrentScriptProto = nsnull;

    // Clear the prototype's loading flag before executing the script or
    // resuming document walks, in case any of those control flows starts a
    // new script load.
    scriptProto->mSrcLoading = PR_FALSE;

    if (NS_SUCCEEDED(aStatus)) {
        // If the including XUL document is a FastLoad document, and we're
        // compiling an out-of-line script (one with src=...), then we must
        // be writing a new FastLoad file.  If we were reading this script
        // from the FastLoad file, XULContentSinkImpl::OpenScript (over in
        // nsXULContentSink.cpp) would have already deserialized a non-null
        // script->mJSObject, causing control flow at the top of LoadScript
        // not to reach here.
        //
        // Start and Select the .js document in the FastLoad multiplexor
        // before serializing script data under scriptProto->Compile, and
        // End muxing afterward.
        nsCOMPtr<nsIURI> uri = scriptProto->mSrcURI;
        if (mIsFastLoad) {
            nsXPIDLCString urispec;
            uri->GetSpec(getter_Copies(urispec));
            rv = gFastLoadService->StartMuxedDocument(uri, urispec,
                                          nsIFastLoadService::NS_FASTLOAD_WRITE);
            NS_ASSERTION(rv != NS_ERROR_NOT_AVAILABLE, "reading FastLoad?!");
            if (NS_SUCCEEDED(rv))
                gFastLoadService->SelectMuxedDocument(uri);
        }

        nsString stringStr; stringStr.AssignWithConversion(string, stringLen);
        rv = scriptProto->Compile(stringStr.get(), stringLen, uri, 1, this,
                                  mMasterPrototype);

        // End muxing the .js file into the FastLoad file.  We don't Abort
        // the FastLoad process here, when writing, as we do when reading.
        // XXXbe maybe we should...
        // NB: we don't need to Select mDocumentURL again, because scripts
        // load after their including prototype document has fully loaded.
        if (mIsFastLoad)
            gFastLoadService->EndMuxedDocument(uri);

        aStatus = rv;
        if (NS_SUCCEEDED(rv) && scriptProto->mJSObject) {
            rv = ExecuteScript(scriptProto->mJSObject);

            // If the XUL cache is enabled, save the script object there in
            // case different XUL documents source the same script.
            // 
            // But don't save the script in the cache unless the master XUL
            // document URL is a chrome: URL.  It is valid for a URL such as
            // about:config to translate into a master document URL, whose
            // prototype document nodes -- including prototype scripts that
            // hold GC roots protecting their mJSObject pointers -- are not
            // cached in the XUL prototype cache.  See StartDocumentLoad,
            // the fillXULCache logic.
            //
            // A document such as about:config is free to load a script via
            // a URL such as chrome://global/content/config.js, and we must
            // not cache that script object without a prototype cache entry
            // containing a companion nsXULPrototypeScript node that owns a
            // GC root protecting the script object.  Otherwise, the script
            // cache entry will dangle once uncached prototype document is
            // released when its owning nsXULDocument is unloaded.
            //
            // (See http://bugzilla.mozilla.org/show_bug.cgi?id=98207 for
            // the true crime story.)
            PRBool useXULCache;
            gXULCache->GetEnabled(&useXULCache);

            if (useXULCache && IsChromeURI(mDocumentURL)) {
                gXULCache->PutScript(scriptProto->mSrcURI,
                                     NS_REINTERPRET_CAST(void*, scriptProto->mJSObject));
            }
        }
        // ignore any evaluation errors
    }

    // balance the addref we added in LoadScript()
    NS_RELEASE(aLoader);

    rv = ResumeWalk();

    // Load a pointer to the prototype-script's list of nsXULDocuments who
    // raced to load the same script
    nsXULDocument** docp = &scriptProto->mSrcLoadWaiters;

    // Resume walking other documents that waited for this one's load, first
    // executing the script we just compiled, in each doc's script context
    nsXULDocument* doc;
    while ((doc = *docp) != nsnull) {
        NS_ASSERTION(doc->mCurrentScriptProto == scriptProto,
                     "waiting for wrong script to load?");
        doc->mCurrentScriptProto = nsnull;

        // Unlink doc from scriptProto's list before executing and resuming
        *docp = doc->mNextSrcLoadWaiter;
        doc->mNextSrcLoadWaiter = nsnull;

        // Execute only if we loaded and compiled successfully, then resume
        if (NS_SUCCEEDED(aStatus) && scriptProto->mJSObject) {
            doc->ExecuteScript(scriptProto->mJSObject);
        }
        doc->ResumeWalk();
        NS_RELEASE(doc);
    }

    return rv;
}


nsresult
nsXULDocument::ExecuteScript(JSObject* aScriptObject)
{
    NS_PRECONDITION(aScriptObject != nsnull, "null ptr");
    if (! aScriptObject)
        return NS_ERROR_NULL_POINTER;

    // Execute the precompiled script with the given version
    nsresult rv;

    NS_ASSERTION(mScriptGlobalObject != nsnull, "no script global object");
    if (! mScriptGlobalObject)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIScriptContext> context;
    rv = mScriptGlobalObject->GetContext(getter_AddRefs(context));
    if (NS_FAILED(rv)) return rv;

    if (! context) return NS_ERROR_UNEXPECTED;

    rv = context->ExecuteScript(aScriptObject, nsnull, nsnull, nsnull);
    return rv;
}


nsresult
nsXULDocument::CreateElement(nsXULPrototypeElement* aPrototype, nsIContent** aResult)
{
    // Create a content model element from a prototype element.
    NS_PRECONDITION(aPrototype != nsnull, "null ptr");
    if (! aPrototype)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_OK;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_ALWAYS)) {
        nsAutoString tagstr;
        aPrototype->mNodeInfo->GetQualifiedName(tagstr);

        nsCAutoString tagstrC;
        tagstrC.AssignWithConversion(tagstr);
        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: creating <%s> from prototype",
                (const char*) tagstrC));
    }
#endif

    nsCOMPtr<nsIContent> result;

    if (aPrototype->mNodeInfo->NamespaceEquals(kNameSpaceID_HTML)) {
        // If it's an HTML element, it's gonna be heavyweight no matter
        // what. So we need to copy everything out of the prototype
        // into the element.

        gHTMLElementFactory->CreateInstanceByTag(aPrototype->mNodeInfo,
                                                 getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;

        rv = result->SetDocument(this, PR_FALSE, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

        rv = AddAttributes(aPrototype, result);
        if (NS_FAILED(rv)) return rv;
    }
    else if (!aPrototype->mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
        // If it's not a XUL element, it's gonna be heavyweight no matter
        // what. So we need to copy everything out of the prototype
        // into the element.

        PRInt32 namespaceID;
        aPrototype->mNodeInfo->GetNamespaceID(namespaceID);

        nsCOMPtr<nsIElementFactory> elementFactory;
        GetElementFactory(namespaceID,
                          getter_AddRefs(elementFactory));
        rv = elementFactory->CreateInstanceByTag(aPrototype->mNodeInfo,
                                                 getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;

        rv = result->SetDocument(this, PR_FALSE, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

        rv = AddAttributes(aPrototype, result);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // If it's a XUL element, it'll be lightweight until somebody
        // monkeys with it.
        rv = nsXULElement::Create(aPrototype, this, PR_TRUE, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }

    result->SetContentID(mNextContentID++);

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
nsXULDocument::CreateOverlayElement(nsXULPrototypeElement* aPrototype, nsIContent** aResult)
{
    nsresult rv;

    // This doesn't really do anything except create a placeholder
    // element. I'd use an XML element, but it gets its knickers in a
    // knot with DOM ranges when you try to remove its children.
    nsCOMPtr<nsIContent> element;
    rv = nsXULElement::Create(aPrototype, this, PR_FALSE, getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    OverlayForwardReference* fwdref = new OverlayForwardReference(this, element);
    if (! fwdref)
        return NS_ERROR_OUT_OF_MEMORY;

    // transferring ownership to ya...
    rv = AddForwardReference(fwdref);
    if (NS_FAILED(rv)) return rv;

    *aResult = element;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsresult
nsXULDocument::AddAttributes(nsXULPrototypeElement* aPrototype, nsIContent* aElement)
{
    nsresult rv;

    for (PRInt32 i = 0; i < aPrototype->mNumAttributes; ++i) {
        nsXULPrototypeAttribute* protoattr = &(aPrototype->mAttributes[i]);
        nsAutoString  valueStr;
        protoattr->mValue.GetValue( valueStr );

        rv = aElement->SetAttr(protoattr->mNodeInfo,
                               valueStr,
                               PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
nsXULDocument::CheckTemplateBuilder(nsIContent* aElement)
{
    // Check aElement for a 'datasources' attribute, and if it has
    // one, create and initialize a template builder.
    nsresult rv;

    // See if the element already has a `database' attribute. If it
    // does, then the template builder has already been created.
    //
    // XXX this approach will crash and burn (well, maybe not _that_
    // bad) if aElement is not a XUL element.
    nsCOMPtr<nsIDOMXULElement> xulele = do_QueryInterface(aElement);
    if (xulele) {
        nsCOMPtr<nsIRDFCompositeDataSource> ds;
        xulele->GetDatabase(getter_AddRefs(ds));
        if (ds)
            return NS_OK;
    }

    nsAutoString datasources;
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::datasources, datasources);
    if (NS_FAILED(rv)) return rv;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE)
        return NS_OK;

    // Get the document and its URL
    nsCOMPtr<nsIDocument> doc;
    aElement->GetDocument(*getter_AddRefs(doc));
    NS_ASSERTION(doc != nsnull, "no document");
    if (! doc)
        return NS_ERROR_UNEXPECTED;

    // construct a new builder
    PRInt32 nameSpaceID = 0;
    nsCOMPtr<nsIAtom> baseTag;

    nsCOMPtr<nsIXBLService> xblService = do_GetService("@mozilla.org/xbl;1");
    if (xblService)
        xblService->ResolveTag(aElement, &nameSpaceID, getter_AddRefs(baseTag));

    if ((nameSpaceID == kNameSpaceID_XUL) &&
        (baseTag.get() == nsXULAtoms::outlinerbody)) {
        nsCOMPtr<nsIXULTemplateBuilder> builder =
            do_CreateInstance("@mozilla.org/xul/xul-outliner-builder;1");

        if (! builder)
            return NS_ERROR_FAILURE;

        // Because the outliner box object won't be created until the
        // frame is available, we need to tuck the template builder
        // away in the binding manager so there's at least one
        // reference to it.
        nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(doc);
        if (xuldoc)
            xuldoc->SetTemplateBuilderFor(aElement, builder);
    }
    else {
        nsCOMPtr<nsIRDFContentModelBuilder> builder
            = do_CreateInstance("@mozilla.org/xul/xul-template-builder;1");

        if (! builder)
            return NS_ERROR_FAILURE;

        builder->SetRootContent(aElement);

        nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
        if (xulcontent) {
            // Mark the XUL element as being lazy, so the template builder
            // will run when layout first asks for these nodes.
            xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
        }
        else {
            // Force construction of immediate template sub-content _now_.
            builder->CreateContents(aElement);
        }
    }

    return NS_OK;
}


nsresult
nsXULDocument::AddPrototypeSheets()
{
    // Add mCurrentPrototype's style sheets to the document.
    nsresult rv;

    nsCOMPtr<nsISupportsArray> sheets;
    rv = mCurrentPrototype->GetStyleSheetReferences(getter_AddRefs(sheets));
    if (NS_FAILED(rv)) return rv;

    PRUint32 count;
    sheets->Count(&count);
    for (PRUint32 i = 0; i < count; ++i) {
        nsISupports* isupports = sheets->ElementAt(i);
        nsCOMPtr<nsIURI> uri = do_QueryInterface(isupports);
        NS_IF_RELEASE(isupports);

        NS_ASSERTION(uri != nsnull, "not a URI!!!");
        if (! uri)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsICSSStyleSheet> sheet;
        rv = gXULCache->GetStyleSheet(uri, getter_AddRefs(sheet));
        if (NS_FAILED(rv)) return rv;

        // If we don't get a style sheet from the cache, then the
        // really rigorous thing to do here would be to go out and try
        // to load it again. (This would allow us to do partial
        // invalidation of the cache, which would be cool, but would
        // also require some more thinking.)
        //
        // Reality is, we end up in this situation if, when parsing
        // the original XUL document, there -was- no style sheet at
        // the specified URL, or the stylesheet was empty. So, just
        // skip it.
        if (! sheet)
            continue;

        nsCOMPtr<nsICSSStyleSheet> newsheet;
        rv = sheet->Clone(*getter_AddRefs(newsheet));
        if (NS_FAILED(rv)) return rv;

        AddStyleSheet(newsheet);
    }

    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsXULDocument::OverlayForwardReference
//

nsForwardReference::Result
nsXULDocument::OverlayForwardReference::Resolve()
{
    // Resolve a forward reference from an overlay element; attempt to
    // hook it up into the main document.
    nsresult rv;

    nsAutoString id;
    rv = mOverlay->GetAttr(kNameSpaceID_None, nsXULAtoms::id, id);
    if (NS_FAILED(rv)) return eResolve_Error;

    nsCOMPtr<nsIDOMElement> domtarget;
    rv = mDocument->GetElementById(id, getter_AddRefs(domtarget));
    if (NS_FAILED(rv)) return eResolve_Error;

    // If we can't find the element in the document, defer the hookup
    // until later.
    if (! domtarget)
        return eResolve_Later;

    nsCOMPtr<nsIContent> target = do_QueryInterface(domtarget);
    NS_ASSERTION(target != nsnull, "not an nsIContent");
    if (! target)
        return eResolve_Error;

    rv = Merge(target, mOverlay);
    if (NS_FAILED(rv)) return eResolve_Error;

    // Add child and any descendants to the element map
    rv = mDocument->AddSubtreeToDocument(target);
    if (NS_FAILED(rv)) return eResolve_Error;

    nsCAutoString idC;
    idC.AssignWithConversion(id);
    PR_LOG(gXULLog, PR_LOG_ALWAYS,
           ("xul: overlay resolved '%s'",
            (const char*) idC));

    mResolved = PR_TRUE;
    return eResolve_Succeeded;
}



nsresult
nsXULDocument::OverlayForwardReference::Merge(nsIContent* aTargetNode,
                                              nsIContent* aOverlayNode)
{
    // This function is given:
    // aTargetNode:  the node in the document whose 'id' attribute
    //               matches a toplevel node in our overlay.
    // aOverlayNode: the node in the overlay document that matches
    //               a node in the actual document.
    //
    // This function merges the tree from the overlay into the tree in
    // the document, overwriting attributes and appending child content
    // nodes appropriately. (See XUL overlay reference for details)

    nsresult rv;

    // Merge attributes from the overlay content node to that of the
    // actual document.
    PRInt32 attrCount, i;
    rv = aOverlayNode->GetAttrCount(attrCount);
    if (NS_FAILED(rv)) return rv;

    for (i = 0; i < attrCount; ++i) {
        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> attr, prefix;
        rv = aOverlayNode->GetAttrNameAt(i, nameSpaceID, *getter_AddRefs(attr), *getter_AddRefs(prefix));
        if (NS_FAILED(rv)) return rv;

        // We don't want to swap IDs, they should be the same.
        if (nameSpaceID == kNameSpaceID_None && attr.get() == nsXULAtoms::id)
            continue;

        nsAutoString value;
        rv = aOverlayNode->GetAttr(nameSpaceID, attr, value);
        if (NS_FAILED(rv)) return rv;

        nsAutoString tempID;
        rv = aOverlayNode->GetAttr(kNameSpaceID_None, nsXULAtoms::id, tempID);

        // Element in the overlay has the 'removeelement' attribute set
        // so remove it from the actual document.
        if (attr.get() == nsXULAtoms::removeelement &&
            value.EqualsIgnoreCase("true")) {
            nsCOMPtr<nsIContent> parent;
            rv = aTargetNode->GetParent(*getter_AddRefs(parent));
            if (NS_FAILED(rv)) return rv;

            rv = RemoveElement(parent, aTargetNode);
            if (NS_FAILED(rv)) return rv;

            return NS_OK;
        }

        nsCOMPtr<nsINodeInfo> ni;
        aTargetNode->GetNodeInfo(*getter_AddRefs(ni));

        if (ni) {
            nsCOMPtr<nsINodeInfoManager> nimgr;
            ni->GetNodeInfoManager(*getter_AddRefs(nimgr));

            nimgr->GetNodeInfo(attr, prefix, nameSpaceID,
                               *getter_AddRefs(ni));
        }

        rv = aTargetNode->SetAttr(ni, value, PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }


    // Walk our child nodes, looking for elements that have the 'id'
    // attribute set. If we find any, we must do a parent check in the
    // actual document to ensure that the structure matches that of
    // the actual document. If it does, we can call ourselves and attempt
    // to merge inside that subtree. If not, we just append the tree to
    // the parent like any other.

    PRInt32 childCount;
    rv = aOverlayNode->ChildCount(childCount);
    if (NS_FAILED(rv)) return rv;

    for (i = 0; i < childCount; i++) {
        nsCOMPtr<nsIContent> currContent;
        rv = aOverlayNode->ChildAt(0, *getter_AddRefs(currContent));
        if (NS_FAILED(rv)) return rv;

        nsAutoString id;
        rv = currContent->GetAttr(kNameSpaceID_None, nsXULAtoms::id, id);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIDOMElement> nodeInDocument;
        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            nsCOMPtr<nsIDocument> document;
            rv = aTargetNode->GetDocument(*getter_AddRefs(document));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIDOMDocument> domDocument(do_QueryInterface(document));
            if (!domDocument) return NS_ERROR_FAILURE;

            rv = domDocument->GetElementById(id, getter_AddRefs(nodeInDocument));
            if (NS_FAILED(rv)) return rv;
        }

        // The item has an 'id' attribute set, and we need to check with
        // the actual document to see if an item with this id exists at
        // this locale. If so, we want to merge the subtree under that
        // node. Otherwise, we just do an append as if the element had
        // no id attribute.
        if (nodeInDocument) {
            // Given two parents, aTargetNode and aOverlayNode, we want
            // to call merge on currContent if we find an associated
            // node in the document with the same id as currContent that
            // also has aTargetNode as its parent.

            nsAutoString documentParentID;
            rv = aTargetNode->GetAttr(kNameSpaceID_None, nsXULAtoms::id,
                                      documentParentID);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIDOMNode> nodeParent;
            rv = nodeInDocument->GetParentNode(getter_AddRefs(nodeParent));
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsIDOMElement> elementParent(do_QueryInterface(nodeParent));

            nsAutoString parentID;
            elementParent->GetAttribute(NS_LITERAL_STRING("id"), parentID);
            if (parentID.Equals(documentParentID)) {
                // The element matches. "Go Deep!"
                nsCOMPtr<nsIContent> childDocumentContent(do_QueryInterface(nodeInDocument));
                rv = Merge(childDocumentContent, currContent);
                if (NS_FAILED(rv)) return rv;
                rv = aOverlayNode->RemoveChildAt(0, PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                continue;
            }
        }

        rv = aOverlayNode->RemoveChildAt(0, PR_FALSE);
        if (NS_FAILED(rv)) return rv;

        rv = InsertElement(aTargetNode, currContent);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}



nsXULDocument::OverlayForwardReference::~OverlayForwardReference()
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_ALWAYS) && !mResolved) {
        nsAutoString id;
        mOverlay->GetAttr(kNameSpaceID_None, nsXULAtoms::id, id);

        nsCAutoString idC;
        idC.AssignWithConversion(id);
        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: overlay failed to resolve '%s'",
                (const char*) idC));
    }
#endif
}


//----------------------------------------------------------------------
//
// nsXULDocument::BroadcasterHookup
//

nsForwardReference::Result
nsXULDocument::BroadcasterHookup::Resolve()
{
    nsresult rv;

    PRBool listener;
    rv = CheckBroadcasterHookup(mDocument, mObservesElement, &listener, &mResolved);
    if (NS_FAILED(rv)) return eResolve_Error;

    return mResolved ? eResolve_Succeeded : eResolve_Later;
}


nsXULDocument::BroadcasterHookup::~BroadcasterHookup()
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_ALWAYS) && !mResolved) {
        // Tell the world we failed
        nsresult rv;

        nsCOMPtr<nsIAtom> tag;
        rv = mObservesElement->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return;

        nsAutoString broadcasterID;
        nsAutoString attribute;

        if (tag.get() == nsXULAtoms::observes) {
            rv = mObservesElement->GetAttr(kNameSpaceID_None, nsXULAtoms::element, broadcasterID);
            if (NS_FAILED(rv)) return;

            rv = mObservesElement->GetAttr(kNameSpaceID_None, nsXULAtoms::attribute, attribute);
            if (NS_FAILED(rv)) return;
        }
        else {
            rv = mObservesElement->GetAttr(kNameSpaceID_None, nsXULAtoms::observes, broadcasterID);
            if (NS_FAILED(rv)) return;

            attribute.Assign(NS_LITERAL_STRING("*"));
        }

        nsAutoString tagStr;
        rv = tag->ToString(tagStr);
        if (NS_FAILED(rv)) return;

        nsCAutoString tagstrC, attributeC,broadcasteridC;
        tagstrC.AssignWithConversion(tagStr);
        attributeC.AssignWithConversion(attribute);
        broadcasteridC.AssignWithConversion(broadcasterID);
        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: broadcaster hookup failed <%s attribute='%s'> to %s",
                (const char*) tagstrC,
                (const char*) attributeC,
                (const char*) broadcasteridC));
    }
#endif
}


//----------------------------------------------------------------------


nsresult
nsXULDocument::CheckBroadcasterHookup(nsXULDocument* aDocument,
                                      nsIContent* aElement,
                                      PRBool* aNeedsHookup,
                                      PRBool* aDidResolve)
{
    // Resolve a broadcaster hookup. Look at the element that we're
    // trying to resolve: it could be an '<observes>' element, or just
    // a vanilla element with an 'observes' attribute on it.
    nsresult rv;

    *aDidResolve = PR_FALSE;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMElement> listener;
    nsAutoString broadcasterID;
    nsAutoString attribute;

    if ((nameSpaceID == kNameSpaceID_XUL) && (tag.get() == nsXULAtoms::observes)) {
        // It's an <observes> element, which means that the actual
        // listener is the _parent_ node. This element should have an
        // 'element' attribute that specifies the ID of the
        // broadcaster element, and an 'attribute' element, which
        // specifies the name of the attribute to observe.
        nsCOMPtr<nsIContent> parent;
        rv = aElement->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIAtom> parentTag;
        rv = parent->GetTag(*getter_AddRefs(parentTag));
        if (NS_FAILED(rv)) return rv;

        // If we're still parented by an 'overlay' tag, then we haven't
        // made it into the real document yet. Defer hookup.
        if (parentTag.get() == nsXULAtoms::overlay) {
            *aNeedsHookup = PR_TRUE;
            return NS_OK;
        }

        listener = do_QueryInterface(parent);

        rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::element, broadcasterID);
        if (NS_FAILED(rv)) return rv;

        rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::attribute, attribute);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // It's a generic element, which means that we'll use the
        // value of the 'observes' attribute to determine the ID of
        // the broadcaster element, and we'll watch _all_ of its
        // values.
        rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::observes, broadcasterID);
        if (NS_FAILED(rv)) return rv;

        // Bail if there's no broadcasterID
        if ((rv != NS_CONTENT_ATTR_HAS_VALUE) || (broadcasterID.Length() == 0)) {
            // Try the command attribute next.
            rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::command, broadcasterID);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE && broadcasterID.Length() > 0) {
              // We've got something in the command attribute.  We only treat this as
              // a normal broadcaster if we are not a menuitem or a key.

              aElement->GetTag(*getter_AddRefs(tag));
              if (tag.get() == nsXULAtoms::menuitem || tag.get() == nsXULAtoms::key) {
                *aNeedsHookup = PR_FALSE;
                return NS_OK;
              }
            }
            else {
              *aNeedsHookup = PR_FALSE;
              return NS_OK;
            }
        }

        listener = do_QueryInterface(aElement);

        attribute.Assign(NS_LITERAL_STRING("*"));
    }

    // Make sure we got a valid listener.
    NS_ASSERTION(listener != nsnull, "no listener");
    if (! listener)
        return NS_ERROR_UNEXPECTED;

    // Try to find the broadcaster element in the document.
    nsCOMPtr<nsIDOMElement> broadcaster;
    rv = aDocument->GetElementById(broadcasterID, getter_AddRefs(broadcaster));
    if (NS_FAILED(rv)) return rv;

    // If we can't find the broadcaster, then we'll need to defer the
    // hookup. We may need to resolve some of the other overlays
    // first.
    if (! broadcaster) {
        *aNeedsHookup = PR_TRUE;
        return NS_OK;
    }

    rv = aDocument->AddBroadcastListenerFor(broadcaster, listener, attribute);
    if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
    // Tell the world we succeeded
    if (PR_LOG_TEST(gXULLog, PR_LOG_ALWAYS)) {
        nsCOMPtr<nsIContent> content =
            do_QueryInterface(listener);

        NS_ASSERTION(content != nsnull, "not an nsIContent");
        if (! content)
            return rv;

        nsCOMPtr<nsIAtom> tag2;
        rv = content->GetTag(*getter_AddRefs(tag2));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagStr;
        rv = tag2->ToString(tagStr);
        if (NS_FAILED(rv)) return rv;

        nsCAutoString tagstrC, attributeC,broadcasteridC;
        tagstrC.AssignWithConversion(tagStr);
        attributeC.AssignWithConversion(attribute);
        broadcasteridC.AssignWithConversion(broadcasterID);
        PR_LOG(gXULLog, PR_LOG_ALWAYS,
               ("xul: broadcaster hookup <%s attribute='%s'> to %s",
                (const char*) tagstrC,
                (const char*) attributeC,
                (const char*) broadcasteridC));
    }
#endif

    *aNeedsHookup = PR_FALSE;
    *aDidResolve = PR_TRUE;
    return NS_OK;
}

nsresult
nsXULDocument::InsertElement(nsIContent* aParent, nsIContent* aChild)
{
    // Insert aChild appropriately into aParent, accounting for a
    // 'pos' attribute set on aChild.
    nsresult rv;

    nsAutoString posStr;
    PRBool wasInserted = PR_FALSE;

    // insert after an element of a given id
    rv = aChild->GetAttr(kNameSpaceID_None, nsXULAtoms::insertafter, posStr);
    if (NS_FAILED(rv)) return rv;
    PRBool isInsertAfter = PR_TRUE;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
        rv = aChild->GetAttr(kNameSpaceID_None, nsXULAtoms::insertbefore, posStr);
        if (NS_FAILED(rv)) return rv;
        isInsertAfter = PR_FALSE;
    }

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        nsCOMPtr<nsIDocument> document;
        rv = aParent->GetDocument(*getter_AddRefs(document));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIDOMXULDocument> xulDocument(do_QueryInterface(document));
        nsCOMPtr<nsIDOMElement> domElement;

        char* str = ToNewCString(posStr);
        char* rest;
        char* token = nsCRT::strtok(str, ", ", &rest);

        while (token) {
            rv = xulDocument->GetElementById(NS_ConvertASCIItoUCS2(token), getter_AddRefs(domElement));
            if (domElement) break;

            token = nsCRT::strtok(rest, ", ", &rest);
        }
        nsMemory::Free(str);
        if (NS_FAILED(rv)) return rv;

        if (domElement) {
            nsCOMPtr<nsIContent> content(do_QueryInterface(domElement));
            NS_ASSERTION(content != nsnull, "null ptr");
            if (!content)
                return NS_ERROR_UNEXPECTED;

            PRInt32 pos;
            aParent->IndexOf(content, pos);

            if (pos != -1) {
                pos = isInsertAfter ? pos + 1 : pos;
                rv = aParent->InsertChildAt(aChild, pos, PR_FALSE, PR_TRUE);
                if (NS_FAILED(rv)) return rv;

                wasInserted = PR_TRUE;
            }
        }
    }

    if (!wasInserted) {

        rv = aChild->GetAttr(kNameSpaceID_None, nsXULAtoms::position, posStr);
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            // Positions are one-indexed.
            PRInt32 pos = posStr.ToInteger(NS_REINTERPRET_CAST(PRInt32*, &rv));
            if (NS_SUCCEEDED(rv)) {
                rv = aParent->InsertChildAt(aChild, pos - 1, PR_FALSE, PR_TRUE);
                if (NS_FAILED(rv)) return rv;

                wasInserted = PR_TRUE;
            }
        }
    }

    if (! wasInserted) {
        rv = aParent->AppendChildTo(aChild, PR_FALSE, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

nsresult
nsXULDocument::RemoveElement(nsIContent* aParent, nsIContent* aChild)
{
    nsresult rv;

    PRInt32 nodeOffset;
    rv = aParent->IndexOf(aChild, nodeOffset);
    if (NS_FAILED(rv)) return rv;

    rv = aParent->RemoveChildAt(nodeOffset, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

void 
nsXULDocument::GetElementFactory(PRInt32 aNameSpaceID,
                                 nsIElementFactory** aResult)
{
    // Retrieve the appropriate factory.
    gNameSpaceManager->GetElementFactory(aNameSpaceID, aResult);

    if (!*aResult) {
        // Nothing found. Use generic XML element.
        *aResult = gXMLElementFactory;
        NS_IF_ADDREF(*aResult);
    }
}

NS_IMETHODIMP
nsXULDocument::GetBoxObjectFor(nsIDOMElement* aElement, nsIBoxObject** aResult)
{
  nsresult rv;

  *aResult = nsnull;

  if (!mBoxObjectTable)
    mBoxObjectTable = new nsSupportsHashtable;
  else {
    nsISupportsKey key(aElement);
    nsCOMPtr<nsISupports> supports =
      getter_AddRefs(NS_STATIC_CAST(nsISupports*, mBoxObjectTable->Get(&key)));
    nsCOMPtr<nsIBoxObject> boxObject(do_QueryInterface(supports));
    if (boxObject) {
      *aResult = boxObject;
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  nsCOMPtr<nsIPresShell> shell;
  GetShellAt(0, getter_AddRefs(shell));
  if (!shell)
    return NS_ERROR_FAILURE;

  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> tag;
  nsCOMPtr<nsIXBLService> xblService =
           do_GetService("@mozilla.org/xbl;1", &rv);
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  xblService->ResolveTag(content, &namespaceID, getter_AddRefs(tag));

  nsCAutoString contractID("@mozilla.org/layout/xul-boxobject");
  if (namespaceID == kNameSpaceID_XUL) {
    if (tag.get() == nsXULAtoms::browser)
      contractID += "-browser";
    else if (tag.get() == nsXULAtoms::editor)
      contractID += "-editor";
    else if (tag.get() == nsXULAtoms::iframe)
      contractID += "-iframe";
    else if (tag.get() == nsXULAtoms::menu)
      contractID += "-menu";
    else if (tag.get() == nsXULAtoms::popup || tag.get() == nsXULAtoms::menupopup ||
             tag.get() == nsXULAtoms::tooltip)
      contractID += "-popup";
    else if (tag.get() == nsXULAtoms::tree)
      contractID += "-tree";
    else if (tag.get() == nsXULAtoms::scrollbox)
      contractID += "-scrollbox";
    else if (tag.get() == nsXULAtoms::outliner)
      contractID += "-outliner";
  }
  contractID += ";1";

  nsCOMPtr<nsIBoxObject> boxObject(do_CreateInstance(contractID));
  if (!boxObject)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsPIBoxObject> privateBox(do_QueryInterface(boxObject));
  if (NS_FAILED(rv = privateBox->Init(content, shell)))
    return rv;

  SetBoxObjectFor(aElement, boxObject);

  *aResult = boxObject;
  NS_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetBoxObjectFor(nsIDOMElement* aElement, nsIBoxObject* aBoxObject)
{
  if (!mBoxObjectTable) {
    if (!aBoxObject)
      return NS_OK;
    mBoxObjectTable = new nsSupportsHashtable;
  }

  nsISupportsKey key(aElement);

  if (aBoxObject)
    mBoxObjectTable->Put(&key, aBoxObject);
  else {
    nsCOMPtr<nsISupports> supp;
    mBoxObjectTable->Remove(&key, getter_AddRefs(supp));
    nsCOMPtr<nsPIBoxObject> boxObject(do_QueryInterface(supp));
    if (boxObject)
      boxObject->SetDocument(nsnull);
  }

  return NS_OK;
}

//----------------------------------------------------------------------
//
// CachedChromeStreamListener
//

nsXULDocument::CachedChromeStreamListener::CachedChromeStreamListener(nsXULDocument* aDocument, PRBool aProtoLoaded)
    : mDocument(aDocument),
      mProtoLoaded(aProtoLoaded)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mDocument);
}


nsXULDocument::CachedChromeStreamListener::~CachedChromeStreamListener()
{
    NS_RELEASE(mDocument);
}


NS_IMPL_ISUPPORTS2(nsXULDocument::CachedChromeStreamListener, nsIRequestObserver, nsIStreamListener);

NS_IMETHODIMP
nsXULDocument::CachedChromeStreamListener::OnStartRequest(nsIRequest *request, nsISupports* acontext)
{
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CachedChromeStreamListener::OnStopRequest(nsIRequest *request,
                                                         nsISupports* aContext,
                                                         nsresult aStatus)
{
    if (! mProtoLoaded)
        return NS_OK;

    nsresult rv;
    rv = mDocument->PrepareToWalk();
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to prepare for walk");
    if (NS_FAILED(rv)) return rv;

    return mDocument->ResumeWalk();
}


NS_IMETHODIMP
nsXULDocument::CachedChromeStreamListener::OnDataAvailable(nsIRequest *request,
                                                           nsISupports* aContext,
                                                           nsIInputStream* aInStr,
                                                           PRUint32 aSourceOffset,
                                                           PRUint32 aCount)
{
    NS_NOTREACHED("CachedChromeStream doesn't receive data");
    return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
//
// ParserObserver
//

nsXULDocument::ParserObserver::ParserObserver(nsXULDocument* aDocument)
    : mDocument(aDocument)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mDocument);
}

nsXULDocument::ParserObserver::~ParserObserver()
{
    NS_IF_RELEASE(mDocument);
}

NS_IMPL_ISUPPORTS1(nsXULDocument::ParserObserver, nsIRequestObserver);

NS_IMETHODIMP
nsXULDocument::ParserObserver::OnStartRequest(nsIRequest *request,
                                              nsISupports* aContext)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ParserObserver::OnStopRequest(nsIRequest *request,
                                             nsISupports* aContext,
                                             nsresult aStatus)
{
    nsresult rv = NS_OK;

    if (NS_FAILED(aStatus)) {
        // If an overlay load fails, we need to nudge the prototype
        // walk along.
#define YELL_IF_MISSING_OVERLAY 1
#if defined(DEBUG) || defined(YELL_IF_MISSING_OVERLAY)

        nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
        if (!aChannel) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIURI> uri;
        aChannel->GetOriginalURI(getter_AddRefs(uri));

        nsXPIDLCString spec;
        uri->GetSpec(getter_Copies(spec));

        printf("*** Failed to load overlay %s\n", (const char*) spec);
#endif

        rv = mDocument->ResumeWalk();
    }

    // Drop the reference to the document to break cycle between the
    // document, the parser, the content sink, and the parser
    // observer.
    NS_RELEASE(mDocument);

    return rv;
}

#ifdef IBMBIDI
/**
 *  Retrieve and get bidi state of the document
 *  set depending on presence of bidi data.
 *  (see nsIDocument.h)
 */

NS_IMETHODIMP
nsXULDocument::GetBidiEnabled(PRBool* aBidiEnabled) const
{
    NS_ENSURE_ARG_POINTER(aBidiEnabled);
    *aBidiEnabled = mBidiEnabled;
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetBidiEnabled(PRBool aBidiEnabled)
{
    NS_ASSERTION(aBidiEnabled, "cannot disable bidi once enabled");
    if (aBidiEnabled) {
        mBidiEnabled = PR_TRUE;
    }
    return NS_OK;
}
#endif // IBMBIDI


//----------------------------------------------------------------------
//
// The XUL element factory
//

class XULElementFactoryImpl : public nsIElementFactory
{
protected:
    XULElementFactoryImpl();
    virtual ~XULElementFactoryImpl();

public:
    friend
    nsresult
    NS_NewXULElementFactory(nsIElementFactory** aResult);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIElementFactory interface
    NS_IMETHOD CreateInstanceByTag(nsINodeInfo *aNodeInfo, nsIContent** aResult);

    static PRBool gIsInitialized;
    static PRInt32 kNameSpaceID_XUL;
};

PRBool XULElementFactoryImpl::gIsInitialized = PR_FALSE;
PRInt32 XULElementFactoryImpl::kNameSpaceID_XUL;

XULElementFactoryImpl::XULElementFactoryImpl()
{
    NS_INIT_REFCNT();

    if (!gIsInitialized) {
        nsCOMPtr<nsINameSpaceManager> nsmgr =
            do_GetService(kNameSpaceManagerCID);

        NS_ASSERTION(nsmgr, "unable to get namespace manager");
        if (!nsmgr)
            return;

#define XUL_NAMESPACE_URI \
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"

        nsmgr->RegisterNameSpace(NS_ConvertASCIItoUCS2(XUL_NAMESPACE_URI),
                                 kNameSpaceID_XUL);

        gIsInitialized = PR_TRUE;
    }
}

XULElementFactoryImpl::~XULElementFactoryImpl()
{
}


NS_IMPL_ISUPPORTS1(XULElementFactoryImpl, nsIElementFactory)


nsresult
NS_NewXULElementFactory(nsIElementFactory** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    XULElementFactoryImpl* result = new XULElementFactoryImpl();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;
    return NS_OK;
}



NS_IMETHODIMP
XULElementFactoryImpl::CreateInstanceByTag(nsINodeInfo *aNodeInfo,
                                           nsIContent** aResult)
{
    return nsXULElement::Create(aNodeInfo, aResult);
}
