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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

    Helper class to implement the nsIDOMNodeList interface.

    XXX It's probably wrong in some sense, as it uses the "naked"
    content interface to look for kids. (I assume in general this is
    bad because there may be pseudo-elements created for presentation
    that aren't visible to the DOM.)

*/

#include "nsDOMCID.h"
#include "nsIDOMNode.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFDOMNodeList.h"
#include "nsContentUtils.h"

////////////////////////////////////////////////////////////////////////
// ctors & dtors


nsRDFDOMNodeList::nsRDFDOMNodeList(void)
    : //mInner(nsnull), Not being used?
      mElements(nsnull)
{
    NS_INIT_REFCNT();
}

nsRDFDOMNodeList::~nsRDFDOMNodeList(void)
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: nsRDFDOMNodeList\n", gInstanceCount);
#endif

    NS_IF_RELEASE(mElements);
    //delete mInner; Not being used?
}

nsresult
nsRDFDOMNodeList::Create(nsRDFDOMNodeList** aResult)
{
    nsRDFDOMNodeList* list = new nsRDFDOMNodeList();
    if (! list)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    if (NS_FAILED(rv = list->Init())) {
        delete list;
        return rv;
    }

    NS_ADDREF(list);
    *aResult = list;
    return NS_OK;
}

nsresult
nsRDFDOMNodeList::CreateWithArray(nsISupportsArray* aArray,
                                  nsRDFDOMNodeList** aResult)
{
    nsRDFDOMNodeList* list = new nsRDFDOMNodeList();
    if (! list)
        return NS_ERROR_OUT_OF_MEMORY;

    list->mElements = aArray;
    NS_IF_ADDREF(aArray);

    NS_ADDREF(list);
    *aResult = list;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface


// QueryInterface implementation for nsRDFDOMNodeList
NS_INTERFACE_MAP_BEGIN(nsRDFDOMNodeList)
    NS_INTERFACE_MAP_ENTRY(nsIDOMNodeList)
    NS_INTERFACE_MAP_ENTRY(nsIRDFNodeList)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNodeList)
    NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XULNodeList)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsRDFDOMNodeList);
NS_IMPL_RELEASE(nsRDFDOMNodeList);


////////////////////////////////////////////////////////////////////////
// nsIDOMNodeList interface

NS_IMETHODIMP
nsRDFDOMNodeList::GetLength(PRUint32* aLength)
{
    NS_ASSERTION(aLength != nsnull, "null ptr");
    if (! aLength)
        return NS_ERROR_NULL_POINTER;

    PRUint32 cnt;
    nsresult rv = mElements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    *aLength = cnt;
    return NS_OK;
}


NS_IMETHODIMP
nsRDFDOMNodeList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
    // It's ok to access past the end of the array here, if we do that
    // we simply return nsnull, as per the DOM spec.

    // Cast is okay because we're in a closed system.
    *aReturn = (nsIDOMNode*) mElements->ElementAt(aIndex);
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
nsRDFDOMNodeList::Init(void)
{
    nsresult rv;
    if (NS_FAILED(rv = NS_NewISupportsArray(&mElements))) {
        NS_ERROR("unable to create elements array");
        return rv;
    }

    return NS_OK;
}


NS_IMETHODIMP
nsRDFDOMNodeList::AppendNode(nsIDOMNode* aNode)
{
    NS_PRECONDITION(aNode != nsnull, "null ptr");
    if (! aNode)
        return NS_ERROR_NULL_POINTER;

    return mElements->AppendElement(aNode);
}

NS_IMETHODIMP
nsRDFDOMNodeList::RemoveNode(nsIDOMNode* aNode)
{
    NS_PRECONDITION(aNode != nsnull, "null ptr");
    if (! aNode)
        return NS_ERROR_NULL_POINTER;

    return mElements->RemoveElement(aNode);
}
