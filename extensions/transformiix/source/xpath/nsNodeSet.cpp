/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Peter Van der Beken, peterv@netscape.com
 *    -- original author.
 *
 */

#include "nsNodeSet.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,  NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

NS_IMPL_ISUPPORTS2(nsNodeSet,
                   nsIDOMNodeList,
                   nsIScriptObjectOwner)

nsNodeSet::nsNodeSet(NodeSet* aNodeSet) {
    NS_INIT_ISUPPORTS();
    mScriptObject = nsnull;

    if (aNodeSet) {
        for (int i=0; i < aNodeSet->size(); i++) {
            mNodes.AppendElement(aNodeSet->get(i)->getNSNode());
        }
    }
}

nsNodeSet::~nsNodeSet() {
}

NS_IMETHODIMP
nsNodeSet::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
    *aReturn = nsnull;
    mNodes.QueryElementAt(aIndex, NS_GET_IID(nsIDOMNode), (void**)aReturn);
    return NS_OK;
}

NS_IMETHODIMP
nsNodeSet::GetLength(PRUint32* aLength)
{
    mNodes.Count(aLength);
    return NS_OK;
}

/* 
 * nsIScriptObjectOwner
 */

NS_IMETHODIMP
nsNodeSet::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    nsresult rv = NS_OK;
    nsIScriptGlobalObject* global = aContext->GetGlobalObject();

    if (nsnull == mScriptObject) {
        nsIDOMScriptObjectFactory *factory;
    
        if (NS_SUCCEEDED(rv = nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                                           NS_GET_IID(nsIDOMScriptObjectFactory),
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
nsNodeSet::SetScriptObject(void* aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}
