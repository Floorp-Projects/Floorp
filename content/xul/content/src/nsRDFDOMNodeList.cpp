/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

    Helper class to implement the nsIDOMNodeList interface.

    XXX It's probably wrong in some sense, as it uses the "naked"
    content interface to look for kids. (I assume in general this is
    bad because there may be pseudo-elements created for presentation
    that aren't visible to the DOM.)

*/

#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"

static NS_DEFINE_IID(kIDOMNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMNodeListIID,        NS_IDOMNODELIST_IID);


class RDFDOMNodeListImpl : public nsIDOMNodeList {
private:
    nsIContent* mElement;

public:
    RDFDOMNodeListImpl(nsIContent* element) : mElement(element) {
        NS_IF_ADDREF(mElement);
    }

    virtual ~RDFDOMNodeListImpl(void) {
        NS_IF_RELEASE(mElement);
    }

    // nsISupports interface
    NS_DECL_ISUPPORTS

    NS_DECL_IDOMNODELIST
};

NS_IMPL_ISUPPORTS(RDFDOMNodeListImpl, kIDOMNodeListIID);

NS_IMETHODIMP
RDFDOMNodeListImpl::GetLength(PRUint32* aLength)
{
    PRInt32 count;
    nsresult rv;
    if (NS_FAILED(rv = mElement->ChildCount(count)))
        return rv;
    *aLength = count;
    return NS_OK;
}


NS_IMETHODIMP
RDFDOMNodeListImpl::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
    // XXX naive. probably breaks when there are pseudo elements or something.
    nsresult rv;
    nsIContent* contentChild;
    if (NS_FAILED(rv = mElement->ChildAt(aIndex, contentChild)))
        return rv;

    rv = contentChild->QueryInterface(kIDOMNodeIID, (void**) aReturn);
    NS_RELEASE(contentChild);

    return rv;
}

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFDOMNodeList(nsIDOMNodeList** aResult, nsIContent* aElement)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    RDFDOMNodeListImpl* list = new RDFDOMNodeListImpl(aElement);
    if (! list)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(list);
    *aResult = list;
    return NS_OK;
}
