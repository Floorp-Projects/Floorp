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
/* describes principals by thier orginating uris*/
#include "nsCodebasePrincipal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsXPIDLString.h"


static NS_DEFINE_IID(kICodebasePrincipalIID, NS_ICODEBASEPRINCIPAL_IID);

NS_IMPL_ISUPPORTS(nsCodebasePrincipal, kICodebasePrincipalIID);

NS_IMETHODIMP
nsCodebasePrincipal::ToJSPrincipal(JSPrincipals * * jsprin)
{
    if (itsJSPrincipals.refcount == 0) {
        NS_ADDREF(this);
    }
    *jsprin = &itsJSPrincipals;
    return NS_OK;
/*
    char * cb;
  this->GetURLString(& cb);
  * jsprin = NS_STATIC_CAST(JSPrincipals *,this);
  (* jsprin)->codebase = PL_strdup(cb);
  return NS_OK;
  */
}

NS_IMETHODIMP
nsCodebasePrincipal::GetURLString(char **cburl)
{
  return itsURL->GetSpec(cburl);
}

NS_IMETHODIMP
nsCodebasePrincipal::GetURL(nsIURI * * url) 
{
  return itsURL->Clone(url);
}

NS_IMETHODIMP
nsCodebasePrincipal::IsCodebaseExact(PRBool * result)
{
	* result = (this->itsType == nsIPrincipal::PrincipalType_CodebaseExact) ? PR_TRUE : PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::IsCodebaseRegex(PRBool * result)
{
	* result = (itsType == nsIPrincipal::PrincipalType_CodebaseRegex) ? PR_TRUE : PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::GetType(PRInt16 * type)
{
	* type = itsType;
	return NS_OK;
}

NS_IMETHODIMP 
nsCodebasePrincipal::IsSecure(PRBool * result)
{ 
//	if ((0 == memcmp("https:", itsKey, strlen("https:"))) ||
//      (0 == memcmp("file:", itsKey, strlen("file:"))))
//    return PR_TRUE;
	return PR_FALSE;
}

NS_IMETHODIMP
nsCodebasePrincipal::ToString(char * * result)
{
	return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::HashCode(PRUint32 * code)
{
	(* code) = 0;
	return NS_OK;
}

NS_IMETHODIMP
nsCodebasePrincipal::Equals(nsIPrincipal * other, PRBool * result)
{
  PRInt16 oType = 0;
  other->GetType(& oType);
  (* result) = (itsType == oType) ? PR_TRUE : PR_FALSE;	
  if ((* result) != PR_TRUE) return NS_OK;
  nsICodebasePrincipal * cbother;
  nsXPIDLCString oCodebase, myCodebase;
  other->QueryInterface(NS_GET_IID(nsICodebasePrincipal),(void * *)& cbother);
  cbother->GetURLString(getter_Copies(oCodebase));
  this->GetURLString(getter_Copies(myCodebase));
  (* result) = (PL_strcmp(myCodebase, oCodebase) == 0) ? PR_TRUE : PR_FALSE;	
  return NS_OK;
}

nsCodebasePrincipal::nsCodebasePrincipal()
{
  NS_INIT_ISUPPORTS();
  itsURL = nsnull;
}

NS_IMETHODIMP
nsCodebasePrincipal::Init(PRInt16 type, nsIURI *uri)
{
  nsresult result;
  NS_ADDREF(this);
  this->itsType = type;
  if (!NS_SUCCEEDED(result = uri->Clone(&itsURL))) return result;
  if (!NS_SUCCEEDED(result = itsJSPrincipals.Init(this))) {
    NS_RELEASE(itsURL);
    return result;
  }
  return NS_OK;
}

nsCodebasePrincipal::~nsCodebasePrincipal(void)
{
    if (itsURL)
        NS_RELEASE(itsURL);
}
