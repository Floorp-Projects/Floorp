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

#include "nsContentTestNode.h"
#include "nsIRDFResource.h"
#include "nsIAtom.h"
#include "nsIDOMElement.h"
#include "nsXULContentUtils.h"
#include "nsIXULTemplateResult.h"
#include "nsIXULTemplateBuilder.h"
#include "nsXULTemplateQueryProcessorRDF.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#endif

nsContentTestNode::nsContentTestNode(nsXULTemplateQueryProcessorRDF* aProcessor,
                                     nsIAtom* aRefVariable)
    : TestNode(nsnull),
      mProcessor(aProcessor),
      mDocument(nsnull),
      mRefVariable(aRefVariable),
      mTag(nsnull)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        nsAutoString tag(NS_LITERAL_STRING("(none)"));
        if (mTag)
            mTag->ToString(tag);

        nsAutoString refvar(NS_LITERAL_STRING("(none)"));
        if (aRefVariable)
            aRefVariable->ToString(refvar);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsContentTestNode[%p]: ref-var=%s tag=%s",
                this, NS_ConvertUTF16toUTF8(refvar).get(),
                NS_ConvertUTF16toUTF8(tag).get()));
    }
#endif
}

nsresult
nsContentTestNode::FilterInstantiations(InstantiationSet& aInstantiations,
                                        bool* aCantHandleYet) const

{
    if (aCantHandleYet)
        *aCantHandleYet = PR_FALSE;
    return NS_OK;
}

nsresult
nsContentTestNode::Constrain(InstantiationSet& aInstantiations)
{
    // contrain the matches to those that have matched in the template builder

    nsIXULTemplateBuilder* builder = mProcessor->GetBuilder();
    if (!builder) {
        aInstantiations.Clear();
        return NS_OK;
    }

    nsresult rv;

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {

        nsCOMPtr<nsIRDFNode> refValue;
        bool hasRefBinding = inst->mAssignments.GetAssignmentFor(mRefVariable,
                                                                   getter_AddRefs(refValue));
        if (hasRefBinding) {
            nsCOMPtr<nsIRDFResource> refResource = do_QueryInterface(refValue);
            if (refResource) {
                bool generated;
                rv = builder->HasGeneratedContent(refResource, mTag, &generated);
                if (NS_FAILED(rv)) return rv;

                if (generated)
                    continue;
            }
        }

        aInstantiations.Erase(inst--);
    }

    return NS_OK;
}
