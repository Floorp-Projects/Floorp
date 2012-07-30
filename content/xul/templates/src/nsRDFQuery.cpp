/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsCOMPtr.h"

#include "nsXULTemplateQueryProcessorRDF.h"
#include "nsRDFQuery.h"

NS_IMPL_CYCLE_COLLECTION_1(nsRDFQuery, mQueryNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsRDFQuery)
  NS_INTERFACE_MAP_ENTRY(nsITemplateRDFQuery)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsRDFQuery)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsRDFQuery)

void
nsRDFQuery::Finish()
{
    // the template builder is going away and the query processor likely as
    // well. Clear the reference to avoid calling it.
    mProcessor = nullptr;
    mCachedResults = nullptr;
}

nsresult
nsRDFQuery::SetCachedResults(nsXULTemplateQueryProcessorRDF* aProcessor,
                             const InstantiationSet& aInstantiations)
{
    mCachedResults = new nsXULTemplateResultSetRDF(aProcessor, this, &aInstantiations);
    if (! mCachedResults)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}


void
nsRDFQuery::UseCachedResults(nsISimpleEnumerator** aResults)
{
    *aResults = mCachedResults;
    NS_IF_ADDREF(*aResults);

    mCachedResults = nullptr;
}
