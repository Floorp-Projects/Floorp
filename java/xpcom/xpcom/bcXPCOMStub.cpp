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


#include "nsIServiceManager.h"

#include "nsIThread.h"


static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
struct CallInfo;

static void* PR_CALLBACK EventHandler(PLEvent *self);
static void  PR_CALLBACK DestroyHandler(PLEvent *self);


struct CallInfo {
    CallInfo(nsIEventQueue * eq, bcICall *c, bcIStub *s) {
        callersEventQ = eq; call = c; stub = s; completed = PR_FALSE;
    }
    nsCOMPtr<nsIEventQueue> callersEventQ;
    bcICall *call;
    bcIStub *stub;
    PRBool completed;
};

bcXPCOMStub::bcXPCOMStub(nsISupports *o) : object(o) {
    NS_ADDREF(object);
    _mOwningThread = NS_CurrentThread();
    eventQService = do_GetService(kEventQueueServiceCID);
    eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(owningEventQ));
}

bcXPCOMStub::~bcXPCOMStub() {
    NS_RELEASE(object);
}

void bcXPCOMStub::Dispatch(bcICall *call) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    if (_mOwningThread == NS_CurrentThread()) {
        PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStub::Dispatch(bcICall *call)\n"));
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
        }
        //nb return value; excepion handling
        XPTC_InvokeByIndex(object, mid, paramCount, params);
        if (mt != NULL) { //nb to do what about nsresult ?
            bcIMarshaler * m = call->GetMarshaler();    
            mt->Marshal(m);
        }
        //nb memory deallocation
        return;
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
    }  
}



static void* EventHandler(PLEvent *self) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--about to EventHandler\n"));
    CallInfo * callInfo = (CallInfo*)PL_GetEventOwner(self);
    callInfo->stub->Dispatch(callInfo->call);
    callInfo->completed = PR_TRUE;
    PR_LOG(log, PR_LOG_DEBUG, ("--about to callInfo->callersEventQ->PostEvent;\n"));
    PLEvent *event = PR_NEW(PLEvent);
    callInfo->callersEventQ->PostEvent(event);
    return NULL;
}

static void  PR_CALLBACK DestroyHandler(PLEvent *self) {
}


