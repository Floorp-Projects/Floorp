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

#include "org_mozilla_xpcom_Components.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

/*
 * Class:     org_mozilla_xpcom_Components
 * Method:    initXPCOM
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_Components_initXPCOM
(JNIEnv *, jclass) {
    static int initCounter = 0;
    /* 
       mozilla/embedding/base/nsEmbedAPI.cpp NS_InitEmbedding was used as
       prototype
     */
    initCounter++;
    if (initCounter > 1) {
        return;
    }
    nsresult rv;
    nsIServiceManager* servMgr;
    rv = NS_InitXPCOM(&servMgr, NULL);
    if (NS_FAILED(rv)) {
        printf("--Components::initXPCOM failed \n");
        return;
    }
    rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                          NULL /* default */);
    if (NS_FAILED(rv))  {
        printf("--Components::initXPCOM failed \n");
        return;
    }                                                                                                                                  
                                                           
}





