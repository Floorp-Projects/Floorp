/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInstantiationNode_h__
#define nsInstantiationNode_h__

#include "mozilla/Attributes.h"
#include "nsRuleNetwork.h"
#include "nsRDFQuery.h"

class nsXULTemplateQueryProcessorRDF;

/**
 * A leaf-level node in the rule network. If any instantiations
 * propagate to this node, then we know we've matched a rule.
 */
class nsInstantiationNode : public ReteNode
{
public:
    nsInstantiationNode(nsXULTemplateQueryProcessorRDF* aProcessor,
                        nsRDFQuery* aRule);

    ~nsInstantiationNode();

    // "downward" propagations
    virtual nsresult Propagate(InstantiationSet& aInstantiations,
                               bool aIsUpdate, bool& aMatched) MOZ_OVERRIDE;

protected:

    nsXULTemplateQueryProcessorRDF* mProcessor;
    nsRDFQuery* mQuery;
};

#endif // nsInstantiationNode_h__
