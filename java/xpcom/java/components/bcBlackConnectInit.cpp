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

#include "bcIBlackConnectInit.h"
#include "nsIModule.h"

static int counter = 0;  //we do not need to call it on unload time;
extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *compMgr,
                                          nsIFile *location,
                                          nsIModule** result)  //I am using it for initialization only
{
    nsresult r;
    if (counter == 0) {
        counter ++;
        bcIBlackConnectInit *blackConnectInit;
        r = nsComponentManager::CreateInstance("bcBlackConnectInit",
                                               nsnull,
                                               NS_GET_IID(bcIBlackConnectInit),
                                               (void**)&blackConnectInit);
        if (NS_SUCCEEDED(r)) {
            nsIComponentManager* cm;
            r = NS_GetGlobalComponentManager(&cm);
            if (NS_SUCCEEDED(r)) {
                blackConnectInit->InitComponentManager(cm);
            }
        }
    }
    return NS_ERROR_FAILURE;
}



