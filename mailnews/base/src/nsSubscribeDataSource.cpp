/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s): 
 * Seth Spitzer <sspitzer@netscape.com>
 */

#include "nsSubscribeDataSource.h"

#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"

static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsSubscribeDataSource::nsSubscribeDataSource()
{
	NS_INIT_REFCNT();
}

nsSubscribeDataSource::~nsSubscribeDataSource()
{
	// do nothing
}

NS_IMPL_THREADSAFE_ADDREF(nsSubscribeDataSource);
NS_IMPL_THREADSAFE_RELEASE(nsSubscribeDataSource);

NS_IMPL_QUERY_INTERFACE1(nsSubscribeDataSource, nsIRDFDataSource) 

nsresult
nsSubscribeDataSource::Init()
{
	nsresult rv;
	mInner = do_CreateInstance(kRDFInMemoryDataSourceCID, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!mInner) return NS_ERROR_FAILURE;

	return NS_OK;
}
