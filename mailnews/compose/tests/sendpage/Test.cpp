#include "nsCOMPtr.h" 
#include "nsIMsgCompFields.h"
#include "nsIMsgSend.h"
#include "prmem.h"
#include "nsIComponentManager.h" 
#include "nsINetService.h"
#include "nsIPref.h"
#include "nsIAllocator.h"
#include "nsIEventQueueService.h"
#include "nsMsgCompCID.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "nsIFileLocator.h"
#include "nsIMsgSendListener.h"

#include <stdio.h>
#ifdef XP_PC
#include <windows.h>
#endif

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define PREF_DLL   "xppref32.dll"
#define APPSHELL_DLL "nsappshell.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define PREF_DLL   "libpref"MOZ_DLL_SUFFIX
#define APPSHELL_DLL "libnsappshell"MOZ_DLL_SUFFIX
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kAllocatorCID,  NS_ALLOCATOR_CID);
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_CID(kMsgSendCID, NS_MSGSEND_CID); 
static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
static NS_DEFINE_CID(kGenericFactoryCID,    NS_GENERICFACTORY_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);


////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
class nsMsgSendLater;
class SendOperationListener : public nsIMsgSendListener
{
public:
  SendOperationListener(void);
  virtual ~SendOperationListener(void);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  /* void OnStartSending (in string aMsgID, in PRUint32 aMsgSize); */
  NS_IMETHOD OnStartSending(const char *aMsgID, PRUint32 aMsgSize);
  
  /* void OnProgress (in string aMsgID, in PRUint32 aProgress, in PRUint32 aProgressMax); */
  NS_IMETHOD OnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax);
  
  /* void OnStatus (in string aMsgID, in wstring aMsg); */
  NS_IMETHOD OnStatus(const char *aMsgID, const PRUnichar *aMsg);
  
  /* void OnStopSending (in string aMsgID, in nsresult aStatus, in wstring aMsg, in nsIFileSpec returnFileSpec); */
  NS_IMETHOD OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                           nsIFileSpec *returnFileSpec);
  
private:
  nsMsgSendLater    *mSendLater;
};

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS(SendOperationListener, nsIMsgSendListener::GetIID());

SendOperationListener::SendOperationListener(void) 
{ 
  mSendLater = nsnull;
  NS_INIT_REFCNT(); 
}

SendOperationListener::~SendOperationListener(void) 
{
}

nsresult
SendOperationListener::OnStartSending(const char *aMsgID, PRUint32 aMsgSize)
{
#ifdef NS_DEBUG
  printf("SendOperationListener::OnStartSending()\n");
#endif
  return NS_OK;
}
  
nsresult
SendOperationListener::OnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax)
{
#ifdef NS_DEBUG
  printf("SendOperationListener::OnProgress()\n");
#endif
  return NS_OK;
}

nsresult
SendOperationListener::OnStatus(const char *aMsgID, const PRUnichar *aMsg)
{
#ifdef NS_DEBUG
  printf("SendOperationListener::OnStatus()\n");
#endif

  return NS_OK;
}
  
PRBool keepOnRunning = PR_TRUE; 
nsresult
SendOperationListener::OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                                     nsIFileSpec *returnFileSpec)
{
  nsresult                    rv = NS_OK;

  if (NS_SUCCEEDED(aStatus))
  {
    printf("Send Mail Operation Completed Successfully!\n");
  }
  else
  {
    printf("Send Mail Operation FAILED!\n");
  }

  printf("Exit code = [%d]\n", aStatus);
  keepOnRunning = PR_FALSE;
  return NS_OK;
}

nsIMsgSendListener **
CreateListenerArray(nsIMsgSendListener *listener)
{
  if (!listener)
    return nsnull;

  nsIMsgSendListener **tArray = (nsIMsgSendListener **)PR_Malloc(sizeof(nsIMsgSendListener *) * 2);
  if (!tArray)
    return nsnull;
  nsCRT::memset(tArray, 0, sizeof(nsIMsgSendListener *) * 2);
  tArray[0] = listener;
  return tArray;
}

// Utility to create a nsIURI object...
nsresult 
nsMsgNewURL(nsIURI** aInstancePtrResult, const nsString& aSpec)
{  
  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  nsINetService *inet = nsnull;
  nsresult rv = nsServiceManager::GetService(kNetServiceCID, nsINetService::GetIID(),
                                             (nsISupports **)&inet);
  if (rv != NS_OK) 
    return rv;

  rv = inet->CreateURL(aInstancePtrResult, aSpec, nsnull, nsnull, nsnull);
  nsServiceManager::ReleaseService(kNetServiceCID, inet);
  return rv;
}

static nsresult
SetupRegistry(void)
{
  // netlib
  nsComponentManager::RegisterComponent(kNetServiceCID,     NULL, NULL, NETLIB_DLL,  PR_FALSE, PR_FALSE);
  
  // xpcom
  nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kEventQueueCID,        NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kGenericFactoryCID,    NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kAllocatorCID,         NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);

  // prefs
  nsComponentManager::RegisterComponent(kPrefCID,              NULL, NULL, PREF_DLL,  PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsIMsgIdentity *
GetHackIdentity()
{
nsresult rv;

  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) 
  {
    printf("Failure on Mail Session Init!\n");
    return nsnull;
  }  

  nsCOMPtr<nsIMsgIdentity>        identity = nsnull;
  nsCOMPtr<nsIMsgAccountManager>  accountManager;

  rv = mailSession->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) 
  {
    printf("Failure getting account Manager!\n");
    return nsnull;
  }  

  rv = mailSession->GetCurrentIdentity(getter_AddRefs(identity));
  if (NS_FAILED(rv)) 
  {
    printf("Failure getting Identity!\n");
    return nsnull;
  }  

  return identity;
}

//
// This is a test stub for the send web page interface.
//
int 
main(int argc, char *argv[]) 
{ 
  nsIMsgCompFields    *pMsgCompFields;
  nsIMsgSend          *pMsgSend;
  nsresult            rv = NS_OK;

  // Before anything, do some sanity checking :-)
  if (argc < 4)
  {
    printf("Usage: %s <recipients> <web_page_url> <email_subject>\n", argv[0]);
    exit(0);
  }
  SetupRegistry();

  // Create the Event Queue for this thread...
	NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv); 
	if (NS_FAILED(rv)) 
  {
    printf("Failed to get event queue\n");
    return rv;
  }

  rv = pEventQService->CreateThreadEventQueue();
	if (NS_FAILED(rv)) 
  {
    printf("Failed to create event queue\n");
    return rv;
  }

	// make sure prefs get initialized and loaded..
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_FAILED(rv) || (!prefs)) 
  {
    printf("Failed on prefs service!\n");
    exit(rv);
  }
  if (NS_FAILED(prefs->ReadUserPrefs()))
  {
    printf("Failed on reading user prefs!\n");
    exit(rv);
  }


  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) 
  {
    printf("Failure on Mail Session Init!\n");
    return rv;
  }  

  rv = nsComponentManager::CreateInstance(kMsgSendCID, NULL, nsIMsgSend::GetIID(), (void **) &pMsgSend); 
  if (NS_SUCCEEDED(rv) && pMsgSend) 
  { 
    printf("We succesfully obtained a nsIMsgSend interface....\n");    
    rv = nsComponentManager::CreateInstance(kMsgCompFieldsCID, NULL, nsIMsgCompFields::GetIID(), 
                                             (void **) &pMsgCompFields); 
    if (NS_SUCCEEDED(rv) && pMsgCompFields) 
    if (rv == NS_OK && pMsgCompFields)
    { 
      nsIMsgIdentity  *ident = GetHackIdentity();
      char            *email = nsnull;

      if (ident)
        ident->GetEmail(&email);

      if (!email)
        email = "rhp@netscape.com";

      pMsgCompFields->SetFrom(email, NULL);
      pMsgCompFields->SetTo(argv[1], NULL);

      pMsgCompFields->SetSubject(argv[3], NULL);
      pMsgCompFields->SetBody(argv[2], NULL);

      nsIURI    *url;
      char      *ptr = argv[2];
      while (*ptr)
      {
        if (*ptr == '\\')
          *ptr = '/';
        ++ptr;
      }

      nsMsgNewURL(&url, argv[2]);
      if (!url)
      {
        printf("Error creating new URL object.\n");
        return -1;
      }

      // Create the listener for the send operation...
      SendOperationListener *mSendListener = new SendOperationListener();
      if (!mSendListener)
      {
        return NS_ERROR_FAILURE;
      }
      
      // set this object for use on completion...
      nsIMsgSendListener **tArray = CreateListenerArray(mSendListener);
      if (!tArray)
      {
        printf("Error creating listener array.\n");
        return NS_ERROR_FAILURE;
      }

      pMsgSend->SendWebPage(pMsgCompFields, url, nsMsgDeliverNow, tArray);
    }    
  }

#ifdef XP_PC
  printf("Sitting in an event processing loop ...Hit Cntl-C to exit...");
  while (keepOnRunning)
  {
    MSG msg;
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
#endif

  printf("Releasing the interface now...\n");
  if (pMsgSend)
    pMsgSend->Release(); 
  if (pMsgCompFields)
    pMsgCompFields->Release(); 

  return 0; 
}

