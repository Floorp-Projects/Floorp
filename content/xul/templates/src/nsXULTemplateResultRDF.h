/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULTemplateResultRDF_h__
#define nsXULTemplateResultRDF_h__

#include "nsCOMPtr.h"
#include "nsIRDFResource.h"
#include "nsXULTemplateQueryProcessorRDF.h"
#include "nsRDFQuery.h"
#include "nsRuleNetwork.h"
#include "nsIXULTemplateResult.h"
#include "nsRDFBinding.h"
#include "mozilla/Attributes.h"

/**
 * A single result of a query on an RDF graph
 */
class nsXULTemplateResultRDF MOZ_FINAL : public nsIXULTemplateResult
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(nsXULTemplateResultRDF)

    NS_DECL_NSIXULTEMPLATERESULT

    nsXULTemplateResultRDF(nsIRDFResource* aNode);

    nsXULTemplateResultRDF(nsRDFQuery* aQuery,
                           const Instantiation& aInst,
                           nsIRDFResource* aNode);

    ~nsXULTemplateResultRDF();

    nsITemplateRDFQuery* Query() { return mQuery; }

    nsXULTemplateQueryProcessorRDF* GetProcessor()
    {
        return (mQuery ? mQuery->Processor() : nsnull);
    }

    /**
     * Get the value of a variable, first by looking in the assignments and
     * then the bindings
     */
    void
    GetAssignment(nsIAtom* aVar, nsIRDFNode** aValue);

    /**
     * Synchronize the bindings after a change in the RDF graph. Bindings that
     * would be affected will be assigned appropriately based on the change.
     */
    bool
    SyncAssignments(nsIRDFResource* aSubject,
                    nsIRDFResource* aPredicate,
                    nsIRDFNode* aTarget);

    /**
     * Return true if the result has an instantiation involving a particular
     * memory element.
     */
    bool
    HasMemoryElement(const MemoryElement& aMemoryElement);

protected:

    // query that generated the result
    nsCOMPtr<nsITemplateRDFQuery> mQuery;

    // resource node
    nsCOMPtr<nsIRDFResource> mNode;

    // data computed from query
    Instantiation mInst;

    // extra assignments made by rules (<binding> tags)
    nsBindingValues mBindingValues;
};

#endif // nsXULTemplateResultRDF_h__
