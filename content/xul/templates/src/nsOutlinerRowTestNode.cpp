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
 *   Chris Waterson <waterson@netscape.com>
 */

#include "nsOutlinerRowTestNode.h"
#include "nsOutlinerRows.h"
#include "nsIRDFResource.h"
#include "nsXULContentUtils.h"
#include "nsConflictSet.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#endif

nsOutlinerRowTestNode::nsOutlinerRowTestNode(InnerNode* aParent,
                                             nsConflictSet& aConflictSet,
                                             nsOutlinerRows& aRows,
                                             PRInt32 aIdVariable)
    : TestNode(aParent),
      mConflictSet(aConflictSet),
      mRows(aRows),
      mIdVariable(aIdVariable)
{
    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("nsOutlinerRowTestNode[%p]: parent=%p id-var=%d",
            this, aParent, aIdVariable));
}

nsresult
nsOutlinerRowTestNode::FilterInstantiations(InstantiationSet& aInstantiations, void* aClosure) const
{
    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        Value idValue;
        PRBool hasIdBinding =
            inst->mAssignments.GetAssignmentFor(mIdVariable, &idValue);

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
            const char* id = "(unbound)";
            if (hasIdBinding)
                VALUE_TO_IRDFRESOURCE(idValue)->GetValueConst(&id);

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("nsOutlinerRowTestNode[%p]: FilterInstantiations() id=[%s]",
                    this, id));
        }
#endif

        if (hasIdBinding) {
            nsIRDFResource* container = VALUE_TO_IRDFRESOURCE(idValue);

            // Is the row in the outliner?
            if ((container == mRows.GetRootResource()) ||
                (mRows.Find(mConflictSet, container) != mRows.Last())) {

                PR_LOG(gXULTemplateLog, PR_LOG_DEBUG, ("    => passed"));
                Element* element =
                    Element::Create(mConflictSet.GetPool(), container);

                if (! element)
                    return NS_ERROR_OUT_OF_MEMORY;

                inst->AddSupportingElement(element);

                continue;
            }
        }

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG, ("    => failed"));
        aInstantiations.Erase(inst--);
    }

    return NS_OK;
}

nsresult
nsOutlinerRowTestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    aVariables.Add(mIdVariable);
    return TestNode::GetAncestorVariables(aVariables);
}
