/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Peter Van der Beken.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

NS_IMPL_ISUPPORTS_INHERITED1(txNodeSetAdaptor, txXPathObjectAdaptor, txINodeSet)

nsresult
txNodeSetAdaptor::Init()
{
    if (!mValue) {
        mValue = new txNodeSet(nsnull);
    }

    return mValue ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
txNodeSetAdaptor::Item(PRUint32 aIndex, nsIDOMNode **aResult)
{
    *aResult = nsnull;

    if (aIndex > (PRUint32)NodeSet()->size()) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    return txXPathNativeNode::getNode(NodeSet()->get(aIndex), aResult);
}

NS_IMETHODIMP
txNodeSetAdaptor::ItemAsNumber(PRUint32 aIndex, double *aResult)
{
    if (aIndex > (PRUint32)NodeSet()->size()) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    nsAutoString result;
    txXPathNodeUtils::appendNodeValue(NodeSet()->get(aIndex), result);

    *aResult = txDouble::toDouble(result);

    return NS_OK;
}

NS_IMETHODIMP
txNodeSetAdaptor::ItemAsString(PRUint32 aIndex, nsAString &aResult)
{
    if (aIndex > (PRUint32)NodeSet()->size()) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    txXPathNodeUtils::appendNodeValue(NodeSet()->get(aIndex), aResult);

    return NS_OK;
}

NS_IMETHODIMP
txNodeSetAdaptor::GetLength(PRUint32 *aLength)
{
    *aLength = (PRUint32)NodeSet()->size();

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
