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
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  An nsIRDFDocument implementation that builds a tree widget XUL
  content model that is to be used with a tree control.

  TO DO

  1) Get a real namespace for XUL. This should go in a 
     header file somewhere.

  2) I really _really_ need to figure out how to factor the logic in
     OnAssert, OnUnassert, OnMove, and OnChange. These all mostly
     kinda do the same sort of thing. It atrocious how much code is
     cut-n-pasted.

 */

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsIHTMLContent.h"
#include "nsIElementFactory.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContainerUtils.h" 
#include "nsIRDFContentModelBuilder.h"
#include "nsIXULDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsITextContent.h"
#include "nsITimer.h"
#include "nsIURL.h"
#include "nsIXMLContent.h"
#include "nsIXPConnect.h"
#include "nsIXULSortService.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsIXULContent.h"
#include "nsIXULContentUtils.h"
#include "nsRDFSort.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsXULElement.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "prlog.h"
#include "rdf.h"
#include "rdfutil.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"

// Return values for EnsureElementHasGenericChild()
#define NS_RDF_ELEMENT_GOT_CREATED NS_RDF_NO_VALUE
#define NS_RDF_ELEMENT_WAS_THERE   NS_OK
static PRLogModuleInfo* gLog;

//----------------------------------------------------------------------

static NS_DEFINE_CID(kHTMLElementFactoryCID,     NS_HTML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,       NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kTextNodeCID,               NS_TEXTNODE_CID);
static NS_DEFINE_CID(kXMLElementFactoryCID,      NS_XML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kXULContentUtilsCID,        NS_XULCONTENTUTILS_CID);
static NS_DEFINE_CID(kXULSortServiceCID,         NS_XULSORTSERVICE_CID);

//----------------------------------------------------------------------

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"

//----------------------------------------------------------------------
//
// RDFGenericBuilderImpl
//

class RDFGenericBuilderImpl : public nsIRDFContentModelBuilder,
                              public nsIRDFObserver
{
public:
    RDFGenericBuilderImpl();
    virtual ~RDFGenericBuilderImpl();

    nsresult Init();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDocument(nsIXULDocument* aDocument);
    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase);
    NS_IMETHOD GetDataBase(nsIRDFCompositeDataSource** aDataBase);
    NS_IMETHOD CreateRootContent(nsIRDFResource* aResource);
    NS_IMETHOD SetRootContent(nsIContent* aElement);
    NS_IMETHOD CreateContents(nsIContent* aElement);
    NS_IMETHOD OpenContainer(nsIContent* aContainer);
    NS_IMETHOD CloseContainer(nsIContent* aContainer);
    NS_IMETHOD RebuildContainer(nsIContent* aContainer);

    // nsIRDFObserver interface
    NS_IMETHOD OnAssert(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget);

    NS_IMETHOD OnUnassert(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget);

    NS_IMETHOD OnChange(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aOldTarget,
                        nsIRDFNode* aNewTarget);

    NS_IMETHOD OnMove(nsIRDFResource* aOldSource,
                      nsIRDFResource* aNewSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget);

    // Implementation methods
    nsresult
    FindTemplate(nsIContent* aElement,
                 nsIRDFResource* aProperty,
                 nsIRDFResource* aChild,
                 nsIContent **theTemplate);

    nsresult
    IsTemplateRuleMatch(nsIContent* aElement,
                        nsIRDFResource* aProperty,
                        nsIRDFResource* aChild,
                        nsIContent *aRule,
                        PRBool *isMatch);

    nsresult
    TagMatches(nsIContent* aElement, nsString& aTag, PRBool* aResult);

    PRBool
    IsIgnoreableAttribute(PRInt32 aNameSpaceID, nsIAtom* aAtom);

    nsresult
    SubstituteText(nsIRDFResource* aResource,
                   nsString& aAttributeValue);

    nsresult
    BuildContentFromTemplate(nsIContent *aTemplateNode,
                             nsIContent *aResourceNode,
                             nsIContent *aRealNode,
                             PRBool aIsUnique,
                             nsIRDFResource* aChild,
                             PRBool aNotify,
                             nsIContent** aContainer,
                             PRInt32* aNewIndexInContainer);

    nsresult
    AddPersistentAttributes(nsIContent* aTemplateNode, nsIRDFResource* aResource, nsIContent* aRealNode);

    nsresult
    CreateWidgetItem(nsIContent* aElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aChild,
                     PRBool aNotify,
                     nsIContent** aContainer,
                     PRInt32* aNewIndexInContainer);

    enum eUpdateAction { eSet, eClear };

    nsresult
    SynchronizeUsingTemplate(nsIContent *aTemplateNode,
                             nsIContent* aRealNode,
                             eUpdateAction aAction,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aValue);

    nsresult
    RemoveWidgetItem(nsIContent* aElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aValue,
                     PRBool aNotify);

    nsresult
    CreateTemplateAndContainerContents(nsIContent* aElement,
                                       nsIContent** aContainer,
                                       PRInt32* aNewIndexInContainer);

    nsresult
    CreateContainerContents(nsIContent* aElement,
                            nsIRDFResource* aResource,
                            PRBool aNotify,
                            nsIContent** aContainer,
                            PRInt32* aNewIndexInContainer);

    nsresult
    CreateTemplateContents(nsIContent* aElement,
                           const nsString& aTemplateID,
                           nsIContent** aContainer,
                           PRInt32* aNewIndexInContainer);

    nsresult
    EnsureElementHasGenericChild(nsIContent* aParent,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aTag,
                                 PRBool aNotify,
                                 nsIContent** aResult);

    PRBool
    IsContainmentProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsIgnoredProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsContainer(nsIContent* aParentElement, nsIRDFResource* aTargetResource);

    PRBool
    IsEmpty(nsIContent* aParentElement, nsIRDFResource* aContainer);

    PRBool
    IsOpen(nsIContent* aElement);

    PRBool
    IsElementInWidget(nsIContent* aElement);
   
    nsresult
    GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource);

    nsresult RemoveGeneratedContent(nsIContent* aElement);

    // XXX. Urg. Hack until layout can batch reflows. See bug 10818.
    PRBool
    IsTreeWidgetItem(nsIContent* aElement);

    PRBool
    IsReflowScheduled();

    nsresult
    ScheduleReflow();

    static void
    ForceTreeReflow(nsITimer* aTimer, void* aClosure);

    static void
    GetElementFactory(PRInt32 aNameSpaceID, nsIElementFactory** aResult);

    nsresult
    AddDatabasePropertyToHTMLElement(nsIContent* aElement, nsIRDFCompositeDataSource* aDataBase);

    nsresult
    GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements);

    nsresult
    CreateElement(PRInt32 aNameSpaceID,
                  nsIAtom* aTag,
                  nsIContent** aResult);

    nsresult
    SetEmpty(nsIContent *element, PRBool empty);

#ifdef PR_LOGGING
    nsresult
    Log(const char* aOperation,
        nsIContent* aElement, 
        nsIRDFResource* aSource,
        nsIRDFResource* aProperty,
        nsIRDFNode* aTarget);

#define LOG(_op, _ele, _src, _prop, _targ) \
    Log(_op, _ele, _src, _prop, _targ)

#else
#define LOG(_op, _ele, _src, _prop, _targ)
#endif

protected:
    nsIXULDocument*            mDocument; // [WEAK]

    // We are an observer of the composite datasource. The cycle is
    // broken by out-of-band SetDataBase(nsnull) call when document is
    // destroyed.
    nsCOMPtr<nsIRDFCompositeDataSource> mDB;
    nsCOMPtr<nsIContent>                mRoot;

    nsCOMPtr<nsIRDFDataSource>		mCache;
    nsCOMPtr<nsITimer>			mTimer;

	nsRDFSortState			sortState;

    // pseudo-constants
    static nsrefcnt gRefCnt;
    static nsIRDFService*         gRDFService;
    static nsIRDFContainerUtils*  gRDFContainerUtils;
    static nsINameSpaceManager*   gNameSpaceManager;
    static nsIElementFactory*     gHTMLElementFactory;
    static nsIElementFactory*  gXMLElementFactory;
    static nsIXULContentUtils*    gXULUtils;

    static nsIAtom* kContainerAtom;
    static nsIAtom* kContainmentAtom;
    static nsIAtom* kEmptyAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kIgnoreAtom;
    static nsIAtom* kInstanceOfAtom;
    static nsIAtom* kIsContainerAtom;
    static nsIAtom* kIsEmptyAtom;
    static nsIAtom* kMenuAtom;
    static nsIAtom* kMenuPopupAtom;
    static nsIAtom* kOpenAtom;
    static nsIAtom* kParentAtom;
    static nsIAtom* kPersistAtom;
    static nsIAtom* kPropertyAtom;
    static nsIAtom* kResourceAtom;
    static nsIAtom* kRuleAtom;
    static nsIAtom* kTemplateAtom;
    static nsIAtom* kTextNodeAtom;
    static nsIAtom* kTreeAtom;
    static nsIAtom* kTreeChildrenAtom;
    static nsIAtom* kTreeItemAtom;
    static nsIAtom* kURIAtom;
    static nsIAtom* kValueAtom;
    static nsIAtom* kXULContentsGeneratedAtom;

    static PRInt32  kNameSpaceID_RDF;
    static PRInt32  kNameSpaceID_XUL;

    static nsIRDFResource* kNC_Title;
    static nsIRDFResource* kNC_child;
    static nsIRDFResource* kNC_Column;
    static nsIRDFResource* kNC_Folder;
    static nsIRDFResource* kRDF_child;
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kXUL_element;

    static nsIXULSortService* gXULSortService;

    static nsString    trueStr;
    static nsString    falseStr;
};

//----------------------------------------------------------------------

nsrefcnt            RDFGenericBuilderImpl::gRefCnt = 0;
nsIXULSortService*  RDFGenericBuilderImpl::gXULSortService = nsnull;

nsIAtom* RDFGenericBuilderImpl::kContainerAtom;
nsIAtom* RDFGenericBuilderImpl::kContainmentAtom;
nsIAtom* RDFGenericBuilderImpl::kEmptyAtom;
nsIAtom* RDFGenericBuilderImpl::kIdAtom;
nsIAtom* RDFGenericBuilderImpl::kIgnoreAtom;
nsIAtom* RDFGenericBuilderImpl::kInstanceOfAtom;
nsIAtom* RDFGenericBuilderImpl::kIsContainerAtom;
nsIAtom* RDFGenericBuilderImpl::kIsEmptyAtom;
nsIAtom* RDFGenericBuilderImpl::kMenuAtom;
nsIAtom* RDFGenericBuilderImpl::kMenuPopupAtom;
nsIAtom* RDFGenericBuilderImpl::kOpenAtom;
nsIAtom* RDFGenericBuilderImpl::kParentAtom;
nsIAtom* RDFGenericBuilderImpl::kPersistAtom;
nsIAtom* RDFGenericBuilderImpl::kPropertyAtom;
nsIAtom* RDFGenericBuilderImpl::kResourceAtom;
nsIAtom* RDFGenericBuilderImpl::kRuleAtom;
nsIAtom* RDFGenericBuilderImpl::kTemplateAtom;
nsIAtom* RDFGenericBuilderImpl::kTextNodeAtom;
nsIAtom* RDFGenericBuilderImpl::kTreeAtom;
nsIAtom* RDFGenericBuilderImpl::kTreeChildrenAtom;
nsIAtom* RDFGenericBuilderImpl::kTreeItemAtom;
nsIAtom* RDFGenericBuilderImpl::kURIAtom;
nsIAtom* RDFGenericBuilderImpl::kValueAtom;
nsIAtom* RDFGenericBuilderImpl::kXULContentsGeneratedAtom;

nsString RDFGenericBuilderImpl::trueStr;
nsString RDFGenericBuilderImpl::falseStr;

PRInt32  RDFGenericBuilderImpl::kNameSpaceID_RDF;
PRInt32  RDFGenericBuilderImpl::kNameSpaceID_XUL;

nsIRDFService*  RDFGenericBuilderImpl::gRDFService;
nsIRDFContainerUtils* RDFGenericBuilderImpl::gRDFContainerUtils;
nsINameSpaceManager* RDFGenericBuilderImpl::gNameSpaceManager;
nsIElementFactory* RDFGenericBuilderImpl::gHTMLElementFactory;
nsIElementFactory* RDFGenericBuilderImpl::gXMLElementFactory;
nsIXULContentUtils* RDFGenericBuilderImpl::gXULUtils;

nsIRDFResource* RDFGenericBuilderImpl::kNC_Title;
nsIRDFResource* RDFGenericBuilderImpl::kNC_child;
nsIRDFResource* RDFGenericBuilderImpl::kNC_Column;
nsIRDFResource* RDFGenericBuilderImpl::kNC_Folder;
nsIRDFResource* RDFGenericBuilderImpl::kRDF_child;
nsIRDFResource* RDFGenericBuilderImpl::kRDF_instanceOf;
nsIRDFResource* RDFGenericBuilderImpl::kXUL_element;


//----------------------------------------------------------------------


nsresult
NS_NewXULTemplateBuilder(nsIRDFContentModelBuilder** aResult)
{
    nsresult rv;
    RDFGenericBuilderImpl* result = new RDFGenericBuilderImpl();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result); // stabilize

    rv = result->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = result->QueryInterface(NS_GET_IID(nsIRDFContentModelBuilder), (void**) aResult);
    }

    NS_RELEASE(result);
    return rv;
}


RDFGenericBuilderImpl::RDFGenericBuilderImpl(void)
    : mDocument(nsnull),
      mDB(nsnull),
      mRoot(nsnull),
      mTimer(nsnull)
{
    NS_INIT_REFCNT();
}

RDFGenericBuilderImpl::~RDFGenericBuilderImpl(void)
{
#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d - RDF: RDFGenericBuilderImpl\n", gInstanceCount);
#endif

    // NS_IF_RELEASE(mDocument) not refcounted

    --gRefCnt;
    if (gRefCnt == 0) {
        NS_IF_RELEASE(kContainerAtom);
        NS_IF_RELEASE(kContainmentAtom);
        NS_IF_RELEASE(kEmptyAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kIgnoreAtom);
        NS_IF_RELEASE(kInstanceOfAtom);
        NS_IF_RELEASE(kIsContainerAtom);
        NS_IF_RELEASE(kIsEmptyAtom);
        NS_IF_RELEASE(kMenuAtom);
        NS_IF_RELEASE(kMenuPopupAtom);
        NS_IF_RELEASE(kOpenAtom);
        NS_IF_RELEASE(kParentAtom);
        NS_IF_RELEASE(kPersistAtom);
        NS_IF_RELEASE(kPropertyAtom);
        NS_IF_RELEASE(kResourceAtom);
        NS_IF_RELEASE(kRuleAtom);
        NS_IF_RELEASE(kTemplateAtom);
        NS_IF_RELEASE(kTextNodeAtom);
        NS_IF_RELEASE(kTreeAtom);
        NS_IF_RELEASE(kTreeChildrenAtom);
        NS_IF_RELEASE(kTreeItemAtom);
        NS_IF_RELEASE(kURIAtom);
        NS_IF_RELEASE(kValueAtom);
        NS_IF_RELEASE(kXULContentsGeneratedAtom);

        NS_IF_RELEASE(kNC_Title);
        NS_IF_RELEASE(kNC_child);
        NS_IF_RELEASE(kNC_Column);
        NS_IF_RELEASE(kNC_Folder);
        NS_IF_RELEASE(kRDF_child);
        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kXUL_element);

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        if (gRDFContainerUtils) {
            nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFContainerUtils);
            gRDFContainerUtils = nsnull;
        }

        if (gXULSortService) {
            nsServiceManager::ReleaseService(kXULSortServiceCID, gXULSortService);
            gXULSortService = nsnull;
        }

        NS_RELEASE(gNameSpaceManager);
        NS_IF_RELEASE(gHTMLElementFactory);

        if (gXULUtils) {
            nsServiceManager::ReleaseService(kXULContentUtilsCID, gXULUtils);
            gXULUtils = nsnull;
        }
    }
}


nsresult
RDFGenericBuilderImpl::Init()
{
    if (gRefCnt++ == 0) {
        kContainerAtom                  = NS_NewAtom("container");
        kContainmentAtom                = NS_NewAtom("containment");
        kEmptyAtom                      = NS_NewAtom("empty");
        kIdAtom                         = NS_NewAtom("id");
        kIgnoreAtom                     = NS_NewAtom("ignore");
        kInstanceOfAtom                 = NS_NewAtom("instanceOf");
        kIsContainerAtom                = NS_NewAtom("iscontainer");
        kIsEmptyAtom                    = NS_NewAtom("isempty");
        kMenuAtom                       = NS_NewAtom("menu");
        kMenuPopupAtom                  = NS_NewAtom("menupopup");
        kOpenAtom                       = NS_NewAtom("open");
        kParentAtom                     = NS_NewAtom("parent");
        kPersistAtom                    = NS_NewAtom("persist");
        kPropertyAtom                   = NS_NewAtom("property");
        kResourceAtom                   = NS_NewAtom("resource");
        kRuleAtom                       = NS_NewAtom("rule");
        kTemplateAtom                   = NS_NewAtom("template");
        kTextNodeAtom                   = NS_NewAtom("textnode");
        kTreeAtom                       = NS_NewAtom("tree");
        kTreeChildrenAtom               = NS_NewAtom("treechildren");
        kTreeItemAtom                   = NS_NewAtom("treeitem");
        kURIAtom                        = NS_NewAtom("uri");
        kValueAtom                      = NS_NewAtom("value");
        kXULContentsGeneratedAtom       = NS_NewAtom("xulcontentsgenerated");

    trueStr = "true";
    falseStr = "false";

        nsresult rv;

        // Register the XUL and RDF namespaces: these'll just retrieve
        // the IDs if they've already been registered by someone else.
        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                nsnull,
                                                NS_GET_IID(nsINameSpaceManager),
                                                (void**) &gNameSpaceManager);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create namespace manager");
        if (NS_FAILED(rv)) return rv;

        // XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
        static const char kXULNameSpaceURI[]
            = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

        static const char kRDFNameSpaceURI[]
            = RDF_NAMESPACE_URI;

        rv = gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");
        if (NS_FAILED(rv)) return rv;

        rv = gNameSpaceManager->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");
        if (NS_FAILED(rv)) return rv;


        // Initialize the global shared reference to the service
        // manager and get some shared resource objects.
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**) &gRDFService);
        if (NS_FAILED(rv)) return rv;

        gRDFService->GetResource(NC_NAMESPACE_URI "Title",   &kNC_Title);
        gRDFService->GetResource(NC_NAMESPACE_URI "child",   &kNC_child);
        gRDFService->GetResource(NC_NAMESPACE_URI "Column",  &kNC_Column);
        gRDFService->GetResource(NC_NAMESPACE_URI "Folder",  &kNC_Folder);
        gRDFService->GetResource(RDF_NAMESPACE_URI "child",  &kRDF_child);
        gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_instanceOf);
        gRDFService->GetResource(XUL_NAMESPACE_URI "element",    &kXUL_element);

        rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
                                          NS_GET_IID(nsIRDFContainerUtils),
                                          (nsISupports**) &gRDFContainerUtils);
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kXULSortServiceCID,
                                          NS_GET_IID(nsIXULSortService),
                                          (nsISupports**) &gXULSortService);
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kHTMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIElementFactory),
                                                (void**) &gHTMLElementFactory);
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kXMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIElementFactory),
                                                (void**) &gXMLElementFactory);
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kXULContentUtilsCID,
                                          NS_GET_IID(nsIXULContentUtils),
                                          (nsISupports**) &gXULUtils);
        if (NS_FAILED(rv)) return rv;
    }

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsRDFGenericBuilder");
#endif

    return NS_OK;
}

NS_IMPL_ISUPPORTS2(RDFGenericBuilderImpl, nsIRDFContentModelBuilder, nsIRDFObserver);

//----------------------------------------------------------------------
//
// nsIRDFContentModelBuilder methods
//

NS_IMETHODIMP
RDFGenericBuilderImpl::SetDocument(nsIXULDocument* aDocument)
{
    // note: null now allowed, it indicates document going away

    mDocument = aDocument; // not refcounted
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::SetDataBase(nsIRDFCompositeDataSource* aDataBase)
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    
    if (mDB)
        mDB->RemoveObserver(this);

    mDB = dont_QueryInterface(aDataBase);

    if (mDB) {
        nsresult rv;
        mDB->AddObserver(this);

        // Now set the database on the element, so that script writers can
        // access it.
        nsCOMPtr<nsIDOMXULElement> element( do_QueryInterface(mRoot) );
        if (element) {
            rv = element->SetDatabase(aDataBase);
            if (NS_FAILED(rv)) return rv;
        }
        else {
            // Hmm. This must be an HTML element. Try to set it as a
            // JS property "by hand".
            rv = AddDatabasePropertyToHTMLElement(mRoot, mDB);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::GetDataBase(nsIRDFCompositeDataSource** aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    *aDataBase = mDB;
    NS_ADDREF(*aDataBase);
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::CreateRootContent(nsIRDFResource* aResource)
{
    // XXX Remove this method from the interface
    NS_NOTREACHED("whoops");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::SetRootContent(nsIContent* aElement)
{
    mRoot = dont_QueryInterface(aElement);
    
    if (mCache)
    {
    	// flush (delete) the cache when re-rerooting the generated content
    	mCache = nsnull;
    }
    
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::CreateContents(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // First, make sure that the element is in the right widget -- ours.
    nsresult rv = NS_OK;
    if (IsElementInWidget(aElement)) {
        rv = CreateTemplateAndContainerContents(aElement, nsnull /* don't care */, nsnull /* don't care */);
    }

    return rv;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OpenContainer(nsIContent* aElement)
{
    nsresult rv;

    // First, make sure that the element is in the right widget -- ours.
    if (!IsElementInWidget(aElement))
        return NS_OK;

    nsCOMPtr<nsIRDFResource> resource;
    rv = gXULUtils->GetElementRefResource(aElement, getter_AddRefs(resource));

    // If it has no resource, there's nothing that we need to be
    // concerned about here.
    if (NS_FAILED(rv))
        return NS_OK;

    // The element has a resource; that means that it corresponds
    // to something in the graph, so we need to go to the graph to
    // create its contents.
    //
    // Create the container's contents "quietly" (i.e., |aNotify ==
    // PR_FALSE|), and then use the |container| and |newIndex| to
    // notify layout where content got created.
    nsCOMPtr<nsIContent> container;
    PRInt32 newIndex;
    rv = CreateContainerContents(aElement, resource, PR_FALSE, getter_AddRefs(container), &newIndex);
    if (NS_FAILED(rv)) return rv;

    if (container && IsTreeWidgetItem(aElement)) {
        // The tree widget is special, and has to be spanked every
        // time we add content to a container.
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
        if (! doc)
            return NS_ERROR_UNEXPECTED;

        rv = doc->ContentAppended(container, newIndex);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::CloseContainer(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // First, make sure that the element is in the right widget -- ours.
    if (!IsElementInWidget(aElement))
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    if (tag.get() == kTreeItemAtom) {
        // Find the tag that contains the children so that we can
        // remove all of the children. This is a -total- hack, that is
        // necessary for the tree control because...I'm not sure. But
        // it's necessary. Maybe we should fix the tree control to
        // reflow itself when the 'open' attribute changes on a
        // treeitem.
        //
        // OTOH, we could treat this as a (premature?) optimization so
        // that nodes which are not being displayed don't hang around
        // taking up space. Unfortunately, the tree widget currently
        // _relies_ on this behavior and will break if we don't do it
        // :-(.

        // Find the <treechildren> beneath the <treeitem>...
        nsCOMPtr<nsIContent> insertionpoint;
        rv = gXULUtils->FindChildByTag(aElement, kNameSpaceID_XUL, kTreeChildrenAtom, getter_AddRefs(insertionpoint));
        if (NS_FAILED(rv)) return rv;

        if (insertionpoint) {
            // ...and blow away all the generated content.
            PRInt32 count;
            rv = insertionpoint->ChildCount(count);
            if (NS_FAILED(rv)) return rv;

            rv = RemoveGeneratedContent(insertionpoint);
            if (NS_FAILED(rv)) return rv;
        }

        // Force the XUL element to remember that it needs to re-generate
        // its kids next time around.
        nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
        NS_ASSERTION(xulcontent != nsnull, "not an nsIXULContent");
        if (! xulcontent)
            return NS_ERROR_UNEXPECTED;

        rv = xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
        if (NS_FAILED(rv)) return rv;

        // Clear the contents-generated attribute so that the next time we
        // come back, we'll regenerate the kids we just killed.
        rv = xulcontent->ClearLazyState(nsIXULContent::eContainerContentsBuilt);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::RebuildContainer(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // First, make sure that the element is in the right widget -- ours.
    if (!IsElementInWidget(aElement))
        return NS_OK;

    nsresult rv;

    // Next, see if it's a XUL element whose contents have never even
    // been generated. If so, short-circuit and bail; there's nothing
    // for us to "rebuild" yet. They'll get built correctly the next
    // time somebody asks for them. 
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);

    if (xulcontent) {
        PRBool containerContentsBuilt;
        rv = xulcontent->GetLazyState(nsIXULContent::eContainerContentsBuilt, containerContentsBuilt);
        if (NS_FAILED(rv)) return rv;

        if (! containerContentsBuilt)
            return NS_OK;
    }

    // If we get here, then we've tried to generate content for this
    // element. Remove it.
    rv = RemoveGeneratedContent(aElement);
    if (NS_FAILED(rv)) return rv;

    // Forces the XUL element to remember that it needs to
    // re-generate its children next time around.
    if (xulcontent) {
        rv = xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
        if (NS_FAILED(rv)) return rv;

        rv = xulcontent->ClearLazyState(nsIXULContent::eTemplateContentsBuilt);
        if (NS_FAILED(rv)) return rv;

        rv = xulcontent->ClearLazyState(nsIXULContent::eContainerContentsBuilt);
        if (NS_FAILED(rv)) return rv;
    }

    // Now, regenerate both the template- and container-generated
    // contents for the current element...
    nsCOMPtr<nsIContent> container;
    PRInt32 newIndex;
    rv = CreateTemplateAndContainerContents(aElement, getter_AddRefs(container), &newIndex);
    if (NS_FAILED(rv)) return rv;

    if (container) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
        if (! doc)
            return NS_ERROR_UNEXPECTED;

        rv = doc->ContentAppended(container, newIndex);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}



//----------------------------------------------------------------------
//
// nsIRDFObserver interface
//

NS_IMETHODIMP
RDFGenericBuilderImpl::OnAssert(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget)
{
    // Just silently fail, because this can happen "normally" as part
    // of tear-down code. (Bug 9098)
    if (! mDocument)
        return NS_OK;

    nsresult rv;

	if (mCache)
	{
		mCache->Assert(aSource, aProperty, aTarget, PR_TRUE /* XXX should be value passed in */);
	}

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new ISupportsArray");
    if (NS_FAILED(rv)) return rv;

    // Find all the elements in the content model that correspond to
    // aSource: for each, we'll try to build XUL children if
    // appropriate.
    rv = GetElementsForResource(aSource, elements);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to retrieve elements from resource");
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
        nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> element( do_QueryInterface(isupports) );
        NS_IF_RELEASE(isupports);

        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        // We'll start by making sure that the element at least has
        // the same parent has the content model builder's root
        if (!IsElementInWidget(element))
            continue;

        LOG("assert", element, aSource, aProperty, aTarget);

        nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(aTarget);
        if (resource && IsContainmentProperty(element, aProperty)) {
            // Okay, the target  _is_ a resource, and the property is
            // a containment property. So this'll be a new item in the widget
            // control.

            // But if the contents of aElement _haven't_ yet been
            // generated, then just ignore the assertion. We do this
            // because we know that _eventually_ the contents will be
            // generated (via CreateContents()) when somebody asks for
            // them later.
            PRBool contentsGenerated;
            nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(element);
            if (xulcontent) {
                rv = xulcontent->GetLazyState(nsIXULContent::eContainerContentsBuilt, contentsGenerated);
                if (NS_FAILED(rv)) return rv;
            }
            else {
                // HTML is always generated
                contentsGenerated = PR_TRUE;
            }

            if (! contentsGenerated)
            { 
                // Update the "empty" attribute
                rv = SetEmpty(element, PR_FALSE);
                return NS_OK;
            }

            // Okay, it's a "live" element, so go ahead and append the new
            // child to this node.
            rv = CreateWidgetItem(element, aProperty, resource, PR_TRUE,
                                  nsnull /* don't care */,
                                  nsnull /* don't care */);

            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create widget item");
            if (NS_FAILED(rv)) return rv;

            // Update the "empty" attribute
            rv = SetEmpty(element, PR_FALSE);
            if(NS_FAILED(rv)) return rv;
        }
        else {
            // Either the target of the assertion is not a resource,
            // or the object is a resource and the predicate is not a
            // containment property. So this won't be a new item in
            // the widget. See if we can use it to set a some
            // substructure on the current element.
            nsAutoString templateID;
            rv = element->GetAttribute(kNameSpaceID_None,
                                       kTemplateAtom,
                                       templateID);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                nsCOMPtr<nsIDOMXULDocument>    xulDoc;
                xulDoc = do_QueryInterface(mDocument);
                if (! xulDoc)
                    return NS_ERROR_UNEXPECTED;

                nsCOMPtr<nsIDOMElement>    domElement;
                rv = xulDoc->GetElementById(templateID, getter_AddRefs(domElement));
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find template node");
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsIContent> templateNode = do_QueryInterface(domElement);
                if (! templateNode)
                    return NS_ERROR_UNEXPECTED;

                // this node was created by a XUL template, so update it accordingly
                rv = SynchronizeUsingTemplate(templateNode, element, eSet, aProperty, aTarget);
                if (NS_FAILED(rv)) return rv;
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::OnUnassert(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
    // Just silently fail, because this can happen "normally" as part
    // of tear-down code. (Bug 9098)
    if (! mDocument)
        return NS_OK;

    nsresult rv;

	if (mCache)
	{
		mCache->Unassert(aSource, aProperty, aTarget);
	}

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new ISupportsArray");
    if (NS_FAILED(rv)) return rv;

    // Find all the elements in the content model that correspond to
    // aSource: for each, we'll try to build XUL children if
    // appropriate.
    rv = GetElementsForResource(aSource, elements);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to retrieve elements from resource");
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = cnt - 1; i >= 0; --i) {
        nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> element( do_QueryInterface(isupports) );
        NS_IF_RELEASE(isupports);
        
        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        // We'll start by making sure that the element at least has
        // the same parent has the content model builder's root
        if (!IsElementInWidget(element))
            continue;
        
        LOG("unassert", element, aSource, aProperty, aTarget);

        nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(aTarget);
        if (resource && IsContainmentProperty(element, aProperty)) {
            // Okay, the object _is_ a resource, and the predicate is
            // a containment property. So we'll need to remove this
            // item from the widget.

            // But if the contents of aElement _haven't_ yet been
            // generated, then just ignore the unassertion: nothing is
            // in the content model to remove.
            PRBool contentsGenerated;
            nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(element);
            if (xulcontent) {
                rv = xulcontent->GetLazyState(nsIXULContent::eContainerContentsBuilt, contentsGenerated);
                if (NS_FAILED(rv)) return rv;
            }
            else {
                // HTML is always generated
                contentsGenerated = PR_TRUE;
            }

            if (! contentsGenerated)
                return NS_OK;

            // Okay, it's a "live" element, so go ahead and remove the
            // child from this node.
            rv = RemoveWidgetItem(element, aProperty, resource, PR_TRUE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to remove widget item");
            if (NS_FAILED(rv)) return rv;

            PRInt32    numKids;
            rv = element->ChildCount(numKids);
            if (NS_FAILED(rv)) return rv;

            if (numKids == 0) {
                nsAutoString empty;
                rv = element->GetAttribute(kNameSpaceID_None, kEmptyAtom, empty);
                if (NS_FAILED(rv)) return rv;

                if ((rv != NS_CONTENT_ATTR_HAS_VALUE) && (! empty.Equals(trueStr))) {
                    rv = element->SetAttribute(kNameSpaceID_None, kEmptyAtom, trueStr, PR_TRUE);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            
        }
        else {
            // Either the target of the assertion is not a resource,
            // or the target is a resource and the property is not a
            // containment property. So this won't be an item in the
            // widget. See if we need to tear down some substructure
            // on the current element.
            nsAutoString templateID;
            rv = element->GetAttribute(kNameSpaceID_None,
                                       kTemplateAtom,
                                       templateID);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                nsCOMPtr<nsIDOMXULDocument>    xulDoc;
                xulDoc = do_QueryInterface(mDocument);
                if (! xulDoc)
                    return NS_ERROR_UNEXPECTED;

                nsCOMPtr<nsIDOMElement>    domElement;
                rv = xulDoc->GetElementById(templateID, getter_AddRefs(domElement));
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find template node");
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsIContent> templateNode = do_QueryInterface(domElement);
                if (! templateNode)
                    return NS_ERROR_UNEXPECTED;

                // this node was created by a XUL template, so update it accordingly
                rv = SynchronizeUsingTemplate(templateNode, element, eClear, aProperty, aTarget);
                if (NS_FAILED(rv)) return rv;
            }
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OnChange(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aOldTarget,
                                nsIRDFNode* aNewTarget)
{
    // Just silently fail, because this can happen "normally" as part
    // of tear-down code. (Bug 9098)
    if (! mDocument)
        return NS_OK;

    nsresult rv;

	if (mCache)
	{
		if (aOldTarget)
		{
			// XXX fix this: in-memory DS doesn't like a null oldTarget
			mCache->Change(aSource, aProperty, aOldTarget, aNewTarget);
		}
		else
		{
			// XXX should get tv via observer interface
			mCache->Assert(aSource, aProperty, aNewTarget, PR_TRUE);
		}
	}

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new ISupportsArray");
    if (NS_FAILED(rv)) return rv;

    // Find all the elements in the content model that correspond to
    // aSource: for each, we'll try to build XUL children if
    // appropriate.
    rv = GetElementsForResource(aSource, elements);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to retrieve elements from resource");
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = cnt - 1; i >= 0; --i) {
        nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> element( do_QueryInterface(isupports) );
        NS_IF_RELEASE(isupports);
        
        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        // We'll start by making sure that the element at least has
        // the same parent has the content model builder's root
        if (!IsElementInWidget(element))
            continue;

        LOG("change", element, aSource, aProperty, aNewTarget);
        
        nsCOMPtr<nsIRDFResource> oldresource = do_QueryInterface(aOldTarget);
        if (oldresource && IsContainmentProperty(element, aProperty)) {
            // Okay, the object _is_ a resource, and the predicate is
            // a containment property. So we'll need to remove the old
            // item from the widget, and then add the new one.

            // But if the contents of aElement _haven't_ yet been
            // generated, then just ignore: nothing is in the content
            // model to remove.
            PRBool contentsGenerated;
            nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(element);
            if (xulcontent) {
                rv = xulcontent->GetLazyState(nsIXULContent::eContainerContentsBuilt, contentsGenerated);
                if (NS_FAILED(rv)) return rv;
            }
            else {
                // HTML is always generated
                contentsGenerated = PR_TRUE;
            }

            if (! contentsGenerated)
                return NS_OK;

            // Okay, it's a "live" element, so go ahead and remove the
            // child from this node.
            rv = RemoveWidgetItem(element, aProperty, oldresource, PR_TRUE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to remove widget item");
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRDFResource> newresource = do_QueryInterface(aNewTarget);

            // XXX Tough. We don't handle the case where they've
            // changed a resource to a literal. But this is
            // bizarre anyway.
            if (! newresource)
                return NS_OK;

            rv = CreateWidgetItem(element, aProperty, newresource, PR_TRUE,
                                  nsnull /* don't care */,
                                  nsnull /* don't care */);
            if (NS_FAILED(rv)) return rv;
        }
        else {
            // Either the target of the assertion is not a resource,
            // or the target is a resource and the property is not a
            // containment property. So this won't be an item in the
            // widget. See if we need to tear down some substructure
            // on the current element.
            nsAutoString templateID;
            rv = element->GetAttribute(kNameSpaceID_None,
                                       kTemplateAtom,
                                       templateID);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                nsCOMPtr<nsIDOMXULDocument>    xulDoc;
                xulDoc = do_QueryInterface(mDocument);
                if (! xulDoc)
                    return NS_ERROR_UNEXPECTED;

                nsCOMPtr<nsIDOMElement>    domElement;
                rv = xulDoc->GetElementById(templateID, getter_AddRefs(domElement));
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find template node");
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsIContent> templateNode = do_QueryInterface(domElement);
                if (! templateNode)
                    return NS_ERROR_UNEXPECTED;

                // this node was created by a XUL template, so update it accordingly
                rv = SynchronizeUsingTemplate(templateNode, element, eSet, aProperty, aNewTarget);
                if (NS_FAILED(rv)) return rv;
            }
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OnMove(nsIRDFResource* aOldSource,
                              nsIRDFResource* aNewSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aTarget)
{
    NS_NOTYETIMPLEMENTED("write me");

	if (mCache)
	{
		mCache->Move(aOldSource, aNewSource, aProperty, aTarget);
	}

    return NS_ERROR_NOT_IMPLEMENTED;
}


//----------------------------------------------------------------------
//
// Implementation methods
//

nsresult
RDFGenericBuilderImpl::IsTemplateRuleMatch(nsIContent* aElement,
                                           nsIRDFResource *aProperty,
                                           nsIRDFResource* aChild,
                                           nsIContent *aRule,
                                           PRBool *aIsMatch)
{
    nsresult rv;

    PRInt32    count;
    rv = aRule->GetAttributeCount(count);
    if (NS_FAILED(rv)) return(rv);

    for (PRInt32 loop=0; loop<count; loop++) {
        PRInt32 attrNameSpaceID;
        nsCOMPtr<nsIAtom> attr;
        rv = aRule->GetAttributeNameAt(loop, attrNameSpaceID, *getter_AddRefs(attr));
        if (NS_FAILED(rv)) return rv;

        // Note: some attributes must be skipped on XUL template rule subtree

        // never compare against rdf:property attribute
        if ((attr.get() == kPropertyAtom) && (attrNameSpaceID == kNameSpaceID_RDF))
            continue;
        // never compare against rdf:instanceOf attribute
        else if ((attr.get() == kInstanceOfAtom) && (attrNameSpaceID == kNameSpaceID_RDF))
            continue;
        // never compare against {}:id attribute
        else if ((attr.get() == kIdAtom) && (attrNameSpaceID == kNameSpaceID_None))
            continue;
        // never compare against {}:xulcontentsgenerated attribute
        else if ((attr.get() == kXULContentsGeneratedAtom) && (attrNameSpaceID == kNameSpaceID_None))
            continue;

        nsAutoString value;
        rv = aRule->GetAttribute(attrNameSpaceID, attr, value);
        if (NS_FAILED(rv)) return rv;

        if ((attrNameSpaceID == kNameSpaceID_None) && (attr.get() == kParentAtom)) {
            PRBool match;
            rv = TagMatches(aElement, value, &match);
            if (NS_FAILED(rv)) return rv;

            if (! match) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
        }
        else if ((attrNameSpaceID == kNameSpaceID_None) && (attr.get() == kIsContainerAtom)) {
            // check and see if aChild is a container
            PRBool isContainer = IsContainer(aRule, aChild);
            if (isContainer && (!value.Equals(trueStr))) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
            else if (!isContainer && (!value.Equals(falseStr))) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
        }
        else if ((attrNameSpaceID == kNameSpaceID_None) && (attr.get() == kIsEmptyAtom)) {
            PRBool isEmpty = IsEmpty(aRule, aChild);
            if (isEmpty && (!value.Equals(trueStr))) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
            else if (!isEmpty && (!value.Equals(falseStr))) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
        }
        else {
            nsCOMPtr<nsIRDFResource> property;
            rv = gXULUtils->GetResource(attrNameSpaceID, attr, getter_AddRefs(property));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRDFNode> target;
            rv = mDB->GetTarget(aChild, property, PR_TRUE, getter_AddRefs(target));
            if (NS_FAILED(rv)) return rv;

            PRUnichar buf[128];
            nsAutoString targetStr(CBufDescriptor(buf, PR_TRUE, sizeof(buf) / sizeof(PRUnichar), 0));
            rv = gXULUtils->GetTextForNode(target, targetStr);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get text for target");
            if (NS_FAILED(rv)) return rv;

            if (!value.Equals(targetStr)) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
        }
    }

    *aIsMatch = PR_TRUE;
    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::TagMatches(nsIContent* aElement, nsString& aTag, PRBool* aMatch)
{
    // See if the tag string 'aTag' matches the tag of the
    // element. Does the necessary magic to make sure namespaces are
    // equivalent. Note that this may mangle 'aTag' if it needs to
    // strip out the namespace prefix.
    nsresult rv;

    // Pessimist!
    *aMatch = PR_FALSE;

    // Deal with namespaces
    PRInt32 indx;
    if ((indx = aTag.FindChar(':')) != -1) {
        nsAutoString nsprefixstr;
        aTag.Left(nsprefixstr, indx - 1);

        nsCOMPtr<nsIAtom> nsprefix = dont_AddRef(NS_NewAtom(nsprefixstr));
        if (! nsprefix)
            return NS_ERROR_OUT_OF_MEMORY;

        // Walk up from aElement until we find an XML content node so
        // we can figure out what namespace this prefix is supposed to
        // represent.
        nsCOMPtr<nsIContent> element = dont_QueryInterface(aElement);
        while (element) {
            nsCOMPtr<nsIXMLContent> xml = do_QueryInterface(element);
            if (xml) {
                nsCOMPtr<nsINameSpace> ns;
                rv = xml->GetContainingNameSpace(*getter_AddRefs(ns));
                if (NS_FAILED(rv)) return PR_FALSE;

                PRInt32 targetNameSpaceID;
                rv = ns->FindNameSpaceID(nsprefix, targetNameSpaceID);
                if (NS_FAILED(rv)) {
                    // Not a match.
                    return NS_OK;
                }

                PRInt32 elementNameSpaceID;
                rv = aElement->GetNameSpaceID(elementNameSpaceID);
                if (NS_FAILED(rv)) return rv;

                if (elementNameSpaceID != targetNameSpaceID) {
                    // Not a match.
                    return NS_OK;
                }

                // Ok, it _is_ a match. Now we have to compare the tags...
                break;
            }

            nsCOMPtr<nsIContent> parent;
            rv = element->GetParent(*getter_AddRefs(parent));
            if (NS_FAILED(rv)) return rv;

            element = parent;
        }

        // strip namespace from tag
        aTag.Cut(0, indx);
    }

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element tag");
    if (NS_FAILED(rv)) return rv;

    nsAutoString tagStr;
    rv = tag->ToString(tagStr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to convert tag atom to string");
    if (NS_FAILED(rv)) return rv;

    *aMatch = tagStr.Equals(aTag);
    return NS_OK;
}



nsresult
RDFGenericBuilderImpl::FindTemplate(nsIContent* aElement,
                                    nsIRDFResource *aProperty,
                                    nsIRDFResource* aChild,
                                    nsIContent **aTemplate)
{
    nsresult rv;

    // First, check and see if the root has a template attribute. This
    // allows a template to be specified "out of line"; e.g.,
    //
    //   <window>
    //     <foo template="MyTemplate">...</foo>
    //     <template id="MyTemplate">...</template>
    //   </window>
    //

    nsAutoString templateID;
    rv = mRoot->GetAttribute(kNameSpaceID_None, kTemplateAtom, templateID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIContent> tmpl;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        nsCOMPtr<nsIDOMXULDocument>    xulDoc;
        xulDoc = do_QueryInterface(mDocument);
        if (! xulDoc)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIDOMElement>    domElement;
        rv = xulDoc->GetElementById(templateID, getter_AddRefs(domElement));
        if (NS_FAILED(rv)) return rv;

        tmpl = do_QueryInterface(domElement);
    }

    // If root node has no template attribute, then look for a child
    // node which is a template tag
    if (! tmpl) {
        PRInt32    count;
        rv = mRoot->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = 0; i < count; i++) {
            nsCOMPtr<nsIContent> child;
            rv = mRoot->ChildAt(i, *getter_AddRefs(child));
            if (NS_FAILED(rv)) return rv;

            PRInt32 nameSpaceID;
            rv = child->GetNameSpaceID(nameSpaceID);
            if (NS_FAILED(rv)) return rv;

            if (nameSpaceID != kNameSpaceID_XUL)
                continue;

            nsCOMPtr<nsIAtom> tag;
            rv = child->GetTag(*getter_AddRefs(tag));
            if (NS_FAILED(rv)) return rv;

            if (tag.get() != kTemplateAtom)
                continue;

            tmpl = child;
            break;
        }
    }

    // Found a template; check against any (optional) rules...
    if (tmpl) {
        PRInt32    count;
        rv = tmpl->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        PRInt32 nrules = 0;

        for (PRInt32 i = 0; i < count; i++) {
            nsCOMPtr<nsIContent> rule;
            rv = tmpl->ChildAt(i, *getter_AddRefs(rule));
            if (NS_FAILED(rv)) return rv;

            PRInt32    nameSpaceID;
            rv = rule->GetNameSpaceID(nameSpaceID);
            if (NS_FAILED(rv)) return rv;

            if (nameSpaceID != kNameSpaceID_XUL)
                continue;

            nsCOMPtr<nsIAtom> tag;
            rv = rule->GetTag(*getter_AddRefs(tag));
            if (NS_FAILED(rv)) return rv;

            if (tag.get() == kRuleAtom) {
                ++nrules;

                PRBool match;
                rv = IsTemplateRuleMatch(aElement, aProperty, aChild, rule, &match);
                if (NS_FAILED(rv)) return rv;

                if (match) {
                    // found a matching rule, use it as the template
                    *aTemplate = rule;
                    NS_ADDREF(*aTemplate);
                    return NS_OK;
                }
            }
        }

        if (nrules == 0) {
            // if no rules are specified in the template, then the
            // contents of the <template> tag are the one-and-only
            // template.
            *aTemplate = tmpl;
            NS_ADDREF(*aTemplate);
            return NS_OK;
        }
    }

    // If we get here, either there was no <template> tag, or there
    // were <rule> tags beneath the <template> tag that didn't match.
    *aTemplate = nsnull;
    return NS_OK;
}



PRBool
RDFGenericBuilderImpl::IsIgnoreableAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute)
{
    // XXX Note that we patently ignore namespaces. This is because
    // HTML elements lie and tell us that their attributes are
    // _always_ in the HTML namespace. Urgh.

    // never copy the ID attribute
    if (aAttribute == kIdAtom) {
        return PR_TRUE;
    }
    // never copy {}:uri attribute
    else if (aAttribute == kURIAtom) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}


nsresult
RDFGenericBuilderImpl::SubstituteText(nsIRDFResource* aResource,
                                      nsString& aAttributeValue)
{
    nsresult rv;

    if (aAttributeValue.Equals("...") || aAttributeValue.Equals("rdf:*")) {
        const char *uri = nsnull;
        aResource->GetValueConst(&uri);
        aAttributeValue = uri;
    }
    else if (aAttributeValue.Find("rdf:") == 0) {
        // found an attribute which wants to bind its value to RDF so
        // look it up in the graph
        aAttributeValue.Cut(0,4);

        nsCOMPtr<nsIRDFResource> property;
        rv = gRDFService->GetUnicodeResource(aAttributeValue.GetUnicode(), getter_AddRefs(property));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFNode> valueNode;
        rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(valueNode));
        if (NS_FAILED(rv)) return rv;

        if ((rv != NS_RDF_NO_VALUE) && (valueNode)) {
            rv = gXULUtils->GetTextForNode(valueNode, aAttributeValue);
            if (NS_FAILED(rv)) return rv;
        }
        else {
            aAttributeValue.Truncate();
        }
    }
    else {
        // Nothing to do!
    }

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::BuildContentFromTemplate(nsIContent *aTemplateNode,
                                                nsIContent *aResourceNode,
                                                nsIContent *aRealNode,
                                                PRBool aIsUnique,
                                                nsIRDFResource* aChild,
                                                PRBool aNotify,
                                                nsIContent** aContainer,
                                                PRInt32* aNewIndexInContainer)
{
    // This is the mother lode. Here is where we grovel through an
    // element in the template, copying children from the template
    // into the "real" content tree, performing substitution as we go
    // by looking stuff up in the RDF graph.
    //
    //   |aTemplateNode| is the element in the "template tree", whose
    //   children we will duplicate and move into the "real" content
    //   tree.
    //
    //   |aResourceNode| is the element in the "real" content tree that
    //   has the "id" attribute set to an RDF resource's URI. This is
    //   not directly used here, but rather passed down to the XUL
    //   sort service to perform container-level sort.
    //
    //   |aRealNode| is the element in the "real" content tree to which
    //   the new elements will be copied.
    //
    //   |aIsUnique| is set to "true" so long as content has been
    //   "unique" (or "above" the resource element) so far in the
    //   template.
    //
    //   |aChild| is the RDF resource at the end of a property link for
    //   which we are building content.
    //
    //   |aNotify| is set to "true" if content should be constructed
    //   "noisily"; that is, whether the document observers should be
    //   notified when new content is added to the content model.
    //
    //   |aContainer| is an out parameter that will be set to the first
    //   container element in the "real" content tree to which content
    //   was appended.
    //
    //   |aNewIndexInContainer| is an out parameter that will be set to
    //   the index in aContainer at which new content is first
    //   constructed.
    //
    // If |aNotify| is "false", then |aContainer| and
    // |aNewIndexInContainer| are used to determine where in the
    // content model new content is constructed. This allows a single
    // notification to be propogated to document observers.
    //

    nsresult rv;

#ifdef PR_LOGGING
    // Dump out the template node's tag, the template ID, and the RDF
    // resource that is being used as the index into the graph.
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIAtom> tag;
        rv = aTemplateNode->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString resourceCStr;
        rv = aChild->GetValue(getter_Copies(resourceCStr));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagstr;
        tag->ToString(tagstr);

        nsAutoString templatestr;
        aTemplateNode->GetAttribute(kNameSpaceID_None, kIdAtom, templatestr);

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfgeneric[%p] build-content-from-template %s (template='%s') [%s]",
                this,
                (const char*) nsCAutoString(tagstr),
                (const char*) nsCAutoString(templatestr),
                (const char*) resourceCStr));
    }
#endif

    // Iterate through all of the template children, constructing
    // "real" content model nodes for each "template" child.
    PRInt32    count;
    rv = aTemplateNode->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 kid = 0; kid < count; kid++) {
        nsCOMPtr<nsIContent> tmplKid;
        rv = aTemplateNode->ChildAt(kid, *getter_AddRefs(tmplKid));
        if (NS_FAILED(rv)) return rv;

        PRInt32 nameSpaceID;
        rv = tmplKid->GetNameSpaceID(nameSpaceID);
        if (NS_FAILED(rv)) return rv;

        // Check whether this element is the "resource" element. The
        // "resource" element is the element that is cookie-cutter
        // copied once for each different RDF resource specified by
        // |aChild|.
        //
        // Nodes that appear -above- the resource element
        // (that is, are ancestors of the resource element in the
        // content model) are unique across all values of |aChild|,
        // and are created only once.
        //
        // Nodes that appear -below- the resource element (that is,
        // are descnendants of the resource element in the conte
        // model), are cookie-cutter copied for each distinct value of
        // |aChild|.
        //
        // For example, in a <tree> template:
        //
        //   <tree>
        //     <template>
        //       <treechildren> [1]
        //         <treeitem uri="rdf:*"> [2]
        //           <treerow> [3]
        //             <treecell value="rdf:urn:foo" /> [4]
        //             <treecell value="rdf:urn:bar" /> [5]
        //           </treerow>
        //         </treeitem>
        //       </treechildren>
        //     </template>
        //   </tree>
        //
        // The <treeitem> element [2] is the "resource element". This
        // element, and all of its descendants ([3], [4], and [5])
        // will be duplicated for each different |aChild|
        // resource. It's ancestor <treechildren> [1] is unique, and
        // will only be created -once-, no matter how many <treeitem>s
        // are created below it.
        //
        // Note that |isResourceElement| and |isUnique| are mutually
        // exclusive.
        PRBool isResourceElement = PR_FALSE;
        PRBool isUnique = aIsUnique;

        {
            // We identify the resource element by presence of a
            // "uri='rdf:*'" attribute. (We also support the older
            // "uri='...'" syntax.)
            PRUnichar buf[128];
            nsAutoString idValue(CBufDescriptor(buf, PR_TRUE, sizeof(buf) / sizeof(PRUnichar), 0));
            rv = tmplKid->GetAttribute(kNameSpaceID_None,
                                       kURIAtom,
                                       idValue);
            if (NS_FAILED(rv)) return rv;

            if ((rv == NS_CONTENT_ATTR_HAS_VALUE) &&
                (idValue.Equals("...") || idValue.Equals("rdf:*"))) {
                // If we -are- the resource element, then we are no
                // matter unique.
                isResourceElement = PR_TRUE;
                isUnique = PR_FALSE;
            }
        }

        nsCOMPtr<nsIAtom> tag;
        rv = tmplKid->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
            nsAutoString tagname;
            tag->ToString(tagname);
            PR_LOG(gLog, PR_LOG_DEBUG,
                   ("rdfgeneric[%p]     building %s %s %s",
                    this, (const char*) nsCAutoString(tagname),
                    (isResourceElement ? "[resource]" : ""),
                    (isUnique ? "[unique]" : "")));
        }
#endif

        // Set to PR_TRUE if the child we're trying to create now
        // already existed in the content model.
        PRBool realKidAlreadyExisted = PR_FALSE;

        nsCOMPtr<nsIContent> realKid;
        if (isUnique) {
            // The content is "unique"; that is, we haven't descended
            // far enough into the tempalte to hit the "resource"
            // element yet. |EnsureElementHasGenericChild()| will
            // conditionally create the element iff it isn't there
            // already.
            rv = EnsureElementHasGenericChild(aRealNode, nameSpaceID, tag, aNotify, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_RDF_ELEMENT_WAS_THERE) {
                realKidAlreadyExisted = PR_TRUE;
            }
            else {
                // Mark the element's contents as being generated so
                // that any re-entrant calls don't trigger an infinite
                // recursion.
                nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(realKid);
                if (xulcontent) {
                    rv = xulcontent->SetLazyState(nsIXULContent::eTemplateContentsBuilt);
                    if (NS_FAILED(rv)) return rv;
                }

                // Potentially remember the index of this element as the first
                // element that we've generated. Note that we remember
                // this -before- we recurse!
                if (aContainer && !*aContainer) {
                    *aContainer = aRealNode;
                    NS_ADDREF(*aContainer);

					PRInt32 indx;
                    aRealNode->ChildCount(indx);

					// Since EnsureElementHasGenericChild() added us, make
					// sure to subtract one for our real index.
					*aNewIndexInContainer = indx - 1;
                }
            }

            // Recurse until we get to the resource element. Since
            // -we're- unique, assume that our child will be
            // unique. The check for the "resource" element at the top
            // of the function will trip this to |false| as soon as we
            // encounter it.
            rv = BuildContentFromTemplate(tmplKid, aResourceNode, realKid, PR_TRUE, aChild, aNotify, aContainer, aNewIndexInContainer);
            if (NS_FAILED(rv)) return rv;
        }
        else if (isResourceElement) {
            // It's the "resource" element. Create a new element using
            // the namespace ID and tag from the template element.
            rv = CreateElement(nameSpaceID, tag, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            // Assign the element an 'id' attribute using the URI of
            // the |aChild| resource.
            const char *uri;
            rv = aChild->GetValueConst(&uri);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource URI");
            if (NS_FAILED(rv)) return rv;

            nsAutoString id(uri);
            rv = realKid->SetAttribute(kNameSpaceID_None, kIdAtom, id, PR_FALSE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set id attribute");
            if (NS_FAILED(rv)) return rv;

            if (! aNotify) {
                // XUL document will watch us, and take care of making
                // sure that we get added to or removed from the
                // element map if aNotify is true. If not, we gotta do
                // it ourselves. Yay.
                rv = mDocument->AddElementForID(id, realKid);
                if (NS_FAILED(rv)) return rv;
            }

            // XXX Hackery to ensure that mailnews works. Force the
            // element to hold a reference to the
            // resource. Unfortunately, this'll break for HTML
            // elements.
            {
                nsCOMPtr<nsIXULContent> xulele = do_QueryInterface(realKid);
                if (xulele) {
                    xulele->ForceElementToOwnResource(PR_TRUE);
                }
            }

            // Set up the element's 'container' and 'empty'
            // attributes.
            if (IsContainer(tmplKid, aChild)) {
                rv = realKid->SetAttribute(kNameSpaceID_None, kContainerAtom, trueStr, PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                // test to see if the container has contents
                nsAutoString isEmpty = IsEmpty(tmplKid, aChild) ? trueStr : falseStr;
                rv = realKid->SetAttribute(kNameSpaceID_None, kEmptyAtom, isEmpty, PR_FALSE);
                if (NS_FAILED(rv)) return rv;
            }
        }
        else if ((tag.get() == kTextNodeAtom) && (nameSpaceID == kNameSpaceID_XUL)) {
            // <xul:text value="..."> is replaced by text of the
            // actual value of the 'rdf:resource' attribute for the
            // given node.
            PRUnichar attrbuf[128];
            nsAutoString attrValue(CBufDescriptor(attrbuf, PR_TRUE, sizeof(attrbuf) / sizeof(PRUnichar), 0));
            rv = tmplKid->GetAttribute(kNameSpaceID_None, kValueAtom, attrValue);
            if (NS_FAILED(rv)) return rv;

            if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (attrValue.Length() > 0)) {
                rv = SubstituteText(aChild, attrValue);
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsITextContent> content;
                rv = nsComponentManager::CreateInstance(kTextNodeCID,
                                                        nsnull,
                                                        NS_GET_IID(nsITextContent),
                                                        getter_AddRefs(content));
                if (NS_FAILED(rv)) return rv;

                rv = content->SetText(attrValue.GetUnicode(), attrValue.Length(), PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                rv = aRealNode->AppendChildTo(nsCOMPtr<nsIContent>( do_QueryInterface(content) ), aNotify);
                if (NS_FAILED(rv)) return rv;

                // XXX Don't bother remembering text nodes as the
                // first element we've generated?
            }
        }
        else {
            // It's just a generic element. Create it!
            rv = CreateElement(nameSpaceID, tag, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;
        }

        if (realKid && !realKidAlreadyExisted) {
            // Potentially remember the index of this element as the
            // first element that we've generated.
            if (aContainer && !*aContainer) {
                *aContainer = aRealNode;
                NS_ADDREF(*aContainer);

                PRInt32 indx;
                aRealNode->ChildCount(indx);

                // Since we haven't inserted any content yet, our new
                // index in the container will be the current count of
                // elements in the container.
                *aNewIndexInContainer = indx;
            }

            // Mark the node with the ID of the template node used to
            // create this element. This allows us to sync back up
            // with the template to incrementally build content.
            nsAutoString templateID;
            rv = tmplKid->GetAttribute(kNameSpaceID_None, kIdAtom, templateID);
            if (NS_FAILED(rv)) return rv;

            rv = realKid->SetAttribute(kNameSpaceID_None, kTemplateAtom, templateID, PR_FALSE);
            if (NS_FAILED(rv)) return rv;

            // Copy all attributes from the template to the new
            // element.
            PRInt32    numAttribs;
            rv = tmplKid->GetAttributeCount(numAttribs);
            if (NS_FAILED(rv)) return rv;

            for (PRInt32 attr = 0; attr < numAttribs; attr++) {
                PRInt32 attribNameSpaceID;
                nsCOMPtr<nsIAtom> attribName;

                rv = tmplKid->GetAttributeNameAt(attr, attribNameSpaceID, *getter_AddRefs(attribName));
                if (NS_FAILED(rv)) return rv;

                if (! IsIgnoreableAttribute(attribNameSpaceID, attribName)) {
                    // Create a buffer here, because there's a good
                    // chance that an attribute in the template is
                    // going to be an RDF URI, which is usually
                    // longish.
                    PRUnichar attrbuf[128];
                    nsAutoString attribValue(CBufDescriptor(attrbuf, PR_TRUE, sizeof(attrbuf) / sizeof(PRUnichar), 0));
                    rv = tmplKid->GetAttribute(attribNameSpaceID, attribName, attribValue);
                    if (NS_FAILED(rv)) return rv;

                    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                        rv = SubstituteText(aChild, attribValue);
                        if (NS_FAILED(rv)) return rv;

                        rv = realKid->SetAttribute(attribNameSpaceID, attribName, attribValue, PR_FALSE);
                        if (NS_FAILED(rv)) return rv;
                    }
                }
            }

            // Add any persistent attributes
            if (isResourceElement) {
                rv = AddPersistentAttributes(tmplKid, aChild, realKid);
                if (NS_FAILED(rv)) return rv;
            }

            
            if (nameSpaceID == kNameSpaceID_HTML) {
                // If we just built HTML, then we have to recurse "by
                // hand" because HTML won't build itself up
                // lazily. Note that we _don't_ need to notify: we'll
                // add the entire subtree in a single whack.
                //
                // Note that we don't bother passing aContainer and
                // aNewIndexInContainer down: since we're HTML, we
                // -know- that we -must- have just been created.
                rv = BuildContentFromTemplate(tmplKid, aResourceNode, realKid, isUnique,
                                              aChild, PR_FALSE,
                                              nsnull /* don't care */,
                                              nsnull /* don't care */);

                if (NS_FAILED(rv)) return rv;

                if (isResourceElement) {
                    rv = CreateContainerContents(realKid, aChild, PR_FALSE,
                                                 nsnull /* don't care */,
                                                 nsnull /* don't care */);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            else {
                // Otherwise, just mark the XUL element as requiring
                // more work to be done. We'll get around to it when
                // somebody asks for it.
                nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(realKid);
                if (! xulcontent)
                    return NS_ERROR_UNEXPECTED;

                rv = xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
                if (NS_FAILED(rv)) return rv;
            }

            // We'll _already_ have added the unique elements; but if
            // it's -not- unique, then use the XUL sort service now to
            // append the element to the content model.
            if (! isUnique) {
                rv = NS_ERROR_UNEXPECTED;

                if (gXULSortService && isResourceElement) {
                    rv = gXULSortService->InsertContainerNode(mDB, &sortState,
                                                              mRoot, aResourceNode,
                                                              aRealNode, realKid,
                                                              aNotify);
                }

                if (NS_FAILED(rv)) {
                    rv = aRealNode->AppendChildTo(realKid, aNotify);
                    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to insert element");
                }
            }
        }
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::AddPersistentAttributes(nsIContent* aTemplateNode, nsIRDFResource* aResource, nsIContent* aRealNode)
{
    nsresult rv;

    nsAutoString persist;
    rv = aTemplateNode->GetAttribute(kNameSpaceID_None, kPersistAtom, persist);
    if (NS_FAILED(rv)) return rv;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE)
        return NS_OK;

    nsAutoString attribute;
    while (persist.Length() > 0) {
        attribute.Truncate();

        PRInt32 offset = persist.FindCharInSet(" ,");
        if (offset > 0) {
            persist.Left(attribute, offset);
            persist.Cut(0, offset + 1);
        }
        else {
            attribute = persist;
            persist.Truncate();
        }

        attribute.Trim(" ");

        if (attribute.Length() == 0)
            break;

        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> tag;
        rv = aTemplateNode->ParseAttributeString(attribute, *getter_AddRefs(tag), nameSpaceID);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> property;
        rv = gXULUtils->GetResource(nameSpaceID, tag, getter_AddRefs(property));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFNode> target;
        rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(target));
        if (NS_FAILED(rv)) return rv;

        if (! target)
            continue;

        nsCOMPtr<nsIRDFLiteral> value = do_QueryInterface(target);
        NS_ASSERTION(value != nsnull, "unable to stomach that sort of node");
        if (! value)
            continue;

        const PRUnichar* valueStr;
        rv = value->GetValueConst(&valueStr);
        if (NS_FAILED(rv)) return rv;

        rv = aRealNode->SetAttribute(nameSpaceID, tag, nsAutoString(valueStr), PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::CreateWidgetItem(nsIContent *aElement,
                                        nsIRDFResource *aProperty,
                                        nsIRDFResource *aChild,
                                        PRBool aNotify,
                                        nsIContent** aContainer,
                                        PRInt32* aNewIndexInContainer)
{
    nsresult rv;

    // Find the template appropriate for the current context
    nsCOMPtr<nsIContent> tmpl;
    rv = FindTemplate(aElement, aProperty, aChild, getter_AddRefs(tmpl));
    if (NS_FAILED(rv)) return rv;

    if (! tmpl) {
        // No template. Just bail.
        return NS_OK;
    }

    // ...and build content with it.
    rv = BuildContentFromTemplate(tmpl, aElement, aElement, PR_TRUE, aChild,
                                  aNotify, aContainer, aNewIndexInContainer);

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to build content from template");
    return rv;
}



nsresult
RDFGenericBuilderImpl::SynchronizeUsingTemplate(nsIContent* aTemplateNode,
                                                nsIContent* aRealElement,
                                                eUpdateAction aAction,
                                                nsIRDFResource* aProperty,
                                                nsIRDFNode* aValue)
{
    nsresult rv;

    // check all attributes on the template node; if they reference a resource,
    // update the equivalent attribute on the content node

    PRInt32    numAttribs;
    rv = aTemplateNode->GetAttributeCount(numAttribs);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        for (PRInt32 aLoop=0; aLoop<numAttribs; aLoop++) {
            PRInt32    attribNameSpaceID;
            nsCOMPtr<nsIAtom> attribName;
            rv = aTemplateNode->GetAttributeNameAt(aLoop,
                                                   attribNameSpaceID,
                                                   *getter_AddRefs(attribName));
            if (NS_FAILED(rv)) return rv;

            nsAutoString attribValue;
            rv = aTemplateNode->GetAttribute(attribNameSpaceID,
                                             attribName,
                                             attribValue);
            if (NS_FAILED(rv)) return rv;

            if (rv != NS_CONTENT_ATTR_HAS_VALUE)
                continue;

            if (attribValue.Find("rdf:") != 0)
                continue;

            // found an attribute which wants to bind its value
            // to RDF so look it up in the graph
            attribValue.Cut(0,4);

            nsCOMPtr<nsIRDFResource> property;
            rv = gRDFService->GetUnicodeResource(attribValue.GetUnicode(),
                                                 getter_AddRefs(property));
            if (NS_FAILED(rv)) return rv;

            if (property.get() == aProperty) {
                nsAutoString text("");

                rv = gXULUtils->GetTextForNode(aValue, text);
                if (NS_FAILED(rv)) return rv;

                if ((text.Length() > 0) && (aAction == eSet)) {
                    aRealElement->SetAttribute(attribNameSpaceID,
                                                   attribName,
                                                   text,
                                                   PR_TRUE);
                }
                else {
                    aRealElement->UnsetAttribute(attribNameSpaceID,
                                                     attribName,
                                                     PR_TRUE);
                }
            }
        }
    }

    // See if we've generated kids for this node yet. If we have, then
    // recursively sync up template kids with content kids
    PRBool contentsGenerated = PR_TRUE;
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aRealElement);
    if (xulcontent) {
        rv = xulcontent->GetLazyState(nsIXULContent::eTemplateContentsBuilt,
                                      contentsGenerated);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // HTML content will _always_ have been generated up-front
    }

    if (contentsGenerated) {
        PRInt32 count;
        rv = aTemplateNode->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 loop=0; loop<count; loop++) {
            nsCOMPtr<nsIContent> tmplKid;
            rv = aTemplateNode->ChildAt(loop, *getter_AddRefs(tmplKid));
            if (NS_FAILED(rv)) return rv;

            if (! tmplKid)
                break;

            nsCOMPtr<nsIContent> realKid;
            rv = aRealElement->ChildAt(loop, *getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            if (! realKid)
                break;

            rv = SynchronizeUsingTemplate(tmplKid, realKid, aAction, aProperty, aValue);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}



nsresult
RDFGenericBuilderImpl::RemoveWidgetItem(nsIContent* aElement,
                                        nsIRDFResource* aProperty,
                                        nsIRDFResource* aValue,
                                        PRBool aNotify)
{
    // This works as follows. It finds all of the elements in the
    // document that correspond to aValue. Any that are contained
    // within aElement are removed from their direct parent.
    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    rv = GetElementsForResource(aValue, elements);
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
        nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> child( do_QueryInterface(isupports) );
        NS_IF_RELEASE(isupports);

        if (! gXULUtils->IsContainedBy(child, aElement))
            continue;

        nsCOMPtr<nsIContent> parent;
        rv = child->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return rv;

        PRInt32 pos;
        rv = parent->IndexOf(child, pos);
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(pos >= 0, "parent doesn't think this child has an index");
        if (pos < 0) continue;

        rv = parent->RemoveChildAt(pos, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
            nsCOMPtr<nsIAtom> parentTag;
            rv = parent->GetTag(*getter_AddRefs(parentTag));
            if (NS_FAILED(rv)) return rv;

            nsAutoString parentTagStr;
            rv = parentTag->ToString(parentTagStr);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIAtom> childTag;
            rv = child->GetTag(*getter_AddRefs(childTag));
            if (NS_FAILED(rv)) return rv;

            nsAutoString childTagStr;
            rv = childTag->ToString(childTagStr);
            if (NS_FAILED(rv)) return rv;

            const char* resourceCStr;
            rv = aValue->GetValueConst(&resourceCStr);
            if (NS_FAILED(rv)) return rv;
            
            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("rdfgeneric[%p] remove-widget-item %s->%s [%s]",
                    this,
                    (const char*) nsCAutoString(parentTagStr),
                    (const char*) nsCAutoString(childTagStr),
                    resourceCStr));
        }
#endif
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::CreateTemplateAndContainerContents(nsIContent* aElement,
                                                          nsIContent** aContainer,
                                                          PRInt32* aNewIndexInContainer)
{
    // Generate both 1) the template content for the current element,
    // and 2) recursive subcontent (if the current element refers to a
    // container resource in the RDF graph).
    nsresult rv;

    // If we're asked to return the first generated child, then
    // initialize to "none".
    if (aContainer) {
        *aContainer = nsnull;
        *aNewIndexInContainer = -1;
    }

    // Create the current resource's contents from the template, if
    // appropriate
    nsAutoString templateID;
    rv = aElement->GetAttribute(kNameSpaceID_None, kTemplateAtom, templateID);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        rv = CreateTemplateContents(aElement, templateID, aContainer, aNewIndexInContainer);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIRDFResource> resource;
    rv = gXULUtils->GetElementRefResource(aElement, getter_AddRefs(resource));
    if (NS_SUCCEEDED(rv)) {
        // The element has a resource; that means that it corresponds
        // to something in the graph, so we need to go to the graph to
        // create its contents.
        rv = CreateContainerContents(aElement, resource, PR_FALSE, aContainer, aNewIndexInContainer);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::CreateContainerContents(nsIContent* aElement,
                                               nsIRDFResource* aResource,
                                               PRBool aNotify,
                                               nsIContent** aContainer,
                                               PRInt32* aNewIndexInContainer)
{
    // Create the contents of a container by iterating over all of the
    // "containment" arcs out of the element's resource.
    nsresult rv;

    if (aContainer) {
        *aContainer = nsnull;
        *aNewIndexInContainer = -1;
    }

    // The tree widget is special. If the item isn't open, then just
    // "pretend" that there aren't any contents here. We'll create
    // them when OpenContainer() gets called.
    if (IsTreeWidgetItem(aElement) && !IsOpen(aElement))
        return NS_OK;

    // See if the element's templates contents have been generated:
    // this prevents a re-entrant call from triggering another
    // generation.
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    if (xulcontent) {
        PRBool contentsGenerated;
        rv = xulcontent->GetLazyState(nsIXULContent::eContainerContentsBuilt, contentsGenerated);
        if (NS_FAILED(rv)) return rv;

        if (contentsGenerated)
            return NS_OK;

        // Now mark the element's contents as being generated so that
        // any re-entrant calls don't trigger an infinite recursion.
        rv = xulcontent->SetLazyState(nsIXULContent::eContainerContentsBuilt);
    }
    else {
        // HTML is always needs to be generated.
        //
        // XXX Big ass-umption here -- I am assuming that this will
        // _only_ ever get called (in the case of an HTML element)
        // when the XUL builder is descending thru the graph and
        // stumbles on a template that is rooted at an HTML element.
        // (/me crosses fingers...)
    }

    // XXX Eventually, we may want to factor this into one method that
    // handles RDF containers (RDF:Bag, et al.) and another that
    // handles multi-attributes. For performance...

    // Create a cursor that'll enumerate all of the outbound arcs
    nsCOMPtr<nsISimpleEnumerator> properties;
    rv = mDB->ArcLabelsOut(aResource, getter_AddRefs(properties));
    if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasMore;
        rv = properties->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = properties->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

        // If it's not a containment property, then it doesn't specify an
        // object that is member of the current container element;
        // rather, it specifies a "simple" property of the container
        // element. Skip it.
        if (! IsContainmentProperty(aElement, property))
            continue;

        // Create a second cursor that'll enumerate all of the values
        // for all of the arcs.
        nsCOMPtr<nsISimpleEnumerator> targets;
        rv = mDB->GetTargets(aResource, property, PR_TRUE, getter_AddRefs(targets));
        if (NS_FAILED(rv)) return rv;

        while (1) {
            PRBool hasMoreElements;
            rv = targets->HasMoreElements(&hasMoreElements);
            if (NS_FAILED(rv)) return rv;

            if (! hasMoreElements)
                break;

            nsCOMPtr<nsISupports> isupportsNext;
            rv = targets->GetNext(getter_AddRefs(isupportsNext));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRDFResource> target = do_QueryInterface(isupportsNext);
            NS_ASSERTION(target != nsnull, "not a resource");
            if (! target)
                continue;

            rv = CreateWidgetItem(aElement, property, target, aNotify, aContainer, aNewIndexInContainer);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create item");
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::CreateTemplateContents(nsIContent* aElement,
                                              const nsString& aTemplateID,
                                              nsIContent** aContainer,
                                              PRInt32* aNewIndexInContainer)
{
    // Create the contents of an element using the templates
    nsresult rv;

    // See if the element's templates contents have been generated:
    // this prevents a re-entrant call from triggering another
    // generation.
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    if (! xulcontent)
        return NS_OK; // HTML content is _always_ generated up-front

    PRBool contentsGenerated;
    rv = xulcontent->GetLazyState(nsIXULContent::eTemplateContentsBuilt, contentsGenerated);
    if (NS_FAILED(rv)) return rv;

    if (contentsGenerated)
        return NS_OK;

    // Now mark the element's contents as being generated so that
    // any re-entrant calls don't trigger an infinite recursion.
    rv = xulcontent->SetLazyState(nsIXULContent::eTemplateContentsBuilt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set template-contents-generated attribute");
    if (NS_FAILED(rv)) return rv;

    // Find the template node that corresponds to the "real" node for
    // which we're trying to generate contents.
    nsCOMPtr<nsIDOMXULDocument> xulDoc;
    xulDoc = do_QueryInterface(mDocument);
    if (! xulDoc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDOMElement> tmplNode;
    rv = xulDoc->GetElementById(aTemplateID, getter_AddRefs(tmplNode));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIContent> tmpl = do_QueryInterface(tmplNode);
    if (! tmpl)
        return NS_ERROR_UNEXPECTED;

    // Crawl up the content model until we find the "resource" element
    // that spawned this template.
    nsCOMPtr<nsIRDFResource> resource;

    nsCOMPtr<nsIContent> element = aElement;
    while (element) {
        rv = gXULUtils->GetElementRefResource(element, getter_AddRefs(resource));
        if (NS_SUCCEEDED(rv)) break;

        nsCOMPtr<nsIContent> parent;
        rv = element->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return rv;

        element = parent;
    }

    rv = BuildContentFromTemplate(tmpl, aElement, aElement, PR_FALSE, resource, PR_FALSE,
                                  aContainer, aNewIndexInContainer);

    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::EnsureElementHasGenericChild(nsIContent* parent,
                                                    PRInt32 nameSpaceID,
                                                    nsIAtom* tag,
                                                    PRBool aNotify,
                                                    nsIContent** result)
{
    nsresult rv;

    rv = gXULUtils->FindChildByTag(parent, nameSpaceID, tag, result);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_RDF_NO_VALUE) {
        // we need to construct a new child element.
        nsCOMPtr<nsIContent> element;

        rv = CreateElement(nameSpaceID, tag, getter_AddRefs(element));
        if (NS_FAILED(rv)) return rv;

        // XXX Note that the notification ensures we won't batch insertions! This could be bad! - Dave
        rv = parent->AppendChildTo(element, aNotify);
        if (NS_FAILED(rv)) return rv;

        *result = element;
        NS_ADDREF(*result);
        return NS_RDF_ELEMENT_GOT_CREATED;
    }
    else {
        return NS_RDF_ELEMENT_WAS_THERE;
    }
}


PRBool
RDFGenericBuilderImpl::IsContainmentProperty(nsIContent* aElement, nsIRDFResource* aProperty)
{
    // XXX is this okay to _always_ treat ordinal properties as tree
    // properties? Probably not...
    nsresult        rv;
    PRBool        isOrdinal;

    rv = gRDFContainerUtils->IsOrdinalProperty(aProperty, &isOrdinal);
    if (NS_FAILED(rv))
        return PR_FALSE;

    if (isOrdinal)
        return PR_TRUE;

    const char        *propertyURI;

    rv = aProperty->GetValueConst(&propertyURI);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get property URI");
    if (NS_FAILED(rv)) return PR_FALSE;

    PRUnichar		buffer[256];
    nsAutoString	containment(CBufDescriptor(buffer, PR_TRUE, sizeof(buffer) / sizeof(PRUnichar), 0));

    // rjc: Optimization: 99% of trees that use "containment='...'" put the
    // attribute on the root of the tree, so check that first
    rv = mRoot->GetAttribute(kNameSpaceID_None, kContainmentAtom, containment);
    if (NS_FAILED(rv)) return rv;
    if (rv == NS_CONTENT_ATTR_HAS_VALUE)
    {
        if (containment.Find(propertyURI) >= 0)
            return(PR_TRUE);
        else    return(PR_FALSE);
    }
/*
    // Walk up the content tree looking for the "containment"
    // attribute, so we can determine if the specified property
    // actually defines containment.
    nsCOMPtr<nsIContent> element( dont_QueryInterface(aElement) );
    while (element) {
        // Is this the "containment" property?
        rv = element->GetAttribute(kNameSpaceID_None, kContainmentAtom, containment);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get attribute value");
        if (NS_FAILED(rv)) return PR_FALSE;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            // Okay we've found the locally-scoped tree properties,
            // a whitespace-separated list of property URIs. So we
            // definitively know whether this is a tree property or
            // not.
            if (containment.Find(propertyURI) >= 0)
                return PR_TRUE;
            else
                return PR_FALSE;
        }

        nsCOMPtr<nsIContent> parent;
        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }
*/
    // If we get here, we didn't find any tree property: so now
    // defaults start to kick in.

#define TREE_PROPERTY_HACK
#if defined(TREE_PROPERTY_HACK)
    if ((aProperty == kNC_child) ||
        (aProperty == kNC_Folder) ||
        (aProperty == kRDF_child)) {
        return PR_TRUE;
    }
#endif // defined(TREE_PROPERTY_HACK)

    return PR_FALSE;
}


PRBool
RDFGenericBuilderImpl::IsIgnoredProperty(nsIContent* aElement, nsIRDFResource* aProperty)
{
    nsresult        rv;
    const char        *propertyURI;

    rv = aProperty->GetValueConst(&propertyURI);
    if (NS_FAILED(rv)) return(PR_FALSE);

    PRUnichar		buffer[256];
    nsAutoString	uri(CBufDescriptor(buffer, PR_TRUE, sizeof(buffer) / sizeof(PRUnichar), 0));
    PRInt32        	nameSpaceID;

    // rjc: Optimization: 99% of trees that use "ignore='...'" put the
    // attribute on the root of the tree, so check that first
    rv = mRoot->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return(PR_FALSE);
        // Never ever ask an HTML element about non-HTML attributes
        if (nameSpaceID != kNameSpaceID_HTML)
        {
            rv = mRoot->GetAttribute(kNameSpaceID_None, kIgnoreAtom, uri);
            if (NS_FAILED(rv)) return(PR_FALSE);
            if (rv == NS_CONTENT_ATTR_HAS_VALUE)
            {
        if (uri.Find(propertyURI) >= 0)
            return(PR_TRUE);
        else    return(PR_FALSE);
            }
        }
/*
    // Walk up the content tree looking for the "rdf:ignore"
    // attribute, so we can determine if the specified property should
    // be ignored.
    nsCOMPtr<nsIContent> element( dont_QueryInterface(aElement) );
    while (element) {
        rv = element->GetNameSpaceID(nameSpaceID);
        if (NS_FAILED(rv)) return(PR_FALSE);

        // Never ever ask an HTML element about non-HTML attributes
        if (nameSpaceID != kNameSpaceID_HTML) {
            rv = element->GetAttribute(kNameSpaceID_None, kIgnoreAtom, uri);
            if (NS_FAILED(rv)) return(PR_FALSE);

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                // Okay, we've found the locally-scoped ignore
                // properties, which is a whitespace-separated list of
                // property URIs. So we definitively know whether this
                // property should be ignored or not.
                if (uri.Find(propertyURI) >= 0)
                    return(PR_TRUE);
                else
                    return(PR_FALSE);
            }
        }

        nsCOMPtr<nsIContent> parent;
        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }
*/
    // Walked _all_ the way to the top and couldn't find anything to
    // ignore.
    return(PR_FALSE);
}

PRBool
RDFGenericBuilderImpl::IsContainer(nsIContent* aElement, nsIRDFResource* aResource)
{
    // Look at all of the arcs extending _out_ of the resource: if any
    // of them are that "containment" property, then we know we'll
    // have children.

    nsCOMPtr<nsISimpleEnumerator> arcs;
    nsresult rv;

    rv = mDB->ArcLabelsOut(aResource, getter_AddRefs(arcs));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get arcs out");
    if (NS_FAILED(rv))
        return PR_FALSE;

    while (1) {
        PRBool hasMore;
        rv = arcs->HasMoreElements(&hasMore);
        NS_ASSERTION(NS_SUCCEEDED(rv), "severe error advancing cursor");
        if (NS_FAILED(rv))
            return PR_FALSE;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = arcs->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv))
            return PR_FALSE;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

        if (! IsContainmentProperty(aElement, property))
            continue;

        return PR_TRUE;
    }

    return PR_FALSE;
}


PRBool
RDFGenericBuilderImpl::IsEmpty(nsIContent* aElement, nsIRDFResource* aContainer)
{
    // Look at all of the arcs extending _out_ of the resource: if any
    // of them are that "containment" property, then we know we'll
    // have children.

    nsCOMPtr<nsISimpleEnumerator> arcs;
    nsresult rv;

    rv = mDB->ArcLabelsOut(aContainer, getter_AddRefs(arcs));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get arcs out");
    if (NS_FAILED(rv))
        return PR_TRUE;

    while (1) {
        PRBool hasMore;
        rv = arcs->HasMoreElements(&hasMore);
        NS_ASSERTION(NS_SUCCEEDED(rv), "severe error advancing cursor");
        if (NS_FAILED(rv))
            return PR_TRUE;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = arcs->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv))
            return PR_TRUE;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

        if (! IsContainmentProperty(aElement, property))
            continue;

        // now that we know its a container, check to see if it's "empty"
        // by testing to see if the property has a target.
        nsCOMPtr<nsIRDFNode> dummy;
        rv = mDB->GetTarget(aContainer, property, PR_TRUE, getter_AddRefs(dummy));
        if (NS_FAILED(rv)) return rv;

        if (dummy)
            return PR_FALSE;
    }

    return PR_TRUE;
}


PRBool
RDFGenericBuilderImpl::IsOpen(nsIContent* aElement)
{
    nsresult rv;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return(PR_FALSE);

    // Treat the 'root' element as always open, -unless- it's a
    // menu/menupopup. We don't need to "fake" these as being open.
    if ((aElement == mRoot.get()) && (tag.get() != kMenuAtom))
      return(PR_TRUE);

    nsAutoString value;
    rv = aElement->GetAttribute(kNameSpaceID_None, kOpenAtom, value);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get open attribute");
    if (NS_FAILED(rv)) return(PR_FALSE);

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        if (value.Equals(trueStr))
            return(PR_TRUE);
    }

    return(PR_FALSE);
}


PRBool
RDFGenericBuilderImpl::IsElementInWidget(nsIContent* aElement)
{
    // Make sure that we're actually creating content for the tree
    // content model that we've been assigned to deal with.

    // Walk up the parent chain from us to the root and
    // see what we find.
    if (aElement == mRoot.get())
      return PR_TRUE;

    // walk up the tree until you find rootAtom
    nsCOMPtr<nsIContent> element(do_QueryInterface(aElement));
    nsCOMPtr<nsIContent> parent;
    element->GetParent(*getter_AddRefs(parent));
    element = parent;
    
    while (element) {
        
        if (element.get() == mRoot.get())
          return PR_TRUE;

        // up to the parent...
//        nsCOMPtr<nsIContent> parent;
        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }
    
    return PR_FALSE;
}

nsresult
RDFGenericBuilderImpl::GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource)
{
    // Given an nsIDOMNode that presumably has been created as a proxy
    // for an RDF resource, pull the RDF resource information out of
    // it.

    nsCOMPtr<nsIContent> element = do_QueryInterface(aNode);
    if (! element)
        return NS_ERROR_UNEXPECTED;

    return gXULUtils->GetElementRefResource(element, aResource);
}


nsresult
RDFGenericBuilderImpl::RemoveGeneratedContent(nsIContent* aElement)
{
    // Remove all the content beneath aElement that has been generated
    // from a template. We can identify this content because it'll
    // have a "template" attribute on it.

    nsresult rv;

    PRInt32 count;
    rv = aElement->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    while (--count >= 0) {
        nsCOMPtr<nsIContent> child;
        rv = aElement->ChildAt(count, *getter_AddRefs(child));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tmplID;
        rv = child->GetAttribute(kNameSpaceID_None, kTemplateAtom, tmplID);
        if (NS_FAILED(rv)) return rv;

        if (rv != NS_CONTENT_ATTR_HAS_VALUE)
            continue;

        // It's a generated element. Remove it, and set its document
        // to null so that it'll get knocked out of the XUL doc's
        // resource-to-element map.
        rv = aElement->RemoveChildAt(count, PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error removing child");

        rv = child->SetDocument(nsnull, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


PRBool
RDFGenericBuilderImpl::IsTreeWidgetItem(nsIContent* aElement)
{
    // Determine if this is a <tree> or a <treeitem> tag, in which
    // case, some special logic will kick in to force batched reflows.
    // XXX Should be removed when Bug 10818 is fixed.
    nsresult rv;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return PR_FALSE;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return PR_FALSE;

    // If we're building content under a <tree> or a <treeitem>,
    // then DO NOT notify layout until we're all done.
    if ((nameSpaceID == kNameSpaceID_XUL) &&
        ((tag.get() == kTreeAtom) || (tag.get() == kTreeItemAtom))) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }

}

PRBool
RDFGenericBuilderImpl::IsReflowScheduled()
{
    return (mTimer != nsnull);
}



nsresult
RDFGenericBuilderImpl::ScheduleReflow()
{
    NS_PRECONDITION(mTimer == nsnull, "reflow scheduled");
    if (mTimer)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;

    rv = NS_NewTimer(getter_AddRefs(mTimer));
    if (NS_FAILED(rv)) return rv;

    mTimer->Init(RDFGenericBuilderImpl::ForceTreeReflow, this, 100);
    NS_ADDREF(this); // the timer will hold a reference to the builder
    
    return NS_OK;
}


void
RDFGenericBuilderImpl::ForceTreeReflow(nsITimer* aTimer, void* aClosure)
{
    // XXX Any lifetime issues we need to deal with here???
    RDFGenericBuilderImpl* builder = NS_STATIC_CAST(RDFGenericBuilderImpl*, aClosure);

    nsresult rv;

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(builder->mDocument);

    // If we've been removed from the document, then this will have
    // been nulled out.
    if (! doc) return;

    // XXX What if kTreeChildrenAtom & kNameSpaceXUL are clobbered b/c
    // the generic builder has been destroyed?
    nsCOMPtr<nsIContent> treechildren;
    rv = gXULUtils->FindChildByTag(builder->mRoot,
                                   kNameSpaceID_XUL,
                                   kTreeChildrenAtom,
                                   getter_AddRefs(treechildren));

    if (NS_FAILED(rv)) return; // couldn't find

    rv = doc->ContentAppended(treechildren, 0);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to notify content appended");

    builder->mTimer = nsnull;
    NS_RELEASE(builder);
}


nsresult
RDFGenericBuilderImpl::AddDatabasePropertyToHTMLElement(nsIContent* aElement, nsIRDFCompositeDataSource* aDataBase)
{
    // Use XPConnect and the JS APIs to whack aDatabase as the
    // 'database' property onto aElement.
    nsresult rv;

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    NS_ASSERTION(doc != nsnull, "no document");
    if (! doc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIScriptGlobalObject> global;
    doc->GetScriptGlobalObject(getter_AddRefs(global));
    if (! global)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIScriptContext> context;
    global->GetContext(getter_AddRefs(context));
    if (! context)
        return NS_ERROR_UNEXPECTED;

    JSContext* jscontext = NS_STATIC_CAST(JSContext*, context->GetNativeContext());
    NS_ASSERTION(context != nsnull, "no jscontext");
    if (! jscontext)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIScriptObjectOwner> owner = do_QueryInterface(aElement);
    NS_ASSERTION(owner != nsnull, "unable to get script object owner");
    if (! owner)
         return NS_ERROR_UNEXPECTED;

    JSObject* jselement;
    rv = owner->GetScriptObject(context, (void**) &jselement);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element's script object");
    if (NS_FAILED(rv)) return rv;

    static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);
    NS_WITH_SERVICE(nsIXPConnect, xpc, kXPConnectCID, &rv);

    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
    rv = xpc->WrapNative(jscontext,                       
                         jselement,
                         aDataBase,
                         NS_GET_IID(nsIRDFCompositeDataSource),
                         getter_AddRefs(wrapper));

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to xpconnect-wrap database");
    if (NS_FAILED(rv)) return rv;

    JSObject* jsobj;
    rv = wrapper->GetJSObject(&jsobj);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get jsobj from xpconnect wrapper");
    if (NS_FAILED(rv)) return rv;

    jsval jsdatabase = OBJECT_TO_JSVAL(jsobj);

    PRBool ok;
    ok = JS_SetProperty(jscontext, jselement, "database", &jsdatabase);
    NS_ASSERTION(ok, "unable to set database property");
    if (! ok)
        return NS_ERROR_FAILURE;

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements)
{
    nsresult rv;

    const char *uri;
    rv = aResource->GetValueConst(&uri);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource URI");
    if (NS_FAILED(rv)) return rv;

    rv = mDocument->GetElementsForID(nsAutoString(uri), aElements);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to retrieve elements from resource");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::CreateElement(PRInt32 aNameSpaceID,
                                     nsIAtom* aTag,
                                     nsIContent** aResult)
{
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    if (! doc)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
    nsCOMPtr<nsIContent> result;

    if (aNameSpaceID == kNameSpaceID_XUL) {
        rv = nsXULElement::Create(aNameSpaceID, aTag, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else if (aNameSpaceID == kNameSpaceID_HTML) {
        const PRUnichar *tagName;
        aTag->GetUnicode(&tagName);

        rv = gHTMLElementFactory->CreateInstanceByTag(tagName, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;
    }
    else {
        const PRUnichar *tagName;
        aTag->GetUnicode(&tagName);

        nsCOMPtr<nsIElementFactory> elementFactory;
        GetElementFactory(aNameSpaceID, getter_AddRefs(elementFactory));
        rv = elementFactory->CreateInstanceByTag(tagName, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;
    }

    // 
    nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(result);
    if (formControl) {
        nsCOMPtr<nsIDOMHTMLFormElement> form;
        rv = mDocument->GetForm(getter_AddRefs(form));
        if (NS_SUCCEEDED(rv) && form)
            formControl->SetForm(form);
    }
    
    rv = result->SetDocument(doc, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set element's document");
    if (NS_FAILED(rv)) return rv;

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::SetEmpty(nsIContent *element, PRBool empty)
{
    nsresult rv;

    nsAutoString newEmptyStr(empty ? trueStr : falseStr);
    nsAutoString emptyStr;

    rv = element->GetAttribute(kNameSpaceID_None, kEmptyAtom, emptyStr);
    if (NS_FAILED(rv)) return rv;

    if ((rv != NS_CONTENT_ATTR_HAS_VALUE) || (! emptyStr.Equals(newEmptyStr))) {
        rv = element->SetAttribute(kNameSpaceID_None, kEmptyAtom, newEmptyStr, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;

}

void 
RDFGenericBuilderImpl::GetElementFactory(PRInt32 aNameSpaceID, nsIElementFactory** aResult)
{
  nsresult rv;
  nsAutoString nameSpace;
  gNameSpaceManager->GetNameSpaceURI(aNameSpaceID, nameSpace);

  nsCAutoString progID = NS_ELEMENT_FACTORY_PROGID_PREFIX;
  progID.Append(nameSpace);

  // Retrieve the appropriate factory.
  NS_WITH_SERVICE(nsIElementFactory, elementFactory, progID, &rv);

  if (!elementFactory)
    elementFactory = gXMLElementFactory; // Nothing found. Use generic XML element.

  *aResult = elementFactory;
  NS_IF_ADDREF(*aResult);
}

#ifdef PR_LOGGING
nsresult
RDFGenericBuilderImpl::Log(const char* aOperation,
                           nsIContent* aElement, 
                           nsIRDFResource* aSource,
                           nsIRDFResource* aProperty,
                           nsIRDFNode* aTarget)
{
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        nsresult rv;

        const char* sourceStr;
        rv = aSource->GetValueConst(&sourceStr);
        if (NS_FAILED(rv)) return rv;

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfgeneric[%p] %8s [%s]--", this, aOperation, sourceStr));

        const char* propertyStr;
        rv = aProperty->GetValueConst(&propertyStr);
        if (NS_FAILED(rv)) return rv;

        nsAutoString targetStr;
        rv = gXULUtils->GetTextForNode(aTarget, targetStr);
        if (NS_FAILED(rv)) return rv;

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("                       --[%s]-->[%s]",
                propertyStr,
                (const char*) nsCAutoString(targetStr)));

        nsCOMPtr<nsIAtom> tag;
        rv = aElement->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagname;
        rv = tag->ToString(tagname);
        if (NS_FAILED(rv)) return rv;

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("               %s(%p)",
                (const char*) nsCAutoString(tagname),
                aElement));
    }
    return NS_OK;
}
#endif
