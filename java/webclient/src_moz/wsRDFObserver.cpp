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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 *
 * Contributor(s): Ed Burns <edburns@acm.org>
 */

#include "wsRDFObserver.h"

#include "rdf_util.h"

#include "plstr.h" // for PL_strstr

///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation

NS_IMPL_ADDREF(wsRDFObserver)
NS_IMPL_RELEASE(wsRDFObserver)

NS_INTERFACE_MAP_BEGIN(wsRDFObserver)
	NS_INTERFACE_MAP_ENTRY(nsISupports)
	NS_INTERFACE_MAP_ENTRY(nsIRDFObserver)
NS_INTERFACE_MAP_END

wsRDFObserver::wsRDFObserver() 
{
    NS_INIT_ISUPPORTS();
}

wsRDFObserver::~wsRDFObserver() {}

// nsIRDFObserver implementation

NS_IMETHODIMP wsRDFObserver::OnAssert(nsIRDFDataSource *aDataSource, 
                                      nsIRDFResource *aSource, 
                                      nsIRDFResource *aProperty, 
                                      nsIRDFNode *aTarget)
{
    nsresult rv;
    nsCOMPtr<nsIRDFResource> nodeResource;
    PRBool isContainer;

    nodeResource = do_QueryInterface(aTarget);
    if (nodeResource) {
        rv = gRDFCU->IsContainer(gBookmarksDataSource, nodeResource, 
                                 &isContainer);
        if (isContainer) {
            folder = nodeResource;
        }
    }

    return NS_OK;
}

nsIRDFResource *wsRDFObserver::getFolder()
{
    return folder;
}

NS_IMETHODIMP wsRDFObserver::OnUnassert(nsIRDFDataSource *aDataSource, 
                                        nsIRDFResource *aSource, 
                                        nsIRDFResource *aProperty, 
                                        nsIRDFNode *aTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP wsRDFObserver::OnChange(nsIRDFDataSource *aDataSource, 
                                      nsIRDFResource *aSource, 
                                      nsIRDFResource *aProperty, 
                                      nsIRDFNode *aOldTarget, 
                                      nsIRDFNode *aNewTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP wsRDFObserver::OnMove(nsIRDFDataSource *aDataSource, 
                                    nsIRDFResource *aOldSource, 
                                    nsIRDFResource *aNewSource, 
                                    nsIRDFResource *aProperty, 
                                    nsIRDFNode *aTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP wsRDFObserver::BeginUpdateBatch(nsIRDFDataSource *aDataSource)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP wsRDFObserver::EndUpdateBatch(nsIRDFDataSource *aDataSource)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


