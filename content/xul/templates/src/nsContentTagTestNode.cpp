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

#include "nsContentTagTestNode.h"
#include "nsISupportsArray.h"
#include "nsString.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#endif

nsContentTagTestNode::nsContentTagTestNode(InnerNode* aParent,
                                           nsConflictSet& aConflictSet,
                                           PRInt32 aContentVariable,
                                           nsIAtom* aTag)
    : TestNode(aParent),
      mConflictSet(aConflictSet),
      mContentVariable(aContentVariable),
      mTag(aTag)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        nsAutoString tag(NS_LITERAL_STRING("(none)"));
        if (mTag)
            mTag->ToString(tag);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("nsContentTagTestNode[%p]: parent=%p content-var=%d tag=%s",
                this, aParent, aContentVariable, NS_ConvertUCS2toUTF8(tag).get()));
    }
#endif
}

nsresult
nsContentTagTestNode::FilterInstantiations(InstantiationSet& aInstantiations, void* aClosure) const
{
    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        Value value;
        if (! inst->mAssignments.GetAssignmentFor(mContentVariable, &value)) {
            NS_ERROR("cannot handle open-ended tag name query");
            return NS_ERROR_UNEXPECTED;
        }

        nsCOMPtr<nsIAtom> tag;
        rv = VALUE_TO_ICONTENT(value)->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        if (tag != mTag) {
            aInstantiations.Erase(inst--);
        }
    }
    
    return NS_OK;
}

nsresult
nsContentTagTestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    return NS_OK;
}

