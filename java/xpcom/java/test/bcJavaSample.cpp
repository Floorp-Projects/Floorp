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
#include "stdlib.h"

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
        printf("--[c++] valueArray[%d]=%s\n",i,(*valueArray)[i]);
    }
    char ** array = (char **)malloc(sizeof(char*)*4);
    array[0] = "1";
    array[1] = "2";
    array[2] = "hello";
    array[3] = "world";
    *valueArray = array;
    return NS_OK;
}
void test() {
    printf("--BlackConnect test start\n");
    nsresult r;
    bcIJavaSample *test;
    bcIJavaSample *a = new bcJavaSample();
    r = nsComponentManager::CreateInstance("bcJavaSample",
					   nsnull,
					   NS_GET_IID(bcIJavaSample),
					   (void**)&test);
    if (NS_FAILED(r)) {
        printf("--[debug] can not load bcJavaSample\n");
        return;
    }
    //sigsend(P_PID, getpid(),SIGINT);
    //test->Test1(2000);
    test->Test1(1000);
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
            printf("valueArray2[%d]=%s\n",i,(*valueArray2)[i]);
        }
    }
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

