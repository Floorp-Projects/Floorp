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
/*creates, registers, and performs logical operations on principals*/
#ifndef _NS_PRINCIPAL_MANAGER_H_
#define _NS_PRINCIPAL_MANAGER_H_

#include "nsIPrincipalManager.h"
#include "nsHashtable.h"

class nsPrincipalManager : public nsIPrincipalManager {

public:

	NS_DECL_ISUPPORTS

	static nsPrincipalManager *
	GetPrincipalManager();

	virtual ~nsPrincipalManager(void);

	NS_IMETHOD
	CreateCodebasePrincipal(const char *codebaseURL, nsIPrincipal * * prin);

	NS_IMETHOD
	CreateCertificatePrincipal(const unsigned char * * certChain, PRUint32 * certChainLengths, PRUint32 noOfCerts, nsIPrincipal * * prin);

	NS_IMETHOD
	RegisterPrincipal(nsIPrincipal * prin);

	NS_IMETHOD
	UnregisterPrincipal(nsIPrincipal * prin, PRBool * result);

	void 
	RegisterSystemPrincipal(nsIPrincipal * principal);

	NS_IMETHOD
	CanExtendTrust(nsIPrincipalArray * fromPrinArray, nsIPrincipalArray * toPrinArray, PRBool * result);

	NS_IMETHOD
	NewPrincipalArray(PRUint32 count, nsIPrincipalArray * * result);

	NS_IMETHOD
	CheckMatchPrincipal(nsIScriptContext * context, nsIPrincipal * principal, PRInt32 callerDepth, PRBool * result);

	static nsIPrincipalArray *
	GetMyPrincipals(PRInt32 callerDepth);

	static nsIPrincipalArray *
	GetMyPrincipals(nsIScriptContext * context, PRInt32 callerDepth);

	nsIPrincipal * 
	GetPrincipalFromString(char * prinName);
	
	static nsIPrincipal * 
	GetSystemPrincipal(void);

	static PRBool 
	HasSystemPrincipal(nsIPrincipalArray * prinArray);

	static nsIPrincipal * 
	GetUnsignedPrincipal(void);

	static nsIPrincipal * 
	GetUnknownPrincipal(void);

	const char * 
	GetAllPrincipalsString(void);

	void 
	AddToPrincipalNameToPrincipalTable(nsIPrincipal * prin);

	void
	RemoveFromPrincipalNameToPrincipalTable(nsIPrincipal * prin);

	nsIPrincipalArray * 
	GetClassPrincipalsFromStack(PRInt32 callerDepth);

	nsIPrincipalArray * 
	GetClassPrincipalsFromStack(nsIScriptContext * context, PRInt32 callerDepth);


private:
	nsPrincipalManager(void);
	nsHashtable * itsPrinNameToPrincipalTable;
};

#endif /* _NS_PRINCIPAL_MANAGER_H_*/
