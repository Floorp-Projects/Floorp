/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*native java calls from oji into security system*/

//	nsresult rv;
//	NS_WITH_SERVICE(nsIPrincipalManager, observer, "component://netscape/observer-service",& rv);

#include "prtypes.h"
#include "prmem.h"
#include "prmon.h"
#include "prlog.h"
#include "admin.h"
#include "nsPrivilegeManager.h"
#include "nsPrincipalManager.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* 
 *             A D M I N   U I   A P I 
 *
 * All of the following methods are used by admin API (the code located
 * in security/lib/nav area).
 */

PR_PUBLIC_API(const char *)
java_netscape_security_getPrincipals(const char *charSetName)
{


	const char * prins = nsPrincipalManager::GetPrincipalManager()->GetAllPrincipalsString();
	PRBool test_admin_api = PR_FALSE;
	if (test_admin_api) {
		char *a1;
		char *a2;
		char *a3;
		java_netscape_security_getPrivilegeDescs(NULL, "raman tenneti", &a1, &a2, &a3);
		java_netscape_security_removePrivilege(NULL, "raman tenneti", "Reading, modification, or deletion of any of your files");
		java_netscape_security_removePrincipal(NULL, "raman tenneti");
	}
	return prins;
}

PR_PUBLIC_API(PRBool)
java_netscape_security_removePrincipal(const char * charSetName, char * prinName)
{
	return nsPrivilegeManager::GetPrivilegeManager()->RemovePrincipal(prinName);
}

PR_PUBLIC_API(void)
java_netscape_security_getPrivilegeDescs(const char * charSetName, char * prinName, char * * forever, char * * session, char * * denied)
{
	nsPrivilegeManager::GetPrivilegeManager()->GetTargetsWithPrivileges(prinName, forever, session, denied);
}

PR_PUBLIC_API(PRBool)
java_netscape_security_removePrivilege(const char * charSetName, char * prinName, char * targetName)
{
	PRBool result;
	nsPrivilegeManager::GetPrivilegeManager()->RemovePrincipalsPrivilege(prinName, targetName,& result);
	return result;

}


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
