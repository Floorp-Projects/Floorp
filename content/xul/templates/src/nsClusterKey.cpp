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

#include "nsClusterKey.h"
#include "nsTemplateRule.h"

nsClusterKey::nsClusterKey(const Instantiation& aInstantiation, const nsTemplateRule* aRule)
{
    PRBool hasassignment;

    mContainerVariable = aRule->GetContainerVariable();
    hasassignment = aInstantiation.mAssignments.GetAssignmentFor(mContainerVariable, &mContainerValue);
    NS_ASSERTION(hasassignment, "no assignment for container variable");

    mMemberVariable = aRule->GetMemberVariable();
    hasassignment = aInstantiation.mAssignments.GetAssignmentFor(mMemberVariable, &mMemberValue);
    NS_ASSERTION(hasassignment, "no assignment for member variable");

    MOZ_COUNT_CTOR(nsClusterKey);
}


PLHashNumber PR_CALLBACK
nsClusterKey::HashClusterKey(const void* aKey)
{
    const nsClusterKey* key = NS_STATIC_CAST(const nsClusterKey*, aKey);
    return key->Hash();
}

PRIntn PR_CALLBACK
nsClusterKey::CompareClusterKeys(const void* aLeft, const void* aRight)
{
    const nsClusterKey* left  = NS_STATIC_CAST(const nsClusterKey*, aLeft);
    const nsClusterKey* right = NS_STATIC_CAST(const nsClusterKey*, aRight);
    return *left == *right;
}
