/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* module registration and factory code. */

#include "xpctest_private.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    nsresult rv;
    NS_ASSERTION(aFactory != nsnull, "bad factory pointer");

    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIGenericFactory* factory;
    rv = compMgr->CreateInstance(kGenericFactoryCID, nsnull, 
                                 nsIGenericFactory::GetIID(), 
                                 (void**)&factory);
    if (NS_FAILED(rv)) return rv;

    // add more factories as 'if else's below...

    if(aClass.Equals(xpctest::GetEchoCID()))
        rv = factory->SetConstructor(xpctest::ConstructEcho);
    else if(aClass.Equals(xpctest::GetChildCID()))
        rv = factory->SetConstructor(xpctest::ConstructChild);
    else if(aClass.Equals(xpctest::GetNoisyCID()))
        rv = factory->SetConstructor(xpctest::ConstructNoisy);
    else if(aClass.Equals(xpctest::GetStringTestCID()))
        rv = factory->SetConstructor(xpctest::ConstructStringTest);
    else if(aClass.Equals(xpctest::GetOverloadedCID()))
        rv = factory->SetConstructor(xpctest::ConstructOverloaded);
    else if(aClass.Equals(xpctest::GetXPCTestObjectReadOnlyCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestObjectReadOnly);
    else if(aClass.Equals(xpctest::GetXPCTestObjectReadWriteCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestObjectReadWrite);
    else if(aClass.Equals(xpctest::GetXPCTestInCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestIn);
    else if(aClass.Equals(xpctest::GetXPCTestOutCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestOut);
    else if(aClass.Equals(xpctest::GetXPCTestInOutCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestInOut);
    else if(aClass.Equals(xpctest::GetXPCTestCallJSCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestCallJS);
    else if(aClass.Equals(xpctest::GetXPCTestConstCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestConst);
#if (0)
    else if(aClass.Equals(xpctest::GetXPCTestConstructWithArgsCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestConstructWithArgs);
    else if(aClass.Equals(xpctest::GetXPCTestScriptableCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestScriptable);

#endif /* 0 */
    else if(aClass.Equals(xpctest::GetXPCTestParentOneCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestParentOne);
    else if(aClass.Equals(xpctest::GetXPCTestParentTwoCID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestParentTwo);
	else if(aClass.Equals(xpctest::GetXPCTestChild2CID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestChild2);
	else if(aClass.Equals(xpctest::GetXPCTestChild3CID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestChild3);
	else if(aClass.Equals(xpctest::GetXPCTestChild4CID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestChild4);
	else if(aClass.Equals(xpctest::GetXPCTestChild5CID()))
        rv = factory->SetConstructor(xpctest::ConstructXPCTestChild5);
    else if(aClass.Equals(xpctest::GetArrayTestCID()))
        rv = factory->SetConstructor(xpctest::ConstructArrayTest);
    else
    {
        NS_ASSERTION(0, "incorrectly registered");
        rv = NS_ERROR_NO_INTERFACE;
    }

    if (NS_FAILED(rv)) {
        NS_RELEASE(factory);
        return rv;
    }
    *aFactory = factory;
    return NS_OK;
}

extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
    nsresult rv;
#ifdef DEBUG
    printf("*** Register XPConnect test components\n");
#endif
    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(xpctest::GetEchoCID(),
                                    "nsEcho", "nsEcho", aPath,
                                    PR_TRUE, PR_TRUE);

    rv = compMgr->RegisterComponent(xpctest::GetChildCID(),
                                    "nsChild", "nsChild", aPath,
                                    PR_TRUE, PR_TRUE);

    rv = compMgr->RegisterComponent(xpctest::GetNoisyCID(),
                                    "nsNoisy", "nsNoisy", aPath,
                                    PR_TRUE, PR_TRUE);

    rv = compMgr->RegisterComponent(xpctest::GetStringTestCID(),
                                    "nsStringTest", "nsStringTest", aPath,
                                    PR_TRUE, PR_TRUE);

    rv = compMgr->RegisterComponent(xpctest::GetOverloadedCID(),
                                    "nsOverloaded", "nsOverloaded", aPath,
                                    PR_TRUE, PR_TRUE);
    rv = compMgr->RegisterComponent(xpctest::GetXPCTestObjectReadOnlyCID(),
                                    "xpcTestObjectReadOnly", "xpcTestObjectReadOnly", aPath,
                                    PR_TRUE, PR_TRUE);
    rv = compMgr->RegisterComponent(xpctest::GetArrayTestCID(),
                                    "nsArrayTest", "nsArrayTest", aPath,
                                    PR_TRUE, PR_TRUE);
    rv = compMgr->RegisterComponent(xpctest::GetXPCTestObjectReadWriteCID(),
                                    "xpcTestObjectReadWrite", "xpcTestObjectReadWrite", aPath,
                                    PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(xpctest::GetXPCTestInCID(),
                                    "xpcTestIn", "xpcTestIn", aPath,
                                    PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(xpctest::GetXPCTestOutCID(),
                                    "xpcTestOut", "xpcTestOut", aPath,
                                    PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(xpctest::GetXPCTestInOutCID(),
                                    "xpcTestOut", "xpcTestInOut", aPath,
                                    PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(xpctest::GetXPCTestConstCID(),
                                    "xpcTestConst", "xpcTestConst", aPath,
                                    PR_TRUE, PR_TRUE);
#if 0
	rv = compMgr->RegisterComponent(xpctest::GetXPCTestConstructWithArgsCID(),
                                    "xpcTestConstructArgs", "xpcTestConstructArgs", aPath,
                                    PR_TRUE, PR_TRUE);
	rv = compMgr->RegisterComponent(xpctest::GetXPCTestScriptableCID(),
                                    "xpcTestScriptable", "xpcTestScriptable", aPath,
                                    PR_TRUE, PR_TRUE);
#endif /* 0 */
	rv = compMgr->RegisterComponent(xpctest::GetXPCTestCallJSCID(),
                                    "xpcTestCallJS", "xpcTestCallJS", aPath,
                                    PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(xpctest::GetXPCTestParentOneCID(),
                                    "xpcTestParentOne", "xpcTestParentOne", aPath,
                                    PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(xpctest::GetXPCTestParentTwoCID(),
                                    "xpcTestParentTwo", "xpcTestParentTwo", aPath,
                                    PR_TRUE, PR_TRUE);
	
	rv = compMgr->RegisterComponent(xpctest::GetXPCTestChild2CID(),
                                    "xpcTestChild2", "xpcTestChild2", aPath,
                                    PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(xpctest::GetXPCTestChild3CID(),
                                    "xpcTestChild3", "xpcTestChild3", aPath,
                                    PR_TRUE, PR_TRUE);
	rv = compMgr->RegisterComponent(xpctest::GetXPCTestChild4CID(),
                                    "xpcTestChild4", "xpcTestChild4", aPath,
                                    PR_TRUE, PR_TRUE);
	rv = compMgr->RegisterComponent(xpctest::GetXPCTestChild5CID(),
                                    "xpcTestChild5", "xpcTestChild5", aPath,
                                    PR_TRUE, PR_TRUE);
	return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;
    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(xpctest::GetEchoCID(), aPath);
    rv = compMgr->UnregisterComponent(xpctest::GetChildCID(), aPath);
    rv = compMgr->UnregisterComponent(xpctest::GetNoisyCID(), aPath);
    rv = compMgr->UnregisterComponent(xpctest::GetStringTestCID(), aPath);
    rv = compMgr->UnregisterComponent(xpctest::GetOverloadedCID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestObjectReadWriteCID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestObjectReadOnlyCID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestInCID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestOutCID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestInOutCID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestConstCID(), aPath);
/*
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestConstructWithArgsCID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestScriptableCID(), aPath);
*/
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestCallJSCID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestParentOneCID(), aPath);
    rv = compMgr->UnregisterComponent(xpctest::GetXPCTestParentTwoCID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestChild2CID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestChild3CID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestChild4CID(), aPath);
	rv = compMgr->UnregisterComponent(xpctest::GetXPCTestChild5CID(), aPath);
    rv = compMgr->UnregisterComponent(xpctest::GetArrayTestCID(), aPath);

    return rv;
}
