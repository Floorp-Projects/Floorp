/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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

#include "nsCodebasePrincipal.h"
#include "xp.h"

static NS_DEFINE_IID(kICodebasePrincipalIID, NS_ICODEBASEPRINCIPAL_IID);

NS_IMPL_ISUPPORTS(nsCodebasePrincipal, kICodebasePrincipalIID);

NS_IMETHODIMP
nsCodebasePrincipal::GetURL(char **cburl)
{
	cburl = (char **)&codeBaseURL;
	return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::IsCodebaseExact(PRBool * result)
{
	* result = (this->itsType == (PRInt16 *)nsIPrincipal::PrincipalType_CodebaseExact) ? PR_TRUE : PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::IsCodebaseRegex(PRBool * result)
{
	* result = (itsType == (PRInt16 *)nsIPrincipal::PrincipalType_CodebaseRegex) ? PR_TRUE : PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::GetType(PRInt16 * type)
{
	type = itsType;
	return NS_OK;
}

NS_IMETHODIMP 
nsCodebasePrincipal::IsSecure(PRBool * result)
{ 
	/*
	if ((0 == memcmp("https:", itsKey, strlen("https:"))) ||
      (0 == memcmp("file:", itsKey, strlen("file:"))))
    return PR_TRUE;
	*/
	return PR_FALSE;
}

NS_IMETHODIMP
nsCodebasePrincipal::ToString(char **result)
{
	return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::HashCode(PRUint32 * code)
{
	code=0;
	return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::Equals(nsIPrincipal * other, PRBool * result)
{
	PRInt16 * oType = 0;
	other->GetType(oType);
	*result = (itsType == oType) ? PR_TRUE : PR_FALSE;	
	return NS_OK;
}
nsCodebasePrincipal::nsCodebasePrincipal(PRInt16 * type, const char * codeBaseURL)
{
	this->itsType=type;
	this->codeBaseURL=codeBaseURL;
}

nsCodebasePrincipal::~nsCodebasePrincipal(void)
{
}
