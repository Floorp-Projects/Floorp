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

#include "prmem.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptcall.h"
#include "bcXPCOMMarshalToolkit.h"
#include "bcXPCOMStub.h"

bcXPCOMStub::bcXPCOMStub(nsISupports *o) {
    object = o;
    NS_ADDREF(object);
}

bcXPCOMStub::~bcXPCOMStub() {
    NS_RELEASE(object);
}

void bcXPCOMStub::Dispatch(bcICall *call) {
    
    bcIID iid; bcOID oid; bcMID mid;
    call->GetParams(&iid, &oid, &mid);
    nsIInterfaceInfo *interfaceInfo;
    nsIInterfaceInfoManager* iimgr;
    if( (iimgr = XPTI_GetInterfaceInfoManager()) ) {
        if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
            return;  //nb exception handling
        }
        NS_RELEASE(iimgr);
    } else {
        return;
    }
    nsXPTCVariant *params;
    nsXPTMethodInfo* info;
    interfaceInfo->GetMethodInfo(mid,(const nsXPTMethodInfo **)&info);
    int paramCount = info->GetParamCount();
    bcXPCOMMarshalToolkit * mt = NULL;
    if (paramCount > 0) {
        printf("--[c++]bcXPCOMStub paramCount %d\n",paramCount);
        params = (nsXPTCVariant *)  PR_Malloc(sizeof(nsXPTCVariant)*paramCount);
        mt = new bcXPCOMMarshalToolkit(mid, interfaceInfo, params);
        bcIUnMarshaler * um = call->GetUnMarshaler();
        mt->UnMarshal(um);
    }
    //nb return value; excepion handling
    XPTC_InvokeByIndex(object, mid, paramCount, params);
    if (mt != NULL) { //nb to do what about nsresult ?
        bcIMarshaler * m = call->GetMarshaler();    
        mt->Marshal(m);
    }
    //nb memory deallocation
    return;
}








