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
#include "prmem.h"
#include "stdio.h"
#include "prthread.h"
#include "nsIComponentManager.h"
#include "iMThreadContext.h"
#include "iMThreadComponent.h"
#include "MTStart.h"
#include "nsMemory.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "nsID.h"

iMThreadContext* context=NULL;
void* eThread=NULL;
FILE *file=NULL;

MTStartImpl::MTStartImpl()
{    
    NS_INIT_REFCNT();
    printf("MTStartImpl::MTStartImp\n");
    nsresult rv = nsComponentManager::CreateInstance(MTHREADCONTEXT_PROGID,
                                            nsnull,
                                            NS_GET_IID(iMThreadContext),
                                            (void**)&context); 
    if(NS_FAILED(rv)) {
        fprintf(stderr, "Create instance of context failed!!!");
        return;
    }
    char* first=(char*)PR_Malloc(sizeof(char*));

    
    context->GetContractID(0,1,&first);
    nsCID firstCID;
    firstCID.Parse(first);
    iMThreadComponent* tComponent=NULL;
    rv = nsComponentManager::CreateInstance(firstCID,
                                            nsnull,
                                            NS_GET_IID(iMThreadComponent),
                                            (void**)&tComponent);   
    if(NS_FAILED(rv)) {
        fprintf(stderr, "Create instance failed from %s!!!",first);
        return;
    }

    eThread = PR_GetCurrentThread();

        char* res=(char*)PR_Malloc(sizeof(char*));
	context->GetResFile(&res);

	if ((file = fopen(res, "w+t")) == NULL) {
		fprintf(stderr, "ERROR: can't open file %s\n",res);
	}
	char* tmp="0,0,";
	fwrite(tmp,1,PL_strlen(tmp),file);
	sprintf(tmp,"%d\n",eThread);
	fwrite(tmp,1,PL_strlen(tmp),file);
	fclose(file);

    tComponent->Initialize(context);
    nsIComponentManager* cm;
    rv = NS_GetGlobalComponentManager(&cm);
    if(NS_FAILED(rv)) {
        fprintf(stderr, "ERROR: Can't get GlobalComponentManager!!\n");
    }
    tComponent->THack(cm);
    tComponent->Execute(0,1);
}

MTStartImpl::~MTStartImpl()
{
}

NS_IMPL_ISUPPORTS1(MTStartImpl, iMTStart);
