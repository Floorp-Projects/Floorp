/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* The privileged system principal. */

#include "nscore.h"
#include "nsSystemPrincipal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"


NS_IMPL_QUERY_INTERFACE2_CI(nsSystemPrincipal, nsIPrincipal, nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER2(nsSystemPrincipal, nsIPrincipal, nsISerializable)

NSBASEPRINCIPALS_ADDREF(nsSystemPrincipal);
NSBASEPRINCIPALS_RELEASE(nsSystemPrincipal);


///////////////////////////////////////
// Methods implementing nsIPrincipal //
///////////////////////////////////////

NS_IMETHODIMP
nsSystemPrincipal::ToString(char **result)
{
    nsAutoString buf;
    buf.AssignWithConversion("[System]");

    *result = ToNewCString(buf);
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsSystemPrincipal::ToUserVisibleString(char **result)
{
    return ToString(result);
}

NS_IMETHODIMP
nsSystemPrincipal::GetPreferences(char** aPrefName, char** aID, 
                                  char** aGrantedList, char** aDeniedList)
{
    // The system principal should never be streamed out
    return NS_ERROR_FAILURE; 
}

NS_IMETHODIMP
nsSystemPrincipal::Equals(nsIPrincipal *other, PRBool *result)
{
    *result = (other == this);
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::HashValue(PRUint32 *result)
{
    *result = NS_PTR_TO_INT32(this);
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::CanEnableCapability(const char *capability, 
                                       PRInt16 *result)
{
    // System principal can enable all capabilities.
    *result = nsIPrincipal::ENABLE_GRANTED;
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::SetCanEnableCapability(const char *capability, 
                                          PRInt16 canEnable)
{
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP 
nsSystemPrincipal::IsCapabilityEnabled(const char *capability, 
                                       void *annotation, 
                                       PRBool *result)
{
    *result = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::EnableCapability(const char *capability, void **annotation)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::RevertCapability(const char *capability, void **annotation)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::DisableCapability(const char *capability, void **annotation) 
{
    // Can't disable the capabilities of the system principal.
    // XXX might be handy to be able to do so!
    return NS_ERROR_FAILURE;
}

//////////////////////////////////////////
// Methods implementing nsISerializable //
//////////////////////////////////////////

NS_IMETHODIMP
nsSystemPrincipal::Read(nsIObjectInputStream* aStream)
{
    // no-op: CID is sufficient to identify the mSystemPrincipal singleton
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::Write(nsIObjectOutputStream* aStream)
{
    // no-op: CID is sufficient to identify the mSystemPrincipal singleton
    return NS_OK;
}

/////////////////////////////////////////////
// Constructor, Destructor, initialization //
/////////////////////////////////////////////

nsSystemPrincipal::nsSystemPrincipal()
{
    NS_INIT_ISUPPORTS();
}

NS_IMETHODIMP
nsSystemPrincipal::Init()
{
    char *codebase = nsCRT::strdup("[System Principal]");
    if (!codebase)
        return NS_ERROR_OUT_OF_MEMORY;
    if (NS_FAILED(mJSPrincipals.Init(codebase))) 
        return NS_ERROR_FAILURE;
    return NS_OK;
}

nsSystemPrincipal::~nsSystemPrincipal(void)
{
}
