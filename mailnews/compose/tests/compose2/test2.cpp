/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"
#include "nsMsgCompCID.h"

#include <stdio.h>
#if defined(XP_PC) && !defined(XP_OS2)
#include <windows.h>
#endif

#include "nsIComponentManager.h" 
#include "nsMsgCompCID.h"
#include "nsIMsgCompFields.h"
#include "nsIMsgSend.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nscore.h"
#include "nsIMsgAccountManager.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsISmtpService.h"
#include "nsISmtpUrl.h"
#include "nsIUrlListener.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsIFileLocator.h"
#include "nsCRT.h"
#include "prmem.h"
#include "nsFileStream.h"

#ifdef XP_PC
#ifdef XP_OS2
#define XPCOM_DLL  "xpcom.dll"
#define PREF_DLL   "xppref.dll"
#else
#define XPCOM_DLL  "xpcom32.dll"
#define PREF_DLL   "xppref32.dll"
#endif
#define APPSHELL_DLL "appshell.dll"
#define MIME_DLL "mime.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define PREF_DLL   "libpref"MOZ_DLL_SUFFIX
#define APPSHELL_DLL "libnsappshell"MOZ_DLL_SUFFIX
#define MIME_DLL "libmime"MOZ_DLL_SUFFIX
#endif
#endif

#ifndef TPATH_LEN
#define TPATH_LEN 1024
#endif


/////////////////////////////////////////////////////////////////////////////////
// Define keys for all of the interfaces we are going to require for this test
/////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
static NS_DEFINE_CID(kMsgComposeCID, NS_MSGCOMPOSE_CID); 
static NS_DEFINE_IID(kIMsgCompFieldsIID, NS_IMSGCOMPFIELDS_IID); 
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_IID(kIMsgSendIID, NS_IMSGSEND_IID); 
static NS_DEFINE_CID(kMsgSendCID, NS_MSGSEND_CID); 
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

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

#if defined(XP_UNIX) || defined(XP_BEOS)
  char *tPath = getenv("TEMP");
  if (!tPath)
    PL_strncpy(retPath, "/tmp/", TPATH_LEN);
  else
    PL_strncpy(retPath, tPath, TPATH_LEN);
#endif

#ifdef XP_MAC
  PL_strncpy(retPath, "", 1024);
#endif

  return retPath;
}

nsIMsgIdentity *
GetHackIdentity()
{
nsresult rv;

  NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                  NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) 
  {
    printf("Failure on Mail Session Init!\n");
    return nsnull;
  }  

  nsCOMPtr<nsIMsgAccount> account;
  rv = accountManager->GetDefaultAccount(getter_AddRefs(account));
  if (NS_FAILED(rv)) return nsnull;

  nsCOMPtr<nsISupportsArray> identities;
  rv = account->GetIdentities(getter_AddRefs(identities));

  nsCOMPtr<nsIMsgIdentity>        identity = nsnull;
  rv = identities->QueryElementAt(0, NS_GET_IID(nsIMsgIdentity),
                                  (void **)getter_AddRefs(identity));
  if (NS_FAILED(rv)) 
  {
    printf("Failure getting Identity!\n");
    return nsnull;
  }  

  return identity;
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

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
PRBool keepOnRunning = PR_TRUE;
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
NS_IMPL_ISUPPORTS(SendOperationListener, NS_GET_IID(nsIMsgSendListener));

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
  
nsresult
SendOperationListener::OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                                     nsIFileSpec *returnFileSpec)
{
  if (NS_SUCCEEDED(aStatus))
  {
    printf("Save Mail File Operation Completed Successfully!\n");
  }
  else
  {
    printf("Save Mail File Operation FAILED!\n");
  }

  keepOnRunning = PR_FALSE;
  printf("Exit code = [%d]\n", aStatus);
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

char *email = {"\
Message-ID: <375FF6D0.3070505@netscape.com>\
\nDate: Thu, 10 Jun 1999 13:33:04 -0500\
\nFrom: %s\
\nUser-Agent: Mozilla 5.0 [en] (Win95; I)\
\nX-Accept-Language: en\
\nMIME-Version: 1.0\
\nTo: rhp@netscape.com\
\nSubject: %s\
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
  nsIMsgIdentity *identity = GetHackIdentity();
  const char *to = "rhp@netscape.com";
  char  *aEmail = nsnull;
  PRUnichar  *aFullName = nsnull;
  char  addr[256];
  char  subject[256];
  char  emailMessage[2048];
  
  identity->GetEmail(&aEmail);
  identity->GetFullName(&aFullName);
  PR_snprintf(addr, sizeof(addr), "%s <%s>", aFullName, aEmail);
  PR_FREEIF(aEmail);
  PR_FREEIF(aFullName);      
  PR_snprintf(subject, sizeof(subject), "Spam from: %s", addr);
  
  PR_snprintf(emailMessage, sizeof(emailMessage), email, to, subject);

  outFile = new nsOutputFileStream(*mailFile);
	if (! outFile->is_open()) 
  {
	    return NS_ERROR_FAILURE;
  }

  outFile->write(emailMessage, PL_strlen(emailMessage));
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
  nsIFileSpec       *mailIFile = nsnull;

  nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kEventQueueCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kPrefCID, nsnull, nsnull, PREF_DLL, PR_TRUE, PR_TRUE);
  nsComponentManager::RegisterComponent(kFileLocatorCID,  NULL, NS_FILELOCATOR_CONTRACTID, APPSHELL_DLL, PR_FALSE, PR_FALSE);

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
    printf("Failed to get the prefs service...\n");
    exit(rv);
  }
  if (NS_FAILED(prefs->ReadUserPrefs()))
  {
    printf("Failed on reading user prefs!\n");
    exit(rv);
  }


  printf("Creating temp mail file...\n");
  mailFile = nsMsgCreateTempFileSpec("mailTest.eml");
  if (NS_FAILED(WriteTempMailFile(mailFile)))
  {
    printf("Failed to create temp mail file!\n");
    return 0;
  }

  NS_NewFileSpecWithSpec(*mailFile, &mailIFile);
	if (!mailIFile)
    return NS_ERROR_FAILURE;

  rv = nsComponentManager::CreateInstance(kMsgSendCID, NULL, kIMsgSendIID, (void **) &pMsgSend); 
  if (NS_SUCCEEDED(rv) && pMsgSend) 
  { 
    printf("We succesfully obtained a nsIMsgSend interface....\n");    
    rv = nsComponentManager::CreateInstance(kMsgCompFieldsCID, NULL, kIMsgCompFieldsIID, 
                                             (void **) &pMsgCompFields); 
    if (NS_SUCCEEDED(rv) && pMsgCompFields)
    { 
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

      pMsgCompFields->SetTo(NS_ConvertASCIItoUCS2("rhp@netscape.com").GetUnicode());
      pMsgSend->SendMessageFile(GetHackIdentity(),  // identity...
                          pMsgCompFields, // nsIMsgCompFields                  *fields,
                          mailIFile,             // nsFileSpec                        *sendFileSpec,
                          PR_TRUE,              // PRBool                            deleteSendFileOnCompletion,
						              PR_FALSE,             // PRBool                            digest_p,
						              nsIMsgSend::nsMsgDeliverNow,      // nsMsgDeliverMode                  mode,
                  			  nsnull, // nsIMessage *msgToReplace
                          tArray, 2);              // nsIMsgSendListener array
    }    
  }

#if defined(XP_PC) && !defined(XP_OS2)
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
  pMsgSend->Release(); 
  pMsgCompFields->Release(); 


  return 0; 
}

