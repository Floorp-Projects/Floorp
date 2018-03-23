/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txNodeSetAdaptor.h"
#include "txXPathTreeWalker.h"

txNodeSetAdaptor::txNodeSetAdaptor()
    : txXPathObjectAdaptor(),
      mWritable(true)
{
}

txNodeSetAdaptor::txNodeSetAdaptor(txNodeSet *aNodeSet)
    : txXPathObjectAdaptor(aNodeSet),
      mWritable(false)
{
}

NS_IMPL_ISUPPORTS_INHERITED(txNodeSetAdaptor, txXPathObjectAdaptor, txINodeSet)

nsresult
txNodeSetAdaptor::Init()
{
    if (!mValue) {
        mValue = new txNodeSet(nullptr);
    }
    return NS_OK;
}

NS_IMETHODIMP
txNodeSetAdaptor::Item(uint32_t aIndex, nsIDOMNode **aResult)
{
    *aResult = nullptr;

    if (aIndex > (uint32_t)NodeSet()->size()) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    return txXPathNativeNode::getNode(NodeSet()->get(aIndex), aResult);
}

NS_IMETHODIMP
txNodeSetAdaptor::ItemAsNumber(uint32_t aIndex, double *aResult)
{
    if (aIndex > (uint32_t)NodeSet()->size()) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    nsAutoString result;
    txXPathNodeUtils::appendNodeValue(NodeSet()->get(aIndex), result);

    *aResult = txDouble::toDouble(result);

    return NS_OK;
}

NS_IMETHODIMP
txNodeSetAdaptor::ItemAsString(uint32_t aIndex, nsAString &aResult)
{
    if (aIndex > (uint32_t)NodeSet()->size()) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    txXPathNodeUtils::appendNodeValue(NodeSet()->get(aIndex), aResult);

    return NS_OK;
}

NS_IMETHODIMP
txNodeSetAdaptor::GetLength(uint32_t *aLength)
{
    *aLength = (uint32_t)NodeSet()->size();

    return NS_OK;
}

NS_IMETHODIMP
txNodeSetAdaptor::Add(nsIDOMNode *aNode)
{
    NS_ENSURE_TRUE(mWritable, NS_ERROR_FAILURE);

    nsAutoPtr<txXPathNode> node(txXPathNativeNode::createXPathNode(aNode,
                                                                   true));

    return node ? NodeSet()->add(*node) : NS_ERROR_OUT_OF_MEMORY;
}
