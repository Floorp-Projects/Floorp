/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTraversal.h"

#include "nsIDOMNode.h"
#include "nsError.h"
#include "nsINode.h"
#include "mozilla/AutoRestore.h"

#include "nsGkAtoms.h"

using namespace mozilla;
using namespace mozilla::dom;

nsTraversal::nsTraversal(nsINode *aRoot,
                         uint32_t aWhatToShow,
                         const NodeFilterHolder &aFilter) :
    mRoot(aRoot),
    mWhatToShow(aWhatToShow),
    mFilter(aFilter),
    mInAcceptNode(false)
{
    NS_ASSERTION(aRoot, "invalid root in call to nsTraversal constructor");
}

nsTraversal::~nsTraversal()
{
    /* destructor code */
}

/*
 * Tests if and how a node should be filtered. Uses mWhatToShow and
 * mFilter to test the node.
 * @param aNode     Node to test
 * @param aResult   Whether we succeeded
 * @returns         Filtervalue. See nsIDOMNodeFilter.idl
 */
int16_t
nsTraversal::TestNode(nsINode* aNode, mozilla::ErrorResult& aResult)
{
    if (mInAcceptNode) {
        aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
        return 0;
    }

    uint16_t nodeType = aNode->NodeType();

    if (nodeType <= 12 && !((1 << (nodeType-1)) & mWhatToShow)) {
        return nsIDOMNodeFilter::FILTER_SKIP;
    }

    if (!mFilter.GetISupports()) {
        // No filter, just accept
        return nsIDOMNodeFilter::FILTER_ACCEPT;
    }

    if (mFilter.HasWebIDLCallback()) {
        AutoRestore<bool> inAcceptNode(mInAcceptNode);
        mInAcceptNode = true;
        // No need to pass in an execution reason, since the generated default,
        // "NodeFilter.acceptNode", is pretty much exactly what we'd say anyway.
        return mFilter.GetWebIDLCallback()->
            AcceptNode(*aNode, aResult, nullptr,
                       CallbackObject::eRethrowExceptions);
    }

    nsCOMPtr<nsIDOMNode> domNode = do_QueryInterface(aNode);
    AutoRestore<bool> inAcceptNode(mInAcceptNode);
    mInAcceptNode = true;
    int16_t filtered;
    nsresult rv = mFilter.GetXPCOMCallback()->AcceptNode(domNode, &filtered);
    if (NS_FAILED(rv)) {
        aResult.Throw(rv);
    }
    return filtered;
}
