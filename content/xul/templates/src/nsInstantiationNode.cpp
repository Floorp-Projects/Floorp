/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsInstantiationNode.h"
#include "nsTemplateRule.h"
#include "nsClusterKeySet.h"
#include "nsConflictSet.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#endif

nsInstantiationNode::nsInstantiationNode(nsConflictSet& aConflictSet,
                                         nsTemplateRule* aRule,
                                         nsIRDFDataSource* aDataSource)
        : mConflictSet(aConflictSet),
          mRule(aRule)
{
#ifdef PR_LOGGING
    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("nsInstantiationNode[%p] rule=%p", this, aRule));
#endif
}


nsInstantiationNode::~nsInstantiationNode()
{
    delete mRule;
}

nsresult
nsInstantiationNode::Propogate(const InstantiationSet& aInstantiations, void* aClosure)
{
    // If we get here, we've matched the rule associated with this
    // node. Extend it with any <bindings> that we might have, add it
    // to the conflict set, and the set of new <content, member>
    // pairs.
    nsClusterKeySet* newkeys = NS_STATIC_CAST(nsClusterKeySet*, aClosure);

    InstantiationSet::ConstIterator last = aInstantiations.Last();
    for (InstantiationSet::ConstIterator inst = aInstantiations.First(); inst != last; ++inst) {
        nsAssignmentSet assignments = inst->mAssignments;

        nsTemplateMatch* match =
            nsTemplateMatch::Create(mConflictSet.GetPool(), mRule, *inst, assignments);

        if (! match)
            return NS_ERROR_OUT_OF_MEMORY;

        // Temporarily AddRef() to keep the match alive.
        match->AddRef();

        mRule->InitBindings(mConflictSet, match);

        mConflictSet.Add(match);

        // Give back our "local" reference. The conflict set will have
        // taken what it needs.
        match->Release(mConflictSet.GetPool());

        newkeys->Add(nsClusterKey(*inst, mRule));
    }
    
    return NS_OK;
}
