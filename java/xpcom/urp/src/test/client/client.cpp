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
 * Sergey Lunegov <lsv@sparc.spb.su>
 */

#include <stdio.h>
#include <stdlib.h>
#include "urpTestImpl.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"






/****************************************************/

#include "nsIModule.h"
#include "nsIServiceManager.h"
#include "bcIXPCOMStubsAndProxies.h"
#include "bcXPCOMStubsAndProxiesCID.h"
#include "bcIORBComponent.h"
#include "bcORBComponentCID.h"
#include "bcIStub.h"
#include "urpStub.h"

static NS_DEFINE_CID(kORBCIID,BC_ORBCOMPONENT_CID);
static NS_DEFINE_CID(kXPCOMStubsAndProxies,BC_XPCOMSTUBSANDPROXIES_CID);

int main(int argc, char *argv[]) {
    
nsIInterfaceInfo *interfaceInfo;
  nsIInterfaceInfoManager* iimgr;
  char *connectString = argv[1];
  nsresult rv = NS_InitXPCOM(NULL, NULL);
  bcIID iid = NS_GET_IID(urpITest);
  if( (iimgr = XPTI_GetInterfaceInfoManager()) ) {
        if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
            return NS_ERROR_FAILURE;  //nb exception handling
        }
        NS_RELEASE(iimgr);
  } else {
        return NS_ERROR_FAILURE;
  }
    nsresult r;
    NS_WITH_SERVICE(bcIORBComponent, _orb, kORBCIID, &r);
    if (NS_FAILED(r)) {
        printf("--urpTestImpl test failed\n");
        return NS_ERROR_FAILURE;
    }

    NS_WITH_SERVICE(bcIXPCOMStubsAndProxies, xpcomStubsAndProxies, kXPCOMStubsAndProxies, &r);
    if (NS_FAILED(r)) {
        printf("--urpTestImpl test failed\n");
        return NS_ERROR_FAILURE;
    }
    bcIORB *orb;
    _orb->GetORB(&orb);
    bcIStub *stub = NULL;
    urpITest *object = new urpTestImpl();
    object->AddRef();
    urpITest *proxy = NULL;
    stub = new urpStub(connectString);
    bcOID oid = orb->RegisterStub(stub);
    printf("---urpTestImpl iid=%s\n",NS_GET_IID(urpITest).ToString());
    r = xpcomStubsAndProxies->GetProxy(oid,NS_GET_IID(urpITest),orb,(nsISupports**)&proxy);
    if (NS_FAILED(r)) {
        printf("--urpTestImpl test failed\n");
        return NS_ERROR_FAILURE;
    }

    /*******************************************/
    char ** valueArray = (char **)malloc(sizeof(char*)*4);
    valueArray[0] = "hi";
    valueArray[1] = "there";
    valueArray[2] = "a";
    valueArray[3] = "b";

    PRInt32 l1 = 1999;
    PRInt32 ret;
    PRInt32 rt;
    object->Test1(&l1);
    l1 = 1999;
    proxy->Test1(&l1);
    printf("--urpTestImpl after Test1 l=%d %d\n",l1,ret);
    /*******************************************/    
    PRInt32 l2 = 2000;
    l1 = 1999;
    proxy->Test2(l1,&l2);
    printf("--urpTestImpl after Test2 l2=%d\n",l2);

    /*******************************************/
    const char * s1 = "s1";
    char * s2 = "s2";
    proxy->Test3(s1,&s2);
    printf("--urpTestImpl after Test3 s2=%s\n",s2);
    /*******************************************/


    proxy->Test4(4,(const char **)valueArray);
    /*******************************************/

    char ***valueArray2 = &valueArray;

    printf("call object\n");
    for (unsigned int i = 0; i < 4; i++) {
        printf("valueArray[%d]=%s\n",i,(*valueArray2)[i]);
    }
    object->Test5(4,valueArray2);
    printf("after calling object\n");
    for (unsigned int i = 0; i < 4; i++) {
        printf("valueArray[%d]=%s\n",i,(*valueArray2)[i]);
    }


    valueArray2 = (char ***)&valueArray;
    proxy->Test5(4,valueArray2);
    for (unsigned int i = 0; i < 4; i++) {
        printf("valueArray[%d]=%s\n",i,(*valueArray2)[i]);
    }

    /*********************************************/
    proxy->Test6(object);
    /*********************************************/
    {
        urpITest *p1;
        proxy->Test7(&p1);
        printf("p1=%p",p1);
        PRInt32 l = 1234;
	PRInt32 r;
        p1->Test1(&l);
        urpITest *p3;
        printf("--before QueryInterface calling \n");
        if (NS_SUCCEEDED(p1->QueryInterface(NS_GET_IID(urpITest),(void**)&p3))) {
            l=2000;
            p3->Test1(&l);
        }


    }
}

