/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Neil Deakin <enndeakin@sympatico.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsInstantiationNode.h"
#include "nsTemplateRule.h"
#include "nsXULTemplateQueryProcessorRDF.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#endif

nsInstantiationNode::nsInstantiationNode(nsXULTemplateQueryProcessorRDF* aProcessor,
                                         nsRDFQuery* aQuery)
        : mProcessor(aProcessor),
          mQuery(aQuery)
{
#ifdef PR_LOGGING
    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("nsInstantiationNode[%p] query=%p", this, aQuery));
#endif

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

    aTakenInstantiations = PR_FALSE;

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
                    nsRefPtr<nsXULTemplateResultRDF> nextresult =
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
            aTakenInstantiations = PR_TRUE;
    }

    return rv;
}
