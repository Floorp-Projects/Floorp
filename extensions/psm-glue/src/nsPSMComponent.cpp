/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Mitch Stoltz <mstoltz@netscape.com>
*/

#include "nsProxiedService.h"
#include "nsPSMUICallbacks.h"
#include "VerReg.h"

#include "nspr.h"
#include "nsPSMComponent.h"

#include "nsCRT.h"

#include "nsIPref.h"
#include "nsIProfile.h"
#include "nsILocalFile.h"
#ifdef XP_MAC
#include "nsILocalFileMac.h"
#endif
#include "nsSpecialSystemDirectory.h"

#include "rsrcids.h"

#include "nsPSMMutex.h"
#include "nsPSMShimLayer.h"
#include "nsPSMUICallbacks.h"

#include "nsISecureBrowserUI.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIScriptSecurityManager.h"

#define PSM_VERSION_REG_KEY "/Netscape/Personal Security Manager"

#ifdef WIN32
#define PSM_FILE_NAME "psm.exe"
#elif XP_UNIX
#define PSM_FILE_NAME "start-psm"
#define PSM_FILE_LOCATION "/opt/netscape/security/start-psm"
#else
#define PSM_FILE_NAME "psm"
#endif


static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);


nsPSMComponent* nsPSMComponent::mInstance = nsnull;

nsPSMComponent::nsPSMComponent()
{
    NS_INIT_REFCNT();
    mControl = nsnull;
}

nsPSMComponent::~nsPSMComponent()
{
    if (mControl)
    {
        CMT_CloseControlConnection(mControl);
        mControl = nsnull;
    }
}


NS_IMETHODIMP      
nsPSMComponent::CreatePSMComponent(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{                                                                  
    if (!aResult) {                                                
        return NS_ERROR_INVALID_POINTER;                           
    }                                                              
    if (aOuter) {                                                  
        *aResult = nsnull;                                         
        return NS_ERROR_NO_AGGREGATION;                              
    }                                                                
    
    if (mInstance == nsnull) 
    {
        mInstance = new nsPSMComponent();
    }

    if (mInstance == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = mInstance->QueryInterface(aIID, aResult);                        
    if (NS_FAILED(rv)) 
    {                                             
        *aResult = nsnull;                                           
    }                                                                
    return rv;                                                       
}

/* nsISupports Implementation for the class */
NS_IMPL_THREADSAFE_ISUPPORTS1 (nsPSMComponent, nsIPSMComponent); 

#define INIT_NUM_PREFS 100
/* preference types */
#define STRING_PREF 0
#define BOOL_PREF 1
#define INT_PREF 2


/* resizable list struct that contains pref items */
typedef struct CMSetPrefList {
    int n; /* number of filled items */
    int capacity; /* allocated memory */
    CMTSetPrefElement* list; /* actual list */
} CMSetPrefList;

static void get_pack_bool_pref(nsIPref *prefManager, char* key, CMTSetPrefElement* list, int* n)
{
    PRBool boolpref;

    list[*n].key = nsCRT::strdup(key);
    list[*n].type = BOOL_PREF;

    if ((prefManager->GetBoolPref(key, &boolpref) != 0) || boolpref) 
    {
        list[*n].value = nsCRT::strdup("true");
    }
    else 
    {
        list[*n].value = nsCRT::strdup("false");
    }

    (*n)++;    /* increment the counter after done packing */

    return;
}

static void SaveAllPrefs(int number, CMTSetPrefElement* list)
{
    nsCOMPtr<nsIPref> prefManager;

    nsresult res = nsServiceManager::GetService(kPrefCID, 
                                                nsIPref::GetIID(), 
                                                getter_AddRefs(prefManager));
    
    if (NS_FAILED(res)  || !prefManager)
    {
        return;
    }


    int i;
    int intval;

    for (i = 0; i < number; i++) 
    {
        if (list[i].key == nsnull) 
        {
            /* misconfigured item: next */
            continue;
        }

        switch (list[i].type) 
        {
             case 0:    /* string type */
                 prefManager->SetCharPref(list[i].key, list[i].value);
                 break;
             case 1:    /* boolean type */
                 if (strcmp(list[i].value, "true") == 0) {
                  prefManager->SetBoolPref(list[i].key, (PRBool)1);
                 }
                 else if (strcmp(list[i].value, "false") == 0) {
                  prefManager->SetBoolPref(list[i].key, (PRBool)0);
                 }
                 break;
             case 2:
                 intval = atoi(list[i].value);
                 prefManager->SetIntPref(list[i].key, intval);
                 break;
             default:
                 break;
         }
    }

    return;
}

NS_IMETHODIMP
nsPSMComponent::PassPrefs()
{
    // if we have not passed anything to psm yet, this function can just return.

    if (!mControl)
        return NS_OK;

    int i;
    nsresult rv = NS_ERROR_FAILURE;
    char* strpref = NULL;
    int intpref;
    PRBool boolpref;
    CMSetPrefList prefs = {0};
    CMTSetPrefElement* list = NULL;

    char* pickAuto = "Select Automatically";
    char* alwaysAsk = "Ask Every Time";
    
    nsCOMPtr<nsIPref> prefManager;

    nsresult res = nsServiceManager::GetService(kPrefCID, 
                                                nsIPref::GetIID(), 
                                                getter_AddRefs(prefManager));

    if (NS_OK != res) 
    {
        return NS_ERROR_FAILURE;
    }

    /* allocate memory for list */
    prefs.n = 0; /* counter */
    prefs.capacity = INIT_NUM_PREFS;
    prefs.list = (CMTSetPrefElement*) new char[(INIT_NUM_PREFS * sizeof(CMTSetPrefElement))];
    
    if (prefs.list == NULL) 
    {
         return rv;
    }

    /* shorthand */
    list = prefs.list;

    /* get preferences */
    get_pack_bool_pref(prefManager, "security.enable_ssl2", (CMTSetPrefElement*)list, &(prefs.n));
    get_pack_bool_pref(prefManager, "security.enable_ssl3", (CMTSetPrefElement*)list, &(prefs.n));

    /* this pref is a boolean pref in nature but a string pref for 
     * historical reason
     */

    list[prefs.n].key = nsCRT::strdup("security.default_personal_cert");
    list[prefs.n].type = STRING_PREF;

    if ((prefManager->CopyCharPref(list[prefs.n].key, &strpref) == 0) && (strcmp(strpref, pickAuto) == 0)) 
    {
         list[prefs.n].value = nsCRT::strdup(pickAuto);
    }
    else 
    {
         /* although one could choose a specific cert for client auth in
          * Nova, that mode is deprecated with PSM and mapped to ASK
          */
         list[prefs.n].value = nsCRT::strdup(alwaysAsk);
    }

    prefs.n++;
    if (strpref != NULL) 
    {
         nsCRT::free(strpref);
    }

    list[prefs.n].key = nsCRT::strdup("security.default_mail_cert");
    list[prefs.n].type = STRING_PREF;
    if (prefManager->CopyCharPref(list[prefs.n].key, &list[prefs.n].value) != 0) 
    {
         list[prefs.n].value = NULL;
    }
    prefs.n++;

    list[prefs.n].key = nsCRT::strdup("security.ask_for_password");
    list[prefs.n].type = INT_PREF;
    if (prefManager->GetIntPref(list[prefs.n].key, &intpref) != 0) 
    {
         intpref = 0;    /* default */
    }

    list[prefs.n].value = PR_smprintf("%d", intpref);
    prefs.n++;

    list[prefs.n].key = nsCRT::strdup("security.password_lifetime");
    list[prefs.n].type = INT_PREF;
    if (prefManager->GetIntPref(list[prefs.n].key, &intpref) != 0) 
    {
         intpref = 30;    /* default */
    }

    list[prefs.n].value = PR_smprintf("%d", intpref);
    prefs.n++;

    /* OCSP preferences */
    /* XXX since these are the new ones added by PSM, we will be more
     *     error-tolerant in fetching them
     */
    if (prefManager->GetBoolPref("security.OCSP.enabled", &boolpref) == 0) 
    {
         if (boolpref) 
        {
             list[prefs.n].value = nsCRT::strdup("true");
         }
         else 
        {
             list[prefs.n].value = nsCRT::strdup("false");
         }
         list[prefs.n].key = nsCRT::strdup("security.OCSP.enabled");
         list[prefs.n].type = BOOL_PREF;
         prefs.n++;
    }

    if (prefManager->GetBoolPref("security.OCSP.useDefaultResponder", &boolpref) == 0) 
    {
         if (boolpref) 
        {
             list[prefs.n].value = nsCRT::strdup("true");
         }
         else 
        {
             list[prefs.n].value = nsCRT::strdup("false");
         }
         list[prefs.n].key = nsCRT::strdup("security.OCSP.useDefaultResponder");
         list[prefs.n].type = BOOL_PREF;
         prefs.n++;
    }

    if (prefManager->CopyCharPref("security.OCSP.URL", &strpref) == 0) 
    {
         list[prefs.n].value = strpref;
         list[prefs.n].key = nsCRT::strdup("security.OCSP.URL");
         list[prefs.n].type = STRING_PREF;
         prefs.n++;
    }

    if (prefManager->CopyCharPref("security.OCSP.signingCA", &strpref) == 0) 
    {
         list[prefs.n].value = strpref;
         list[prefs.n].key = nsCRT::strdup("security.OCSP.signingCA");
         list[prefs.n].type = STRING_PREF;
         prefs.n++;
    }

    /* now application-specific preferences */
    /* get navigator preferences */
    get_pack_bool_pref(prefManager, "security.warn_entering_secure", (CMTSetPrefElement*)list, &prefs.n);
    get_pack_bool_pref(prefManager, "security.warn_leaving_secure",  (CMTSetPrefElement*)list, &prefs.n);
    get_pack_bool_pref(prefManager, "security.warn_viewing_mixed",   (CMTSetPrefElement*)list, &prefs.n);
    get_pack_bool_pref(prefManager, "security.warn_submit_insecure", (CMTSetPrefElement*)list, &prefs.n);

    // Add any other prefs here such as ldap or mail/news.
     
     CMT_SetSavePrefsCallback(mControl, (savePrefsCallback_fn)SaveAllPrefs);

    if (CMT_PassAllPrefs(mControl, prefs.n, (CMTSetPrefElement*)prefs.list) !=  CMTSuccess) 
    {
         goto loser;
    }

    rv = NS_OK; /* success */
loser:
    /* clean out memory for prefs */
    for (i = 0; i < prefs.n; i++) 
    {
         if (prefs.list[i].key != NULL) 
        {
             nsCRT::free(prefs.list[i].key);
         }
         
        if (prefs.list[i].value != NULL) 
        {
             nsCRT::free(prefs.list[i].value);
         }
    }

    if (prefs.list != NULL) 
    {
         delete(prefs.list);
    }
    return rv;
}

NS_IMETHODIMP
nsPSMComponent::GetControlConnection( CMT_CONTROL * *_retval )
{
     nsresult rv;
     *_retval = nsnull;
    if (mControl) 
    {
        *_retval = mControl;
        return NS_OK;
    }
    else      /* initialize mutex, sock table, etc. */
    {
            
        if (nsPSMMutexInit() != PR_SUCCESS)
            return NS_ERROR_FAILURE;

        mControl = CMT_ControlConnect(&nsPSMMutexTbl, &nsPSMShimTbl);
        
        if (mControl == nsnull)
        {
            //Try to find it.
            int err;
            char filepath[MAXREGPATHLEN];

            err = VR_GetPath(PSM_VERSION_REG_KEY, sizeof(filepath), filepath);
            if ( err == REGERR_OK )
            {
                nsFileSpec psmSpec(filepath);
                psmSpec += PSM_FILE_NAME;
		
                if (psmSpec.Exists())
                {
                    mControl = CMT_EstablishControlConnection((char *)psmSpec.GetNativePathCString(), &nsPSMShimTbl, &nsPSMMutexTbl);
                }
            }
         }

#ifndef XP_MAC
        if (mControl == nsnull)
        {
            nsSpecialSystemDirectory sysDir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
            nsFileSpec spec = sysDir;

            spec += "psm/";
            spec += PSM_FILE_NAME;

            if (spec.Exists())
            {
                mControl = CMT_EstablishControlConnection((char *)spec.GetNativePathCString(), &nsPSMShimTbl, &nsPSMMutexTbl);
            }
        }
#else
    if (mControl == nsnull)
    {
      // Attempt to locate "Personal Security Manager" in "Essential Files".
      nsCOMPtr<nsILocalFile> aPSMApp = do_CreateInstance(NS_LOCAL_FILE_PROGID, &rv);
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsILocalFileMac> psmAppMacFile = do_QueryInterface(aPSMApp, &rv);
        if (NS_SUCCEEDED(rv))
        {
          rv = psmAppMacFile->InitFindingAppByCreatorCode('nPSM');
          if (NS_SUCCEEDED(rv))
          {
            rv = psmAppMacFile->LaunchAppWithDoc(nsnull, PR_TRUE);
            if (NS_SUCCEEDED(rv))
            {
                 const PRUint32 kMaxWaitTicks = 180;          // max 3 seconds
              PRUint32 endTicks = ::TickCount() + kMaxWaitTicks;
              
              do
              {
                EventRecord theEvent;
                    WaitNextEvent(0, &theEvent, 5, NULL);
                mControl = CMT_ControlConnect(&nsPSMMutexTbl, &nsPSMShimTbl);
              } while (!mControl && (::TickCount() < endTicks));

            }
          }
        }
      }
      NS_ASSERTION(NS_SUCCEEDED(rv), "Launching Personal Security Manager failed");
    }

#endif

#ifdef XP_UNIX
        if (mControl == nsnull)
        {
            nsFileSpec psmSpec(PSM_FILE_LOCATION);
            if (psmSpec.Exists())
            {
                mControl = CMT_EstablishControlConnection(PSM_FILE_LOCATION, &nsPSMShimTbl, &nsPSMMutexTbl);
            }
        }
#endif

        if (mControl == nsnull)
        {
            char* filePath = nsnull;
            
            NS_WITH_PROXIED_SERVICE(nsIPSMUIHandler, handler, nsPSMUIHandlerImpl::GetCID(), NS_UI_THREAD_EVENTQ, &rv);
            if(NS_SUCCEEDED(rv))
            {
                NS_WITH_SERVICE(nsIStringBundleService, service, kCStringBundleServiceCID, &rv);
                if (NS_FAILED(rv)) return rv;
            
                nsILocale* locale = nsnull;
                nsCOMPtr<nsIStringBundle> stringBundle;

                rv = service->CreateBundle(SECURITY_STRING_BUNDLE_URL, locale, getter_AddRefs(stringBundle));
                if (NS_FAILED(rv)) return rv;

                PRUnichar *ptrv = nsnull;
                rv = stringBundle->GetStringFromName( NS_ConvertASCIItoUCS2("FindText").GetUnicode(), &ptrv);
                if (NS_FAILED(rv)) return rv;                
                
                handler->PromptForFile(ptrv, PSM_FILE_NAME, PR_TRUE, &filePath);

                nsAllocator::Free(ptrv);

            }
            if (! filePath)
                return NS_ERROR_FAILURE;

            mControl = CMT_EstablishControlConnection(filePath, &nsPSMShimTbl, &nsPSMMutexTbl);
        }


        if (!mControl || InitPSMUICallbacks(mControl) != PR_SUCCESS)
            goto failure;

        nsFileSpec profileSpec;
        PRUnichar*      profileName;
        
        NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
        if (NS_FAILED(rv)) goto failure;
        
        rv = profile->GetCurrentProfileDir(&profileSpec);
        if (NS_FAILED(rv)) goto failure;;
        
        rv = profile->GetCurrentProfile(&profileName);
        if (NS_FAILED(rv)) goto failure;
          
        CMTStatus psmStatus;
        nsCAutoString profilenameC;
        profilenameC.AssignWithConversion(profileName);
        psmStatus = CMT_Hello( mControl, 
                                   PROTOCOL_VERSION, 
                                   profilenameC, 
                                   (char*)profileSpec.GetNativePathCString());        

        if (psmStatus == CMTFailure)
        {  
            PR_FREEIF(profileName);       
            goto failure;
        }
        
        if (NS_FAILED(PassPrefs()))
        {  
            PR_FREEIF(profileName);       
            goto failure;
        }

        PR_FREEIF(profileName);
        
        *_retval = mControl;
        return NS_OK;
    }

failure:
#ifdef DEBUG
    printf("*** Failure setting up Cartman! \n");
#endif

    if (mControl)
    {
        CMT_CloseControlConnection(mControl);
        mControl = NULL;
    }

    // TODO we need to unregister our UI callback BEFORE destroying our mutex.
    // nsPSMMutexDestroy();

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPSMComponent::DisplaySecurityAdvisor(const char *pickledStatus, const char *hostName)
{
    CMT_CONTROL *controlConnection;
    GetControlConnection( &controlConnection );
    if (DisplayPSMUIDialog(controlConnection, pickledStatus, hostName) == PR_SUCCESS)
        return NS_OK;
    return NS_ERROR_FAILURE;
}

//-----------------------------------------
// Secure Hash Functions 
//-----------------------------------------
NS_IMETHODIMP
nsPSMComponent::HashBegin(PRUint32 alg, PRUint32* id)
{
  CMT_CONTROL *controlConnection;
  if (NS_FAILED(GetControlConnection( &controlConnection )))
    return NS_ERROR_FAILURE;

  if(CMT_HashCreate(controlConnection, alg, (CMUint32*)id) != CMTSuccess)
    return NS_ERROR_FAILURE;
  if(CMT_HASH_Begin(controlConnection, *id) != CMTSuccess)
    return NS_ERROR_FAILURE;

  return NS_OK;  
}

NS_IMETHODIMP
nsPSMComponent::HashUpdate(PRUint32 id, const char* buf, PRUint32 buflen)
{
  CMT_CONTROL *controlConnection;

  if (NS_FAILED(GetControlConnection( &controlConnection )))
    return NS_ERROR_FAILURE;
  if (CMT_HASH_Update(controlConnection, id, 
      (const unsigned char*)buf, buflen) != CMTSuccess)    
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}

NS_IMETHODIMP
nsPSMComponent::HashEnd(PRUint32 id, char** hash, PRUint32* hashlen,
                        PRUint32 maxLen)
{
  if (!hash)
    return NS_ERROR_ILLEGAL_VALUE;

  CMT_CONTROL *controlConnection;
  if (NS_FAILED(GetControlConnection( &controlConnection )))
    return NS_ERROR_FAILURE;
 
  if(CMT_HASH_End(controlConnection, id, (unsigned char*)*hash,
                        (CMUint32*)hashlen, maxLen) != CMTSuccess)
    return NS_ERROR_FAILURE;
  CMT_HASH_Destroy(controlConnection, id);
  return NS_OK;
}

//-----------------------------------------
// Signature Verification Functions
//-----------------------------------------
PR_STATIC_CALLBACK(void)
UselessPK7DataSink(void* arg, const char* buf, CMUint32 len)
{
}

NS_IMETHODIMP
nsPSMComponent::VerifyRSABegin(PRUint32* id)
{
  if (!id)
    return NS_ERROR_ILLEGAL_VALUE;

  CMT_CONTROL *controlConnection;
  if (NS_FAILED(GetControlConnection( &controlConnection )))
    return NS_ERROR_FAILURE;
  
  CMInt32* blah = nsnull;
  CMTStatus result = CMT_PKCS7DecoderStart(controlConnection, nsnull,
                                           (CMUint32*)id, blah,
                                           UselessPK7DataSink, nsnull);
  if (result == CMTSuccess)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPSMComponent::VerifyRSAUpdate(PRUint32 id, const char* buf, PRUint32 buflen)
{
  CMT_CONTROL *controlConnection;
  if (NS_FAILED(GetControlConnection( &controlConnection )))
    return NS_ERROR_FAILURE;

  CMTStatus result = CMT_PKCS7DecoderUpdate(controlConnection, id, buf, buflen);
  if (result == CMTSuccess)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPSMComponent::VerifyRSAEnd(PRUint32 id, const char* plaintext,
                             PRUint32 plaintextLen,
                             PRBool aKeepCert, 
                             nsIPrincipal** aPrincipal,
                             PRInt32* aVerifyError)
{
  *aVerifyError = -1;
  CMT_CONTROL *controlConnection;
  if (NS_FAILED(GetControlConnection( &controlConnection )))
    return NS_ERROR_FAILURE;
  
  CMUint32 contentInfo;
  CMTStatus result = CMT_PKCS7DecoderFinish(controlConnection, 
                                            id, &contentInfo);
  if (result != CMTSuccess)
    return NS_ERROR_FAILURE;

  //-- Make sure a signature is present
  CMInt32 isSigned;
  result = CMT_GetNumericAttribute(controlConnection, contentInfo,
                                   SSM_FID_P7CINFO_IS_SIGNED, &isSigned);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;
  if (!isSigned)
  {
    *aPrincipal = nsnull;
    *aVerifyError = nsIPSMComponent::VERIFY_NOSIG;
    return NS_OK;
  }
  // SHA1 hash the plaintext to compare it to the signature
  CMUint32 hashId;
  CMT_HashCreate(controlConnection, nsIPSMComponent::SHA1, &hashId);
  CMT_HASH_Begin(controlConnection, hashId);
  result = CMT_HASH_Update(controlConnection, hashId, 
                           (const unsigned char*)plaintext, plaintextLen);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;

  unsigned char* hash = (unsigned char*)PR_MALLOC(nsIPSMComponent::SHA1_LENGTH);
  if (!hash) return NS_ERROR_OUT_OF_MEMORY;
  CMUint32 hashLen;
  result = CMT_HASH_End(controlConnection, hashId, hash,
                        &hashLen, nsIPSMComponent::SHA1_LENGTH);
  NS_ASSERTION(hashLen == nsIPSMComponent::SHA1_LENGTH,
               "PSMComponent: Hash too short.");
  CMT_HASH_Destroy(controlConnection, hashId);
  if (result != CMTSuccess) 
  {
    PR_FREEIF(hash);
    return NS_ERROR_FAILURE;
  }
  //-- Verify signature
  CMTItemStr hashItem;
  hashItem.data = hash;
  hashItem.len = hashLen;
  result = CMT_PKCS7VerifyDetachedSignature(controlConnection, contentInfo,
                                            6 /* =Object Signing Cert */,
                                            3 /* =SHA1 algorithm (MD5=2)*/,
                                            (CMUint32)aKeepCert,
                                            &hashItem, (CMInt32*)aVerifyError);
  PR_FREEIF(hash);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;
  //-- Did it verify?
  
  if (*aVerifyError != 0)
    *aPrincipal = nsnull;
  else
    {
      //-- Generate a principal from the cert
      CMUint32 certID;
      result = CMT_GetRIDAttribute(controlConnection, contentInfo,
                                       SSM_FID_P7CINFO_SIGNER_CERT, &certID);
      if (result != CMTSuccess) return NS_ERROR_FAILURE;
      if (NS_FAILED(CreatePrincipalFromCert(certID, aPrincipal))) 
        return NS_ERROR_FAILURE;
    }

  CMT_PKCS7DestroyContentInfo(controlConnection, contentInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsPSMComponent::CreatePrincipalFromCert(PRUint32 aCertID, nsIPrincipal** aPrincipal)
{
  CMT_CONTROL *controlConnection;
  if (NS_FAILED(GetControlConnection( &controlConnection )))
    return NS_ERROR_FAILURE;

  //-- Read cert info
  CMTStatus result;
  CMTItem issuerItem;
  result = CMT_GetStringAttribute(controlConnection, aCertID,
                                  SSM_FID_CERT_COMMON_NAME, &issuerItem);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;
  CMTItemStr serialNumberItem;
  result = CMT_GetStringAttribute(controlConnection, aCertID,
                                  SSM_FID_CERT_SERIAL_NUMBER, &serialNumberItem);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;
  CMTItemStr companyNameItem;
  result = CMT_GetStringAttribute(controlConnection, aCertID,
                                  SSM_FID_CERT_ORG_NAME, &companyNameItem);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;  
  //-- Get a principal
  nsresult rv;
  NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                  NS_SCRIPTSECURITYMANAGER_PROGID, &rv)
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  rv = secMan->GetCertificatePrincipal((char*)issuerItem.data, 
                                       (char*)serialNumberItem.data,
                                       (char*)companyNameItem.data,
                                       aPrincipal);
  return rv;
}
 
