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

#include "nsIInterfaceInfoManager.h"

#include "bcXPCOMProxy.h"
#include "bcXPCOMMarshalToolkit.h"

NS_IMPL_ISUPPORTS(bcXPCOMProxy, NS_GET_IID(bcXPCOMProxy));


bcXPCOMProxy::bcXPCOMProxy(bcOID _oid, const nsIID &_iid, bcIORB *_orb) {
    NS_INIT_REFCNT();
    oid = _oid;
    iid = _iid;
    orb = _orb;
    interfaceInfo = NULL;
}

bcXPCOMProxy::~bcXPCOMProxy() {
    NS_IF_RELEASE(interfaceInfo);
}
 

NS_IMETHODIMP bcXPCOMProxy::GetInterfaceInfo(nsIInterfaceInfo** info) {
    if(!info) {
        return NS_ERROR_FAILURE;
    }
    if (!interfaceInfo) {
        nsIInterfaceInfoManager* iimgr;
        if(iimgr = XPTI_GetInterfaceInfoManager()) {
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
    printf("--bcXPCOMProxy::CallMethod %d\n",methodIndex);
    //sigsend(P_PID, getpid(),SIGINT);
    bcICall *call = orb->CreateCall(&iid, &oid, methodIndex);
    bcIMarshaler *marshaler = call->GetMarshaler();
    bcXPCOMMarshalToolkit * mt = new bcXPCOMMarshalToolkit(methodIndex, interfaceInfo, params);
    mt->Marshal(marshaler);
    orb->SendReceive(call);
    bcIUnMarshaler * unmarshaler = call->GetUnMarshaler();
    //nb *******
    delete call; delete marshaler; delete unmarshaler;
    return NS_OK;
}





