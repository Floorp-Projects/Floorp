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
#include "bcXPCOMLog.h"
#include "bcIXPCOMStubsAndProxies.h"
#include "bcXPCOMStubsAndProxiesCID.h"

#include "nsIServiceManager.h"
#include "nsIThread.h"


static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kStubsAndProxies,BC_XPCOMSTUBSANDPROXIES_CID);


static void* PR_CALLBACK EventHandler(PLEvent *self);
static void  PR_CALLBACK DestroyHandler(PLEvent *self);


struct CallInfo {
    CallInfo(nsIEventQueue * eq, bcICall *c, bcXPCOMStub *s) {
        callersEventQ = eq; call = c; stub = s; completed = PR_FALSE;
    }
    nsCOMPtr<nsIEventQueue> callersEventQ;
    bcICall *call;
    bcXPCOMStub *stub;
    PRBool completed;
};

bcXPCOMStub::bcXPCOMStub(nsISupports *o) : object(o) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStub::bcXPCOMStub\n"));
    NS_ADDREF(object);
    nsresult rv;
    _mOwningThread = PR_CurrentThread();
    eventQService = do_GetService(kEventQueueServiceCID);
    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(owningEventQ));
    if (NS_FAILED(rv)) { 
        PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStub::bcXPCOMStub no eventQ in the current thread\n"));
        /* there is no eventQ in the current thread. Let's try using UI thread eventQ
           what else can we do? supposedly calling from inside UI thread is safe
        */
        rv = eventQService->GetThreadEventQueue(NS_UI_THREAD, getter_AddRefs(owningEventQ));
        if (NS_FAILED(rv)) {
            PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStub::bcXPCOMStub no eventQ in the UI thread\n"));
            owningEventQ = NULL;
        }
    }
}

bcXPCOMStub::~bcXPCOMStub() {
    NS_RELEASE(object);
}

void bcXPCOMStub::DispatchAndSaveThread(bcICall *call, nsIEventQueue *eventQueue) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStub::DispatchAndSaveThreade\n"));
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
        PR_LOG(log, PR_LOG_DEBUG, ("--[c++]bcXPCOMStub paramCount %d\n",paramCount));
        params = (nsXPTCVariant *)  PR_Malloc(sizeof(nsXPTCVariant)*paramCount);
        mt = new bcXPCOMMarshalToolkit(mid, interfaceInfo, params, call->GetORB());
        bcIUnMarshaler * um = call->GetUnMarshaler();
        mt->UnMarshal(um);
        //delete um;
    }
    //push caller eventQ
    nsCOMPtr<bcIXPCOMStubsAndProxies> stubsAndProxiesService;
    stubsAndProxiesService = do_GetService(kStubsAndProxies);
    stubsAndProxiesService->PushEventQueue(eventQueue);
    //nb return value; excepion handling
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStub::DispatchAndSaveThreade about to XPTC_InvokeByIndex\n"));
    XPTC_InvokeByIndex(object, mid, paramCount, params);
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStub::DispatchAndSaveThreade after XPTC_InvokeByIndex\n"));                
    if (mt != NULL) { //nb to do what about nsresult ?
        bcIMarshaler * m = call->GetMarshaler();    
        PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStub::DispatchAndSaveThreade about to mt->Marshal\n"));                
        mt->Marshal(m);
        //delete m;
    }
    delete mt;
    //pop caller eventQueue
    stubsAndProxiesService->PopEventQueue(NULL);
    return;
}
void bcXPCOMStub::Dispatch(bcICall *call) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    if (_mOwningThread == NS_CurrentThread()
        || NULL == (void*)owningEventQ) {
        DispatchAndSaveThread(call);
    } else {
        PRBool eventLoopCreated = PR_FALSE;
        nsresult rv = NS_OK;

        nsCOMPtr<nsIEventQueue> eventQ;
        rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
        if (NS_FAILED(rv)) {
            rv = eventQService->CreateMonitoredThreadEventQueue();
            eventLoopCreated = PR_TRUE;
            if (NS_FAILED(rv))
                return;
            rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
        }
        
        if (NS_FAILED(rv)) {
            return;
        }
        CallInfo * callInfo = new CallInfo(eventQ,call,this);
        PLEvent *event = PR_NEW(PLEvent);
        PL_InitEvent(event, 
                     callInfo,
                     EventHandler,
                     DestroyHandler);
        owningEventQ->PostEvent(event);
        while (1) {
            PR_LOG(log, PR_LOG_DEBUG, ("--Dispatch we got new event\n"));
            PLEvent *nextEvent;
            rv = eventQ->WaitForEvent(&nextEvent);
            if (callInfo->completed) {
                PR_DELETE(nextEvent);
                break; //we are done
            }
            if (NS_FAILED(rv)) {
                break;
            }
            eventQ->HandleEvent(nextEvent);
        }
        if (eventLoopCreated) {
            eventQService->DestroyThreadEventQueue();
            eventQ = NULL;
        }
    }  
}



static void* EventHandler(PLEvent *self) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--about to EventHandler\n"));
    CallInfo * callInfo = (CallInfo*)PL_GetEventOwner(self);
    callInfo->stub->DispatchAndSaveThread(callInfo->call,callInfo->callersEventQ);
    callInfo->completed = PR_TRUE;
    PR_LOG(log, PR_LOG_DEBUG, ("--about to callInfo->callersEventQ->PostEvent;\n"));
    PLEvent *event = PR_NEW(PLEvent); //nb who is doing free ?
    callInfo->callersEventQ->PostEvent(event);
    return NULL;
}

static void  PR_CALLBACK DestroyHandler(PLEvent *self) {
}




