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

#ifndef nsInstantiationNode_h__
#define nsInstantiationNode_h__

#include "nsRuleNetwork.h"
class nsIRDFDataSource;
class nsConflictSet;
class nsTemplateRule;

/**
 * A leaf-level node in the rule network. If any instantiations
 * propogate to this node, then we know we've matched a rule.
 */
class nsInstantiationNode : public ReteNode
{
public:
    nsInstantiationNode(nsConflictSet& aConflictSet,
                        nsTemplateRule* aRule,
                        nsIRDFDataSource* aDataSource);

    ~nsInstantiationNode();

    // "downward" propogations
    virtual nsresult Propogate(const InstantiationSet& aInstantiations, void* aClosure);

protected:
    nsConflictSet& mConflictSet;

    /**
     * The rule that the node instantiates. The instantiation node
     * assumes ownership of the rule in its ctor, and will destroy
     * the rule in its dtor.
     */
    nsTemplateRule* mRule;
};

#endif // nsInstantiationNode_h__
