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
 * Leila.Garin@eng.sun.com
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include "initImpl.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIComponentManager.h"
#include "bcIJavaSample.h"

static char * className = "initImpl";

NS_IMPL_ISUPPORTS1(initImpl, urpInit);

initImpl::initImpl() {
  NS_INIT_REFCNT();
  printf("Constructor of initImpl\n");
  nsresult rv;
  bcIJavaSample * serverComponent;
//  urpITest* anComp;
  rv = nsComponentManager::CreateInstance("bcJavaSample",
                                        nsnull,
                                        NS_GET_IID(bcIJavaSample),
                                        (void**)&serverComponent);
  if (NS_FAILED(rv)) {
        printf("Create instance failed in initImpl!!!");
	exit(-1);
  }
/*
  rv = nsComponentManager::CreateInstance("urpTest",
                                        nsnull,
                                        NS_GET_IID(urpITest),
                                        (void**)&anComp);
  if (NS_FAILED(rv)) {
        printf("Create instance failed in initImpl sec!!!");
	exit(-1);
  }
*/
  int l = 2000;
  serverComponent->Test1(100);
  printf("in initImpl after Test1 %d\n",l);
/*
  anComp->Test1(&l);
  printf("in initImpl after Test1 %d\n",l);
*/
    /*******************************************/
/*
    PRInt32 l2 = 2000;
    l = 1999;
    serverComponent->Test2(l,&l2);
    printf("--urpTestImpl after Test2 l2=%d\n",l2);
*/
    /*******************************************/
/*
    const char * s1 = "s1";
    char * s2 = "s2";
*/
    int intArray[] = {1,2,3};
    serverComponent->Test3(3, intArray);
//    printf("--urpTestImpl after Test3 s2=%s\n",s2);
    /*******************************************/

    char ** valueArray = (char **)malloc(sizeof(char*)*4);
    valueArray[0] = "hi";
    valueArray[1] = "there";
    valueArray[2] = "a";
    valueArray[3] = "b";

    char *** valueArray2 = &valueArray;
    serverComponent->Test4(4,valueArray2);
    for (int i = 0; i < 4; i++) {
            char *str = (*valueArray2)[i];
            printf("--[c++] valueArray2[%d]=%s\n",i,(str == NULL) ? "null" : str);
        }
    {

        nsIComponentManager* cm;
        nsresult rv = NS_GetGlobalComponentManager(&cm);
        printf("--[c++] bcJavaSample before test->Test5(cm)\n");
        serverComponent->Test5(cm);

        nsIEnumerator *enumerator;
        rv = cm->EnumerateCLSIDs(&enumerator);
        if (NS_FAILED(rv)) {
            printf("--[c++] can get enumerator\n");
        }
        printf("--[c++] bcJavaSample after test->Test5(cm)\n");
    }
    /*******************************************/
/*
    char ***valueArray2 = &valueArray;

    printf("call object\n");
    for (unsigned int i = 0; i < 4; i++) {
        printf("valueArray[%d]=%s\n",i,(*valueArray2)[i]);
    }
     printf("after calling object\n");
    for (unsigned int i = 0; i < 4; i++) {
        printf("valueArray[%d]=%s\n",i,(*valueArray2)[i]);
    }


    valueArray2 = (char ***)&valueArray;
    serverComponent->Test5(4,valueArray2);
    for (unsigned int i = 0; i < 4; i++) {
        printf("valueArray[%d]=%s\n",i,(*valueArray2)[i]);
    }
    {
        urpITest *p1;
        serverComponent->Test7(&p1);
        printf("p1=%p",p1);
        PRInt32 l = 1234;
        PRInt32 r;
        p1->Test1(&l);
        urpITest *p3;
        printf("--before QueryInterface calling \n");
        if (NS_SUCCEEDED(p1->QueryInterface(NS_GET_IID(urpITest),(void**)&p3)))
 {
            l=2000;
            p3->Test1(&l);
            printf("l in client after test1 %ld\n",l);
        }
	delete p1;
	delete p3;


    }
*/
//    delete serverComponent;
}

initImpl::~initImpl() {
}

