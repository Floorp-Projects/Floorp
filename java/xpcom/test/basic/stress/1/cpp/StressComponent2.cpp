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
#include "stdio.h"
#include "nsIComponentManager.h"
#include "StressComponent2.h"
#include "iStressComponent1.h"
#include "nsMemory.h"
#include "prthread.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "nsID.h"

#define MAX_COUNT  500
int count = 0;

StressComponent2Impl::StressComponent2Impl()
{    
    NS_INIT_REFCNT();
    printf("StressComponent2Impl::StressComponent2Imp\n");
    Increase();
    printf("PASSED:Stress test 1 passed. Corrtly return from 500 recursive invokations\n");
}

StressComponent2Impl::~StressComponent2Impl()
{
}

NS_IMPL_ISUPPORTS1(StressComponent2Impl, iStressComponent2);


NS_IMETHODIMP StressComponent2Impl::Increase(){
    count++;
    if(count >MAX_COUNT) return;
    printf("####StressComponent2Impl::Increase %d\n",count);
    iStressComponent1* st1;
    nsresult rv = nsComponentManager::CreateInstance(STRESSCOMPONENT1_PROGID,
                                            nsnull,
                                            NS_GET_IID(iStressComponent1),
                                            (void**)&st1);   
    if(NS_FAILED(rv)) {
        printf("StressComponent2Impl:Create instance failed from %s!!!",STRESSCOMPONENT1_PROGID);
        return rv;
    }
    rv = st1->ReCall(this);
    NS_RELEASE(st1);
    return rv;
}








