/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


/**
 *
 * A sample of XPConnect. This file contains an implementation nsSample
 * of the interface nsISample.
 *
 */
#include "plstr.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "stdio.h"
#include "nsIComponentManager.h"
#include "BCTestLoader.h"
#include "prprf.h"
#include "prmem.h"
#include "nsMemory.h"
#include "BCTest.h"

//////////////////


/////////////////

////////////////////////////////////////////////////////////////////////

BCTestLoaderImpl::BCTestLoaderImpl()
{    
    NS_INIT_REFCNT();
    printf("DEbug:avm:BCTestLoaderImpl::BCTestLoaderImpl()\n");
    testLocation = getenv(BC_TEST_LOCATION_VAR_NAME);
    if(!testLocation) {
        fprintf(stderr, "ERROR: %s enviroment variable isn't set !\n",BC_TEST_LOCATION_VAR_NAME);
        return;
    }
    char** testCase = LoadTestList();
    int i = 0;
    if (!testCase) {
        fprintf(stderr, "ERROR: Can't load test list !\n");
        return;
    }
    for(i=0; testCase[i]; i++) {
        RunTest(testCase[i]);
    }
}

BCTestLoaderImpl::~BCTestLoaderImpl()
{
    /*
    if (mValue)
        PL_strfree(mValue);
    */
    printf("DEbug:avm:BCTestLoaderImpl::~BCTestLoaderImpl()\n");
}

/**
 * NS_IMPL_ISUPPORTS1 expands to a simple implementation of the nsISupports
 * interface.  This includes a proper implementation of AddRef, Release,
 * and QueryInterface.  If this class supported more interfaces than just
 * nsISupports, 
 * you could use NS_IMPL_ADDREF() and NS_IMPL_RELEASE() to take care of the
 * simple stuff, but you would have to create QueryInterface on your own.
 * nsSampleFactory.cpp is an example of this approach.
 * Notice that the second parameter to the macro is the static IID accessor
 * method, and NOT the #defined IID.
 */
NS_IMPL_ISUPPORTS1(BCTestLoaderImpl, BCITestLoader);

/**
 * Notice that in the protoype for this function, the NS_IMETHOD macro was
 * used to declare the return type.  For the implementation, the return
 * type is declared by NS_IMETHODIMP
 */


    /**
     * Another buffer passing convention is that buffers passed INTO your
     * object ARE NOT YOURS.  Keep your hands off them, unless they are
     * declared "inout".  If you want to keep the value for posterity,
     * you will have to make a copy of it.
     */
 
/**

Custom methods

**/

NS_IMETHODIMP BCTestLoaderImpl::RunTest(char* ID) {
    nsresult rv;
    iClientTestComponent* client = NULL;
    printf("DEbug:avm:Running test %s\n",ID);
    rv = InitClient(ID,&client);
    if(NS_FAILED(rv)) {
        return rv;
    }
    rv = client->Execute();
    return rv;
}


NS_IMETHODIMP BCTestLoaderImpl::InitClient(char* ID, iClientTestComponent** client) {
    char * testProgID = NULL;
    char * clientProgID = NULL;
    char * serverProgID = NULL;
    nsresult rv;
    testProgID = PL_strdup(BC_PROGID_PREFIX);
    clientProgID = PR_sprintf_append(clientProgID,"%s%sClient;1",testProgID, ID);   
    serverProgID = PR_sprintf_append(serverProgID,"%s%sServer;1",testProgID, ID);
    fprintf(stderr,"DEbug:avm:Loading %s\n",clientProgID);
    char** exclusionList = LoadExclusionList(ID);
    if(ID[0] == 'J') {
        fprintf(stderr,"DEbug:avm:Loks that we are running J2X tests\n");
        iJClientTestComponent* client1=(iJClientTestComponent*)client;
        rv = nsComponentManager::CreateInstance(clientProgID,
                                                nsnull,
                                                NS_GET_IID(iJClientTestComponent),
                                                (void**)&client1);
        if(NS_FAILED(rv)) {
            fprintf(stderr, "ERROR: Can't create instance of %s !\n",clientProgID);
            return rv;
        }
        nsIComponentManager* cm;
        rv = NS_GetGlobalComponentManager(&cm);
        if(NS_FAILED(rv)) {
            fprintf(stderr, "ERROR: Can't get GlobalComponentManager!!\n");
            return rv;
        }
        rv = client1->THack(cm, serverProgID);
        client1->QueryInterface(NS_GET_IID(iClientTestComponent),(void**)client);
        NS_RELEASE(client1);
    }else {
        rv = nsComponentManager::CreateInstance(clientProgID,
                                                nsnull,
                                                NS_GET_IID(iClientTestComponent),
                                                (void**)client);   
        if(NS_FAILED(rv)) {
            fprintf(stderr, "ERROR: Can't create instance of %s !\n",clientProgID);
            return rv;
        }
    
        rv = (*client)->Initialize(serverProgID);
        if(NS_FAILED(rv)) {
            fprintf(stderr, "ERROR: Can't initialize client with param  %s !\n",serverProgID);
            return rv;
        }
    }
    if(exclusionList != NULL) {
        iExclusionSupport* exClient;
        rv = (*client)->QueryInterface(NS_GET_IID(iExclusionSupport),(void**)&exClient);
        if(NS_FAILED(rv)) {
            fprintf(stderr, "Looks, that %s tests is still not support exculsion!!!\n",ID);
            return NS_OK;
        }else {
            fprintf(stderr, "Looks, that %s tests support exculsion!!!\n",ID);
        }
        PRInt32 count;
        for(count=0; exclusionList[count]; count++);
        exClient->Exclude(count,(const char **) exclusionList);
        NS_RELEASE(exClient);
    }
    return NS_OK;
}


/*
***  char** BCTestLoaderImpl::LoadTestList().
***  Loads list of tests to execute. Derived from OJI TestLoader loadTestList();
*/

char** BCTestLoaderImpl::LoadTestList() {
	struct stat st;
	FILE *file = NULL;
	char *content;
	int nRead, nTests, count = 0;
	char *pos, *pos1;
	char **testList;
    char *lstFile = NULL;
    lstFile = PR_sprintf_append(lstFile,"%s/%s",testLocation,BC_TEST_LIST_FILE_NAME);
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
		fprintf(stderr, "WARNING: can't read entire file in text mode (%d of %ld) !\n", nRead, st.st_size);
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



/*
***  char** BCTestLoaderImpl::LoadExclusionList().
***  Loads list of tests to exclude from execution. 
*/

char** BCTestLoaderImpl::LoadExclusionList(char* ID) {
	struct stat st;
	FILE *file = NULL;
	char *content;
	int nRead, nTests, count = 0;
	char *pos, *pos1;
	char **excludeList;
    char *excludeFile = NULL;
    excludeFile = PR_sprintf_append(excludeFile,"%s/%s.exclude",testLocation,ID);
	if (stat(excludeFile, &st) < 0) {
		fprintf(stderr, "Can't found exclusion file %s for %s tests\n",excludeFile,ID);
		return NULL;
	}
	content = (char*)PR_Calloc(1, st.st_size+1);
	if ((file = fopen(excludeFile, "r")) == NULL) {
		fprintf(stderr, "ERROR: can't open file %s\n",excludeFile);
		return NULL;
	}
	if ((nRead = fread(content, 1, st.st_size, file)) < st.st_size) {
		fprintf(stderr, "WARNING: can't read entire file in text mode (%d of %ld) !\n", nRead, st.st_size);
		//return;
	}
	content[nRead] = 0;
	printf("File content: %s\n", content);
	fclose(file);

	//allocate maximal possible size
	nTests = countChars(content, '\n') + 2;
	printf("nTests = %d\n", nTests);
	excludeList = (char**)PR_Calloc(sizeof(char*), nTests);
	excludeList[0] = 0;
	pos = content;
	while((pos1 = PL_strchr(pos, '\n'))) {
		*pos1 = 0;
		if(PL_strlen(pos) > 0 && *pos != '#') {
		  //printf("First char: %c\n", *pos);
			excludeList[count++] = PL_strdup(pos);
		}
		pos = pos1+1;
		if (!(*pos)) {
			printf("Parser done: %d .. ", count);
			excludeList[count] = 0;
			printf("ok\n");
			break;
		}
	}
	//If there is no \n after last line
	if (PL_strlen(pos) > 0 && *pos != '#') {
		excludeList[count++] = PL_strdup(pos);
		excludeList[count] = 0;
	}
	//free(content);
	return excludeList;
	
}

int BCTestLoaderImpl::countChars(char* buf, char ch) {
	char *pos = buf;
	int count = 0;

	while((pos = PL_strchr(pos, ch))) { 
		pos++;
		count++;
	}
	return count;
} 



/*
NS_IMETHODIMP
BCTestLoaderImpl::Start()
{
    printf("Test method called");
    return NS_OK;
}
*/












