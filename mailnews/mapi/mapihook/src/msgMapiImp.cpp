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
 * The Original Code is Mozilla
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *                 Krishna Mohan Khandrika (kkhandrika@netscape.com)
 *                 Rajiv Dayal (rdayal@netscape.com)
 *                 David Bienvenu <bienvenu@nventure.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifdef MOZ_LOGGING
// this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include <mapidefs.h>
#include <mapi.h>
#include "msgMapi.h"
#include "msgMapiImp.h"
#include "msgMapiFactory.h"
#include "msgMapiMain.h"

#include "nsMsgCompFields.h"
#include "msgMapiHook.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsMsgCompCID.h"
#include "nsIMsgDatabase.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgHdr.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgFolder.h"
#include "nsIMsgImapMailFolder.h"
#include <time.h>
#include "nsIInputStream.h"
#include "nsIMsgHeaderParser.h"
#include "nsILineInputStream.h"
#include "nsISeekableStream.h"
#include "nsIFileSpec.h"
#include "nsFileSpec.h"
#include "nsILocalFile.h"
#include "nsIFileStreams.h"
#include "nsNetCID.h"

PRLogModuleInfo *MAPI;

CMapiImp::CMapiImp()
: m_cRef(1)
{
    m_Lock = PR_NewLock();
  if (!MAPI)
    MAPI = PR_NewLogModule("MAPI");
}

CMapiImp::~CMapiImp() 
{ 
    if (m_Lock)
        PR_DestroyLock(m_Lock);
}

STDMETHODIMP CMapiImp::QueryInterface(const IID& aIid, void** aPpv)
{    
    if (aIid == IID_IUnknown)
    {
        *aPpv = static_cast<nsIMapi*>(this); 
    }
    else if (aIid == IID_nsIMapi)
    {
        *aPpv = static_cast<nsIMapi*>(this);
    }
    else
    {
        *aPpv = nsnull;
        return E_NOINTERFACE;
    }

    reinterpret_cast<IUnknown*>(*aPpv)->AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CMapiImp::AddRef()
{
    return PR_AtomicIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CMapiImp::Release() 
{
    PRInt32 temp;
    temp = PR_AtomicDecrement(&m_cRef);
    if (m_cRef == 0)
    {
        delete this;
        return 0;
    }

    return temp;
}

STDMETHODIMP CMapiImp::IsValid()
{
    return S_OK;
}

STDMETHODIMP CMapiImp::IsValidSession(unsigned long aSession)
{
    nsMAPIConfiguration *pConfig = nsMAPIConfiguration::GetMAPIConfiguration();
    if (pConfig && pConfig->IsSessionValid(aSession))
        return S_OK;

    return E_FAIL;
}

STDMETHODIMP CMapiImp::Initialize()
{
    HRESULT hr = E_FAIL;

    if (!m_Lock)
        return E_FAIL;

    PR_Lock(m_Lock);

    // Initialize MAPI Configuration

    nsMAPIConfiguration *pConfig = nsMAPIConfiguration::GetMAPIConfiguration();
    if (pConfig != nsnull)
        if (nsMapiHook::Initialize())
            hr = S_OK;

    PR_Unlock(m_Lock);

    return hr;
}

STDMETHODIMP CMapiImp::Login(unsigned long aUIArg, LOGIN_PW_TYPE aLogin, LOGIN_PW_TYPE aPassWord,
                unsigned long aFlags, unsigned long *aSessionId)
{
    HRESULT hr = E_FAIL;
     PRBool bNewSession = PR_FALSE;
    char *id_key = nsnull;

    PR_LOG(MAPI, PR_LOG_DEBUG, ("CMapiImp::Login using flags %d\n", aFlags));
    if (aFlags & MAPI_NEW_SESSION)
        bNewSession = PR_TRUE;

    // Check For Profile Name
    if (aLogin != nsnull && aLogin[0] != '\0')
    {
        if (nsMapiHook::VerifyUserName(aLogin, &id_key) == PR_FALSE)
        {
            *aSessionId = MAPI_E_LOGIN_FAILURE;
            PR_LOG(MAPI, PR_LOG_DEBUG, ("CMapiImp::Login failed for username %s\n", aLogin));
            NS_ASSERTION(PR_FALSE, "failed verifying user name");
            return hr;
        }
    }
    else
    {
      // get default account
      nsresult rv;
      nsCOMPtr <nsIMsgAccountManager> accountManager = 
        do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv); 
      NS_ENSURE_SUCCESS(rv,MAPI_E_LOGIN_FAILURE);
      nsCOMPtr <nsIMsgAccount> account;
      nsCOMPtr <nsIMsgIdentity> identity;
       rv = accountManager->GetDefaultAccount(getter_AddRefs(account));
      NS_ENSURE_SUCCESS(rv,MAPI_E_LOGIN_FAILURE);
      account->GetDefaultIdentity(getter_AddRefs(identity));
      NS_ENSURE_SUCCESS(rv,MAPI_E_LOGIN_FAILURE);
      identity->GetKey(&id_key);

    }

    // finally register(create) the session.
    PRUint32 nSession_Id;
    PRInt16 nResult = 0;

    nsMAPIConfiguration *pConfig = nsMAPIConfiguration::GetMAPIConfiguration();
    if (pConfig != nsnull)
        nResult = pConfig->RegisterSession(aUIArg, aLogin, aPassWord,
                                           (aFlags & MAPI_FORCE_DOWNLOAD), bNewSession,
                                           &nSession_Id, id_key);
    switch (nResult)
    {
        case -1 :
        {
            *aSessionId = MAPI_E_TOO_MANY_SESSIONS;
            return hr;
        }
        case 0 :
        {
            *aSessionId = MAPI_E_INSUFFICIENT_MEMORY;
            return hr;
        }
        default :
        {
            *aSessionId = nSession_Id;
            PR_LOG(MAPI, PR_LOG_DEBUG, ("CMapiImp::Login succeeded\n"));
            break;
        }
    }

    return S_OK;
}

STDMETHODIMP CMapiImp::SendMail( unsigned long aSession, lpnsMapiMessage aMessage,
     short aRecipCount, lpnsMapiRecipDesc aRecips , short aFileCount, lpnsMapiFileDesc aFiles , 
     unsigned long aFlags, unsigned long aReserved)
{
    nsresult rv = NS_OK ;

    PR_LOG(MAPI, PR_LOG_DEBUG, ("CMapiImp::SendMail using flags %d\n", aFlags));
    // Assign the pointers in the aMessage struct to the array of Recips and Files
    // recieved here from MS COM. These are used in BlindSendMail and ShowCompWin fns 
    aMessage->lpRecips = aRecips ;
    aMessage->lpFiles = aFiles ;

    PR_LOG(MAPI, PR_LOG_DEBUG, ("CMapiImp::SendMail flags=%x subject: %s sender: %s\n", 
      aFlags, (char *) aMessage->lpszSubject, (aMessage->lpOriginator) ? aMessage->lpOriginator->lpszAddress : ""));

    /** create nsIMsgCompFields obj and populate it **/
    nsCOMPtr<nsIMsgCompFields> pCompFields = do_CreateInstance(NS_MSGCOMPFIELDS_CONTRACTID, &rv) ;
    if (NS_FAILED(rv) || (!pCompFields) ) return MAPI_E_INSUFFICIENT_MEMORY ;

    if (aFlags & MAPI_UNICODE)
        rv = nsMapiHook::PopulateCompFields(aMessage, pCompFields) ;
    else
        rv = nsMapiHook::PopulateCompFieldsWithConversion(aMessage, pCompFields) ;
    
    if (NS_SUCCEEDED (rv))
    {
        // see flag to see if UI needs to be brought up
        if (!(aFlags & MAPI_DIALOG))
        {
            rv = nsMapiHook::BlindSendMail(aSession, pCompFields);
        }
        else
        {
            rv = nsMapiHook::ShowComposerWindow(aSession, pCompFields);
        }
    }
    
    return nsMAPIConfiguration::GetMAPIErrorFromNSError (rv) ;
}


STDMETHODIMP CMapiImp::SendDocuments( unsigned long aSession, LPTSTR aDelimChar,
                            LPTSTR aFilePaths, LPTSTR aFileNames, ULONG aFlags)
{
    nsresult rv = NS_OK ;

    PR_LOG(MAPI, PR_LOG_DEBUG, ("CMapiImp::SendDocument using flags %d\n", aFlags));
    /** create nsIMsgCompFields obj and populate it **/
    nsCOMPtr<nsIMsgCompFields> pCompFields = do_CreateInstance(NS_MSGCOMPFIELDS_CONTRACTID, &rv) ;
    if (NS_FAILED(rv) || (!pCompFields) ) return MAPI_E_INSUFFICIENT_MEMORY ;

    if (aFilePaths)
    {
        rv = nsMapiHook::PopulateCompFieldsForSendDocs(pCompFields, aFlags, aDelimChar, aFilePaths) ;
    }

    if (NS_SUCCEEDED (rv)) 
        rv = nsMapiHook::ShowComposerWindow(aSession, pCompFields);
    else
      PR_LOG(MAPI, PR_LOG_DEBUG, ("CMapiImp::SendDocument error rv = %lx, paths = %s names = %s\n", rv, aFilePaths, aFileNames));

    return nsMAPIConfiguration::GetMAPIErrorFromNSError (rv) ;
}

nsresult CMapiImp::GetDefaultInbox(nsIMsgFolder **inboxFolder)
{
  // get default account
  nsresult rv;
  nsCOMPtr <nsIMsgAccountManager> accountManager = 
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIMsgAccount> account;
  rv = accountManager->GetDefaultAccount(getter_AddRefs(account));
  NS_ENSURE_SUCCESS(rv,rv);

  // get incoming server
  nsCOMPtr <nsIMsgIncomingServer> server;
  rv = account->GetIncomingServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString type;
  rv = server->GetType(getter_Copies(type));
  NS_ENSURE_SUCCESS(rv,rv);

  // we only care about imap and pop3
  if (!(nsCRT::strcmp(type.get(), "imap")) ||
      !(nsCRT::strcmp(type.get(), "pop3"))) 
  {
    // imap and pop3 account should have an Inbox
    nsCOMPtr<nsIMsgFolder> rootMsgFolder;
    rv = server->GetRootMsgFolder(getter_AddRefs(rootMsgFolder));
    NS_ENSURE_SUCCESS(rv,rv);
 
    if (!rootMsgFolder)
      return NS_ERROR_FAILURE;
 
    PRUint32 numFolders = 0;
    rv = rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1, &numFolders, inboxFolder);
    NS_ENSURE_SUCCESS(rv,rv);
 
    if (!*inboxFolder)
     return NS_ERROR_FAILURE;
 
  }
  return NS_OK;
}

//*****************************************************************************
// Encapsulate the XP DB stuff required to enumerate messages

class MsgMapiListContext 
{
public:
  MsgMapiListContext () {}
  ~MsgMapiListContext ();
  
  nsresult OpenDatabase (nsIMsgFolder *folder);
  
  nsMsgKey GetNext ();
  nsresult MarkRead (nsMsgKey key, PRBool read);
  
  lpnsMapiMessage GetMessage (nsMsgKey, unsigned long flFlags);
  PRBool IsIMAPHost(void);
  PRBool DeleteMessage(nsMsgKey key);
  
protected:
  
  char *ConvertDateToMapiFormat (time_t);
  char *ConvertBodyToMapiFormat (nsIMsgDBHdr *hdr);
  void ConvertRecipientsToMapiFormat (nsIMsgHeaderParser *parser,  
                          const char *ourRecips, lpnsMapiRecipDesc mapiRecips,
                          int mapiRecipClass);
  
  nsCOMPtr <nsIMsgFolder> m_folder;
  nsCOMPtr <nsIMsgDatabase> m_db;
  nsCOMPtr <nsISimpleEnumerator> m_msgEnumerator;
};


LONG CMapiImp::InitContext(unsigned long session, MsgMapiListContext **listContext)
{
  nsMAPIConfiguration * pMapiConfig = nsMAPIConfiguration::GetMAPIConfiguration() ;
  if (!pMapiConfig)
    return NS_ERROR_FAILURE ;  // get the singelton obj
  *listContext = (MsgMapiListContext *) pMapiConfig->GetMapiListContext(session);
  // This is the first message
  if (!*listContext)
  {
    nsCOMPtr <nsIMsgFolder> inboxFolder;
    nsresult rv = GetDefaultInbox(getter_AddRefs(inboxFolder));
    if (NS_FAILED(rv))
    {
      NS_ASSERTION(PR_FALSE, "in init context, no inbox");
      return(MAPI_E_NO_MESSAGES);
    }

    *listContext = new MsgMapiListContext;
    if (!*listContext)
      return MAPI_E_INSUFFICIENT_MEMORY;

    rv = (*listContext)->OpenDatabase(inboxFolder);
    if (NS_FAILED(rv))
    {
      pMapiConfig->SetMapiListContext(session, NULL);
      delete *listContext;
      NS_ASSERTION(PR_FALSE, "in init context, unable to open db");
      return MAPI_E_NO_MESSAGES;
    }
    else
      pMapiConfig->SetMapiListContext(session, *listContext);
  }
  return SUCCESS_SUCCESS;
}

STDMETHODIMP CMapiImp::FindNext(unsigned long aSession, unsigned long ulUIParam, LPTSTR lpszMessageType,
                              LPTSTR lpszSeedMessageID, unsigned long flFlags, unsigned long ulReserved,
                              unsigned char lpszMessageID[64])

{
  //
  // If this is true, then this is the first call to this FindNext function
  // and we should start the enumeration operation.
  //

  *lpszMessageID = '\0';
  nsMAPIConfiguration * pMapiConfig = nsMAPIConfiguration::GetMAPIConfiguration() ;
  if (!pMapiConfig) 
  {
    NS_ASSERTION(PR_FALSE, "failed to get config in findnext");
    return NS_ERROR_FAILURE ;  // get the singelton obj
  }
  MsgMapiListContext *listContext;
  LONG ret = InitContext(aSession, &listContext);
  if (ret != SUCCESS_SUCCESS)
  {
    NS_ASSERTION(PR_FALSE, "init context failed");
    return ret;
  }
  NS_ASSERTION(listContext, "initContext returned null context");
  if (listContext)
  {
//    NS_ASSERTION(PR_FALSE, "find next init context succeeded");
    nsMsgKey nextKey = listContext->GetNext();
    if (nextKey == nsMsgKey_None)
    {
      pMapiConfig->SetMapiListContext(aSession, NULL);
      delete listContext;
      return(MAPI_E_NO_MESSAGES);
    }

//    TRACE("MAPI: ProcessMAPIFindNext() Found message id = %d\n", nextKey);

    sprintf((char *) lpszMessageID, "%d", nextKey);
  }

  PR_LOG(MAPI, PR_LOG_DEBUG, ("CMapiImp::FindNext returning key %s\n", (char *) lpszMessageID));
  return(SUCCESS_SUCCESS);
}

STDMETHODIMP CMapiImp::ReadMail(unsigned long aSession, unsigned long ulUIParam, LPTSTR lpszMessageID,
                              unsigned long flFlags, unsigned long ulReserved, lpnsMapiMessage *lppMessage)
{
  PRInt32 irv;
  nsCAutoString keyString((char *) lpszMessageID);
  PR_LOG(MAPI, PR_LOG_DEBUG, ("CMapiImp::ReadMail asking for key %s\n", (char *) lpszMessageID));
  nsMsgKey msgKey = keyString.ToInteger(&irv);
  if (irv)
  {
    NS_ASSERTION(PR_FALSE, "invalid lpszMessageID");
    return MAPI_E_INVALID_MESSAGE;
  }
  MsgMapiListContext *listContext;
  LONG ret = InitContext(aSession, &listContext);
  if (ret != SUCCESS_SUCCESS)
  {
    NS_ASSERTION(PR_FALSE, "init context failed in ReadMail");
    return ret;
  }
  *lppMessage = listContext->GetMessage (msgKey, flFlags);
  NS_ASSERTION(*lppMessage, "get message failed");

  return (*lppMessage) ? SUCCESS_SUCCESS : E_FAIL;
}


STDMETHODIMP CMapiImp::DeleteMail(unsigned long aSession, unsigned long ulUIParam, LPTSTR lpszMessageID,
                              unsigned long flFlags, unsigned long ulReserved)
{
  PRInt32 irv;
  nsCAutoString keyString((char *) lpszMessageID);
  nsMsgKey msgKey = keyString.ToInteger(&irv);
  if (irv)
    return SUCCESS_SUCCESS;
  MsgMapiListContext *listContext;
  LONG ret = InitContext(aSession, &listContext);
  if (ret != SUCCESS_SUCCESS)
    return ret;
  return (listContext->DeleteMessage(msgKey)) ? SUCCESS_SUCCESS : MAPI_E_INVALID_MESSAGE;
}

STDMETHODIMP CMapiImp::SaveMail(unsigned long aSession, unsigned long ulUIParam,  lpnsMapiMessage lppMessage,
                              unsigned long flFlags, unsigned long ulReserved, LPTSTR lpszMessageID)
{
  MsgMapiListContext *listContext;
  LONG ret = InitContext(aSession, &listContext);
  if (ret != SUCCESS_SUCCESS)
    return ret;
  return S_OK;
}


STDMETHODIMP CMapiImp::Logoff (unsigned long aSession)
{
    nsMAPIConfiguration *pConfig = nsMAPIConfiguration::GetMAPIConfiguration();

    if (pConfig->UnRegisterSession((PRUint32)aSession))
        return S_OK;

    return E_FAIL;
}

STDMETHODIMP CMapiImp::CleanUp()
{
    nsMapiHook::CleanUp();
    return S_OK;
}


#define           MAX_NAME_LEN    256


MsgMapiListContext::~MsgMapiListContext ()
{
  if (m_db)
    m_db->Close(PR_FALSE);
}


nsresult MsgMapiListContext::OpenDatabase (nsIMsgFolder *folder)
{
  nsresult dbErr = NS_ERROR_FAILURE;
  if (folder)
  {
    m_folder = folder;
    dbErr = folder->GetMsgDatabase(nsnull, getter_AddRefs(m_db));
    if (m_db)
      dbErr = m_db->EnumerateMessages(getter_AddRefs(m_msgEnumerator));
  }
  return dbErr;
}

PRBool 
MsgMapiListContext::IsIMAPHost(void)
{
  if (!m_folder) 
    return FALSE;
  nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(m_folder);

  return imapFolder != nsnull;
}

nsMsgKey MsgMapiListContext::GetNext ()
{
  nsMsgKey key = nsMsgKey_None;
  PRBool    keepTrying = TRUE;
  
//  NS_ASSERTION (m_msgEnumerator && m_db, "need enumerator and db");
  if (m_msgEnumerator && m_db)
  {
    
    do 
    {
      keepTrying = FALSE;  
      nsCOMPtr <nsISupports> hdrISupports;
      nsCOMPtr <nsIMsgDBHdr> msgHdr;
      if (NS_SUCCEEDED(m_msgEnumerator->GetNext(getter_AddRefs(hdrISupports))) && hdrISupports)
      {
        msgHdr = do_QueryInterface(hdrISupports);
        msgHdr->GetMessageKey(&key);
        
        // Check here for IMAP message...if not, just return...
        if (!IsIMAPHost())
          return key;
        
        // If this is an IMAP message, we have to make sure we have a valid
        // body to work with.
        PRUint32  flags = 0;
        
        (void) msgHdr->GetFlags(&flags);
        if (flags & MSG_FLAG_OFFLINE) 
          return key;
        
        // Ok, if we get here, we have an IMAP message without a body!
        // We need to keep trying by calling the GetNext member recursively...
        keepTrying = TRUE;
      }
    } while (keepTrying);
  }
  
  return key;
}


nsresult MsgMapiListContext::MarkRead (nsMsgKey key, PRBool read)
{
  nsresult err = NS_ERROR_FAILURE;
  NS_ASSERTION(m_db, "no db");
  if (m_db)
    err = m_db->MarkRead (key, read, nsnull);
  return err;
}


lpnsMapiMessage MsgMapiListContext::GetMessage (nsMsgKey key, unsigned long flFlags)
{
  lpnsMapiMessage message = (lpnsMapiMessage) CoTaskMemAlloc (sizeof(nsMapiMessage));
  memset(message, 0, sizeof(nsMapiMessage));
  if (message)
  {
    nsXPIDLCString subject;
    nsXPIDLCString author;
    nsCOMPtr <nsIMsgDBHdr> msgHdr;

    nsresult rv = m_db->GetMsgHdrForKey (key, getter_AddRefs(msgHdr));
    if (msgHdr)
    {
      msgHdr->GetSubject (getter_Copies(subject));
      message->lpszSubject = (char *) CoTaskMemAlloc(subject.Length() + 1);
      strcpy((char *) message->lpszSubject, subject.get());
      PRUint32 date;
      (void) msgHdr->GetDateInSeconds(&date);
      message->lpszDateReceived = ConvertDateToMapiFormat (date);
      
      // Pull out the flags info
      // anything to do with MAPI_SENT? Since we're only reading the Inbox, I guess not
      PRUint32 ourFlags;
      (void) msgHdr->GetFlags(&ourFlags);
      if (!(ourFlags & MSG_FLAG_READ))
        message->flFlags |= MAPI_UNREAD;
      if (ourFlags & (MSG_FLAG_MDN_REPORT_NEEDED | MSG_FLAG_MDN_REPORT_SENT))
        message->flFlags |= MAPI_RECEIPT_REQUESTED;
      
      nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID);
      if (!parser)
        return nsnull;
      // Pull out the author/originator info
      message->lpOriginator = (lpnsMapiRecipDesc) CoTaskMemAlloc (sizeof(nsMapiRecipDesc));
      memset(message->lpOriginator, 0, sizeof(nsMapiRecipDesc));
      if (message->lpOriginator)
      {
        msgHdr->GetAuthor (getter_Copies(author));
        ConvertRecipientsToMapiFormat (parser, author.get(), message->lpOriginator, MAPI_ORIG);
      }
      // Pull out the To/CC info
      nsXPIDLCString recipients, ccList;
      msgHdr->GetRecipients(getter_Copies(recipients));
      msgHdr->GetCcList(getter_Copies(ccList));

      PRUint32 numToRecips;
      PRUint32 numCCRecips;
      parser->ParseHeaderAddresses(nsnull, recipients, nsnull, nsnull, &numToRecips);
      parser->ParseHeaderAddresses(nsnull, ccList, nsnull, nsnull, &numCCRecips);

      message->lpRecips = (lpnsMapiRecipDesc) CoTaskMemAlloc ((numToRecips + numCCRecips) * sizeof(MapiRecipDesc));
      memset(message->lpRecips, 0, (numToRecips + numCCRecips) * sizeof(MapiRecipDesc));
      if (message->lpRecips)
      {
        ConvertRecipientsToMapiFormat (parser, recipients, message->lpRecips, MAPI_TO);
        ConvertRecipientsToMapiFormat (parser, ccList, &message->lpRecips[numToRecips], MAPI_CC);
      }
  
      PR_LOG(MAPI, PR_LOG_DEBUG, ("MsgMapiListContext::GetMessage flags=%x subject %s date %s sender %s\n", 
        flFlags, (char *) message->lpszSubject,(char *) message->lpszDateReceived, author.get()) );

      // Convert any body text that we have locally
      if (!(flFlags & MAPI_ENVELOPE_ONLY))
        message->lpszNoteText = (char *) ConvertBodyToMapiFormat (msgHdr);
      
    }
    if (! (flFlags & (MAPI_PEEK | MAPI_ENVELOPE_ONLY)))
      m_db->MarkRead(key, PR_TRUE, nsnull);
  }
  return message;
}


char *MsgMapiListContext::ConvertDateToMapiFormat (time_t ourTime)
{
  char *date = (char*) CoTaskMemAlloc(32);
  if (date)
  {
    // MAPI time format is YYYY/MM/DD HH:MM
    // Note that we're not using XP_StrfTime because that localizes the time format,
    // and the way I read the MAPI spec is that their format is canonical, not localized.
    struct tm *local = localtime (&ourTime);
    if (local)
      strftime (date, 32, "%Y/%m/%d %I:%M", local); //use %H if hours should be 24 hour format
  }
  return date;
}


void MsgMapiListContext::ConvertRecipientsToMapiFormat (nsIMsgHeaderParser *parser, const char *recipients, lpnsMapiRecipDesc mapiRecips,
                                                        int mapiRecipClass)
{
  char *names = nsnull;
  char *addresses = nsnull;
  
  if (!parser)
    return ;
  PRUint32 numAddresses = 0;
  parser->ParseHeaderAddresses(nsnull, recipients, &names, &addresses, &numAddresses);
  
  if (numAddresses > 0)
  {
    char *walkNames = names;
    char *walkAddresses = addresses;
    for (int i = 0; i < numAddresses; i++)
    {
      if (walkNames)
      {
        if (*walkNames)
        {
          mapiRecips[i].lpszName = (char *) CoTaskMemAlloc(strlen(walkNames) + 1);
          if (mapiRecips[i].lpszName )
            strcpy((char *) mapiRecips[i].lpszName, walkNames);
        }
        walkNames += strlen (walkNames) + 1;
      }
      
      if (walkAddresses)
      {
        if (*walkAddresses)
        {
          mapiRecips[i].lpszAddress = (char *) CoTaskMemAlloc(strlen(walkAddresses) + 1);
          if (mapiRecips[i].lpszAddress)
            strcpy((char *) mapiRecips[i].lpszAddress, walkAddresses);
        }
        walkAddresses += strlen (walkAddresses) + 1;
      }
      
      mapiRecips[i].ulRecipClass = mapiRecipClass;
    }
  }
  
  PR_Free(names);
  PR_Free(addresses);
}


char *MsgMapiListContext::ConvertBodyToMapiFormat (nsIMsgDBHdr *hdr)
{
  const int kBufLen = 64000; // I guess we only return the first 64K of a message.
  int bytesUsed = 0;
#define EMPTY_MESSAGE_LINE(buf) (buf[0] == nsCRT::CR || buf[0] == nsCRT::LF || buf[0] == '\0')

  nsCOMPtr <nsIMsgFolder> folder;
  hdr->GetFolder(getter_AddRefs(folder));
  if (!folder)
    return nsnull;

  nsCOMPtr <nsIInputStream> inputStream;
  nsCOMPtr <nsIFileSpec> fileSpec;
  folder->GetPath(getter_AddRefs(fileSpec));
  nsFileSpec realSpec;
  fileSpec->GetFileSpec(&realSpec);
  nsCOMPtr <nsILocalFile> localFile;
  NS_FileSpecToIFile(&realSpec, getter_AddRefs(localFile));

  nsresult rv;
  nsCOMPtr<nsIFileInputStream> fileStream = do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = fileStream->Init(localFile,  PR_RDONLY, 0664, PR_FALSE);  //just have to read the messages
  inputStream = do_QueryInterface(fileStream);

  if (inputStream)
  {
    nsCOMPtr <nsILineInputStream> fileLineStream = do_QueryInterface(inputStream);
    if (!fileLineStream)
      return nsnull;
    // ### really want to skip past headers...
    PRUint32 messageOffset;
    PRUint32 lineCount;
    hdr->GetMessageOffset(&messageOffset);
    hdr->GetLineCount(&lineCount);
    nsCOMPtr <nsISeekableStream> seekableStream = do_QueryInterface(inputStream);
    seekableStream->Seek(PR_SEEK_SET, messageOffset);
    PRBool hasMore = PR_TRUE;
    nsCAutoString curLine;
    PRBool inMessageBody = PR_FALSE;
    nsresult rv = NS_OK;
    while (hasMore) // advance past message headers
    {
      nsresult rv = fileLineStream->ReadLine(curLine, &hasMore);
      if (NS_FAILED(rv) || EMPTY_MESSAGE_LINE(curLine))
        break;
    }
    PRUint32 msgSize;
    hdr->GetMessageSize(&msgSize);
    if (msgSize > kBufLen)
      msgSize = kBufLen - 1;
    // this is too big, since it includes the msg hdr size...oh well
    char *body = (char*) CoTaskMemAlloc (msgSize + 1);

    if (!body)
      return nsnull;
    PRInt32 bytesCopied = 0;
    for (hasMore = TRUE; lineCount > 0 && hasMore && NS_SUCCEEDED(rv); lineCount--)
    {
      rv = fileLineStream->ReadLine(curLine, &hasMore);
      if (NS_FAILED(rv))
        break;
      curLine.Append(CRLF);
      // make sure we have room left
      if (bytesCopied + curLine.Length() < msgSize)
      {
        strcpy(body + bytesCopied, curLine.get());
        bytesCopied += curLine.Length();
      }
    }
    PR_LOG(MAPI, PR_LOG_DEBUG, ("ConvertBodyToMapiFormat size=%x allocated size %x body = %100.100s\n", 
        bytesCopied, msgSize + 1, (char *) body) );
    body[bytesCopied] = '\0';   // rhp - fix last line garbage...
    return body;
  }
  return nsnull;
}


//*****************************************************************************
// MSGMAPI API implementation



static void msg_FreeMAPIFile(lpMapiFileDesc f)
{
  if (f)
  {
    CoTaskMemFree(f->lpszPathName);
    CoTaskMemFree(f->lpszFileName);
  }
}

static void msg_FreeMAPIRecipient(lpMapiRecipDesc rd)
{
  if (rd)
  {
    if (rd->lpszName)
      CoTaskMemFree(rd->lpszName);
    if (rd->lpszAddress)
      CoTaskMemFree(rd->lpszAddress);
    // CoTaskMemFree(rd->lpEntryID);  
  }
}

extern "C" void MSG_FreeMapiMessage (lpMapiMessage msg)
{
  ULONG i;
  
  if (msg)
  {
    CoTaskMemFree(msg->lpszSubject);
    CoTaskMemFree(msg->lpszNoteText);
    CoTaskMemFree(msg->lpszMessageType);
    CoTaskMemFree(msg->lpszDateReceived);
    CoTaskMemFree(msg->lpszConversationID);
    
    if (msg->lpOriginator)
      msg_FreeMAPIRecipient(msg->lpOriginator);
    
    for (i=0; i<msg->nRecipCount; i++)
      if (&(msg->lpRecips[i]) != nsnull)
        msg_FreeMAPIRecipient(&(msg->lpRecips[i]));
      
      CoTaskMemFree(msg->lpRecips);
      
      for (i=0; i<msg->nFileCount; i++)
        if (&(msg->lpFiles[i]) != nsnull)
          msg_FreeMAPIFile(&(msg->lpFiles[i]));
        
      CoTaskMemFree(msg->lpFiles);
      
      CoTaskMemFree(msg);
  }
}


extern "C" PRBool MsgMarkMapiMessageRead (nsIMsgFolder *folder, nsMsgKey key, PRBool read)
{
  PRBool success = FALSE;
  MsgMapiListContext *context = new MsgMapiListContext();
  if (context)
  {
    if (NS_SUCCEEDED(context->OpenDatabase(folder)))
    {
      if (NS_SUCCEEDED(context->MarkRead (key, read)))
        success = TRUE;
    }
    delete context;
  }
  return success;
}

PRBool 
MsgMapiListContext::DeleteMessage(nsMsgKey key)
{
  if (!m_db)
    return FALSE;
  
  nsMsgKeyArray messageKeys;      
  messageKeys.InsertAt(0, key);
  
  if ( !IsIMAPHost() )
  {
    return NS_SUCCEEDED((m_db->DeleteMessages(&messageKeys, nsnull)));
  }
  else
  {
    return FALSE;
#if 0 
    if ( m_folder->GetIMAPFolderInfoMail() )
    {
      (m_folder->GetIMAPFolderInfoMail())->DeleteSpecifiedMessages(pane, messageKeys, nsMsgKey_None);
      m_db->DeleteMessage(key, nsnull, FALSE);
      return TRUE;
    }
    else
    {
      return FALSE;
    }
#endif
  }
}

/* Return TRUE on success, FALSE on failure */
extern "C" PRBool MSG_DeleteMapiMessage(nsIMsgFolder *folder, nsMsgKey key)
{
  PRBool success = FALSE;
  MsgMapiListContext *context = new MsgMapiListContext();
  if (context) 
  {
    if (NS_SUCCEEDED(context->OpenDatabase(folder)))
    {
      success = context->DeleteMessage(key);
    }
    
    delete context;
  }
  
  return success;
}

