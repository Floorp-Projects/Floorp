/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

#include "pratom.h"
#include "nsIInterfaceInfoManager.h"

#include "bcXPCOMProxy.h"
#include "bcXPCOMMarshalToolkit.h"

#include "signal.h"
//NS_IMPL_ISUPPORTS(bcXPCOMProxy, NS_GET_IID(bcXPCOMProxy));


bcXPCOMProxy::bcXPCOMProxy(bcOID _oid, const nsIID &_iid, bcIORB *_orb) {
    NS_INIT_REFCNT();
    oid = _oid;
    iid = _iid;
    orb = _orb;
    interfaceInfo = NULL;
    printf("--[c++] bcXPCOMProxy::bcXPCOMProxy this: %p iid: %s\n",this, iid.ToString());
}

bcXPCOMProxy::~bcXPCOMProxy() {
    NS_IF_RELEASE(interfaceInfo);
}
 

NS_IMETHODIMP bcXPCOMProxy::GetInterfaceInfo(nsIInterfaceInfo** info) {
    printf("--[c++] bcXPCOMProxy::GetInterfaceInfo iid=%s\n",iid.ToString());
    if(!info) {
        return NS_ERROR_FAILURE;
    }
    if (!interfaceInfo) {
        nsIInterfaceInfoManager* iimgr;
        if((iimgr = XPTI_GetInterfaceInfoManager())) {
            if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
                printf("--bcXPCOMProxy::GetInterfaceInfo failed\n");
                return NS_ERROR_FAILURE;
            }
            NS_RELEASE(iimgr);
        } else {
            return NS_ERROR_FAILURE;
        }
    }
    NS_ADDREF(interfaceInfo);
    *info =  interfaceInfo;
    return NS_OK;
}

NS_IMETHODIMP bcXPCOMProxy::CallMethod(PRUint16 methodIndex,
                                           const nsXPTMethodInfo* info,
                                           nsXPTCMiniVariant* params) {
    printf("--bcXPCOMProxy::CallMethod %s [%d]\n",info->GetName(),methodIndex);
    bcICall *call = orb->CreateCall(&iid, &oid, methodIndex);
    bcIMarshaler *marshaler = call->GetMarshaler();
    bcXPCOMMarshalToolkit * mt = new bcXPCOMMarshalToolkit(methodIndex, interfaceInfo, params);
    mt->Marshal(marshaler);
    orb->SendReceive(call);
    bcIUnMarshaler * unmarshaler = call->GetUnMarshaler();
    mt->UnMarshal(unmarshaler);
    delete call; delete marshaler; delete unmarshaler; delete mt;
    return NS_OK;
}

nsrefcnt bcXPCOMProxy::AddRef(void) { 
    nsrefcnt cnt = (nsrefcnt) PR_AtomicIncrement((PRInt32*)&mRefCnt);
    printf("--[c++] bcXPCOMProxy::AddRef %d\n",(unsigned)cnt);
    return cnt;
}

nsrefcnt bcXPCOMProxy::Release(void) {
    nsrefcnt cnt = (nsrefcnt) PR_AtomicDecrement((PRInt32*)&mRefCnt);
    printf("--[c++] bcXPCOMProxy::Release %d\n",(unsigned)cnt);
    if(0 == cnt) {  
        delete this;
    }
    return cnt;
}


NS_IMETHODIMP bcXPCOMProxy::QueryInterface(REFNSIID aIID, void** aInstancePtr) {
    PRUint16 methodIndex = 0;
    const nsXPTMethodInfo *info;
    nsIInterfaceInfo *inInfo;
    GetInterfaceInfo(&inInfo);  //nb add error handling 
    
    /* These do *not* make copies ***explicit bending of XPCOM rules***/
    inInfo->GetMethodInfo(methodIndex,&info);
    NS_RELEASE(inInfo); 
    nsXPTCMiniVariant params[2];
    params[0].val.p = (void*)&aIID;
    params[1].val.p = aInstancePtr;
    nsresult r = CallMethod(methodIndex,info,params);
    if (*aInstancePtr == NULL) {
        printf("--bcXPCOMProxy.QueryInterface nointerface %s\n",aIID.ToString());
        r = NS_NOINTERFACE;
    }
    printf("--bcXPCOMProxy.QueryInterface we got interface %s\n",aIID.ToString());
    return r;
}

