/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

#include "rdf_util.h"

#include "ns_globals.h" // for prLogModuleInfo
#include "nsString.h"

#include "nsIServiceManager.h"

#include "nsRDFCID.h" // for NS_RDFCONTAINER_CID

#include "prlog.h" // for PR_ASSERT

static PRBool rdf_inited = PR_FALSE;

nsCOMPtr<nsIRDFContainerUtils> gRDFCU = nsnull;
nsCOMPtr<nsIRDFService> gRDF = nsnull;
nsCOMPtr<nsIBookmarksService> gBookmarks = nsnull; // PENDING(edburns): this should be in WebclientContext
nsCOMPtr<nsIRDFDataSource> gBookmarksDataSource = nsnull;

nsCOMPtr<nsIRDFResource> kNC_BookmarksRoot = nsnull;
nsCOMPtr<nsIRDFResource> kNC_Name = nsnull;
nsCOMPtr<nsIRDFResource> kNC_URL = nsnull;
nsCOMPtr<nsIRDFResource> kNC_parent = nsnull;
nsCOMPtr<nsIRDFResource> kNC_Folder = nsnull;
nsCOMPtr<nsIRDFResource> kRDF_type = nsnull;

nsCOMPtr<nsIRDFResource> kNewFolderCommand;
nsCOMPtr<nsIRDFResource> kNewBookmarkCommand;

static NS_DEFINE_CID(kRDFContainerCID, NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,       NS_RDFCONTAINERUTILS_CID);

nsresult rdf_startup()
{
    nsresult rv = NS_ERROR_FAILURE;

    if (rdf_inited) {
        return NS_OK;
    }

    // Init the global instances
    
    if (nsnull == gBookmarks) {
        // Get the bookmarks service
        gBookmarks = do_GetService(NS_BOOKMARKS_SERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) {
            return rv;
        }
#ifdef _WIN32

        // PENDING(edburns): HACK workaround for bug 59530.  This
        // workaround forces the nsBookmarksService to leak so that it
        // never gets destructed, thus the timer never gets canceled and
        // thus the fact that the static nsCOMPtr instance has gone away
        // doesn't matter.

        nsIBookmarksService *raw = (nsIBookmarksService *) gBookmarks.get();
        raw->AddRef();
#endif
    }

    if (nsnull == gBookmarksDataSource) {
        // get the bookmarks service as an RDFDataSource
        rv = gBookmarks->QueryInterface(NS_GET_IID(nsIRDFDataSource),
                                        getter_AddRefs(gBookmarksDataSource));
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    if (nsnull == gRDF) {
        // get the RDF service
        gRDF = do_GetService(kRDFServiceCID, &rv);
        if (NS_FAILED(rv)) {
            return rv;
        }
    }
    
    if (nsnull == gRDFCU) {
        // get the RDF service
        gRDFCU = do_GetService(kRDFContainerUtilsCID, &rv);
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    // init the properties
    // find the nsIRDFResource for the bookmarks
    if (nsnull == kNC_BookmarksRoot) {
        rv = gRDF->GetResource(nsDependentCString(BOOKMARKS_URI),
                               getter_AddRefs(kNC_BookmarksRoot));
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    if (nsnull == kNC_Name) {
        rv = gRDF->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#Name"),
                               getter_AddRefs(kNC_Name));
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    if (nsnull == kNC_URL) {
        rv = gRDF->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#URL"),
                               getter_AddRefs(kNC_URL));
        if (NS_FAILED(rv)) {
            return rv;
        }
    }
    if (nsnull == kNC_parent) {
        rv = gRDF->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#parent"),
                               getter_AddRefs(kNC_parent));
        if (NS_FAILED(rv)) {
            return rv;
        }
    }
    if (nsnull == kNC_Folder) {
      rv = gRDF->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#Folder"),
                            getter_AddRefs(kNC_Folder));
      if (NS_FAILED(rv)) {
	return rv;
      }
    }
  
    if (nsnull == kRDF_type) {
      rv = gRDF->GetResource(NS_LITERAL_CSTRING("http://www.w3.org/1999/02/22-rdf-syntax-ns"),
			     getter_AddRefs(kRDF_type));
      if (NS_FAILED(rv)) {
	return rv;
      }
    }     
    if (nsnull == kNewFolderCommand) {
        rv = gRDF->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#command?cmd=newfolder"),
                               getter_AddRefs(kNewFolderCommand));
        if (NS_FAILED(rv)) {
            return rv;
        }
    }
    if (nsnull == kNewBookmarkCommand) {
        rv = gRDF->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#command?cmd=newbookmark"),
                               getter_AddRefs(kNewBookmarkCommand));
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    rv = gBookmarks->ReadBookmarks(&rdf_inited);

    return rv;
}

nsresult rdf_shutdown() 
{
    kNewBookmarkCommand = nsnull;

    kNewFolderCommand = nsnull;

    kRDF_type = nsnull;

    kNC_Folder = nsnull;

    kNC_parent = nsnull;

    kNC_URL = nsnull;

    kNC_Name = nsnull;

    kNC_BookmarksRoot = nsnull;

    gRDFCU = nsnull;

    gRDF = nsnull;

    gBookmarksDataSource = nsnull;

#ifdef _WIN32

    nsIBookmarksService *raw = (nsIBookmarksService *) gBookmarks.get();
    raw->Release();
#endif
    
    gBookmarks = nsnull;

    return NS_OK;
}



void rdf_recursiveResourceTraversal(nsCOMPtr<nsIRDFResource> currentResource)
{
    nsresult rv;
    PRBool result;
    const PRUnichar *textForNode = nsnull;
    nsCOMPtr<nsISupports> supportsResult;
    nsCOMPtr<nsIRDFNode> node;
    nsCOMPtr<nsIRDFLiteral> literal;
    nsCOMPtr<nsIRDFResource> childResource;
    nsCOMPtr<nsIRDFContainer> container;
    nsCOMPtr<nsISimpleEnumerator> elements;
        
    // ASSERT(nsnull != gRDFCU)

    rv = gRDFCU->IsContainer(gBookmarksDataSource, currentResource, 
                             &result);

    if (result) {
        // It's a folder
        
        rdf_printArcLabels(currentResource);

        // see if it has a name target

        rv = gBookmarksDataSource->GetTarget(currentResource,
                                                    kNC_Name, PR_TRUE, 
                                                    getter_AddRefs(node));
        
        if (NS_SUCCEEDED(rv)) {
            // if so, make sure it's an nsIRDFLiteral
            rv = node->QueryInterface(NS_GET_IID(nsIRDFLiteral), 
                                      getter_AddRefs(literal));
            if (NS_SUCCEEDED(rv)) {
                rv = literal->GetValueConst(&textForNode);
            }
            else {
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("recursiveResourceTraversal: node is not an nsIRDFLiteral.\n"));
                }
            }
            
        } 
        else {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("recursiveResourceTraversal: can't get name from currentResource.\n"));
            }
        }
            
        // get a container in order to recurr
        container = do_CreateInstance(kRDFContainerCID);
        if (!container) {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("recursiveResourceTraversal: can't get a new container\n"));
            }
            return;
        }
        
        rv = container->Init(gBookmarksDataSource, currentResource);
        if (NS_FAILED(rv)) {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("recursiveResourceTraversal: can't init container\n"));
            }
            return;
        }
        
        // go over my children
        rv = container->GetElements(getter_AddRefs(elements));

        if (NS_FAILED(rv)) {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("recursiveResourceTraversal: can't get elements from folder\n"));
            }
            return;
        }

        rv = elements->HasMoreElements(&result);
        if (NS_FAILED(rv)) {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("recursiveResourceTraversal: folder %s has no children.\n",
                        textForNode)); 
            }
            return;
        }
        while (result) {
            rv = elements->GetNext(getter_AddRefs(supportsResult));
            if (NS_FAILED(rv)) {
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("Exception: Can't get next from enumerator.\n"));
                }
                return;
            }

            // make sure it's an RDFResource
            rv = supportsResult->QueryInterface(NS_GET_IID(nsIRDFResource), 
                                                getter_AddRefs(childResource));
            if (NS_FAILED(rv)) {
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("Exception: next from enumerator is not an nsIRDFResource.\n"));
                }
                return;
            }

            rdf_recursiveResourceTraversal(childResource);

            rv = elements->HasMoreElements(&result);
            if (NS_FAILED(rv)) {
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("Exception: can't tell if we have more elements.\n")); 
                }
                return;
            }
        }
    }
    else {
        // It's a bookmark
        printf("BOOKMARK:\n");
        rdf_printArcLabels(currentResource);

        // see if it has a URL target

        rv = gBookmarksDataSource->GetTarget(currentResource,
                                                    kNC_URL, PR_TRUE, 
                                                    getter_AddRefs(node));
        if (NS_FAILED(rv)) {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("recursiveResourceTraversal: can't get url from currentResource\n"));
            }
            return;
        }
        
        // if so, make sure it's an nsIRDFLiteral
        rv = node->QueryInterface(NS_GET_IID(nsIRDFLiteral), 
                                  getter_AddRefs(literal));
        if (NS_FAILED(rv)) {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("recursiveResourceTraversal: node is not an nsIRDFLiteral.\n"));
            }
            return;
        }

        // get the value of the literal
        rv = literal->GetValueConst(&textForNode);
        if (NS_FAILED(rv)) {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("recursiveResourceTraversal: node doesn't have a value.\n"));
            }
            return;
        }

#if DEBUG_RAPTOR_CANVAS
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("\turl: %S\n", textForNode));
        }
#endif
        
    }
    
}

void rdf_printArcLabels(nsCOMPtr<nsIRDFResource> currentResource)
{
    if (!currentResource) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, ("resource: null\n"));
        }
        return;
    }
        
    nsCOMPtr<nsISimpleEnumerator> labels;
    nsCOMPtr<nsISupports> supportsResult;
    nsCOMPtr<nsIRDFResource> resourceResult;
    nsresult rv;
    PRBool result;
    const char *arcLabel;
    const char *currentResourceValue;

    rv = currentResource->GetValueConst(&currentResourceValue);
    if (NS_SUCCEEDED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("resource: %s\n", currentResourceValue));
        }
    }
    else {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("printArcLabels: can't get value from current resource.\n"));
        }
    }

    rv = gBookmarksDataSource->ArcLabelsOut(currentResource, 
                                            getter_AddRefs(labels));

    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("printArcLabels: currentResource has no outgoing arcs.\n"));
        }
        return;
    }
    
    rv = labels->HasMoreElements(&result);
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("printArcLabels: can't get elements from currentResource's enum.\n"));
        }
        return;
    }

    while (result) {
        rv = labels->GetNext(getter_AddRefs(supportsResult));
        if (NS_FAILED(rv)) {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("printArcLabels: Can't get next arc from enumerator.\n"));
            }
            return;
        }
        
        // make sure it's an RDFResource
        rv = supportsResult->QueryInterface(NS_GET_IID(nsIRDFResource), 
                                            getter_AddRefs(resourceResult));
        if (NS_SUCCEEDED(rv)) {
            rv = resourceResult->GetValueConst(&arcLabel);
            if (NS_SUCCEEDED(rv)) {
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("\tarc label: %s\n", arcLabel));
                }
            }
            else {
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("printArcLabels: can't get value from current arc.\n"));
                }
            }
        }
        else {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("printArcLabels: next arc from enumerator is not an nsIRDFResource.\n"));
            }
        }
        rv = labels->HasMoreElements(&result);
        if (NS_FAILED(rv)) {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("printArcLabels: can't get elements from currentResource's enum.\n"));
            }
            return;
        }
    }
}

nsresult rdf_getChildAt(int index, nsIRDFResource *theParent,
                        nsIRDFResource **retval)
{
    // Since there's no getChildAt type thing in RDF, we just get an
    // enumeration and get the indexth child.
    nsCOMPtr<nsIRDFResource> parent = theParent;
    nsCOMPtr<nsIRDFContainer> container;
    nsCOMPtr<nsISimpleEnumerator> elements;
    nsCOMPtr<nsISupports> supportsResult;
    nsCOMPtr<nsIRDFResource> childResource;
    nsresult rv;
    PRBool result, hasChildAtIndex = PR_FALSE;
    *retval = nsnull;
    PRInt32 i = 0;

    rv = gRDFCU->IsContainer(gBookmarksDataSource, parent, 
                             &result);
    if (NS_FAILED(rv)) {
        return rv;
    }

    if (PR_FALSE == result) {
        return NS_OK;
    }

    container = do_CreateInstance(kRDFContainerCID);
    if (!container) {
        return rv;
    }
    
    rv = container->Init(gBookmarksDataSource, parent);
    if (NS_FAILED(rv)) {
        return rv;
    }

    rv = container->GetElements(getter_AddRefs(elements));
    
    if (NS_FAILED(rv)) {
        return rv;
    }
    
    rv = elements->HasMoreElements(&result);
    if (NS_FAILED(rv)) {
        return rv;
    }
    while (result) {
        rv = elements->GetNext(getter_AddRefs(supportsResult));
        if (NS_FAILED(rv)) {
            return rv;
        }

        if (index == i) {
            hasChildAtIndex = PR_TRUE;
            break;
        }
        i++;
        rv = elements->HasMoreElements(&result);
        if (NS_FAILED(rv)) {
            return rv;
        }
    }
    if (hasChildAtIndex) {
        rv = supportsResult->QueryInterface(NS_GET_IID(nsIRDFResource), 
                                            getter_AddRefs(childResource));
        if (NS_FAILED(rv)) {
            return rv;
        }
        *retval = childResource;
    }
    return NS_OK;
}

nsresult rdf_getChildCount(nsIRDFResource *theParent, PRInt32 *count)
{
    nsresult rv;
    nsCOMPtr<nsIRDFResource> parent = theParent;
    nsCOMPtr<nsIRDFContainer> container;
    PRBool result;
    *count = 0;

    rv = gRDFCU->IsContainer(gBookmarksDataSource, parent, 
                             &result);
    if (NS_FAILED(rv)) {
        return rv;
    }
    
    if (PR_FALSE == result) {
        return NS_OK;
    }
    container = do_CreateInstance(kRDFContainerCID);
    if (!container) {
        return rv;
    }
    
    rv = container->Init(gBookmarksDataSource, parent);
    if (NS_FAILED(rv)) {
        return rv;
    }


    rv = container->GetCount(count);
    if (NS_FAILED(rv)) {
        return rv;
    }
    return NS_OK;
}

nsresult rdf_getIndexOfChild(nsIRDFResource *theParent, 
                             nsIRDFResource *theChild, 
                             PRInt32 *index)
{
    nsCOMPtr<nsIRDFResource> parent = theParent;
    nsCOMPtr<nsIRDFResource> child = theChild;
    nsCOMPtr<nsIRDFContainer> container;
    nsCOMPtr<nsISimpleEnumerator> elements;
    nsCOMPtr<nsISupports> supportsResult;
    nsresult rv;
    PRBool result;
    *index = -1;
    PRInt32 i = 0;

    rv = gRDFCU->IsContainer(gBookmarksDataSource, parent, 
                             &result);
    if (NS_FAILED(rv)) {
        return rv;
    }

    if (PR_FALSE == result) {
        return NS_OK;
    }
    container = do_CreateInstance(kRDFContainerCID);
    if (container) {
        return rv;
    }
    
    rv = container->Init(gBookmarksDataSource, parent);
    if (NS_FAILED(rv)) {
        return rv;
    }

    rv = container->GetElements(getter_AddRefs(elements));
    
    if (NS_FAILED(rv)) {
        return rv;
    }
    
    rv = elements->HasMoreElements(&result);
    if (NS_FAILED(rv)) {
        return rv;
    }
    while (result) {
        rv = elements->GetNext(getter_AddRefs(supportsResult));
        if (NS_FAILED(rv)) {
            return rv;
        }

        if (supportsResult == child) {
            *index = i;
            rv = NS_OK;
            break;
        }
        i++;
        rv = elements->HasMoreElements(&result);
        if (NS_FAILED(rv)) {
            return rv;
        }
    }
    return rv;
}
