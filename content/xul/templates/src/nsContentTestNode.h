/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentTestNode_h__
#define nsContentTestNode_h__

#include "nscore.h"
#include "nsRuleNetwork.h"
#include "nsFixedSizeAllocator.h"
#include "nsIAtom.h"
#include "nsIDOMDocument.h"

class nsIXULTemplateBuilder;

/**
 * The nsContentTestNode is always the top node in a query's rule network. It
 * exists so that Constrain can filter out resources that aren't part of a
 * result.
 */
class nsContentTestNode : public TestNode
{
public:
    nsContentTestNode(nsXULTemplateQueryProcessorRDF* aProcessor,
                      nsIAtom* aContentVariable);

    virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations,
                                          bool* aCantHandleYet) const;

    nsresult
    Constrain(InstantiationSet& aInstantiations);

    void SetTag(nsIAtom* aTag, nsIDOMDocument* aDocument)
    {
        mTag = aTag;
        mDocument = aDocument;
    }

protected:
    nsXULTemplateQueryProcessorRDF *mProcessor;
    nsIDOMDocument* mDocument;
    nsCOMPtr<nsIAtom> mRefVariable;
    nsCOMPtr<nsIAtom> mTag;
};

#endif // nsContentTestNode_h__

