/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Module level methods. */

#include "xpcprivate.h"

// Module implementation for the xpconnect library

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSID)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCException)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIXPConnect, nsXPConnect::GetXPConnect)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIJSContextStack, nsXPCThreadJSContextStackImpl::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIJSRuntimeService, nsJSRuntimeServiceImpl::GetSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptError)

// XXX contractids need to be standardized!
static nsModuleComponentInfo components[] = {
  {nsnull, NS_JS_ID_CID,                      "@mozilla.org/js/xpc/ID;1",                   nsJSIDConstructor             },
  {nsnull, NS_XPCONNECT_CID,                  "@mozilla.org/js/xpc/XPConnect;1",           nsIXPConnectConstructor       },
  {nsnull, NS_XPC_THREAD_JSCONTEXT_STACK_CID, "@mozilla.org/js/xpc/ContextStack;1", nsIJSContextStackConstructor  },
  {nsnull, NS_XPCEXCEPTION_CID,               "@mozilla.org/js/xpc/Exception;1",         nsXPCExceptionConstructor     },
  {nsnull, NS_JS_RUNTIME_SERVICE_CID,         "@mozilla.org/js/xpc/RuntimeService;1",     nsIJSRuntimeServiceConstructor},
  {NS_SCRIPTERROR_CLASSNAME, NS_SCRIPTERROR_CID, NS_SCRIPTERROR_CONTRACTID, nsScriptErrorConstructor}
};

PR_STATIC_CALLBACK(void)
xpcModuleDtor(nsIModule* self)
{
    // Release our singletons
    nsXPConnect::ReleaseXPConnectSingleton();
    nsXPCThreadJSContextStackImpl::FreeSingleton();
    nsJSRuntimeServiceImpl::FreeSingleton();
}

NS_IMPL_NSGETMODULE_WITH_DTOR("xpconnect", components, xpcModuleDtor)
