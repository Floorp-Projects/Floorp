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
 *                 Srilatha Moturi (srilatha@netscape.com)
 *                 Rajiv Dayal (rdayal@netscape.com)
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

#define MAPI_STARTUP_ARG       "/MAPIStartUp"

#define MAPI_STARTUP_ARG       "/MAPIStartUp"

#ifdef MOZ_LOGGING
// this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include <mapidefs.h>
#include <mapi.h>
#include <tchar.h>

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsIPromptService.h"
#include "nsAppShellCIDs.h"
#include "nsIDOMWindowInternal.h"
#include "nsIAppShellService.h"
#include "nsINativeAppSupport.h"
#include "nsICmdLineService.h"
#include "nsIProfileInternal.h"
#include "nsIMsgAccountManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsMsgBaseCID.h"
#include "nsIStringBundle.h"
#include "nsIPref.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsIMsgAttachment.h"
#include "nsIMsgCompFields.h"
#include "nsIMsgComposeParams.h"
#include "nsIMsgCompose.h"
#include "nsMsgCompCID.h"
#include "nsIMsgSend.h"
#include "nsIProxyObjectManager.h"
#include "nsIMsgComposeService.h"
#include "nsProxiedService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIDirectoryService.h"
#include "nsMsgI18N.h"
#include "msgMapi.h"
#include "msgMapiHook.h"
#include "msgMapiSupport.h"
#include "msgMapiMain.h"
#include "nsNetUtil.h"

extern PRLogModuleInfo *MAPI;


static NS_DEFINE_CID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);

class nsMAPISendListener : public nsIMsgSendListener
{
public:

    virtual ~nsMAPISendListener() { }

    // nsISupports interface
    NS_DECL_ISUPPORTS

    /* void OnStartSending (in string aMsgID, in PRUint32 aMsgSize); */
    NS_IMETHOD OnStartSending(const char *aMsgID, PRUint32 aMsgSize) { return NS_OK; }

    /* void OnProgress (in string aMsgID, in PRUint32 aProgress, in PRUint32 aProgressMax); */
    NS_IMETHOD OnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax) { return NS_OK;}

    /* void OnStatus (in string aMsgID, in wstring aMsg); */
    NS_IMETHOD OnStatus(const char *aMsgID, const PRUnichar *aMsg) { return NS_OK;}

    /* void OnStopSending (in string aMsgID, in nsresult aStatus, in wstring aMsg, in nsIFileSpec returnFileSpec); */
    NS_IMETHOD OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                           nsIFileSpec *returnFileSpec) {
        PR_CEnterMonitor(this);
        PR_CNotifyAll(this);
        m_done = PR_TRUE;
        PR_CExitMonitor(this);
        return NS_OK ;
    }

	/* void OnSendNotPerformed */
	NS_IMETHOD OnSendNotPerformed(const char *aMsgID, nsresult aStatus)
	{
		return OnStopSending(aMsgID, aStatus, nsnull, nsnull) ;
	}
 
    /* void OnGetDraftFolderURI (); */
    NS_IMETHOD OnGetDraftFolderURI(const char *aFolderURI) {return NS_OK;}

    static nsresult CreateMAPISendListener( nsIMsgSendListener **ppListener);

    PRBool IsDone() { return m_done ; }

protected :
    nsMAPISendListener() {
        m_done = PR_FALSE;
    }

    PRBool          m_done;
};


NS_IMPL_THREADSAFE_ISUPPORTS1(nsMAPISendListener, nsIMsgSendListener)

nsresult nsMAPISendListener::CreateMAPISendListener( nsIMsgSendListener **ppListener)
{
    NS_ENSURE_ARG_POINTER(ppListener) ;

    *ppListener = new nsMAPISendListener();
    if (! *ppListener)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*ppListener);
    return NS_OK;
}

PRBool nsMapiHook::isMapiService = PR_FALSE;

PRBool nsMapiHook::Initialize()
{
    nsresult rv;
    nsCOMPtr<nsINativeAppSupport> native;

    nsCOMPtr<nsICmdLineService> cmdLineArgs(do_GetService(kCmdLineServiceCID, &rv));
    if (NS_FAILED(rv)) return PR_FALSE;

    nsCOMPtr<nsIAppShellService> appShell (do_GetService( "@mozilla.org/appshell/appShellService;1", &rv));
    if (NS_FAILED(rv)) return PR_FALSE;

    rv = appShell->GetNativeAppSupport( getter_AddRefs( native ));
    if (NS_FAILED(rv)) return PR_FALSE;

    rv = native->EnsureProfile(cmdLineArgs);
    if (NS_FAILED(rv)) return PR_FALSE;

    return PR_TRUE;
}

void nsMapiHook::CleanUp()
{
    // This routine will be fully implemented in future
    // to cleanup mapi related stuff inside mozilla code.
}

PRBool nsMapiHook::DisplayLoginDialog(PRBool aLogin, PRUnichar **aUsername, 
                      PRUnichar **aPassword)
{
    nsresult rv;
    PRBool btnResult = PR_FALSE;

    nsCOMPtr<nsIAppShellService> appShell(do_GetService( "@mozilla.org/appshell/appShellService;1", &rv));
    if (NS_FAILED(rv) || !appShell) return PR_FALSE;
   
    nsCOMPtr<nsIPromptService> dlgService(do_GetService("@mozilla.org/embedcomp/prompt-service;1", &rv));
    if (NS_SUCCEEDED(rv) && dlgService)
    {
        nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
        if (NS_FAILED(rv) || !bundleService) return PR_FALSE;

        nsCOMPtr<nsIStringBundle> bundle;
        rv = bundleService->CreateBundle(MAPI_PROPERTIES_CHROME, getter_AddRefs(bundle));
        if (NS_FAILED(rv) || !bundle) return PR_FALSE;

        nsCOMPtr<nsIStringBundle> brandBundle;
        rv = bundleService->CreateBundle(
                        "chrome://global/locale/brand.properties",
                        getter_AddRefs(brandBundle));
        if (NS_FAILED(rv)) return PR_FALSE;

        nsXPIDLString brandName;
        rv = brandBundle->GetStringFromName(
                           NS_LITERAL_STRING("brandShortName").get(),
                           getter_Copies(brandName));
        if (NS_FAILED(rv)) return PR_FALSE;
        
        nsXPIDLString loginTitle;
        const PRUnichar *brandStrings[] = { brandName.get() };
        NS_NAMED_LITERAL_STRING(loginTitlePropertyTag, "loginTitle");
        const PRUnichar *dTitlePropertyTag = loginTitlePropertyTag.get();
        rv = bundle->FormatStringFromName(dTitlePropertyTag, brandStrings, 1,
                                          getter_Copies(loginTitle));
        if (NS_FAILED(rv)) return PR_FALSE;
        
        if (aLogin)
        {
            nsXPIDLString loginText;
            rv = bundle->GetStringFromName(NS_LITERAL_STRING("loginTextwithName").get(),
                                           getter_Copies(loginText));
            if (NS_FAILED(rv) || !loginText) return PR_FALSE;

            rv = dlgService->PromptUsernameAndPassword(nsnull, loginTitle,
                                                       loginText, aUsername, aPassword,
                                                       nsnull, PR_FALSE, &btnResult);
        }
        else
        {
            //nsString loginString; 
            nsXPIDLString loginText;
            const PRUnichar *userNameStrings[] = { *aUsername };

            NS_NAMED_LITERAL_STRING(loginTextPropertyTag, "loginText");
            const PRUnichar *dpropertyTag = loginTextPropertyTag.get();
            rv = bundle->FormatStringFromName(dpropertyTag, userNameStrings, 1,
                                              getter_Copies(loginText));
            if (NS_FAILED(rv)) return PR_FALSE;

            rv = dlgService->PromptPassword(nsnull, loginTitle, loginText,
                                            aPassword, nsnull, PR_FALSE, &btnResult);
        }
    }            

    return btnResult;
}

PRBool nsMapiHook::VerifyUserName(const PRUnichar *aUsername, char **aIdKey)
{
    nsresult rv;

    if (aUsername == nsnull)
        return PR_FALSE;

    nsCOMPtr<nsIMsgAccountManager> accountManager(do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return PR_FALSE;
    nsCOMPtr<nsISupportsArray> identities;
    rv = accountManager->GetAllIdentities(getter_AddRefs(identities));
    if (NS_FAILED(rv)) return PR_FALSE;
    PRUint32 numIndentities;
    identities->Count(&numIndentities);

    for (PRUint32 i = 0; i < numIndentities; i++)
    {
        // convert supports->Identity
        nsCOMPtr<nsISupports> thisSupports;
        rv = identities->GetElementAt(i, getter_AddRefs(thisSupports));
        if (NS_FAILED(rv)) continue;
        nsCOMPtr<nsIMsgIdentity> thisIdentity(do_QueryInterface(thisSupports, &rv));
        if (NS_SUCCEEDED(rv) && thisIdentity)
        {
            nsXPIDLCString email;
            rv = thisIdentity->GetEmail(getter_Copies(email));
            if (NS_FAILED(rv)) continue;

            // get the username from the email and compare with the username
            nsCAutoString aEmail(email.get());
            PRInt32 index = aEmail.FindChar('@');
            if (index != -1)
                aEmail.Truncate(index);

			if (nsDependentString(aUsername) == NS_ConvertASCIItoUCS2(aEmail))  // == overloaded
                return NS_SUCCEEDED(thisIdentity->GetKey(aIdKey));
        }
    }

    return PR_FALSE;
}

PRBool 
nsMapiHook::IsBlindSendAllowed()
{
    PRBool enabled = PR_FALSE;
    PRBool warn = PR_TRUE;
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
    if (prefs) {
        prefs->GetBoolPref(PREF_MAPI_WARN_PRIOR_TO_BLIND_SEND,&warn);
            prefs->GetBoolPref(PREF_MAPI_BLIND_SEND_ENABLED,&enabled);
    } 
    if (!enabled)
        return PR_FALSE;

    if (!warn)
        return PR_TRUE; // Everything is okay.  

    nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
    if (NS_FAILED(rv) || !bundleService) return PR_FALSE;

    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(MAPI_PROPERTIES_CHROME, getter_AddRefs(bundle));
    if (NS_FAILED(rv) || !bundle) return PR_FALSE;

    nsXPIDLString warningMsg;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("mapiBlindSendWarning").get(),
                                        getter_Copies(warningMsg));
    if (NS_FAILED(rv)) return PR_FALSE;
    
	nsXPIDLString dontShowAgainMessage;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("mapiBlindSendDontShowAgain").get(),
                                        getter_Copies(dontShowAgainMessage));
    if (NS_FAILED(rv)) return PR_FALSE;

    nsCOMPtr<nsIPromptService> dlgService(do_GetService("@mozilla.org/embedcomp/prompt-service;1", &rv));
    if (NS_FAILED(rv) || !dlgService) return PR_FALSE;
    
    PRBool continueToWarn = PR_TRUE;
    PRBool okayToContinue = PR_FALSE;
    dlgService->ConfirmCheck(nsnull, nsnull, warningMsg, dontShowAgainMessage, &continueToWarn, &okayToContinue);
        
    if (!continueToWarn && okayToContinue && prefs)
        prefs->SetBoolPref(PREF_MAPI_WARN_PRIOR_TO_BLIND_SEND,PR_FALSE);
    
    return okayToContinue;
        
}

// this is used for Send without UI
nsresult nsMapiHook::BlindSendMail (unsigned long aSession, nsIMsgCompFields * aCompFields) 
{
    nsresult rv = NS_OK ;

    if (!IsBlindSendAllowed())
        return NS_ERROR_FAILURE;

    /** create nsIMsgComposeParams obj and other fields to populate it **/    

    nsCOMPtr<nsIDOMWindowInternal>  hiddenWindow;
    // get parent window
    nsCOMPtr<nsIAppShellService> appService = do_GetService( "@mozilla.org/appshell/appShellService;1", &rv);
    if (NS_FAILED(rv)|| (!appService) ) return rv ;

    rv = appService->GetHiddenDOMWindow(getter_AddRefs(hiddenWindow));
    if ( NS_FAILED(rv) ) return rv ;
    // smtp password and Logged in used IdKey from MapiConfig (session obj)
    nsMAPIConfiguration * pMapiConfig = nsMAPIConfiguration::GetMAPIConfiguration() ;
    if (!pMapiConfig) return NS_ERROR_FAILURE ;  // get the singelton obj
    PRUnichar * password = pMapiConfig->GetPassword(aSession) ;
    // password
    nsCAutoString smtpPassword ;
    smtpPassword.AssignWithConversion (password) ;
    // Id key
    char * MsgIdKey = pMapiConfig->GetIdKey(aSession) ;

    // get the MsgIdentity for the above key using AccountManager
    nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService (NS_MSGACCOUNTMANAGER_CONTRACTID) ;
    if (NS_FAILED(rv) || (!accountManager) ) return rv ;

    nsCOMPtr <nsIMsgIdentity> pMsgId ;
    rv = accountManager->GetIdentity (MsgIdKey, getter_AddRefs(pMsgId)) ;
    if (NS_FAILED(rv) ) return rv ;

    // create a send listener to get back the send status
    nsCOMPtr <nsIMsgSendListener> sendListener ;
    rv = nsMAPISendListener::CreateMAPISendListener(getter_AddRefs(sendListener)) ; 
    if (NS_FAILED(rv) || (!sendListener) ) return rv;

    // create the compose params object 
    nsCOMPtr<nsIMsgComposeParams> pMsgComposeParams (do_CreateInstance(NS_MSGCOMPOSEPARAMS_CONTRACTID, &rv));
    if (NS_FAILED(rv) || (!pMsgComposeParams) ) return rv ;

    // populate the compose params
    pMsgComposeParams->SetType(nsIMsgCompType::New);
    pMsgComposeParams->SetFormat(nsIMsgCompFormat::Default);
    pMsgComposeParams->SetIdentity(pMsgId);
    pMsgComposeParams->SetComposeFields(aCompFields); 
    pMsgComposeParams->SetSendListener(sendListener) ;
    pMsgComposeParams->SetSmtpPassword(smtpPassword.get());

    // create the nsIMsgCompose object to send the object
    nsCOMPtr<nsIMsgCompose> pMsgCompose (do_CreateInstance(NS_MSGCOMPOSE_CONTRACTID, &rv));
    if (NS_FAILED(rv) || (!pMsgCompose) ) return rv ;

    /** initialize nsIMsgCompose, Send the message, wait for send completion response **/

    rv = pMsgCompose->Initialize(hiddenWindow, pMsgComposeParams) ;
    if (NS_FAILED(rv)) return rv ;

    return pMsgCompose->SendMsg(nsIMsgSend::nsMsgDeliverNow, pMsgId, nsnull, nsnull, nsnull) ;
    if (NS_FAILED(rv)) return rv ;

    // assign to interface pointer from nsCOMPtr to facilitate typecast below
    nsIMsgSendListener * pSendListener = sendListener ;  

    // we need to wait here to make sure that we return only after send is completed
    // so we will have a event loop here which will process the events till the Send IsDone.
    nsCOMPtr<nsIEventQueueService> pEventQService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
    nsCOMPtr<nsIEventQueue> eventQueue;
    pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,getter_AddRefs(eventQueue));
    while ( !((nsMAPISendListener *) pSendListener)->IsDone() )
    {
        PR_CEnterMonitor(pSendListener);
        PR_CWait(pSendListener, PR_MicrosecondsToInterval(1000UL));
        PR_CExitMonitor(pSendListener);
        eventQueue->ProcessPendingEvents();
    }

    return rv ;
}

// this is used to populate comp fields with Unicode data
nsresult nsMapiHook::PopulateCompFields(lpnsMapiMessage aMessage, 
                                    nsIMsgCompFields * aCompFields)
{
    nsresult rv = NS_OK ;

    if (aMessage->lpOriginator)
    {
        char * From = aMessage->lpOriginator->lpszAddress ;
        nsAutoString FromStr; FromStr.AssignWithConversion(From);
        aCompFields->SetFrom (FromStr) ;
    }

    nsAutoString To ;
    nsAutoString Cc ; 
    nsAutoString Bcc ;

    NS_NAMED_LITERAL_STRING(Comma, ",") ;

    if (aMessage->lpRecips)
    {
        for (int i=0 ; i < (int) aMessage->nRecipCount ; i++)
        {
            if (aMessage->lpRecips[i].lpszAddress)
            {
                const char *addressWithoutType = aMessage->lpRecips[i].lpszAddress;
                if (!PL_strncasecmp(addressWithoutType, "SMTP:", 5))
                  addressWithoutType += 5;
                switch (aMessage->lpRecips[i].ulRecipClass)
                {
                case MAPI_TO :
                    if (!To.IsEmpty())
                        To += Comma ;
                    To.AppendWithConversion(addressWithoutType) ;
                    break ;

                case MAPI_CC :
                    if (!Cc.IsEmpty())
                        Cc += Comma ;
                    Cc.AppendWithConversion(addressWithoutType) ;
                    break ;

                case MAPI_BCC :
                    if (!Bcc.IsEmpty())
                        Bcc += Comma ;
                    Bcc.AppendWithConversion(addressWithoutType) ; 
                    break ;
                }
            }
        }
    }

    PR_LOG(MAPI, PR_LOG_DEBUG, ("to: %s cc: %s bcc: %s \n", NS_ConvertUCS2toUTF8(To).get(), NS_ConvertUCS2toUTF8(Cc).get(), NS_ConvertUCS2toUTF8(Bcc).get()));
    // set To, Cc, Bcc
    aCompFields->SetTo (To) ;
    aCompFields->SetCc (Cc) ;
    aCompFields->SetBcc (Bcc) ;

    // set subject
    if (aMessage->lpszSubject)
    {
        nsAutoString Subject;

        Subject.AssignWithConversion(aMessage->lpszSubject);
        aCompFields->SetSubject(Subject) ;
    }

    // handle attachments as File URL
    rv = HandleAttachments (aCompFields, aMessage->nFileCount, aMessage->lpFiles, PR_TRUE) ;
    if (NS_FAILED(rv)) return rv ;    

    // set body
    if (aMessage->lpszNoteText)
    {
        nsString Body;
        Body.AssignWithConversion(aMessage->lpszNoteText);
        if (Body.Last() != nsCRT::LF)
          Body.AppendLiteral(CRLF);
        rv = aCompFields->SetBody(Body) ;
    }

#ifdef RAJIV_DEBUG
    // testing what all was set in CompFields
    printf ("To : %S \n", To.get()) ;
    printf ("CC : %S \n", Cc.get() ) ;
    printf ("BCC : %S \n", Bcc.get() ) ;
#endif

    return rv ;

}

nsresult nsMapiHook::HandleAttachments (nsIMsgCompFields * aCompFields, PRInt32 aFileCount, 
                                        lpnsMapiFileDesc aFiles, BOOL aIsUnicode)
{
    nsresult rv = NS_OK ;

    nsCAutoString Attachments ;
    nsCAutoString TempFiles ;

    nsCOMPtr <nsILocalFile> pFile = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID, &rv) ;
    if (NS_FAILED(rv) || (!pFile) ) return rv ;        
    nsCOMPtr <nsILocalFile> pTempDir = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID, &rv) ;
    if (NS_FAILED(rv) || (!pTempDir) ) return rv ;        

    for (int i=0 ; i < aFileCount ; i++)
    {
        PRBool bTempFile = PR_FALSE ;
        if (aFiles[i].lpszPathName)
        {
            // check if attachment exists
            if (aIsUnicode)
                pFile->InitWithPath (nsDependentString(aFiles[i].lpszPathName)); 
            else
                pFile->InitWithNativePath (nsDependentCString((const char*)aFiles[i].lpszPathName)); 

            PRBool bExist ;
            rv = pFile->Exists(&bExist) ;
            if (NS_FAILED(rv) || (!bExist) ) return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST ;

            //Temp Directory
            nsCOMPtr <nsIFile> pTempFileDir;
            NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(pTempFileDir));
            nsCOMPtr <nsILocalFile> pTempDir = do_QueryInterface(pTempFileDir);

            // create a new sub directory called moz_mapi underneath the temp directory
            pTempDir->AppendRelativePath(NS_LITERAL_STRING("moz_mapi"));
            pTempDir->Exists (&bExist) ;
            if (!bExist)
            {
                rv = pTempDir->Create(nsIFile::DIRECTORY_TYPE, 777) ;
                if (NS_FAILED(rv)) return rv ;
            }

            // rename or copy the existing temp file with the real file name
            
            nsAutoString leafName ;
            // convert to Unicode using Platform charset
            // leafName already contains a unicode leafName from lpszPathName. If we were given
            // a value for lpszFileName, use it. Otherwise stick with leafName
            if (aFiles[i].lpszFileName)
            {
                if (aIsUnicode)
                    leafName.Assign(aFiles[i].lpszFileName);
                else
                    ConvertToUnicode(nsMsgI18NFileSystemCharset(), (char *) aFiles[i].lpszFileName, leafName);
            }
            else 
              pFile->GetLeafName (leafName);

            // copy the file to its new location and file name
            pFile->CopyTo(pTempDir, leafName);
            // point pFile to the new location of the attachment
            pFile->InitWithFile(pTempDir); 
            pFile->Append(leafName);

            // create MsgCompose attachment object
            nsCOMPtr<nsIMsgAttachment> attachment = do_CreateInstance(NS_MSGATTACHMENT_CONTRACTID, &rv);
            NS_ENSURE_SUCCESS(rv, rv);
            attachment->SetTemporary(PR_TRUE); // this one is a temp file so set the flag for MsgCompose

            // now set the attachment object
            nsCAutoString pURL ;
            NS_GetURLSpecFromFile(pFile, pURL);
            attachment->SetUrl(pURL.get()) ;

            // add the attachment
            rv = aCompFields->AddAttachment (attachment);
        }
    }
    return rv ;
}


// this is used to convert non Unicode data and then populate comp fields
nsresult nsMapiHook::PopulateCompFieldsWithConversion(lpnsMapiMessage aMessage, 
                                    nsIMsgCompFields * aCompFields)
{
    nsresult rv = NS_OK ;

    if (aMessage->lpOriginator)
    {
        nsAutoString From ;
        From.AssignWithConversion((char *) aMessage->lpOriginator->lpszAddress);
        aCompFields->SetFrom (From) ;
    }

    nsAutoString To ;
    nsAutoString Cc ; 
    nsAutoString Bcc ;

    NS_NAMED_LITERAL_STRING(Comma, ",") ;

    if (aMessage->lpRecips)
    {
        for (int i=0 ; i < (int) aMessage->nRecipCount ; i++)
        {
            if (aMessage->lpRecips[i].lpszAddress)
            {
                const char *addressWithoutType = aMessage->lpRecips[i].lpszAddress;
                if (!PL_strncasecmp(addressWithoutType, "SMTP:", 5))
                  addressWithoutType += 5;

                switch (aMessage->lpRecips[i].ulRecipClass)
                {
                case MAPI_TO :
                    if (!To.IsEmpty())
                        To += Comma ;
                    To.AppendWithConversion (addressWithoutType);
                    break ;

                case MAPI_CC :
                    if (!Cc.IsEmpty())
                        Cc += Comma ;
                    Cc.AppendWithConversion (addressWithoutType);
                    break ;

                case MAPI_BCC :
                    if (!Bcc.IsEmpty())
                        Bcc += Comma ;
                    Bcc.AppendWithConversion (addressWithoutType) ; 
                    break ;
                }
            }
        }
    }
    
    // set To, Cc, Bcc
    aCompFields->SetTo (To) ;
    aCompFields->SetCc (Cc) ;
    aCompFields->SetBcc (Bcc) ;

    PR_LOG(MAPI, PR_LOG_DEBUG, ("to: %s cc: %s bcc: %s \n", NS_ConvertUCS2toUTF8(To).get(), NS_ConvertUCS2toUTF8(Cc).get(), NS_ConvertUCS2toUTF8(Bcc).get()));

    nsCAutoString platformCharSet;
    // set subject
    if (aMessage->lpszSubject)
    {
        nsAutoString Subject ;
        if (platformCharSet.IsEmpty())
            platformCharSet.Assign(nsMsgI18NFileSystemCharset());
        rv = ConvertToUnicode(platformCharSet.get(), (char *) aMessage->lpszSubject, Subject);
        if (NS_FAILED(rv)) return rv ;         
        aCompFields->SetSubject(Subject) ;
    }

    // handle attachments as File URL
    rv = HandleAttachments (aCompFields, aMessage->nFileCount, aMessage->lpFiles, PR_FALSE) ;
    if (NS_FAILED(rv)) return rv ;    

    // set body
    if (aMessage->lpszNoteText)
    {
        nsAutoString Body ;
        if (platformCharSet.IsEmpty())
            platformCharSet.Assign(nsMsgI18NFileSystemCharset());
        rv = ConvertToUnicode(platformCharSet.get(), (char *) aMessage->lpszNoteText, Body);
        if (NS_FAILED(rv)) return rv ;
        if (Body.Last() != nsCRT::LF)
          Body.AppendLiteral(CRLF);
        rv = aCompFields->SetBody(Body) ;
    }

#ifdef RAJIV_DEBUG
    // testing what all was set in CompFields
    printf ("To : %S \n", To.get()) ;
    printf ("CC : %S \n", Cc.get() ) ;
    printf ("BCC : %S \n", Bcc.get() ) ;
#endif

    return rv ;
}

// this is used to populate the docs as attachments in the Comp fields for Send Documents
nsresult nsMapiHook::PopulateCompFieldsForSendDocs(nsIMsgCompFields * aCompFields, ULONG aFlags, 
                            PRUnichar * aDelimChar, PRUnichar * aFilePaths)
{
    nsAutoString strDelimChars ;
    nsString strFilePaths;
    nsresult rv = NS_OK ;
    PRBool bExist ;

    if (aFlags & MAPI_UNICODE)
    {
        if (aDelimChar)
            strDelimChars.Assign (aDelimChar) ;
        if (aFilePaths)
            strFilePaths.Assign (aFilePaths) ;
    }
    else
    {
        if (aDelimChar)
            strDelimChars.AssignWithConversion ((char*) aDelimChar) ;
        if (aFilePaths)
            strFilePaths.AssignWithConversion ((char *) aFilePaths) ;
    }

    // check for comma in filename 
    if (strDelimChars.Find (",") == kNotFound)  // if comma is not in the delimiter specified by user
    {
        if (strFilePaths.Find(",") != kNotFound) // if comma found in filenames return error
            return NS_ERROR_FILE_INVALID_PATH ;
    }

    nsCString Attachments ;

    // only 1 file is to be sent, no delim specified
    if (strDelimChars.IsEmpty())
        strDelimChars.AssignLiteral(";");

    PRInt32 offset = 0 ;
    PRInt32 FilePathsLen = strFilePaths.Length() ;
    if (FilePathsLen)
    {
        nsAutoString Subject ;

        // multiple files to be sent, delim specified
        nsCOMPtr <nsILocalFile> pFile = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID, &rv) ;
        if (NS_FAILED(rv) || (!pFile) ) return rv ;        

        //Temp Directory
        nsCOMPtr <nsIFile> pTempFileDir;
        NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(pTempFileDir));
        nsCOMPtr <nsILocalFile> pTempDir = do_QueryInterface(pTempFileDir);

        // if not already existing, create another temp dir for mapi within Win temp dir
        // this is windows only so we can do "\\"
        pTempDir->AppendRelativePath (NS_LITERAL_STRING("moz_mapi"));
        pTempDir->Exists(&bExist) ;
        if (!bExist)
        {
            rv = pTempDir->Create(nsIFile::DIRECTORY_TYPE, 777) ;
            if (NS_FAILED(rv)) return rv ;
    }

        PRUnichar * newFilePaths = (PRUnichar *) strFilePaths.get() ;
        while (offset != kNotFound)
        {
            nsString RemainingPaths ;
            RemainingPaths.Assign(newFilePaths) ;
            offset = RemainingPaths.Find (strDelimChars) ;
            if (offset != kNotFound)
            {
                RemainingPaths.SetLength (offset) ;
                if ((offset + strDelimChars.Length()) < FilePathsLen)
                    newFilePaths += offset + strDelimChars.Length() ;
            }

            pFile->InitWithPath (RemainingPaths) ;
            rv = pFile->Exists(&bExist) ;
            if (NS_FAILED(rv) || (!bExist) ) return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST ;

            // filename of the file attachment
            nsAutoString leafName ;
            pFile->GetLeafName (leafName) ;
            if(NS_FAILED(rv) || leafName.IsEmpty()) return rv ;

            if (!Subject.IsEmpty()) 
                Subject.AppendLiteral(", ");
            Subject += leafName;

            // create MsgCompose attachment object
            nsCOMPtr<nsIMsgAttachment> attachment = do_CreateInstance(NS_MSGATTACHMENT_CONTRACTID, &rv);
            NS_ENSURE_SUCCESS(rv, rv);

            nsDependentString fileNameNative(leafName.get());
            rv = pFile->CopyTo(pTempDir, fileNameNative);
            if (NS_FAILED(rv)) return rv ;

            // now turn pTempDir into a full file path to the temp file
            pTempDir->Append(fileNameNative);

            // this one is a temp file so set the flag for MsgCompose
            attachment->SetTemporary(PR_TRUE) ;

            // now set the attachment object
            nsCAutoString pURL ;
            NS_GetURLSpecFromFile(pTempDir, pURL);
            attachment->SetUrl(pURL.get()) ;

            // add the attachment
            rv = aCompFields->AddAttachment (attachment);
            if (NS_FAILED(rv)) return rv ;
        }

        rv = aCompFields->SetBody(Subject) ;
    }

    return rv ;
}

// this used for Send with UI
nsresult nsMapiHook::ShowComposerWindow (unsigned long aSession, nsIMsgCompFields * aCompFields) 
{
    nsresult rv = NS_OK ;

    // create a send listener to get back the send status
    nsCOMPtr <nsIMsgSendListener> sendListener ;
    rv = nsMAPISendListener::CreateMAPISendListener(getter_AddRefs(sendListener)) ; 
    if (NS_FAILED(rv) || (!sendListener) ) return rv ;

    // create the compose params object 
    nsCOMPtr<nsIMsgComposeParams> pMsgComposeParams (do_CreateInstance(NS_MSGCOMPOSEPARAMS_CONTRACTID, &rv));
    if (NS_FAILED(rv) || (!pMsgComposeParams) ) return rv ;

    // populate the compose params
    pMsgComposeParams->SetType(nsIMsgCompType::New);
    pMsgComposeParams->SetFormat(nsIMsgCompFormat::Default);
    pMsgComposeParams->SetComposeFields(aCompFields); 
    pMsgComposeParams->SetSendListener(sendListener) ;

    /** get the nsIMsgComposeService object to open the compose window **/
    nsCOMPtr <nsIMsgComposeService> compService = do_GetService (NS_MSGCOMPOSESERVICE_CONTRACTID) ;
    if (NS_FAILED(rv)|| (!compService) ) return rv ;

    rv = compService->OpenComposeWindowWithParams(nsnull, pMsgComposeParams) ;
    if (NS_FAILED(rv)) return rv ;

    return rv ;
}
