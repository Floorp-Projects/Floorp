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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "xp.h"
#include "nsUserTarget.h"
#include "prtypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "jpermission.h"

static PRBool displayUI=PR_FALSE;

static nsPermState 
displayPermissionDialog(char *prinStr, char *targetStr, char *riskStr, PRBool isCert, void *cert)
{
	return nsJSJavaDisplayDialog(prinStr, targetStr, riskStr, isCert, cert);

}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

nsUserTarget::~nsUserTarget(void)
{
}

#define OPTION "<option>"
NS_IMETHODIMP
nsUserTarget::EnablePrivilege(nsIPrincipal * prin, void * data, nsIPrivilege * * result)
{
	PRInt16 prinType;
	prin->GetType(& prinType);
	PRInt16 privState = nsIPrivilege::PrivilegeState_Allowed;
	PRInt16 privDuration = nsIPrivilege::PrivilegeDuration_Session;
	if ((nsCapsGetRegistrationModeFlag()) && (prin != NULL)) {
		if (prinType == (PRInt16) nsIPrincipal::PrincipalType_CodebaseExact
			||
			prinType == (PRInt16) nsIPrincipal::PrincipalType_CodebaseRegex) {
			privState = nsIPrivilege::PrivilegeState_Allowed;
			privDuration = nsIPrivilege::PrivilegeDuration_Session;
		}
	} 
	* result = nsPrivilegeManager::FindPrivilege(privState, privDuration);
	return NS_OK;
}
