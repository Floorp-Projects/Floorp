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
#include "bcIJavaSample.h"

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsIEnumerator.h"
#include "stdlib.h"
#include "prthread.h"
#include "prmon.h"

#define BC_JAVA_SAMPLE_CID \
{0x072fa586, 0x1dd2, 0x11b2, \
{ 0xb2, 0x3a, 0x81, 0xe8, 0x16, 0x49, 0xe8, 0x8b }}

class bcJavaSample : public bcIJavaSample {
    NS_DECL_ISUPPORTS  
    NS_DECL_BCIJAVASAMPLE
    bcJavaSample();
    virtual ~bcJavaSample();
};

NS_IMPL_ISUPPORTS1(bcJavaSample, bcIJavaSample)

bcJavaSample::bcJavaSample()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

bcJavaSample::~bcJavaSample()
{
  /* destructor code */
}

NS_IMETHODIMP bcJavaSample::Test0()
{   printf("--[c++] bcJavaSample::Test0() \n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void test1 (in long l); */
NS_IMETHODIMP bcJavaSample::Test1(PRInt32 l)
{
    printf("--[c++] bcJavaSample.test1 l=%d\n",l);
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void test2 (in bcIJavaSample o); */
NS_IMETHODIMP bcJavaSample::Test2(bcIJavaSample *o)
{ 
    printf("--[c++] bcJavaSample.test2\n");
    o->Test0();
    return NS_OK;
}


/* void test3 (in PRUint32 count, [array, size_is (count)] in long valueArray); */
NS_IMETHODIMP bcJavaSample::Test3(PRUint32 count, PRInt32 *valueArray) {
    printf("--[c++] bcJavaSample.test3 coutn %d\n",count);
    for(unsigned int i = 0; i < count; i++) {
        printf("--[c++] valueArray[%d]=%d\n",i,valueArray[i]);
    }
    return NS_OK;
}

/* void test4 (in PRUint32 count, [array, size_is (count)] inout string valueArray); */
NS_IMETHODIMP bcJavaSample::Test4(PRUint32 count, char ***valueArray) {
    printf("--[c++] bcJavaSample.test4 coutn %d\n",count);
    for(unsigned int i = 0; i < count; i++) {
        char *str = (*valueArray)[i];
        printf("--[c++] valueArray[%d]=%s\n",i,(str == NULL) ? "null" : str);
    }
    char ** array = (char **)malloc(sizeof(char*)*4);
    array[0] = "1";
    array[1] = "2";
    array[2] = "hello";
    array[3] = "world";
    *valueArray = array;
    return NS_OK;
}

/* void test5 (in nsIComponentManager cm); */
NS_IMETHODIMP bcJavaSample::Test5(nsIComponentManager *cm) {
    return NS_OK;
}

/* void test6 (in PRUint32 count, [array, size_is (count)] in string valueArray); */
NS_IMETHODIMP bcJavaSample::Test6(PRUint32 count, const char **valueArray) {
    printf("--[c++] bcJavaSample.test6 coutn %d\n",count);
    for(unsigned int i = 0; i < count; i++) {
        printf("--[c++] valueArray[%d]=%s\n",i,valueArray[i]);
    }
    return NS_OK;
}

/* void test7 (in PRUint32 count, [array, size_is (count)] out char valueArray); */
NS_IMETHODIMP bcJavaSample::Test7(PRUint32 *count, char **valueArray) {
    return NS_OK;
}

/* void test8 (in nsCIDRef cid); */
NS_IMETHODIMP bcJavaSample::Test8(const nsCID & cid) {
    
    printf("--[c++]bcJavaSample::Test8 %s\n",cid.ToString());
    return NS_OK;
}

/* void test9 (out nsIIDPtr po); */
NS_IMETHODIMP bcJavaSample::Test9(nsIID * *po) {
    if (po != NULL) {
        printf("--[c++]bcJavaSample::Test9 %s\n", (*po)->ToString());
    } else {
        printf("--[c++]bcJavaSample::Test9 %s\n", "null");
    }
    return NS_OK;
}


static bcIJavaSample * javaSample = NULL;

#if 1
void thread_start( void *arg ) {   
    printf("--thread_start currentThread=%d\n",PR_GetCurrentThread());
    nsresult r;
    if (javaSample == NULL) {
        r = nsComponentManager::CreateInstance("bcJavaSample",
                                               nsnull,
                                               NS_GET_IID(bcIJavaSample),
                                               (void**)&javaSample);
    }
    javaSample->Test1((int)PR_GetCurrentThread());
    printf("--thread_start after first invocation \n");
    javaSample->Test1((int)PR_GetCurrentThread());
    printf("--thread_start after second invocation \n");
}
#endif 

#if 0
void thread_start( void *arg ) {   
    printf("--thread_start currentThread=%p\n",PR_GetCurrentThread());
    nsresult r;
    bcIJavaSample *t;
    javaSample->Test1((int)PR_GetCurrentThread());
    printf("--thread_start after first invocation \n");
}

#endif 

void test() {
    printf("--BlackConnect test start\n");
    nsresult r;
    bcIJavaSample *test;
    bcIJavaSample *a = new bcJavaSample();
#if 0
    r = nsComponentManager::CreateInstance("bcJavaSample",
					   nsnull,
					   NS_GET_IID(bcIJavaSample),
					   (void**)&test);
    if (NS_FAILED(r)) {
        printf("--[debug] can not load bcJavaSample\n");
        return;
    }
#endif 
    //sigsend(P_PID, getpid(),SIGINT);
    //test->Test1(2000);
    
#if 1
    {
        PRThread *thr = PR_CreateThread( PR_USER_THREAD,
                                         thread_start,
                                         test,
                                         PR_PRIORITY_NORMAL,
                                         PR_LOCAL_THREAD,
                                         PR_JOINABLE_THREAD,
                                         0);
        PR_JoinThread(thr);

        for (int i = 0; i < 200; i++) {
            printf("\n--we are creating threads i=%d\n",i);
            PRThread *thr = PR_CreateThread( PR_USER_THREAD,
                                             thread_start,
                                             test,
                                             PR_PRIORITY_NORMAL,
                                             PR_LOCAL_THREAD,
                                             PR_JOINABLE_THREAD,
                                             0);
            //PR_JoinThread(thr);
        }
    }
#endif            
    //thread_start(test); 
    if (javaSample == NULL) {
        return;
    } 
    test = javaSample;
    test->Test1((int)PR_GetCurrentThread());
    return;
    bcIJavaSample *test1;
    if (NS_FAILED(r)) {
        printf("failed to get component. try to restart test\n");
    } else {
        test->Test2(a);
    }
    test->QueryInterface(NS_GET_IID(bcIJavaSample),(void**)&test1);
    int intArray[] = {1,2,3};
    test->Test3(3, intArray);
    {
        char ** valueArray = (char **)malloc(sizeof(char*)*4);
        valueArray[0] = "hi";
        valueArray[1] = "there";
        valueArray[2] = "a";
        valueArray[3] = "b";
        char *** valueArray2 = &valueArray;
        test->Test4(4,valueArray2);
        for (int i = 0; i < 4; i++) {
            char *str = (*valueArray2)[i];
            printf("--[c++] valueArray2[%d]=%s\n",i,(str == NULL) ? "null" : str);
        }
    }
#if 1
    {
        
        nsIComponentManager* cm;
        nsresult rv = NS_GetGlobalComponentManager(&cm);
        printf("--[c++] bcJavaSample before test->Test5(cm)\n");
        test->Test5(cm);

        nsIEnumerator *enumerator;
        rv = cm->EnumerateCLSIDs(&enumerator);
        if (NS_FAILED(rv)) {
            printf("--[c++] can get enumerator\n");
        }
        printf("--[c++] bcJavaSample after test->Test5(cm)\n");
    }

    {
        const char ** valueArray = (const char **)malloc(sizeof(char*)*4);
        valueArray[0] = "hi";
        valueArray[1] = "there";
        valueArray[2] = "a";
        //valueArray[3] = "b";

        valueArray[3] = NULL;
        test->Test6(4,valueArray);
    }

    {
        printf("--[c++]about to test7\n");
        PRUint32 count; 
        char *charArray;
        test->Test7(&count,&charArray);
        for (int i = 0; i < count; i++) {
            printf("--[c++] charArray[%d]=%c\n",i,charArray[i]);
        }
        printf("--[c++]end of test7\n");
    }

    {
        printf("--[c++]about to test8\n");
        
        test->Test8(NS_GET_IID(bcIJavaSample));
        printf("--[c++]end of test8\n");
    }
    {
        nsCID cid = NS_GET_IID(bcIJavaSample);
        nsCID *cidParam = &cid;
        printf("--[c++]about to test9\n");
        test->Test9(&cidParam);
        printf("--[c++]end of test9\n");
    }
#endif
    printf("--BlackConnect test end\n");
}

static int counter = 0;  //we do not need to call it on unload time;
extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *compMgr,
                                          nsIFile *location,
                                          nsIModule** result)  //I am using it for runnig test *only*
{
    if (counter == 0) {
        counter ++;
        printf("--bcJavaSample before test\n");
        test();
        printf("--bcJavaSample after test\n");
    }
    return NS_ERROR_FAILURE;
}

