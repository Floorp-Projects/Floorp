/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributors:
 *   Mike Shaver <shaver@zeroknowledge.com>
 *   John Bandhauer <jband@netscape.com>
 *   IBM Corp.
 *   Robert Ginda <rginda@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    return catman->AddCategoryEntry("module-loader",
                                    MOZJSCOMPONENTLOADER_TYPE_NAME,
                                    MOZJSCOMPONENTLOADER_CONTRACTID,
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
    rv = catman->GetCategoryEntry("module-loader",
                                  MOZJSCOMPONENTLOADER_TYPE_NAME,
                                  getter_Copies(jsLoader));
    if (NS_FAILED(rv)) return rv;

    // only unregister if we're the current JS component loader
    if (!strcmp(jsLoader, MOZJSCOMPONENTLOADER_CONTRACTID)) {
        return catman->DeleteCategoryEntry("module-loader",
                                           MOZJSCOMPONENTLOADER_TYPE_NAME,
                                           PR_TRUE);
    }
    return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSComponentLoader)

#ifndef NO_SUBSCRIPT_LOADER
NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSSubScriptLoader)
#endif
