/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* The privileged system principal. */

#include "nscore.h"
#include "nsSystemPrincipal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIClassInfoImpl.h"

NS_IMPL_CLASSINFO(nsSystemPrincipal, NULL,
                  nsIClassInfo::SINGLETON | nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_SYSTEMPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE2_CI(nsSystemPrincipal,
                            nsIPrincipal,
                            nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER2(nsSystemPrincipal,
                             nsIPrincipal,
                             nsISerializable)

NS_IMETHODIMP_(nsrefcnt) 
nsSystemPrincipal::AddRef()
{
  NS_PRECONDITION(PRInt32(refcount) >= 0, "illegal refcnt");
  nsrefcnt count = PR_ATOMIC_INCREMENT(&refcount);
  NS_LOG_ADDREF(this, count, "nsSystemPrincipal", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(nsrefcnt)
nsSystemPrincipal::Release()
{
  NS_PRECONDITION(0 != refcount, "dup release");
  nsrefcnt count = PR_ATOMIC_DECREMENT(&refcount);
  NS_LOG_RELEASE(this, count, "nsSystemPrincipal");
  if (count == 0) {
    delete this;
  }

  return count;
}

static const char SYSTEM_PRINCIPAL_SPEC[] = "[System Principal]";

void
nsSystemPrincipal::GetScriptLocation(nsACString &aStr)
{
    aStr.Assign(SYSTEM_PRINCIPAL_SPEC);
}

#ifdef DEBUG
void nsSystemPrincipal::dumpImpl()
{
  fprintf(stderr, "nsSystemPrincipal (%p)\n", this);
}
#endif 


///////////////////////////////////////
// Methods implementing nsIPrincipal //
///////////////////////////////////////

NS_IMETHODIMP
nsSystemPrincipal::GetPreferences(char** aPrefName, char** aID,
                                  char** aSubjectName,
                                  char** aGrantedList, char** aDeniedList,
                                  bool* aIsTrusted)
{
    // The system principal should never be streamed out
    *aPrefName = nsnull;
    *aID = nsnull;
    *aSubjectName = nsnull;
    *aGrantedList = nsnull;
    *aDeniedList = nsnull;
    *aIsTrusted = false;

    return NS_ERROR_FAILURE; 
}

NS_IMETHODIMP
nsSystemPrincipal::Equals(nsIPrincipal *other, bool *result)
{
    *result = (other == this);
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::EqualsIgnoringDomain(nsIPrincipal *other, bool *result)
{
    return Equals(other, result);
}

NS_IMETHODIMP
nsSystemPrincipal::Subsumes(nsIPrincipal *other, bool *result)
{
    *result = true;
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::SubsumesIgnoringDomain(nsIPrincipal *other, bool *result)
{
    *result = true;
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::CheckMayLoad(nsIURI* uri, bool aReport)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetHashValue(PRUint32 *result)
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
                                       bool *result)
{
    *result = true;
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::EnableCapability(const char *capability, void **annotation)
{
    *annotation = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::RevertCapability(const char *capability, void **annotation)
{
    *annotation = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::DisableCapability(const char *capability, void **annotation) 
{
    // Can't disable the capabilities of the system principal.
    // XXX might be handy to be able to do so!
    *annotation = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsSystemPrincipal::GetURI(nsIURI** aURI)
{
    *aURI = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::GetOrigin(char** aOrigin)
{
    *aOrigin = ToNewCString(NS_LITERAL_CSTRING(SYSTEM_PRINCIPAL_SPEC));
    return *aOrigin ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP 
nsSystemPrincipal::GetFingerprint(nsACString& aID)
{
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP 
nsSystemPrincipal::GetPrettyName(nsACString& aName)
{
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP 
nsSystemPrincipal::GetSubjectName(nsACString& aName)
{
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsSystemPrincipal::GetCertificate(nsISupports** aCertificate)
{
    *aCertificate = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsSystemPrincipal::GetHasCertificate(bool* aResult)
{
    *aResult = false;
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetCsp(nsIContentSecurityPolicy** aCsp)
{
  *aCsp = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::SetCsp(nsIContentSecurityPolicy* aCsp)
{
  // CSP on a null principal makes no sense
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetDomain(nsIURI** aDomain)
{
    *aDomain = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::SetDomain(nsIURI* aDomain)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetSecurityPolicy(void** aSecurityPolicy)
{
    *aSecurityPolicy = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::SetSecurityPolicy(void* aSecurityPolicy)
{
    return NS_OK;
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
}

nsSystemPrincipal::~nsSystemPrincipal()
{
}
