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

#define BC_JAVA_SAMPLE_CID \
{0x072fa586, 0x1dd2, 0x11b2, \
{ 0xb2, 0x3a, 0x81, 0xe8, 0x16, 0x49, 0xe8, 0x8b }}

#define BC_JAVA_SAMPLE_PROGID "javaSample.cpp"



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
    printf("--[c++] bcJavaSample.test1\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void test2 (in bcIJavaSample o); */
NS_IMETHODIMP bcJavaSample::Test2(bcIJavaSample *o)
{   printf("--[c++] bcJavaSample.test2\n");
    return NS_ERROR_NOT_IMPLEMENTED;
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
    //sigsend(P_PID, getpid(),SIGINT);
    //test->Test1(2000);
    //test->Test1(1000);
    if (NS_FAILED(r)) {
	printf("failed to get component. try to restart test\n");
    } else {
        test->Test2(a);
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

