/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Denis Sharypov <sdv@sparc.spb.su>
 */
#include "bcIDOMAccessor.h"

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsIServiceManager.h"
#include "nsCURILoader.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIDocument.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMDocument.h"
#include "nsIDocShell.h"
#include "bcNode.h"
#include "bcDocument.h"
#include "bcElement.h"

//  #include "nsIEnumerator.h"
//  #include "stdlib.h"

static nsIDOMDocument* GetDocument(nsIDocumentLoader* loader);
#if defined(DEBUG)
#include <stdio.h>
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNamedNodeMap.h"
#endif

static NS_DEFINE_IID(kIDocShellIID, NS_IDOCSHELL_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENT_LOADER_OBSERVER_IID);

/* 151dd030-9ed0-11d4-a983-00105ae3801e */
#define BC_DOM_ACCESSOR_CID \
{0x151dd030, 0x9ed0, 0x11d4, \
{0xa9, 0x83, 0x00, 0x10, 0x5a, 0xe3, 0x80, 0x1e}}

static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);

class bcDOMAccessor : public bcIDOMAccessor, public nsIDocumentLoaderObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_BCIDOMACCESSOR
  NS_DECL_NSIDOCUMENTLOADEROBSERVER
      
  bcDOMAccessor(bcIDOMAccessor* accessor);
    virtual ~bcDOMAccessor();
    /* additional members */
private:
    bcIDOMAccessor* accessor;  
    
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(bcDOMAccessor, bcIDOMAccessor)

bcDOMAccessor::bcDOMAccessor(bcIDOMAccessor* accessor)
{
  NS_INIT_ISUPPORTS();
  this->accessor = accessor;
  /* member initializers and constructor code */
}

bcDOMAccessor::~bcDOMAccessor()
{
  /* destructor code */
}

NS_IMETHODIMP bcDOMAccessor::EndDocumentLoad(const char *url, Document* doc)
//  NS_IMETHODIMP bcDOMAccessor::EndDocumentLoad(const char *url, Node *node, Element *elem)
{
    // wrap bcDOMDocumentImpl(doc)
  // addref(bcDOMDocumentImpl)
    if (accessor)
        accessor->EndDocumentLoad(url, doc);
//          accessor->EndDocumentLoad(url, node, elem);
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onStartDocumentLoad (in nsIDocumentLoader aLoader, in nsIURI aURL, in string aCommand); */
NS_IMETHODIMP bcDOMAccessor::OnStartDocumentLoad(nsIDocumentLoader *aLoader, nsIURI *aURL, const char *aCommand)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onEndDocumentLoad (in nsIDocumentLoader loader, in nsIChannel aChannel, in unsigned long aStatus); */
NS_IMETHODIMP bcDOMAccessor::OnEndDocumentLoad(nsIDocumentLoader *loader, nsIChannel *aChannel, PRUint32 aStatus)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onStartURLLoad (in nsIDocumentLoader aLoader, in nsIChannel channel); */
NS_IMETHODIMP bcDOMAccessor::OnStartURLLoad(nsIDocumentLoader *aLoader, nsIChannel *channel)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onProgressURLLoad (in nsIDocumentLoader aLoader, in nsIChannel aChannel, in unsigned long aProgress, in unsigned long aProgressMax); */
NS_IMETHODIMP bcDOMAccessor::OnProgressURLLoad(nsIDocumentLoader *aLoader, nsIChannel *aChannel, PRUint32 aProgress, PRUint32 aProgressMax)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void onStatusURLLoad (in nsIDocumentLoader loader, in nsIChannel channel, in nsStringRef aMsg); */
NS_IMETHODIMP bcDOMAccessor::OnStatusURLLoad(nsIDocumentLoader *loader, nsIChannel *channel, nsString & aMsg)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



/* void onEndURLLoad (in nsIDocumentLoader aLoader, 
                      in nsIChannel aChannel, 
                      in unsigned long aStatus); */

NS_IMETHODIMP bcDOMAccessor::OnEndURLLoad(nsIDocumentLoader *loader, 
                                          nsIChannel *channel, 
                                          PRUint32 aStatus)
{
  char* urlSpec = (char*) "";
  nsIURI* url = nsnull;
  if (channel && NS_SUCCEEDED(channel->GetURI(&url)))
    url->GetSpec(&urlSpec);

  nsIDOMDocument* domDoc = GetDocument(loader);
  nsIDOMElement* docEl;
  domDoc->GetDocumentElement(&docEl);
  nsIDOMNode* node = nsnull;
  nsresult rv = domDoc->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)&node);
  // nb: error check
  EndDocumentLoad(urlSpec, new bcDocument(domDoc));
  return NS_OK;
}

void test() {
    printf("--DOMAccessor test start\n");
    nsresult rv = NS_OK; 
    bcIDOMAccessor *accessor = NULL;
    rv = nsComponentManager::CreateInstance("bcDOMAccessor",
					   nsnull,
					   NS_GET_IID(bcIDOMAccessor),
					   (void**)&accessor);
    if (NS_FAILED(rv)) {
        printf(" === [debug] can not load bcDOMAccessor\n");
        return;
    }
    NS_WITH_SERVICE(nsIDocumentLoader, docLoaderService, kDocLoaderServiceCID, &rv);
    if (NS_FAILED(rv) || !docLoaderService) {
        printf("=== no doc loader found...\n");
    } else {
        rv = docLoaderService->AddObserver((nsIDocumentLoaderObserver*)(new bcDOMAccessor(accessor)));
        if (NS_FAILED(rv)) {
            printf("=== AddObserver(DOMAccessor) failed x\n", rv);
        }
    }
    printf("--DOMAccessor test end\n");
}

static int counter = 0;  //we do not need to call it on unload time;
extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *compMgr,
                                          nsIFile *location,
                                          nsIModule** result)  //I am using it for runnig test *only*
{
  if (counter == 0) {
    counter ++;
    printf("--DOMAccessor before test\n");
    test();
    printf("--DOMAccessor after test\n");
  }
  return NS_ERROR_FAILURE;
}

static nsIDOMDocument* GetDocument(nsIDocumentLoader* loader)
{
    nsIDocShell* docshell = nsnull;
    nsISupports* container = nsnull;
    nsIContentViewer* contentv = nsnull;
    nsIDocumentViewer* docv = nsnull;
    nsIDocument* document = nsnull;
    nsIDOMDocument* domDoc = nsnull;
    
    nsresult rv = loader->GetContainer(&container);
    if (NS_SUCCEEDED(rv) && container) {
        rv = container->QueryInterface(kIDocShellIID, (void**) &docshell);
        container->Release();
        if (NS_SUCCEEDED(rv) && docshell) {
            rv = docshell->GetContentViewer(&contentv);
            docshell->Release();
            if (NS_SUCCEEDED(rv) && contentv) {
                rv = contentv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
                contentv->Release();	
                if (NS_SUCCEEDED(rv) && docv) {
                    rv = docv->GetDocument(document);
                    docv->Release();
                    if (NS_SUCCEEDED(rv) && document) {
                        rv = document->QueryInterface(kIDOMDocumentIID, (void**) &domDoc);
                        if (NS_SUCCEEDED(rv) && docv) {
                            return domDoc;
                        }
                    }
                }
            }
        }
    }
    
    fprintf(stderr, 
            "=== GetDocument: failed: "
            "container=%x, webshell=%x, contentViewer=%x, "
            "documentViewer=%x, document=%x, "
            "domDocument=%x, error=%x\n", 
            (unsigned) (void*) container, 
            (unsigned) (void*) docshell, 
            (unsigned) (void*) contentv, 
            (unsigned) (void*) docv, 
            (unsigned) (void*) document, 
            (unsigned) (void*) domDoc, 
            rv);
  return NULL;
}
