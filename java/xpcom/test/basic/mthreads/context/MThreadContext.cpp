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
#include "prenv.h"
#include "stdio.h"
#include "nsMemory.h"
#include "BCTest.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "MThreadContext.h"
#include "prmem.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include "nsID.h"

nsIComponentManager* cm;
char **componentList;
int currentPosition = 0;
#define MT_COMPONENT_LIST_VAR_NAME "MT_COMPONENT_LIST"



MThreadContextImpl::MThreadContextImpl()
{    
    NS_INIT_REFCNT();
    printf("DEbug:avm:MThreadContextImpl::MThreadContextImp\n");
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if(NS_FAILED(rv)) {
        fprintf(stderr, "ERROR: Can't get GlobalComponentManager!!\n");
    }
    componentList = LoadComponentList();
}

MThreadContextImpl::~MThreadContextImpl()
{
    //nb 
}

NS_IMPL_ISUPPORTS1(MThreadContextImpl, iMThreadContext);


NS_IMETHODIMP MThreadContextImpl::GetNext(char **_retstr) {
    nsCID aClass;
    if(componentList == NULL) {
        *_retstr = NULL;
	return NS_OK; //nb
    }			 
    if(componentList[currentPosition]) {
        *_retstr = componentList[currentPosition];
        printf("MThreadContextImpl::GetNext. ContractID = %s\n",*_retstr);
        nsresult rv = cm->ContractIDToClassID(*_retstr, &aClass);
        if(NS_FAILED(rv)) {
            printf("MThreadContextImpl::GetNext. Can't convert ContractID to ClassID\n");
            *_retstr = NULL;
            return NS_OK;
        }else {
            printf("MThreadContextImpl::GetNext. SUCCESS convert ContractID to ClassID\n");
        }
        *_retstr = aClass.ToString();
        printf("MThreadContextImpl::GetNext. ClassID = %s\n",*_retstr);
        currentPosition++;
    } else {
        *_retstr = NULL;
    }
    return NS_OK;
}
/*
NS_IMETHODIMP MThreadContextImpl::Initialize(const char* serverProgID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MThreadContextImpl::Execute(){
    return NS_ERROR_NOT_IMPLEMENTED;
}
*/
NS_IMETHODIMP MThreadContextImpl::GetComponentManager(nsIComponentManager **_retval) {
    *_retval = cm;
    return NS_OK;
}

char** MThreadContextImpl::LoadComponentList() {
	struct stat st;
	FILE *file = NULL;
	char *content;
	int nRead, nTests, count = 0;
	char *pos, *pos1;
	char **testList;
    char *lstFile = PR_GetEnv(MT_COMPONENT_LIST_VAR_NAME);
    if(lstFile == NULL) {
        fprintf(stderr, "ERROR: %s value is'n set\n",MT_COMPONENT_LIST_VAR_NAME);
        return NULL;
    }

	if (stat(lstFile, &st) < 0) {
		fprintf(stderr, "ERROR: can't get stat from file %s\n",lstFile);
		return NULL;
	}
	content = (char*)calloc(1, st.st_size+1);
	if ((file = fopen(lstFile, "r")) == NULL) {
		fprintf(stderr, "ERROR: can't open file %s\n",lstFile);
		return NULL;
	}
	if ((nRead = fread(content, 1, st.st_size, file)) < st.st_size) {
		fprintf(stderr, "WARNING: can't read entire file in text mode (%d of %d) !\n", nRead, st.st_size);
		//return;
	}
	content[nRead] = 0;
	printf("File content: %s\n", content);
	fclose(file);

	//allocate maximal possible size
	nTests = countChars(content, '\n') + 2;
	printf("nTests = %d\n", nTests);
	testList = (char**)calloc(sizeof(char*), nTests);
	testList[0] = 0;
	pos = content;
	while((pos1 = PL_strchr(pos, '\n'))) {
		*pos1 = 0;
		if(PL_strlen(pos) > 0 && *pos != '#') {
		  //printf("First char: %c\n", *pos);
			testList[count++] = PL_strdup(pos);
		}
		pos = pos1+1;
		if (!(*pos)) {
			printf("Parser done: %d .. ", count);
			testList[count] = 0;
			printf("ok\n");
			break;
		}
	}
	//If there is no \n after last line
	if (PL_strlen(pos) > 0 && *pos != '#') {
		testList[count++] = PL_strdup(pos);
		testList[count] = 0;
	}
	//free(content);
	return testList;
	
}

int MThreadContextImpl::countChars(char* buf, char ch) {
	char *pos = buf;
	int count = 0;

	while((pos = PL_strchr(pos, ch))) { 
		pos++;
		count++;
	}
	return count;
} 









