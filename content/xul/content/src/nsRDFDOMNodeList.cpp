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

#include "nsDOMCID.h"
#include "nsIDOMNode.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFDOMNodeList.h"

////////////////////////////////////////////////////////////////////////
// GUID definitions

static NS_DEFINE_IID(kIDOMNodeIID,                NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMNodeListIID,            NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIDOMScriptObjectFactoryIID, NS_IDOM_SCRIPT_OBJECT_FACTORY_IID);

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,  NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

////////////////////////////////////////////////////////////////////////
// ctors & dtors

nsRDFDOMNodeList::nsRDFDOMNodeList(void)
    : mInner(nsnull),
      mElements(nsnull),
      mScriptObject(nsnull)
{
    NS_INIT_REFCNT();
}

nsRDFDOMNodeList::~nsRDFDOMNodeList(void)
{
    NS_IF_RELEASE(mElements);
    delete mInner;
}

nsresult
nsRDFDOMNodeList::Create(nsRDFDOMNodeList** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

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


////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(nsRDFDOMNodeList);
NS_IMPL_RELEASE(nsRDFDOMNodeList);

nsresult
nsRDFDOMNodeList::QueryInterface(REFNSIID aIID, void** aResult)
{
static NS_DEFINE_IID(kISupportsIID,          NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(nsIDOMNodeList::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIDOMNodeList*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (aIID.Equals(kIScriptObjectOwnerIID)) {
        *aResult = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

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
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    PRUint32 cnt;
    nsresult rv = mElements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    NS_PRECONDITION(aIndex < cnt, "invalid arg");
    if (aIndex >= (PRUint32) cnt)
        return NS_ERROR_INVALID_ARG;

    // Cast is okay because we're in a closed system.
    *aReturn = (nsIDOMNode*) mElements->ElementAt(aIndex);
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIScriptObjectOwner interface

NS_IMETHODIMP
nsRDFDOMNodeList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    nsresult rv = NS_OK;
    nsIScriptGlobalObject* global = aContext->GetGlobalObject();

    if (nsnull == mScriptObject) {
        nsIDOMScriptObjectFactory *factory;
    
        if (NS_SUCCEEDED(rv = nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                                           kIDOMScriptObjectFactoryIID,
                                                           (nsISupports **)&factory))) {
            rv = factory->NewScriptNodeList(aContext, 
                                            (nsISupports*)(nsIDOMNodeList*)this, 
                                            global, 
                                            (void**)&mScriptObject);

            nsServiceManager::ReleaseService(kDOMScriptObjectFactoryCID, factory);
        }
    }
    *aScriptObject = mScriptObject;

    NS_RELEASE(global);
    return rv;
}

NS_IMETHODIMP
nsRDFDOMNodeList::SetScriptObject(void* aScriptObject)
{
    mScriptObject = aScriptObject;
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


nsresult
nsRDFDOMNodeList::AppendNode(nsIDOMNode* aNode)
{
    NS_PRECONDITION(aNode != nsnull, "null ptr");
    if (! aNode)
        return NS_ERROR_NULL_POINTER;

    return mElements->AppendElement(aNode);
}
