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
#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsHashtable.h"
#include "bcXPCOMStubsAndProxies.h"
#include "bcXPCOMStub.h"
#include "bcXPCOMProxy.h"
#include "bcXPCOMLog.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(bcXPCOMStubsAndProxies);

static  nsModuleComponentInfo components[] =
{
    {
        "Black Connect XPCOM stubs and proxies",
	    BC_XPCOMSTUBSANDPROXIES_CID,
        BC_XPCOMSTUBSANDPROXIES_ContractID,
        bcXPCOMStubsAndProxiesConstructor
    }
};

NS_IMPL_NSGETMODULE("BlackConnect XPCOM stubs and proxies",components);

NS_IMPL_THREADSAFE_ISUPPORTS(bcXPCOMStubsAndProxies,NS_GET_IID(bcXPCOMStubsAndProxies));



class bcOIDKey : public nsHashKey { 
protected:
    bcOID key;
public:
    bcOIDKey(bcOID oid) {
        key = oid;
    }
    virtual ~bcOIDKey() {
    }
    PRUint32 HashCode(void) const {                                               
        return (PRUint32)key;                                                      
    }                                                                             
                                                                                
    PRBool Equals(const nsHashKey *aKey) const {                                  
        return (key == ((const bcOIDKey *) aKey)->key);                          
    }  
    nsHashKey *Clone() const {                                                    
        return new bcOIDKey(key);                                                 
    }                                      
};


bcXPCOMStubsAndProxies::bcXPCOMStubsAndProxies() {
    NS_INIT_REFCNT();
    oid2objectMap = new nsSupportsHashtable(256, PR_TRUE);
    threadPrivateIndex = 0;
}

bcXPCOMStubsAndProxies::~bcXPCOMStubsAndProxies() {
    delete oid2objectMap;
}

NS_IMETHODIMP bcXPCOMStubsAndProxies::GetStub(nsISupports *obj, bcIStub **stub) {
    if (!stub) {
        return NS_ERROR_NULL_POINTER;
    }
    *stub = new bcXPCOMStub(obj);
    return NS_OK;
}

NS_IMETHODIMP bcXPCOMStubsAndProxies::GetOID(nsISupports *obj, bcIORB *orb, bcOID *oid) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    bcOID *tmp;
    if (NS_SUCCEEDED(obj->QueryInterface(NS_GET_IID(bcXPCOMProxy),(void**)&tmp))) {
        *oid = *tmp;
        PR_LOG(log, PR_LOG_DEBUG,("--[c++] bcXPCOMProxy::GetOID obj %p is a proxy to oid %d\n",obj,*oid));
    } else {
        bcIStub *stub = NULL;
        GetStub(obj, &stub);
        *oid = orb->RegisterStub(stub);
        oid2objectMap->Put(new bcOIDKey(*oid),obj);
    }
    return NS_OK;
}

NS_IMETHODIMP bcXPCOMStubsAndProxies::GetProxy(bcOID oid, const nsIID &iid, bcIORB *orb, nsISupports **proxy) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    nsresult rv = NS_OK;
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::GetProxy iid=%s\n",iid.ToString()));
    if (!proxy) {
        PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::GetProxy failed\n"));
        return NS_ERROR_NULL_POINTER;
    }
    *proxy = NULL;
    nsHashKey * key =  new bcOIDKey(oid);
    *proxy = oid2objectMap->Get(key); //doing shortcut
    delete key;
    if (*proxy != NULL) {
        rv = (*proxy)->QueryInterface(iid,(void **)proxy);
        PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::GetProxy we have shortcut for oid=%d\n",oid));
    } else {
        *proxy = new bcXPCOMProxy(oid,iid,orb);
        NS_IF_ADDREF(*proxy);
    }
    return rv;
}


struct EventQueueStack {
    PRUint32 stackPointer;
    nsIEventQueue *stack[200];
    EventQueueStack() {
        stackPointer = 0;
    }
};

NS_IMETHODIMP bcXPCOMStubsAndProxies::GetEventQueue(nsIEventQueue **eventQueue) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::GetEventQueue\n"));
    EventQueueStack *stack;
    if (threadPrivateIndex == 0) {
        *eventQueue = NULL;
    } else {
        stack = (EventQueueStack*)PR_GetThreadPrivate(threadPrivateIndex);
        if (stack !=0 
            && stack->stackPointer != 0) {
            *eventQueue = stack->stack[(stack->stackPointer)-1];
            PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::GetEventQueue eventQueue=%p\n",*eventQueue));
            NS_IF_ADDREF(*eventQueue);
        } else {
            *eventQueue = NULL;
        }
    }
    return NS_OK;
}
NS_IMETHODIMP bcXPCOMStubsAndProxies::PopEventQueue(nsIEventQueue **_eventQueue) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::PopEventQueue\n"));
    EventQueueStack *stack;
    nsIEventQueue * eventQueue;
    if (threadPrivateIndex == 0) {
        *_eventQueue = NULL;
        PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::PopEventQueue 1\n"));
    } else {
        stack = (EventQueueStack*)PR_GetThreadPrivate(threadPrivateIndex);
        if (stack !=0 
            && stack->stackPointer != 0) {
            PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::PopEventQueue 2\n"));
            eventQueue = stack->stack[--(stack->stackPointer)];
        } else {
            PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::PopEventQueue 3\n"));
            eventQueue = NULL;
        }
        if (_eventQueue != NULL) {
            PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::PopEventQueue 4\n"));
            *_eventQueue = eventQueue;
        } else {
            PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::PopEventQueue eventQueue=%p\n",eventQueue));
            NS_IF_RELEASE(eventQueue);
        }
    }
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::PopEventQueue 6\n"));
    return NS_OK;    
}
NS_IMETHODIMP bcXPCOMStubsAndProxies::PushEventQueue(nsIEventQueue *eventQueue) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::PushEventQueue eventQueue=%p\n",eventQueue));
    nsresult r;
    if (threadPrivateIndex == 0) {
        r = PR_NewThreadPrivateIndex(&threadPrivateIndex,NULL);
        PR_ASSERT(threadPrivateIndex != 0
                  && NS_SUCCEEDED(r));
    }
    EventQueueStack *stack;
    stack = (EventQueueStack*)PR_GetThreadPrivate(threadPrivateIndex);
    if (stack == NULL) {
        stack = new EventQueueStack();
        PR_SetThreadPrivate(threadPrivateIndex, (void *)stack);
    }
    NS_IF_ADDREF(eventQueue);
    stack->stack[(stack->stackPointer)++] = eventQueue;
    return NS_OK;
}

