/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *   Mike Shaver <shaver@zeroknowledge.com>
 *   John Bandhauer <jband@netscape.com>
 *   IBM Corp.
 *   Robert Ginda <rginda@netscape.com>
 */

#ifdef XPCONNECT_STANDALONE
#define NO_SUBSCRIPT_LOADER
#endif

#include "nsIGenericFactory.h"
#include "nsICategoryManager.h"
#include "mozJSComponentLoader.h"

#ifndef NO_SUBSCRIPT_LOADER
#include "mozJSSubScriptLoader.h"
const char mozJSSubScriptLoadContractID[] = "@mozilla.org/moz/jssubscript-loader;1";
#endif

static NS_METHOD
RegisterJSLoader(nsIComponentManager *aCompMgr, nsIFile *aPath,
                 const char *registryLocation, const char *componentType,
                 const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString previous;
    return catman->AddCategoryEntry("component-loader", jsComponentTypeName,
                                    mozJSComponentLoaderContractID,
                                    PR_TRUE, PR_TRUE, getter_Copies(previous));
}

static NS_METHOD
UnregisterJSLoader(nsIComponentManager *aCompMgr, nsIFile *aPath,
                   const char *registryLocation,
                   const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString jsLoader;
    rv = catman->GetCategoryEntry("component-loader", jsComponentTypeName,
                                  getter_Copies(jsLoader));
    if (NS_FAILED(rv)) return rv;

    // only unregister if we're the current JS component loader
    if (!strcmp(jsLoader, mozJSComponentLoaderContractID)) {
        return catman->DeleteCategoryEntry("component-loader",
                                           jsComponentTypeName, PR_TRUE);
    }
    return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSComponentLoader)

#ifndef NO_SUBSCRIPT_LOADER
NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSSubScriptLoader)
#endif
