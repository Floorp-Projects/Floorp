/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
 Client QA Team, St. Petersburg, Russia
*/

#include "plstr.h"
#include "stdio.h"
#include "nsIComponentManager.h"
#include "MThreadComponent2.h"
#include "nsMemory.h"
#include "prthread.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "nsID.h"

iMThreadContext* context;
void* iThread;
void* eThread;

MThreadComponent2Impl::MThreadComponent2Impl()
{    
    NS_INIT_REFCNT();
    printf("MThreadComponent2Impl::MThreadComponent2Imp\n");
}

MThreadComponent2Impl::~MThreadComponent2Impl()
{
}

NS_IMPL_ISUPPORTS2(MThreadComponent2Impl, iMThreadComponent2, iMThreadComponent);

NS_IMETHODIMP MThreadComponent2Impl::Initialize(iMThreadContext* c){
    context = c;
    printf("MThreadComponent2Impl::Initialize\n");
    iThread =(void*) PR_GetCurrentThread();
    return NS_OK;
}

NS_IMETHODIMP MThreadComponent2Impl::Execute(const char* tName){
    printf("MThreadComponent2Impl::Execute. Thread is %s\n", tName);
    eThread = PR_GetCurrentThread();
    if(eThread == iThread) {
        printf("###Execute and initialize were executed in one thread\n");
    }else {
        printf("###Execute and initialize were executed in DIFFERENT threads\n");
    }
    char* progID;
    context->GetNext(&progID);
    nsCID classID;
    classID.Parse(progID);
    printf("MThreadComponent2Impl::Execute. progID is %s\n",progID);
    iMThreadComponent* tComponent;
    nsresult rv = nsComponentManager::CreateInstance(classID,
                                            nsnull,
                                            NS_GET_IID(iMThreadComponent),
                                            (void**)&tComponent);   
    if(NS_FAILED(rv)) {
        printf("MThreadComponent2Impl:Create instance failed from %s!!!",classID.ToString());
        return rv;
    }
    tComponent->Initialize(context);
    tComponent->Execute(tName);
    
}








