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
#include "nsIInterfaceInfoManager.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIThread.h"

#include "bcXPCOMProxy.h"
#include "bcXPCOMMarshalToolkit.h"

//#include "signal.h"
//NS_IMPL_ISUPPORTS(bcXPCOMProxy, NS_GET_IID(bcXPCOMProxy));
#include "bcXPCOMLog.h"
#include "bcIXPCOMStubsAndProxies.h"
#include "bcXPCOMStubsAndProxiesCID.h"


static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kStubsAndProxies,BC_XPCOMSTUBSANDPROXIES_CID);

bcXPCOMProxy::bcXPCOMProxy(bcOID _oid, const nsIID &_iid, bcIORB *_orb) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    NS_INIT_REFCNT();
    oid = _oid;
    iid = _iid;
    orb = _orb;
    interfaceInfo = NULL;
    PR_LOG(log, PR_LOG_DEBUG, ("--[c++] bcXPCOMProxy::bcXPCOMProxy this: %p iid: %s\n",this, iid.ToString()));
    INVOKE_ADDREF(&oid,&iid,orb);
}

bcXPCOMProxy::~bcXPCOMProxy() {
    NS_IF_RELEASE(interfaceInfo);
    INVOKE_RELEASE(&oid,&iid,orb);
}
 

NS_IMETHODIMP bcXPCOMProxy::GetInterfaceInfo(nsIInterfaceInfo** info) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG,("--[c++] bcXPCOMProxy::GetInterfaceInfo iid=%s\n",iid.ToString()));
    if(!info) {
        return NS_ERROR_FAILURE;
    }
    if (!interfaceInfo) {
        nsIInterfaceInfoManager* iimgr;
        if((iimgr = XPTI_GetInterfaceInfoManager())) {
            if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
                PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMProxy::GetInterfaceInfo failed\n"));
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


struct CallInfo {
    CallInfo(nsIEventQueue *eq, bcIORB *o, bcICall *c ) {
        callersEventQ = eq; call = c; orb = o; completed = PR_FALSE;
    }
    bcICall *call;
    bcIORB *orb;
    nsCOMPtr<nsIEventQueue> callersEventQ;
    PRBool completed;
};

static void* EventHandler(PLEvent *self);
static void  PR_CALLBACK DestroyHandler(PLEvent *self);

NS_IMETHODIMP bcXPCOMProxy::CallMethod(PRUint16 methodIndex,
                                           const nsXPTMethodInfo* info,
                                           nsXPTCMiniVariant* params) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMProxy::CallMethod %s [%d]\n",info->GetName(),methodIndex));
    bcICall *call = orb->CreateCall(&iid, &oid, methodIndex);
    bcIMarshaler *marshaler = call->GetMarshaler();
    bcXPCOMMarshalToolkit * mt = new bcXPCOMMarshalToolkit(methodIndex, interfaceInfo, params,orb);
    mt->Marshal(marshaler);
    nsIEventQueue * eventQ;
    nsCOMPtr<bcIXPCOMStubsAndProxies> stubsAndProxiesService;
    stubsAndProxiesService = do_GetService(kStubsAndProxies);
    stubsAndProxiesService->GetEventQueue(&eventQ);
    if (eventQ == NULL) {
        orb->SendReceive(call);
    } else {
        nsCOMPtr<nsIEventQueue> currentEventQ;
        PRBool eventLoopCreated = PR_FALSE;
        nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(kEventQueueServiceCID);
        nsresult rv = NS_OK;

        rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(currentEventQ));
        if (NS_FAILED(rv)) {
            rv = eventQService->CreateMonitoredThreadEventQueue();
            eventLoopCreated = PR_TRUE;
            if (NS_FAILED(rv))
                return rv;
            rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(currentEventQ));
        }
        
        if (NS_FAILED(rv)) {
            return rv;
        }
        CallInfo * callInfo = new CallInfo(currentEventQ,orb,call);
        PLEvent *event = PR_NEW(PLEvent);
        PL_InitEvent(event, 
                     callInfo,
                     EventHandler,
                     DestroyHandler);
        eventQ->PostEvent(event);
        while (1) {
            PR_LOG(log, PR_LOG_DEBUG, ("--Dispatch we got new event\n"));
            PLEvent *nextEvent;
            rv = currentEventQ->WaitForEvent(&nextEvent);
            if (callInfo->completed) {
                PR_DELETE(nextEvent);
                break; //we are done
            }
            if (NS_FAILED(rv)) {
                break;
            }
            currentEventQ->HandleEvent(nextEvent);
        }
        if (eventLoopCreated) {
            eventQService->DestroyThreadEventQueue();
            eventQ = NULL;
        }
    }  
    NS_IF_RELEASE(eventQ);
    bcIUnMarshaler * unmarshaler = call->GetUnMarshaler();
    nsresult result;
    unmarshaler->ReadSimple(&result, bc_T_U32);
    if (NS_SUCCEEDED(result)) {
        mt->UnMarshal(unmarshaler);
    }
    delete call; delete marshaler; delete unmarshaler; delete mt;
    return result;
}

nsrefcnt bcXPCOMProxy::AddRef(void) { 
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    nsrefcnt cnt = (nsrefcnt) PR_AtomicIncrement((PRInt32*)&mRefCnt);
    PR_LOG(log, PR_LOG_DEBUG, ("--[c++] bcXPCOMProxy::AddRef %d\n",(unsigned)cnt));
    return cnt;
}

nsrefcnt bcXPCOMProxy::Release(void) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    nsrefcnt cnt = (nsrefcnt) PR_AtomicDecrement((PRInt32*)&mRefCnt);
    PR_LOG(log, PR_LOG_DEBUG, ("--[c++] bcXPCOMProxy::Release %d\n",(unsigned)cnt));
    if(0 == cnt) {  
       delete this;
    }
    return cnt;
}


NS_IMETHODIMP bcXPCOMProxy::QueryInterface(REFNSIID aIID, void** aInstancePtr) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    if (aIID.Equals(NS_GET_IID(bcXPCOMProxy))) { //hack for getting oid
        *aInstancePtr = &oid;
        return NS_OK;
    }
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
        PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMProxy.QueryInterface nointerface %s\n",aIID.ToString()));
        r = NS_NOINTERFACE;
    }
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMProxy.QueryInterface we got interface %s\n",aIID.ToString()));
    return r;
}


static void* EventHandler(PLEvent *self) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--about to bcXPCOMProxy EventHandler\n"));
    CallInfo *callInfo = (CallInfo*)PL_GetEventOwner(self);
    callInfo->orb->SendReceive(callInfo->call);
    callInfo->completed = PR_TRUE;
    PR_LOG(log, PR_LOG_DEBUG, ("--about to callInfo->callersEventQ->PostEvent;\n"));
    PLEvent *event = PR_NEW(PLEvent); //nb who is doing free ?
    callInfo->callersEventQ->PostEvent(event);
    return NULL;
}

static void  PR_CALLBACK DestroyHandler(PLEvent *self) {
}
