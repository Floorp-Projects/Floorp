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
#include "iMThreadContext.h"
#include "iMThreadComponent.h"
#include "MTStart.h"
#include "nsMemory.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "nsID.h"

iMThreadContext* context;
MTStartImpl::MTStartImpl()
{    
    NS_INIT_REFCNT();
    printf("MTStartImpl::MTStartImp\n");
    nsresult rv = nsComponentManager::CreateInstance(MTHREADCONTEXT_PROGID,
                                            nsnull,
                                            NS_GET_IID(iMThreadContext),
                                            (void**)&context); 
    if(NS_FAILED(rv)) {
        printf("Create instance of context failed!!!");
        return;
    }
    char* first;
    context->GetNext(&first);
    nsCID firstCID;
    firstCID.Parse(first);
    printf("ClassID is %s\n", firstCID.ToString());
    iMThreadComponent* tComponent;
    rv = nsComponentManager::CreateInstance(firstCID,
                                            nsnull,
                                            NS_GET_IID(iMThreadComponent),
                                            (void**)&tComponent);   
    if(NS_FAILED(rv)) {
        printf("Create instance failed from %s!!!",first);
        return;
    }
    tComponent->Initialize(context);
    tComponent->Execute("First from MTStart");
}

MTStartImpl::~MTStartImpl()
{
}

NS_IMPL_ISUPPORTS1(MTStartImpl, iMTStart);









