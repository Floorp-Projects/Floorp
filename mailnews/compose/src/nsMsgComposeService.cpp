/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Jean-Francois Ducarroz <ducarroz@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *  Seth Spitzer <sspitzer@netscape.com> 
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build (sorry this breaks the PCH) */
#endif

#include "nsMsgComposeService.h"
#include "nsMsgCompCID.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsIMsgIdentity.h"
#include "nsISmtpUrl.h"
#include "nsIURI.h"
#include "nsMsgI18N.h"
#include "nsIMsgDraft.h"
#include "nsIMsgComposeParams.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsEscape.h"

#include "nsIDocShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIXULWindow.h"
#include "nsIWindowMediator.h"

#include "nsMsgBaseCID.h"
#include "nsIMsgAccountManager.h"

#ifdef MSGCOMP_TRACE_PERFORMANCE
#include "prlog.h"
#include "nsIPref.h"
#include "nsIMsgHdr.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#endif

#ifdef NS_DEBUG
static PRBool _just_to_be_sure_we_create_only_on_compose_service_ = PR_FALSE;
#endif

#define DEFAULT_CHROME  "chrome://messenger/content/messengercompose/messengercompose.xul"

#ifdef MSGCOMP_TRACE_PERFORMANCE
static PRLogModuleInfo *MsgComposeLogModule = nsnull;

static PRUint32 GetMessageSizeFromURI(const char * originalMsgURI)
{
  PRUint32 msgSize = 0;

  if (originalMsgURI && *originalMsgURI)
  {
    nsCOMPtr <nsIMsgDBHdr> originalMsgHdr;
    GetMsgDBHdrFromURI(originalMsgURI, getter_AddRefs(originalMsgHdr));
    if (originalMsgHdr)
    originalMsgHdr->GetMessageSize(&msgSize);
  }
  
  return msgSize;
}
#endif

nsMsgComposeService::nsMsgComposeService()
{
#ifdef NS_DEBUG
  NS_ASSERTION(!_just_to_be_sure_we_create_only_on_compose_service_, "You cannot create several message compose service!");
  _just_to_be_sure_we_create_only_on_compose_service_ = PR_TRUE;
#endif
  
  NS_INIT_REFCNT();

// Defaulting the value of mLogComposePerformance to FALSE to prevent logging.
  mLogComposePerformance = PR_FALSE;
#ifdef MSGCOMP_TRACE_PERFORMANCE
  if (!MsgComposeLogModule)
      MsgComposeLogModule = PR_NewLogModule("msgcompose");

  mStartTime = PR_IntervalNow();
  mPreviousTime = mStartTime;
#endif

  mMaxRecycledWindows = 0;
  mCachedWindows = nsnull;
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS2(nsMsgComposeService, nsIMsgComposeService, nsICmdLineHandler);

nsMsgComposeService::~nsMsgComposeService()
{
  if (mCachedWindows)
    delete [] mCachedWindows;
}

/* the following Init is to initialize the mLogComposePerformance variable */

nsresult nsMsgComposeService::Init()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
  if (!prefs)
    return NS_ERROR_FAILURE;

  rv = prefs->GetIntPref("mail.compose.max_recycled_windows", &mMaxRecycledWindows);
  if (mMaxRecycledWindows > 0 && !mCachedWindows)
  {
    mCachedWindows = new nsMsgCachedWindowInfo[mMaxRecycledWindows];
    if (!mCachedWindows)
      mMaxRecycledWindows = 0;
  }
  
  rv = prefs->GetBoolPref("mailnews.logComposePerformance", &mLogComposePerformance);

  return rv;
}


// Utility function to open a message compose window and pass an nsIMsgComposeParams parameter to it.
nsresult nsMsgComposeService::OpenWindow(const char *chrome, nsIMsgComposeParams *params)
{
  nsresult rv;
  
  //if we have a cached window for the default chrome, try to reuse it...
  if (chrome == nsnull || nsCRT::strcasecmp(chrome, DEFAULT_CHROME) == 0)
  {
    MSG_ComposeFormat format;
    params->GetFormat(&format);

    nsCOMPtr<nsIMsgIdentity> identity;
    params->GetIdentity(getter_AddRefs(identity));

    PRBool composeHTML = PR_TRUE;
    rv = DetermineComposeHTML(identity, format, &composeHTML);
    if (NS_SUCCEEDED(rv))
    {
      PRInt32 i;
      for (i = 0; i < mMaxRecycledWindows; i ++)
      {
        if (mCachedWindows[i].window && (mCachedWindows[i].htmlCompose == composeHTML) && mCachedWindows[i].listener)
        {
          mCachedWindows[i].listener->OnReopen(params);
          rv = ShowCachedComposeWindow(mCachedWindows[i].window, PR_TRUE);
          if (NS_SUCCEEDED(rv))
          {
            mCachedWindows[i].Clear();
            return NS_OK;
          }
        }
      }
    }
  }
      
  //Else, create a new one...
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  if (!wwatch)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupportsInterfacePointer> msgParamsWrapper =
    do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  msgParamsWrapper->SetData(params);
  msgParamsWrapper->SetDataIID(&NS_GET_IID(nsIMsgComposeParams));

  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = wwatch->OpenWindow(0, chrome && *chrome ? chrome : DEFAULT_CHROME,
                 "_blank", "chrome,dialog=no,all", msgParamsWrapper,
                 getter_AddRefs(newWindow));

  return rv;
}

NS_IMETHODIMP
nsMsgComposeService::DetermineComposeHTML(nsIMsgIdentity *aIdentity, MSG_ComposeFormat aFormat, PRBool *aComposeHTML)
{
  NS_ENSURE_ARG_POINTER(aComposeHTML);

  *aComposeHTML = PR_TRUE;
  switch (aFormat) {
    case nsIMsgCompFormat::HTML: 
      *aComposeHTML = PR_TRUE;					
      break;
    case nsIMsgCompFormat::PlainText:
      *aComposeHTML = PR_FALSE;					
      break;
      
    default:
      nsCOMPtr<nsIMsgIdentity> identity = aIdentity;
      if (!identity)
      {
        nsresult rv;
        nsCOMPtr<nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv) && accountManager)
        {
          nsCOMPtr<nsIMsgAccount> defaultAccount;
          rv = accountManager->GetDefaultAccount(getter_AddRefs(defaultAccount));
          if (NS_SUCCEEDED(rv) && defaultAccount)
            defaultAccount->GetDefaultIdentity(getter_AddRefs(identity));
        }
      }
      
      if (identity)
      {
        identity->GetComposeHtml(aComposeHTML);
        if (aFormat == nsIMsgCompFormat::OppositeOfDefault)
          *aComposeHTML = !*aComposeHTML;
      }
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeService::OpenComposeWindow(const char *msgComposeWindowURL, const char *originalMsgURI,
  MSG_ComposeType type, MSG_ComposeFormat format, nsIMsgIdentity * identity, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;
  
  /* Actually, the only way to implement forward inline is to simulate a template message. 
     Maybe one day when we will have more time we can change that
  */
  if (type == nsIMsgCompType::ForwardInline || type == nsIMsgCompType::Draft || type == nsIMsgCompType::Template)
  {
    nsCOMPtr<nsIMsgDraft> pMsgDraft (do_CreateInstance(NS_MSGDRAFT_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv) && pMsgDraft)
    {
      nsCAutoString uriToOpen(originalMsgURI);
      uriToOpen.Append("?fetchCompleteMessage=true"); 

      switch(type)
      {
        case nsIMsgCompType::ForwardInline:
            rv = pMsgDraft->OpenDraftMsg(uriToOpen.get(), nsnull, identity, PR_TRUE, aMsgWindow);
          break;
        case nsIMsgCompType::Draft:
            rv = pMsgDraft->OpenDraftMsg(uriToOpen.get(), nsnull, identity, PR_FALSE, aMsgWindow);
          break;
        case nsIMsgCompType::Template:
            rv = pMsgDraft->OpenEditorTemplate(uriToOpen.get(), nsnull, identity, aMsgWindow);
          break;
      }
    }
    return rv;
  }

  nsCOMPtr<nsIMsgComposeParams> pMsgComposeParams (do_CreateInstance(NS_MSGCOMPOSEPARAMS_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv) && pMsgComposeParams)
  {
    nsCOMPtr<nsIMsgCompFields> pMsgCompFields (do_CreateInstance(NS_MSGCOMPFIELDS_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv) && pMsgCompFields)
    {
      pMsgComposeParams->SetType(type);
      pMsgComposeParams->SetFormat(format);
      pMsgComposeParams->SetIdentity(identity);
      
      if (originalMsgURI && *originalMsgURI)
        if (type == nsIMsgCompType::NewsPost) 
        {
          nsCAutoString newsURI(originalMsgURI);
          nsCAutoString group;
          nsCAutoString host;
        
          PRInt32 slashpos = newsURI.FindChar('/');
          if (slashpos > 0 )
          {
            // uri is "host/group"
            newsURI.Left(host, slashpos);
            newsURI.Right(group, newsURI.Length() - slashpos - 1);
          }
          else
            group = originalMsgURI;

          pMsgCompFields->SetNewsgroups(group.get());
          pMsgCompFields->SetNewshost(host.get());
      }
      else
        pMsgComposeParams->SetOriginalMsgURI(originalMsgURI);
        
      pMsgComposeParams->SetComposeFields(pMsgCompFields);

      if(mLogComposePerformance)
      {
#ifdef MSGCOMP_TRACE_PERFORMANCE
        // ducarroz, properly fix this in the case of new message (not a reply)
        if (type != nsIMsgCompType::NewsPost) {
          char buff[256];
          sprintf(buff, "Start opening the window, message size = %d", GetMessageSizeFromURI(originalMsgURI));
          TimeStamp(buff, PR_TRUE);
        }
#endif
      }//end if(mLogComposePerformance)

      rv = OpenWindow(msgComposeWindowURL, pMsgComposeParams);
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgComposeService::OpenComposeWindowWithURI(const char * aMsgComposeWindowURL, nsIURI * aURI)
{
  nsresult rv = NS_OK;
  if (aURI)
  { 
    nsCOMPtr<nsIMailtoUrl> aMailtoUrl;
    rv = aURI->QueryInterface(NS_GET_IID(nsIMailtoUrl), getter_AddRefs(aMailtoUrl));
    if (NS_SUCCEEDED(rv))
    {
       PRBool aPlainText = PR_FALSE;
       nsXPIDLCString aToPart;
       nsXPIDLCString aCcPart;
       nsXPIDLCString aBccPart;
       nsXPIDLCString aSubjectPart;
       nsXPIDLCString aBodyPart;
       nsXPIDLCString aNewsgroup;

       aMailtoUrl->GetMessageContents(getter_Copies(aToPart), getter_Copies(aCcPart), 
                                    getter_Copies(aBccPart), nsnull /* from part */,
                                    nsnull /* follow */, nsnull /* organization */, 
                                    nsnull /* reply to part */, getter_Copies(aSubjectPart),
                                    getter_Copies(aBodyPart), nsnull /* html part */, 
                                    nsnull /* a ref part */, nsnull /* attachment part */,
                                    nsnull /* priority */, getter_Copies(aNewsgroup), nsnull /* host */,
                                    &aPlainText);

       MSG_ComposeFormat format = nsIMsgCompFormat::Default;
       if (aPlainText)
         format = nsIMsgCompFormat::PlainText;

      nsCOMPtr<nsIMsgComposeParams> pMsgComposeParams (do_CreateInstance(NS_MSGCOMPOSEPARAMS_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv) && pMsgComposeParams)
      {
        pMsgComposeParams->SetType(nsIMsgCompType::MailToUrl);
        pMsgComposeParams->SetFormat(format);


        nsCOMPtr<nsIMsgCompFields> pMsgCompFields (do_CreateInstance(NS_MSGCOMPFIELDS_CONTRACTID, &rv));
        if (pMsgCompFields)
        {
       //ugghh more conversion work!!!!
          pMsgCompFields->SetTo(NS_ConvertUTF8toUCS2(aToPart).get());
          pMsgCompFields->SetCc(NS_ConvertUTF8toUCS2(aCcPart).get());
          pMsgCompFields->SetBcc(NS_ConvertUTF8toUCS2(aBccPart).get());
          pMsgCompFields->SetNewsgroups(aNewsgroup);
          pMsgCompFields->SetSubject(NS_ConvertUTF8toUCS2(aSubjectPart).get());
          pMsgCompFields->SetBody(NS_ConvertUTF8toUCS2(aBodyPart).get());

          pMsgComposeParams->SetComposeFields(pMsgCompFields);

          rv = OpenComposeWindowWithParams(aMsgComposeWindowURL, pMsgComposeParams);
        }
      }
    }
  }

  return rv;
}

nsresult nsMsgComposeService::OpenComposeWindowWithValues(const char *msgComposeWindowURL,
                              MSG_ComposeType type,
                              MSG_ComposeFormat format,
                              const PRUnichar *to,
                              const PRUnichar *cc,
                              const PRUnichar *bcc,
                              const char *newsgroups,
                              const PRUnichar *subject,
                              const PRUnichar *body,
                              const char *attachment,
                              nsIMsgIdentity *identity)
{
  NS_ASSERTION(0, "nsMsgComposeService::OpenComposeWindowWithValues is not supported anymore, use OpenComposeWindowWithParams instead\n");

  nsresult rv;
  nsCOMPtr<nsIMsgCompFields> pCompFields (do_CreateInstance(NS_MSGCOMPFIELDS_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv) && pCompFields)
  {
    if (to)         {pCompFields->SetTo(to);}
    if (cc)         {pCompFields->SetCc(cc);}
    if (bcc)        {pCompFields->SetBcc(bcc);}
    if (newsgroups) {pCompFields->SetNewsgroups(newsgroups);}
    if (subject)    {pCompFields->SetSubject(subject);}
//    if (attachment) {pCompFields->SetAttachments(attachment);}
    if (body)       {pCompFields->SetBody(body);}
  
    rv = OpenComposeWindowWithCompFields(msgComposeWindowURL, type, format, pCompFields, identity);
  }
    
  return rv;
}

nsresult nsMsgComposeService::OpenComposeWindowWithCompFields(const char *msgComposeWindowURL,
                              MSG_ComposeType type,
                              MSG_ComposeFormat format,
                              nsIMsgCompFields *compFields,
                              nsIMsgIdentity *identity)
{
  NS_ASSERTION(0, "nsMsgComposeService::OpenComposeWindowWithCompFields is not supported anymore, use OpenComposeWindowWithParams instead\n");

  nsresult rv;

  nsCOMPtr<nsIMsgComposeParams> pMsgComposeParams (do_CreateInstance(NS_MSGCOMPOSEPARAMS_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv) && pMsgComposeParams)
  {
      pMsgComposeParams->SetType(type);
      pMsgComposeParams->SetFormat(format);
      pMsgComposeParams->SetIdentity(identity);
      pMsgComposeParams->SetComposeFields(compFields);    

      if(mLogComposePerformance)
      {
#ifdef MSGCOMP_TRACE_PERFORMANCE
        TimeStamp("Start opening the window", PR_TRUE);
#endif
      }//end -if (mLogComposePerformance)
      rv = OpenWindow(msgComposeWindowURL, pMsgComposeParams);
  }

  return rv;
}

nsresult nsMsgComposeService::OpenComposeWindowWithParams(const char *msgComposeWindowURL,
                              nsIMsgComposeParams *params)
{
  NS_ENSURE_ARG_POINTER(params);
  if(mLogComposePerformance)
  {
#ifdef MSGCOMP_TRACE_PERFORMANCE
    TimeStamp("Start opening the window", PR_TRUE);
#endif
  }//end - if(mLogComposePerformance)

  return OpenWindow(msgComposeWindowURL, params);
}

NS_IMETHODIMP nsMsgComposeService::InitCompose(nsIDOMWindowInternal *aWindow,
                                          nsIMsgComposeParams *params,
                                          nsIMsgCompose **_retval)
{
  nsresult rv;
  
  nsCOMPtr <nsIMsgCompose> msgCompose = do_CreateInstance(NS_MSGCOMPOSE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
	
  rv = msgCompose->Initialize(aWindow, params);
  NS_ENSURE_SUCCESS(rv,rv);
	
	*_retval = msgCompose;
  NS_IF_ADDREF(*_retval);

 	return rv;
}

/* readonly attribute boolean logComposePerformance; */
NS_IMETHODIMP nsMsgComposeService::GetLogComposePerformance(PRBool *aLogComposePerformance)
{
  *aLogComposePerformance = mLogComposePerformance;
  return NS_OK;
}

NS_IMETHODIMP nsMsgComposeService::TimeStamp(const char * label, PRBool resetTime)
{
  if (!mLogComposePerformance)
    return NS_OK;
  
#ifdef MSGCOMP_TRACE_PERFORMANCE

  PRIntervalTime now;

  if (resetTime)
  {      
    PR_LOG(MsgComposeLogModule, PR_LOG_ALWAYS, ("--------------------\n"));

    mStartTime = PR_IntervalNow();
    mPreviousTime = mStartTime;
    now = mStartTime;
  }
  else
    now = PR_IntervalNow();

  PRIntervalTime totalTime = PR_IntervalToMilliseconds(now - mStartTime);
  PRIntervalTime deltaTime = PR_IntervalToMilliseconds(now - mPreviousTime);

#if defined(DEBUG_ducarroz)
  printf(">>> Time Stamp: [%5d][%5d] - %s\n", totalTime, deltaTime, label);
#endif
  PR_LOG(MsgComposeLogModule, PR_LOG_ALWAYS, ("[%5d][%5d] - %s\n", totalTime, deltaTime, label));

  mPreviousTime = now;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeService::IsCachedWindow(nsIDOMWindowInternal *aCachedWindow, PRBool *aIsCachedWindow)
{
  NS_ENSURE_ARG_POINTER(aCachedWindow);
  NS_ENSURE_ARG_POINTER(aIsCachedWindow);

  PRInt32 i;
  for (i = 0; i < mMaxRecycledWindows; i ++)
    if (mCachedWindows[i].window.get() == aCachedWindow)
    {
      *aIsCachedWindow = PR_TRUE;
      return NS_OK;
    }
    
 *aIsCachedWindow = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeService::CacheWindow(nsIDOMWindowInternal *aWindow, PRBool aComposeHTML, nsIMsgComposeRecyclingListener * aListener)
{
  NS_ENSURE_ARG_POINTER(aWindow);
  NS_ENSURE_ARG_POINTER(aListener);

  nsresult rv;

  PRInt32 i;
  for (i = 0; i < mMaxRecycledWindows; i ++)
    if (!mCachedWindows[i].window)
    {
      rv = ShowCachedComposeWindow(aWindow, PR_FALSE);
      if (NS_SUCCEEDED(rv))
        mCachedWindows[i].Initialize(aWindow, aListener, aComposeHTML);
      
      return rv;
    }
  
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsMsgComposeService::ShowCachedComposeWindow(nsIDOMWindowInternal *aComposeWindow, PRBool aShow)
{
  nsresult rv = NS_OK;

  nsCOMPtr <nsIScriptGlobalObject> globalScript = do_QueryInterface(aComposeWindow, &rv);

  NS_ENSURE_SUCCESS(rv,rv);
  nsCOMPtr <nsIDocShell> docShell;

  rv = globalScript->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIWebShell> webShell = do_QueryInterface(docShell, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIWebShellContainer> webShellContainer;
  rv = webShell->GetContainer(*getter_AddRefs(webShellContainer));
  NS_ENSURE_SUCCESS(rv,rv);
  
  if (webShellContainer) {
    nsCOMPtr <nsIWebShellWindow> webShellWindow = do_QueryInterface(webShellContainer, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    // hide (show) the cached window
    rv = webShellWindow->Show(aShow);
    NS_ENSURE_SUCCESS(rv,rv);

    // remove (add) the window from the mediator, so that it will be removed (added) from the task list
    nsCOMPtr <nsIXULWindow> xulWindow = do_QueryInterface(webShellWindow, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIWindowMediator> windowMediator = 
	         do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    if (aShow)
      rv = windowMediator->RegisterWindow(xulWindow);
    else
      rv = windowMediator->UnregisterWindow(xulWindow);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else {
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}


CMDLINEHANDLER_IMPL(nsMsgComposeService, "-compose", "general.startup.messengercompose", DEFAULT_CHROME,
                    "Start with messenger compose.", NS_MSGCOMPOSESTARTUPHANDLER_CONTRACTID, "Messenger Compose Startup Handler",
                    PR_TRUE, "about:blank", PR_TRUE)

