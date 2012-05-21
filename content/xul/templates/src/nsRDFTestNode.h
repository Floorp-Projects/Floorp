/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRDFTestNode_h__
#define nsRDFTestNode_h__

#include "nsRuleNetwork.h"

class nsIRDFResource;
class nsIRDFNode;

/**
 * An abstract base class for all of the RDF-related tests. This interface
 * allows us to iterate over all of the RDF tests to find the one in the
 * network that is apropos for a newly-added assertion.
 */
class nsRDFTestNode : public TestNode
{
public:
    nsRDFTestNode(TestNode* aParent)
        : TestNode(aParent) {}

    /**
     * Determine whether the node can propagate an assertion
     * with the specified source, property, and target. If the
     * assertion can be propagated, aInitialBindings will be
     * initialized with appropriate variable-to-value assignments
     * to allow the rule network to start a constrain and propagate
     * search from this node in the network.
     *
     * @return true if the node can propagate the specified
     * assertion.
     */
    virtual bool CanPropagate(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget,
                                Instantiation& aInitialBindings) const = 0;

    /**
     *
     */
    virtual void Retract(nsIRDFResource* aSource,
                         nsIRDFResource* aProperty,
                         nsIRDFNode* aTarget) const = 0;
};

#endif // nsRDFTestNode_h__
