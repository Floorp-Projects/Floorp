/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsInstantiationNode.h"
#include "nsTemplateRule.h"
#include "nsXULTemplateQueryProcessorRDF.h"

#include "mozilla/Logging.h"
extern mozilla::LazyLogModule gXULTemplateLog;

nsInstantiationNode::nsInstantiationNode(nsXULTemplateQueryProcessorRDF* aProcessor,
                                         nsRDFQuery* aQuery)
        : mProcessor(aProcessor),
          mQuery(aQuery)
{
    MOZ_LOG(gXULTemplateLog, LogLevel::Debug,
           ("nsInstantiationNode[%p] query=%p", this, aQuery));

    MOZ_COUNT_CTOR(nsInstantiationNode);
}


nsInstantiationNode::~nsInstantiationNode()
{
    MOZ_COUNT_DTOR(nsInstantiationNode);
}

nsresult
nsInstantiationNode::Propagate(InstantiationSet& aInstantiations,
                               bool aIsUpdate, bool& aTakenInstantiations)
{
    // In update mode, iterate through the results and call the template
    // builder to update them. In non-update mode, cache them in the processor
    // to be used during processing. The results are cached in the processor
    // so that the simple rules are only computed once. In this situation, all
    // data for all queries are calculated at once.
    nsresult rv = NS_OK;

    aTakenInstantiations = false;

    if (aIsUpdate) {
        // Iterate through newly added keys to determine which rules fired.
        //
        // XXXwaterson Unfortunately, this could also lead to retractions;
        // e.g., (container ?a ^empty false) could become "unmatched". How
        // to track those?
        nsCOMPtr<nsIDOMNode> querynode;
        mQuery->GetQueryNode(getter_AddRefs(querynode));

        InstantiationSet::ConstIterator last = aInstantiations.Last();
        for (InstantiationSet::ConstIterator inst = aInstantiations.First(); inst != last; ++inst) {
            nsAssignmentSet assignments = inst->mAssignments;

            nsCOMPtr<nsIRDFNode> node;
            assignments.GetAssignmentFor(mQuery->mMemberVariable,
                                         getter_AddRefs(node));
            if (node) {
                nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(node);
                if (resource) {
                    RefPtr<nsXULTemplateResultRDF> nextresult =
                        new nsXULTemplateResultRDF(mQuery, *inst, resource);
                    if (! nextresult)
                        return NS_ERROR_OUT_OF_MEMORY;

                    rv = mProcessor->AddMemoryElements(*inst, nextresult);
                    if (NS_FAILED(rv))
                        return rv;

                    mProcessor->GetBuilder()->AddResult(nextresult, querynode);
                }
            }
        }
    }
    else {
        nsresult rv = mQuery->SetCachedResults(mProcessor, aInstantiations);
        if (NS_SUCCEEDED(rv))
            aTakenInstantiations = true;
    }

    return rv;
}
