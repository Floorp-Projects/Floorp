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

#ifndef nsRDFTestNode_h__
#define nsRDFTestNode_h__

#include "nsRuleNetwork.h"
class nsIRDFResource;
class nsIRDFNode;
class nsTemplateMatchSet;

/**
 * An abstract base class for all of the RDF-related tests. This interface
 * allows us to iterate over all of the RDF tests to find the one in the
 * network that is apropos for a newly-added assertion.
 */
class nsRDFTestNode : public TestNode
{
public:
    nsRDFTestNode(InnerNode* aParent)
        : TestNode(aParent) {}

    /**
     * Determine wether the node can propogate an assertion
     * with the specified source, property, and target. If the
     * assertion can be propogated, aInitialBindings will be
     * initialized with appropriate variable-to-value assignments
     * to allow the rule network to start a constrain and propogate
     * search from this node in the network.
     *
     * @return PR_TRUE if the node can propogate the specified
     * assertion.
     */
    virtual PRBool CanPropogate(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget,
                                Instantiation& aInitialBindings) const = 0;

    /**
     *
     */
    virtual void Retract(nsIRDFResource* aSource,
                         nsIRDFResource* aProperty,
                         nsIRDFNode* aTarget,
                         nsTemplateMatchSet& aFirings,
                         nsTemplateMatchSet& aRetractions) const = 0;
};

#endif // nsRDFTestNode_h__
