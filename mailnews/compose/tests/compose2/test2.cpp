#include "msgCore.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"
#include "nsMsgCompCID.h"

#include <stdio.h>
#ifdef XP_PC
#include <windows.h>
#endif

#include "nsIComponentManager.h" 
#include "nsMsgCompCID.h"
#include "nsIMsgCompFields.h"
#include "nsIMsgSend.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nscore.h"
#include "nsIMsgMailSession.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsISmtpService.h"
#include "nsISmtpUrl.h"
#include "nsIUrlListener.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsIFileLocator.h"
#include "MsgCompGlue.h"
#include "nsCRT.h"
#include "prmem.h"
#include "nsIMimeURLUtils.h"
#include "nsFileStream.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define PREF_DLL   "xppref32.dll"
#define APPSHELL_DLL "nsappshell.dll"
#define MIME_DLL "mime.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define PREF_DLL   "libpref"MOZ_DLL_SUFFIX
#define APPCORES_DLL  "libappcores"MOZ_DLL_SUFFIX
#define APPSHELL_DLL "libnsappshell"MOZ_DLL_SUFFIX
#define MIME_DLL "libmime"MOZ_DLL_SUFFIX
#endif
#endif


/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kMsgComposeCID, NS_MSGCOMPOSE_CID); 
static NS_DEFINE_IID(kIMsgCompFieldsIID, NS_IMSGCOMPFIELDS_IID); 
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_IID(kIMsgSendIID, NS_IMSGSEND_IID); 
static NS_DEFINE_CID(kMsgSendCID, NS_MSGSEND_CID); 
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kMimeURLUtilsCID, NS_IMIME_URLUTILS_CID);

static char *
GetTheTempDirectoryOnTheSystem(void)
{
  char *retPath = (char *)PR_Malloc(1024);
  if (!retPath)
    return nsnull;

  retPath[0] = '\0';
#ifdef WIN32
  if (getenv("TEMP"))
    PL_strncpy(retPath, getenv("TEMP"), 1024);  // environment variable
  else if (getenv("TMP"))
    PL_strncpy(retPath, getenv("TMP"), 1024);   // How about this environment variable?
  else
    GetWindowsDirectory(retPath, 1024);
#endif 

  // RICHIE - should do something better here!

#ifdef XP_UNIX
  PL_strncpy(retPath, "/tmp/", 1024);
#endif

#ifdef XP_MAC
  PL_strncpy(retPath, "", 1024);
#endif

  return retPath;
}

//
// Create a file spec for the a unique temp file
// on the local machine. Caller must free memory
//
nsFileSpec * 
nsMsgCreateTempFileSpec(char *tFileName)
{
  if ((!tFileName) || (!*tFileName))
    tFileName = "nsmail.tmp";

  // Age old question, where to store temp files....ugh!
  char  *tDir = GetTheTempDirectoryOnTheSystem();
  if (!tDir)
    return (new nsFileSpec("mozmail.tmp"));  // No need to I18N

  nsFileSpec *tmpSpec = new nsFileSpec(tDir);
  if (!tmpSpec)
  {
    PR_FREEIF(tDir);
    return (new nsFileSpec("mozmail.tmp"));  // No need to I18N
  }

  *tmpSpec += tFileName;
  tmpSpec->MakeUnique();

  PR_FREEIF(tDir);
  return tmpSpec;
}

nsresult
CallMe(nsresult aExitCode, void *tagData, nsFileSpec *fs)
{
  nsIMsgSend  *ptr = (nsIMsgSend  *)tagData;
  
  printf("Called ME!\n");
  printf("Exit code = %d\n", aExitCode);

  ptr->Release();

  return NS_OK;
}

char *email = {"\
Message-ID: <375FF6D0.3070505@netscape.com>\
\nDate: Thu, 10 Jun 1999 13:33:04 -0500\
\nFrom: rhp@netscape.com\
\nX-Mailer: Mozilla 5.0 [en] (Win95; I)\
\nX-Accept-Language: en\
\nMIME-Version: 1.0\
\nTo: rhp@netscape.com\
\nSubject: [spam] test\
\nContent-Type: text/html; charset=\
\nContent-Transfer-Encoding: 7bit\
\n\
<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">\n\
<html>\n\
<body text=\"#000000\" bgcolor=\"#FFFFFF\" link=\"#FF0000\" vlink=\"#800080\" alink=\"#0000FF\">\n\
<b><font face=\"Arial,Helvetica\"><font color=\"#FF0000\">Here is some HTML\n\
in RED!</font></font></b>\n\
<br><b><font face=\"Arial,Helvetica\"><font color=\"#FF0000\">Now a picture:</font></font></b>\n\
<br><img SRC=\"http://people.netscape.com/rhp/rhp-home2.gif\">\n\
<br>All done!\n\
<br>&nbsp;\n\
</body>\n\
</html>"};

nsresult
WriteTempMailFile(nsFileSpec *mailFile)
{
  nsOutputFileStream      *outFile;         // the actual output file stream

  outFile = new nsOutputFileStream(*mailFile);
	if (! outFile->is_open()) 
  {
	    return NS_ERROR_FAILURE;
  }

  outFile->write(email, PL_strlen(email));
  outFile->close();
  return NS_OK;
}

/* 
 * This is a test stub for mail composition where we create the outgoing mail
 * message ourselves and use the nsIMsgSend to drive the send operation only
 */
int main(int argc, char *argv[]) 
{ 
  nsIMsgCompFields  *pMsgCompFields;
  nsIMsgSend        *pMsgSend;
  nsresult          rv = NS_OK;
  nsFileSpec        *mailFile = nsnull;

  nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kEventQueueCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
	nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NULL, APPSHELL_DLL, PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kMimeURLUtilsCID,  NULL, NULL, MIME_DLL, PR_FALSE, PR_FALSE);

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
	// mscott - this is just a bad bad bad hack right now until prefs
	// has the ability to take nsnull as a parameter. Once that happens,
	// prefs will do the work of figuring out which prefs file to load...
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_FAILED(rv) || (prefs == nsnull)) 
  {
    exit(rv);
  }

  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) 
  {
    printf("Failure on Mail Session Init!\n");
    return rv;
  }  

  rv = nsComponentManager::CreateInstance(kMsgCompFieldsCID, NULL, 
                                           nsIMsgCompFields::GetIID(), (void **) &pMsgCompFields);   
  if (rv == NS_OK && pMsgCompFields) { 
    printf("We succesfully obtained a nsIMsgCompFields interface....\n");
    printf("Releasing the interface now...\n");
    pMsgCompFields->Release(); 
  } 
  
  printf("Creating temp mail file...\n");
  mailFile = nsMsgCreateTempFileSpec("mailTest.tmp");

  if (NS_FAILED(WriteTempMailFile(mailFile)))
  {
    printf("Failed to create temp mail file!\n");
    return 0;
  }

  rv = nsComponentManager::CreateInstance(kMsgSendCID, NULL, kIMsgSendIID, (void **) &pMsgSend); 
  if (rv == NS_OK && pMsgSend) 
  { 
    printf("We succesfully obtained a nsIMsgSend interface....\n");    
    rv = nsComponentManager::CreateInstance(kMsgCompFieldsCID, NULL, kIMsgCompFieldsIID, 
                                             (void **) &pMsgCompFields); 
    if (rv == NS_OK && pMsgCompFields)
    { 
      pMsgCompFields->SetTo("rhp@netscape.com", NULL);
      pMsgSend->SendMessageFile(pMsgCompFields, // nsIMsgCompFields                  *fields,
                          mailFile,             // nsFileSpec                        *sendFileSpec,
                          PR_TRUE,              // PRBool                            deleteSendFileOnCompletion,
						              PR_FALSE,             // PRBool                            digest_p,
						              nsMsgDeliverNow,      // nsMsgDeliverMode                  mode,
                          CallMe,               // nsMsgSendCompletionCallback       completionCallback,
                          pMsgSend);              // void                              *tagData);
    }    
  }

#ifdef XP_PC
  printf("Sitting in an event processing loop ...Hit Cntl-C to exit...");
  while (1)
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
  pMsgSend->Release(); 
  pMsgCompFields->Release(); 


  return 0; 
}

