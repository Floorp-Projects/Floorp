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
#include "urpTestImpl.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"

static char * className = "urpTestImpl";

NS_IMPL_ISUPPORTS1(urpTestImpl, urpITest);

urpTestImpl::urpTestImpl() {
  NS_INIT_REFCNT();
}

urpTestImpl::~urpTestImpl() {
}

/*
nsrefcnt urpTestImpl::AddRef(void) { 
    static const char * methodName="AddRef";
    printf("--%s::%s this=%p",className, methodName,this);
    nsrefcnt cnt = (nsrefcnt) PR_AtomicIncrement((PRInt32*)&mRefCnt);
    return cnt;
}

nsrefcnt urpTestImpl::Release(void) {
    static const char * methodName="Release";
    nsrefcnt cnt = (nsrefcnt) PR_AtomicDecrement((PRInt32*)&mRefCnt);
    printf("--%s::%s this=%p",className, methodName,this);
    if(0 == cnt) {  
        delete this;
    }
    return cnt;
}

NS_IMETHODIMP urpTestImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr) {
    static const char * methodName="QueryInterface";
    printf("--%s::%s aIID=%s this=%p",className, methodName,aIID.ToString(),this);
    if ( !aInstancePtr ) {
        return NS_ERROR_NULL_POINTER;
    }
    if ( aIID.Equals(NS_GET_IID(nsISupports)) 
         || aIID.Equals(NS_GET_IID(urpITest)))
    {
        *(nsISupports**)aInstancePtr = this;
        return NS_OK;
    } else {
        return NS_NOINTERFACE;
    }
    
}
*/

/*void test1(inout long l);*/
NS_IMETHODIMP urpTestImpl::Test1(PRInt32 *l) {
    static char * methodName="Test1";
    printf("--%s::%s ",className, methodName);
    printf("this=%p l=%d\n", this, *l);
    *l = 1234;
    printf("--%s::%s ",className, methodName);
    printf("After l assignment. l=%d\n",*l);
//    *_retval = 500;
    return NS_OK;
}
    
/*void test2(in long l1,inout long l2);*/
NS_IMETHODIMP urpTestImpl::Test2(PRInt32 l1, PRInt32* l2) {
    static char * methodName="Test2";
    printf("this=%p l1=%d l2=%d\n", this, l1, *l2);
    *l2 = l1;
    printf("--%s::%s ",className, methodName);
    printf("After assignment. l1=%d l2=%d\n",l1,*l2);
    return NS_OK;
}

/*void test3(in string s1,inout string s2);*/
NS_IMETHODIMP urpTestImpl::Test3(const char *s1, char **s2) {
    static char * methodName="Test3";
    printf("--%s::%s ",className, methodName);
    printf("this=%p s1=%s s2=%s\n",this, s1,*s2);
    *s2 =  "hi";
    printf("--%s::%s ",className, methodName);
    printf("After assignment. s2=%s\n",*s2);
    return NS_OK;
}

/*void test4(in PRUint32 count,[array, size_is(count)] in string valueArray);*/
NS_IMETHODIMP urpTestImpl::Test4(PRUint32 count, const char **valueArray) {
    static char * methodName="Test4";
    printf("--%s::%s ",className, methodName);
    printf("this=%p count=%d",this, count);
    for (unsigned int i = 0; i < count; i++) {
        printf("--%s::%s ",className, methodName);
        printf("valueArray[%d]=%s\n",i,valueArray[i]);
    }
    return NS_OK;
}

/*void test5(in PRUint32 count,[array, size_is(count)] inout string valueArray);*/
NS_IMETHODIMP urpTestImpl::Test5(PRUint32 count, char ***valueArray) {
    static char * methodName="Test5";
    printf("--%s::%s ",className, methodName);
    printf("this=%p count=%d\n",this, count);
    //    char ***value = valueArray;
    //printf("value = %p *value = %p **value %p\n",value, *(char***)value,
    //       **(char***)value);

    for (unsigned int i = 0; i < count; i++) {
        printf("--%s::%s ",className, methodName);
        printf("valueArray[%d]=%s\n",i,(*valueArray)[i]);
    }
    
    char ** array = (char **)malloc(sizeof(char*)*4);
    array[0] = "1";
    array[1] = "2";
    array[2] = "hello";
    array[3] = "world";
    *valueArray = array;
    //printf("value = %p *value = %p **value %p\n",value, *(char***)value,
    //       **(char***)value);
    
    return NS_OK;
}

NS_IMETHODIMP urpTestImpl::Test6(class urpITest *o) {
    static const char * methodName="Test6";
    printf("--%s::%s ",className, methodName);
    printf("this=%p o=%p\n",this, o);
    PRInt32 l = 1234;
    PRInt32 r;
    o->Test1(&l);
    return NS_OK;
}

/* void test7 (out urpITest o); */
NS_IMETHODIMP urpTestImpl::Test7(urpITest **o) {
    static const char * methodName="Test7";
    printf("--%s::%s ",className, methodName);
    printf("this=%p *o=%p\n",this,*o);
    if (o == NULL) {
        return NS_ERROR_NULL_POINTER;
    }
    *o = new urpTestImpl();
    printf("--%s::%s ",className, methodName);
    printf("o=%p\n",o);
    return NS_OK;
}

