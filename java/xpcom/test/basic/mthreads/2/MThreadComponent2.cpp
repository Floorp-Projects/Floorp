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
#include "nsIComponentManager.h"
#include "MThreadComponent2.h"
#include "nsMemory.h"
#include "prthread.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "nsID.h"


iMThreadContext* context=NULL;
iMThreadComponent* tComponent=NULL;
void* iThread=NULL;
void* eThread=NULL;
int newT=0, newS=0;

void Exec(void* arg) {
	tComponent->Initialize(context);
        nsIComponentManager* cm;
        nsresult rv = NS_GetGlobalComponentManager(&cm);
        if(NS_FAILED(rv)) {
            fprintf(stderr, "ERROR: Can't get GlobalComponentManager!!\n");
	    return;
        }
        tComponent->THack(cm);
	tComponent->Execute(newT,newS);
}
  
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

NS_IMETHODIMP MThreadComponent2Impl::THack(nsIComponentManager* cm){
    return NS_OK;
}


NS_IMETHODIMP MThreadComponent2Impl::Execute(int thread, int stage){

	char* tmp=(char*)PR_Malloc(sizeof(char*));
	tmp="";
	FILE *file;

    printf("MThreadComponent2Impl::Execute.\n");
    eThread = PR_GetCurrentThread();
    if(eThread == iThread) {
        printf("###Execute and initialize were executed in one thread\n");
    } else {
        printf("###Execute and initialize were executed in DIFFERENT threads\n");
    }

	char* progID;
        char* res=(char*)PR_Malloc(sizeof(char*));

	context->GetResFile(&res);

	if ((file = fopen(res, "a+t")) == NULL) {
		fprintf(stderr, "ERROR: can't open file %s\n",res);
		return NS_OK;
	}
	sprintf(tmp,"%d,%d,",thread,stage);
	fwrite(tmp,1,strlen(tmp),file);
	sprintf(tmp,"%d\n",eThread);
	fwrite(tmp,1,strlen(tmp),file);
	fclose(file);

    context->GetContractID(thread,stage+1,&progID);
    nsCID classID;

	    classID.Parse(progID);
	    nsresult rv = nsComponentManager::CreateInstance(classID,
	                                            nsnull,
	                                            NS_GET_IID(iMThreadComponent),
	                                            (void**)&tComponent);   
    if(NS_FAILED(rv)) {
        fprintf(stderr, "MThreadComponent2Impl:Create instance failed from %s!!!\n",classID.ToString());
    } else {
	tComponent->Initialize(context);
        nsIComponentManager* cm;
        rv = NS_GetGlobalComponentManager(&cm);
        if(NS_FAILED(rv)) {
            fprintf(stderr, "ERROR: Can't get GlobalComponentManager!!\n");
	    return NS_OK;
        }
        tComponent->THack(cm);
	tComponent->Execute(thread,stage+1);
    }


    context->GetContractID(thread+1,stage+1,&progID);
    classID.Parse(progID);
    rv = nsComponentManager::CreateInstance(classID, nsnull,
                                         NS_GET_IID(iMThreadComponent),
                                         (void**)&tComponent);   
    if(NS_FAILED(rv)) {
        fprintf(stderr, "MThreadComponent2Impl: 2: Create instance failed from %s!!!\n",progID);
    } else {
	newT=thread+1;
	newS=stage+1;
	PR_CreateThread(PR_USER_THREAD, Exec,
			this, PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
			PR_UNJOINABLE_THREAD, 0);
    }

 return NS_OK;    
}
