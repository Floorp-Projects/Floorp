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

#include "nsNetUtil.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsIURIContentListener.h"

#include "nsIPref.h"
#include "nsIProfile.h"
#include "nsILocalFile.h"

#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "rsrcids.h"

#include "nsPSMMutex.h"
#include "nsPSMShimLayer.h"
#include "nsPSMUICallbacks.h"

#include "nsISecureBrowserUI.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIScriptSecurityManager.h"
#include "nsICertificatePrincipal.h"

#include "nsIProtocolProxyService.h"
#include "nsXPIDLString.h"
#include "nsCURILoader.h"

#define PSM_VERSION_REG_KEY "/Netscape/Personal Security Manager"

#ifdef WIN32
#define PSM_FILE_NAME "psm.exe"
#elif XP_UNIX
#define PSM_FILE_NAME "start-psm"
#else
#define PSM_FILE_NAME "psm"
#endif


static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kProtocolProxyServiceCID, NS_PROTOCOLPROXYSERVICE_CID);

nsPSMComponent* nsPSMComponent::mInstance = nsnull;

nsPSMComponent::nsPSMComponent()
{
    NS_INIT_REFCNT();
    mControl = nsnull;
    mCertContentListener = nsnull;
}

nsPSMComponent::~nsPSMComponent()
{
    if (mControl)
    {
        CMT_CloseControlConnection(mControl);
        mControl = nsnull;
    }
    if (mCertContentListener) {
      nsresult rv = NS_ERROR_FAILURE;
      
      NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_PROGID, &rv);
      if (NS_SUCCEEDED(rv)) {
        rv = dispatcher->UnRegisterContentListener(mCertContentListener);
      }
      
      mCertContentListener = nsnull;
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
        if (mInstance)
          mInstance->RegisterCertContentListener();
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

nsresult
nsPSMComponent::RegisterCertContentListener()
{
  nsresult rv = NS_OK;
  if (mCertContentListener == nsnull) {
    NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_PROGID, &rv);
    if (NS_SUCCEEDED(rv)) {
      mCertContentListener = do_CreateInstance(NS_CERTCONTENTLISTEN_PROGID);
      rv = dispatcher->RegisterContentListener(mCertContentListener);
    }
  }
  return rv;
}

/* nsISupports Implementation for the class */
NS_IMPL_THREADSAFE_ISUPPORTS3(nsPSMComponent, 
                              nsIPSMComponent, 
                              nsIContentHandler,
                              nsISignatureVerifier);

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

#ifdef XP_MAC
extern "C" {
    void RunMacPSM(void* arg);
    PRThread* SSM_CreateAndRegisterThread(PRThreadType type, void (*start)(void *arg),
                                          void *arg, PRThreadPriority priority,
                                          PRThreadScope scope, PRThreadState state,
                                          PRUint32 stackSize);
    void SSM_KillAllThreads(void);
}
#endif
            
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

#ifdef XP_MAC
        /* FIXME:  Really need better error handling in PSM, which simply exits on error. */
        /* use a cached monitor to rendezvous with the PSM thread. */
        PRMonitor* monitor = PR_CEnterMonitor(this);
        if (monitor != nsnull) {
            /* create the Cartman thread, and let it run awhile to get things going. */
            PRThread* cartmanThread = SSM_CreateAndRegisterThread(PR_USER_THREAD, RunMacPSM, 
							                                      this, PR_PRIORITY_NORMAL,
							                                      PR_LOCAL_THREAD, PR_UNJOINABLE_THREAD, 0);
            if (cartmanThread != nsnull) {
                /* need a good way to rendezvouz with the Cartman thread. */
                PR_CWait(this, PR_INTERVAL_NO_TIMEOUT);
            }
            
            PR_CExitMonitor(this);
        }
#endif

        // Try to see if it is open already
        mControl = CMT_ControlConnect(&nsPSMMutexTbl, &nsPSMShimTbl);
        
        // Find the one in the bin directory
        if (mControl == nsnull)
        {
            nsCOMPtr<nsILocalFile> psmAppFile;
            NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
            if (NS_FAILED(rv)) return rv;

            directoryService->Get( NS_XPCOM_CURRENT_PROCESS_DIR,
                                   NS_GET_IID(nsIFile), 
                                   getter_AddRefs(psmAppFile));

            if (psmAppFile)
            {
              psmAppFile->Append("psm");
              psmAppFile->Append(PSM_FILE_NAME);
        
              PRBool isExecutable, exists;
              psmAppFile->Exists(&exists);
              psmAppFile->IsExecutable(&isExecutable);
              if (exists && isExecutable)
              {
                  nsXPIDLCString path;
                  psmAppFile->GetPath(getter_Copies(path));
                  // FIX THIS.  using a file path is totally wrong here.  
                  mControl = CMT_EstablishControlConnection((char*)(const char*)path, &nsPSMShimTbl, &nsPSMMutexTbl);

              }
            }
        }

        // Get the one in the version registry           
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

        
        if (!mControl || InitPSMUICallbacks(mControl) != PR_SUCCESS)
            goto failure;

        nsFileSpec profileSpec;
        PRUnichar*      profileName;
        
        NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
        if (NS_FAILED(rv)) goto failure;
        
        rv = profile->GetCurrentProfileDir(&profileSpec);
        if (NS_FAILED(rv)) goto failure;;
        
#ifdef XP_MAC
        profileSpec += "Security";
        // make sure the dir exists
        profileSpec.CreateDirectory();
#endif
        
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
        
        if (InitPSMEventLoop(mControl) != PR_SUCCESS)
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
        
        nsCOMPtr<nsIProtocolProxyService> proxySvc = do_GetService(kProtocolProxyServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
        proxySvc->AddNoProxyFor("127.0.0.1", mControl->port);
        
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

class CertDownloader : public nsIStreamListener
{
    public:
        CertDownloader() {NS_ASSERTION(0, "don't use this constructor."); }
        CertDownloader(PRInt32 type);
        virtual ~CertDownloader();

        NS_DECL_ISUPPORTS
        NS_DECL_NSISTREAMOBSERVER
        NS_DECL_NSISTREAMLISTENER
    protected:
        char* mByteData;
        PRInt32 mBufferOffset;
        PRInt32 mContentLength;
        PRInt32 mType;
};


CertDownloader::CertDownloader(PRInt32 type)
{
    NS_INIT_REFCNT();
    mByteData = nsnull;
    mType = type;
}

CertDownloader::~CertDownloader()
{
    if (mByteData)
        nsMemory::Free(mByteData);
}

NS_IMPL_ISUPPORTS(CertDownloader,NS_GET_IID(nsIStreamListener));

const PRInt32 kDefaultCertAllocLength = 2048;

NS_IMETHODIMP
CertDownloader::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
	nsresult rv;
	
    rv = channel->GetContentLength(&mContentLength);
    if (rv != NS_OK || mContentLength == -1)
      mContentLength = kDefaultCertAllocLength;
    
    mBufferOffset = 0;
    mByteData = (char*) nsMemory::Alloc(mContentLength);
    if (!mByteData)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}


NS_IMETHODIMP
CertDownloader::OnDataAvailable(nsIChannel* channel, 
                                nsISupports* context,
                                nsIInputStream *aIStream, 
                                PRUint32 aSourceOffset,
                                PRUint32 aLength)
{
    if (!mByteData)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 amt;
    nsresult err;
    //Do a check to see if we need to allocate more memory.
    if ((mBufferOffset + (PRInt32)aLength) > mContentLength) {
      size_t newSize = mContentLength + kDefaultCertAllocLength;
      char *newBuffer;
      newBuffer = (char*)nsMemory::Realloc(mByteData, newSize);
      if (newBuffer == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mByteData = newBuffer;
      mContentLength = newSize;
    }
    do 
    {
        err = aIStream->Read(mByteData+mBufferOffset, 
                             mContentLength-mBufferOffset, &amt);
        if (amt == 0) break;
        if (NS_FAILED(err))  return err;
        
        aLength -= amt;
        mBufferOffset += amt;

    } while (aLength > 0);
    
    return NS_OK;
}


NS_IMETHODIMP
CertDownloader::OnStopRequest(nsIChannel* channel, 
                              nsISupports* context,
                              nsresult aStatus,
                              const PRUnichar* aMsg)
{

    nsCOMPtr<nsIPSMComponent> psm = do_QueryInterface(context);

    if (!psm) {
      nsresult rv = nsPSMComponent::CreatePSMComponent(nsnull, 
                                                  NS_GET_IID(nsIPSMComponent),
                                                       getter_AddRefs(psm));
      if (NS_FAILED(rv)) 
        return rv;

    }

    CMT_CONTROL *controlConnection;
    psm->GetControlConnection( &controlConnection );
    unsigned int certID;
                    
    certID = CMT_DecodeAndCreateTempCert(controlConnection, mByteData, 
                                         mBufferOffset, mType);

    if (certID)
        CMT_DestroyResource(controlConnection, certID, SSM_RESTYPE_CERTIFICATE);

  return NS_OK;
}


/* other mime types that we should handle sometime:

application/x-pkcs7-crl
application/x-pkcs7-mime
application/pkcs7-signature
application/pre-encrypted

*/

const char * kCACert     = "application/x-x509-ca-cert";
const char * kServerCert = "application/x-x509-server-cert";
const char * kUserCert   = "application/x-x509-user-cert";
const char * kEmailCert  = "application/x-x509-email-cert";

CMUint32
getPSMCertType(const char * aContentType)
{
  CMUint32 type = (CMUint32)-1;

  if ( nsCRT::strcasecmp(aContentType, kCACert) == 0)
    {
      type = 1;  //CA cert
    }
  else if (nsCRT::strcasecmp(aContentType, kServerCert) == 0)
    {
      type =  2; //Server cert
    }
  else if (nsCRT::strcasecmp(aContentType, kUserCert) == 0)
    {
      type =  3; //User cert
    }
  else if (nsCRT::strcasecmp(aContentType, kEmailCert) == 0)
    {
      type =  4; //Someone else's email cert
    }
  return type;
}

NS_IMETHODIMP 
nsPSMComponent::HandleContent(const char * aContentType, 
                              const char * aCommand, 
                              const char * aWindowTarget, 
                              nsISupports* aWindowContext, 
                              nsIChannel * aChannel)
{
    // We were called via CI.  We better protect ourselves and addref.
    NS_ADDREF_THIS();

    nsresult rv = NS_OK;
    if (!aChannel) return NS_ERROR_NULL_POINTER;
    
    CMUint32 type = getPSMCertType(aContentType);

    if (type != -1)
    {
        // I can't directly open the passed channel cause it fails :-(

        nsCOMPtr<nsIURI> uri;
        rv = aChannel->GetURI(getter_AddRefs(uri));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIChannel> channel;
        rv = NS_OpenURI(getter_AddRefs(channel), uri);
        if (NS_FAILED(rv)) return rv;

        return channel->AsyncRead(new CertDownloader(type), NS_STATIC_CAST(nsIPSMComponent*,this));
    }

    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_ISUPPORTS(CertContentListener, NS_GET_IID(nsIURIContentListener)); 


CertContentListener::CertContentListener()
{
  NS_INIT_REFCNT();
  mLoadCookie = nsnull;
  mParentContentListener = nsnull;
}

CertContentListener::~CertContentListener()
{

}

nsresult
CertContentListener::init()
{
  return NS_OK;
}

NS_IMETHODIMP
CertContentListener::OnStartURIOpen(nsIURI *aURI, const char *aWindowTarget,
                                    PRBool *aAbortOpen)
{
  //if we don't want to handle the URI, return PR_TRUE in
  //*aAbortOpen

  return NS_OK;
}

NS_IMETHODIMP
CertContentListener::GetProtocolHandler(nsIURI *aURI, 
                                        nsIProtocolHandler **aProtocolHandler)
{
  *aProtocolHandler = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
CertContentListener::IsPreferred(const char * aContentType,
                                 nsURILoadCommand aCommand,
                                 const char * aWindowTarget,
                                 char ** aDesiredContentType,
                                 PRBool * aCanHandleContent)
{
  return CanHandleContent(aContentType, aCommand, aWindowTarget, 
                          aDesiredContentType, aCanHandleContent);
}

NS_IMETHODIMP
CertContentListener::CanHandleContent(const char * aContentType,
                                      nsURILoadCommand aCommand,
                                      const char * aWindowTarget,
                                      char ** aDesiredContentType,
                                      PRBool * aCanHandleContent)
{
  CMUint32 type;

  type = getPSMCertType(aContentType);
  if (type != (CMUint32)-1) {
    *aCanHandleContent = PR_TRUE;
  } else {
    *aCanHandleContent = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
CertContentListener::DoContent(const char * aContentType,
                               nsURILoadCommand aCommand,
                               const char * aWindowTarget,
                               nsIChannel * aOpenedChannel,
                               nsIStreamListener ** aContentHandler,
                               PRBool * aAbortProcess)
{
  CMUint32 type;
  CertDownloader *downLoader;
  type = getPSMCertType(aContentType);
  if (type != (CMUint32)-1) {
    downLoader = new CertDownloader(type);
    if (downLoader) {
      downLoader->QueryInterface(NS_GET_IID(nsIStreamListener), 
                                            (void **)aContentHandler);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CertContentListener::GetLoadCookie(nsISupports * *aLoadCookie)
{
  *aLoadCookie = mLoadCookie;
  NS_IF_ADDREF(*aLoadCookie);
  return NS_OK;
}

NS_IMETHODIMP
CertContentListener::SetLoadCookie(nsISupports * aLoadCookie)
{
  mLoadCookie = aLoadCookie;
  return NS_OK;
}

NS_IMETHODIMP
CertContentListener::GetParentContentListener(nsIURIContentListener ** aContentListener)
{
  *aContentListener = mParentContentListener;
  NS_IF_ADDREF(*aContentListener);
  return NS_OK;
}

NS_IMETHODIMP
CertContentListener::SetParentContentListener(nsIURIContentListener * aContentListener)
{
  mParentContentListener = aContentListener;
  return NS_OK;
}

//---------------------------------------------
// Functions Implenenting NSISignatureVerifier
//---------------------------------------------
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
nsPSMComponent::HashEnd(PRUint32 id, unsigned char** hash, 
                        PRUint32* hashLen, PRUint32 maxLen)
{
  if (!hash)
    return NS_ERROR_ILLEGAL_VALUE;

  CMT_CONTROL *controlConnection;
  if (NS_FAILED(GetControlConnection( &controlConnection )))
    return NS_ERROR_FAILURE;
 
  if(CMT_HASH_End(controlConnection, id, *hash,
                        (CMUint32*)hashLen, maxLen) != CMTSuccess)
    return NS_ERROR_FAILURE;
  CMT_HASH_Destroy(controlConnection, id);
  return NS_OK;
}

NS_IMETHODIMP
nsPSMComponent::CreatePrincipalFromSignature(const char* aRSABuf, PRUint32 aRSABufLen,
                                             nsIPrincipal** aPrincipal)
{
  PRInt32 errorCode;
  return VerifySignature(aRSABuf, aRSABufLen, nsnull, 0, &errorCode, aPrincipal);
}

PR_STATIC_CALLBACK(void)
UselessPK7DataSink(void* arg, const char* buf, CMUint32 len)
{
}

NS_IMETHODIMP
nsPSMComponent::VerifySignature(const char* aRSABuf, PRUint32 aRSABufLen, 
                                const char* aPlaintext, PRUint32 aPlaintextLen,
                                PRInt32* aErrorCode,
                                nsIPrincipal** aPrincipal)
{
  if (!aPrincipal || !aErrorCode)
    return NS_ERROR_NULL_POINTER;
  *aErrorCode = 0;
  *aPrincipal = nsnull;

  CMT_CONTROL *controlConnection;
  if (NS_FAILED(GetControlConnection( &controlConnection )))
    return NS_ERROR_FAILURE;
  
  //-- Decode the signature stream
  CMUint32 decoderID;
  CMInt32* blah = nsnull;
  CMTStatus result = CMT_PKCS7DecoderStart(controlConnection, nsnull,
                                           &decoderID, blah,
                                           UselessPK7DataSink, nsnull);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;
  result = CMT_PKCS7DecoderUpdate(controlConnection, decoderID, aRSABuf, aRSABufLen);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;
  CMUint32 contentInfo;
  result = CMT_PKCS7DecoderFinish(controlConnection, 
                                            decoderID, &contentInfo);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;

  CMTItem hashItem;
  hashItem.data = 0;
  hashItem.len = 0;
  //-- If a plaintext was provided, hash it.
  if (aPlaintext)
  {
    CMUint32 hashId;
    CMT_HashCreate(controlConnection, nsISignatureVerifier::SHA1, &hashId);
    CMT_HASH_Begin(controlConnection, hashId);
    CMTStatus result = CMT_HASH_Update(controlConnection, hashId, 
                                       (const unsigned char*)aPlaintext, aPlaintextLen);
    if (result != CMTSuccess) return NS_ERROR_FAILURE;
    
    unsigned char* hash = (unsigned char*)PR_MALLOC(nsISignatureVerifier::SHA1_LENGTH);
    if (!hash) return NS_ERROR_OUT_OF_MEMORY;
    CMUint32 hashLen;
    result = CMT_HASH_End(controlConnection, hashId, hash,
                          &hashLen, nsISignatureVerifier::SHA1_LENGTH);
    if (result != CMTSuccess)
    {
      PR_FREEIF(hash);
      return NS_ERROR_FAILURE;
    }
    NS_ASSERTION(hashLen == nsISignatureVerifier::SHA1_LENGTH,
                 "PSMComponent: Hash too short.");
    CMT_HASH_Destroy(controlConnection, hashId);
    hashItem.data = hash;
    hashItem.len = hashLen;
  }

  //-- Verify signature
  // We need to call this function even if we're only creating a principal, not
  // verifying, because PSM won't give us certificate information unless this
  // function has been called.
  result = CMT_PKCS7VerifyDetachedSignature(controlConnection, contentInfo,
                                            6 /* =Object Signing Cert */,
                                            3 /* =SHA1 algorithm (MD5=2)*/,
                                            1,/* Save Certificate */
                                            &hashItem, (CMInt32*)aErrorCode);

  if (result != CMTSuccess) return NS_ERROR_FAILURE;
  if (aPlaintext && *aErrorCode != 0) return NS_OK; // Verification failed.

  CMUint32 certID;
  result = CMT_GetRIDAttribute(controlConnection, contentInfo,
                               SSM_FID_P7CINFO_SIGNER_CERT, &certID);
  if ((result != CMTSuccess) || !certID) return NS_OK; // No signature present
  
  CMTItem fingerprint;
  result = CMT_GetStringAttribute(controlConnection, certID,
                                  SSM_FID_CERT_FINGERPRINT, &fingerprint);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;

  CMTItem common;
  result = CMT_GetStringAttribute(controlConnection, certID,
                                  SSM_FID_CERT_COMMON_NAME, &common);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;

  //-- Get a principal
  nsresult rv;
  NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                  NS_SCRIPTSECURITYMANAGER_PROGID, &rv)
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  rv = secMan->GetCertificatePrincipal((const char*)fingerprint.data,
                                       aPrincipal);
  if (NS_FAILED(rv)) return rv;

  //-- Get common name and store it in the principal.
  //   Using common name + organizational unit as the user-visible certificate name
  nsCOMPtr<nsICertificatePrincipal> certificate = do_QueryInterface(*aPrincipal, &rv);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  CMTItem subject;
  result = CMT_GetStringAttribute(controlConnection, certID,
                                  SSM_FID_CERT_SUBJECT_NAME, &subject);
  if (result != CMTSuccess) return NS_ERROR_FAILURE;

  nsCAutoString commonName;
  commonName = (char*)common.data;
  static const char orgUnitTag[] = " OU=";
  char* orgUnitPos = PL_strstr((char*)subject.data, orgUnitTag);
  if (orgUnitPos)
  {
    orgUnitPos += sizeof(orgUnitTag)-1;
    char* orgUnitEnd = PL_strchr(orgUnitPos, ',');
    PRInt32 orgUnitLen;
    if(orgUnitEnd)
      orgUnitLen = orgUnitEnd - orgUnitPos;
    else
      orgUnitLen = PL_strlen(orgUnitPos);
    commonName.Append(' ');
    commonName.Append(orgUnitPos, orgUnitLen);
  }
  nsXPIDLCString commonChar;
  commonChar = commonName.GetBuffer();
  if (!commonChar) return NS_ERROR_OUT_OF_MEMORY;
  rv = certificate->SetCommonName(commonChar);
  return rv;
}

