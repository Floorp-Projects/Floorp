/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jean-Francois Ducarroz <ducarroz@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Seth Spitzer <sspitzer@netscape.com>
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build (sorry this breaks the PCH) */
#endif

#include "nsMsgComposeService.h"
#include "nsMsgCompCID.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsXPIDLString.h"
#include "nsIMsgIdentity.h"
#include "nsISmtpUrl.h"
#include "nsIURI.h"
#include "nsMsgI18N.h"
#include "nsIMsgDraft.h"
#include "nsIMsgComposeParams.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsEscape.h"
#include "nsIContentViewer.h"

#include "nsIDocShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIXULWindow.h"
#include "nsIWindowMediator.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsIPref.h"
#include "nsIPrefBranchInternal.h"

#include "nsMsgBaseCID.h"
#include "nsIMsgAccountManager.h"

#ifdef MSGCOMP_TRACE_PERFORMANCE
#include "prlog.h"
#include "nsIMsgHdr.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#endif

// <for functions="HTMLSantinize">
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIContentSink.h"
#include "mozISanitizingSerializer.h"

#ifdef XP_WIN32
#include <windows.h>
#include <shellapi.h>
#include "nsIWidget.h"
#endif

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);
static NS_DEFINE_CID(kNavDTDCID, NS_CNAVDTD_CID);
// </for>

#ifdef NS_DEBUG
static PRBool _just_to_be_sure_we_create_only_one_compose_service_ = PR_FALSE;
#endif

#define DEFAULT_CHROME  "chrome://messenger/content/messengercompose/messengercompose.xul"

#define PREF_MAIL_COMPOSE_MAXRECYCLEDWINDOWS  "mail.compose.max_recycled_windows"

#define MAIL_ROOT_PREF                             "mail."
#define MAILNEWS_ROOT_PREF                         "mailnews."
#define HTMLDOMAINUPDATE_VERSION_PREF_NAME         "global_html_domains.version"
#define HTMLDOMAINUPDATE_DOMAINLIST_PREF_NAME      "global_html_domains"
#define USER_CURRENT_HTMLDOMAINLIST_PREF_NAME      "html_domains"
#define USER_CURRENT_PLAINTEXTDOMAINLIST_PREF_NAME "plaintext_domains"
#define DOMAIN_DELIMITER                           ","

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
  NS_ASSERTION(!_just_to_be_sure_we_create_only_one_compose_service_, "You cannot create several message compose service!");
  _just_to_be_sure_we_create_only_one_compose_service_ = PR_TRUE;
#endif

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

NS_IMPL_ISUPPORTS4(nsMsgComposeService, nsIMsgComposeService, nsIObserver, nsICmdLineHandler, nsISupportsWeakReference)

nsMsgComposeService::~nsMsgComposeService()
{
  if (mCachedWindows)
  {
    DeleteCachedWindows();
    delete [] mCachedWindows;
  }
}

nsresult nsMsgComposeService::Init()
{
  nsresult rv = NS_OK;
  // Register observers

  // Register for quit application and profile change, we will need to clear the cache.
  nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_SUCCEEDED(rv))
  {
    rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
    rv = observerService->AddObserver(this, "profile-do-change", PR_TRUE);
  }

  // Register some pref observer
  nsCOMPtr<nsIPrefBranchInternal> pbi = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (pbi)
    rv = pbi->AddObserver(PREF_MAIL_COMPOSE_MAXRECYCLEDWINDOWS, this, PR_TRUE);

  Reset();

  AddGlobalHtmlDomains();
	
  return rv;
}

void nsMsgComposeService::Reset()
{
  nsresult rv = NS_OK;

  if (mCachedWindows)
  {
    DeleteCachedWindows();
    delete [] mCachedWindows;
    mCachedWindows = nsnull;
    mMaxRecycledWindows = 0;
  }

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if(prefs)
    rv = prefs->GetIntPref(PREF_MAIL_COMPOSE_MAXRECYCLEDWINDOWS, &mMaxRecycledWindows);
  if (NS_SUCCEEDED(rv) && mMaxRecycledWindows > 0)
  {
    mCachedWindows = new nsMsgCachedWindowInfo[mMaxRecycledWindows];
    if (!mCachedWindows)
      mMaxRecycledWindows = 0;
  }
  
  rv = prefs->GetBoolPref("mailnews.logComposePerformance", &mLogComposePerformance);

  return;
}

void nsMsgComposeService::DeleteCachedWindows()
{
  PRInt32 i;
  for (i = 0; i < mMaxRecycledWindows; i ++)
  {
    CloseWindow(mCachedWindows[i].window);
    mCachedWindows[i].Clear();
  }
}

// Utility function to open a message compose window and pass an nsIMsgComposeParams parameter to it.
nsresult nsMsgComposeService::OpenWindow(const char *chrome, nsIMsgComposeParams *params)
{
  nsresult rv;
  
  NS_ENSURE_ARG_POINTER(params);
  
  //Use default identity if no identity has been specified
  nsCOMPtr<nsIMsgIdentity> identity; 
  params->GetIdentity(getter_AddRefs(identity));
  if (!identity)
  {
    GetDefaultIdentity(getter_AddRefs(identity));
    params->SetIdentity(identity);
  }

  //if we have a cached window for the default chrome, try to reuse it...
  if (chrome == nsnull || nsCRT::strcasecmp(chrome, DEFAULT_CHROME) == 0)
  {
    MSG_ComposeFormat format;
    params->GetFormat(&format);

    PRBool composeHTML = PR_TRUE;
    rv = DetermineComposeHTML(identity, format, &composeHTML);
    if (NS_SUCCEEDED(rv))
    {
      PRInt32 i;
      for (i = 0; i < mMaxRecycledWindows; i ++)
      {
        if (mCachedWindows[i].window && (mCachedWindows[i].htmlCompose == composeHTML) && mCachedWindows[i].listener)
        {
          /* We need to save the window pointer as OnReopen will call nsMsgComposeService::InitCompose which will
             clear the cache entry if everything goes well
          */
          nsCOMPtr<nsIDOMWindowInternal> domWindow(mCachedWindows[i].window);
          rv = ShowCachedComposeWindow(domWindow, PR_TRUE);
          if (NS_SUCCEEDED(rv))
          {
            mCachedWindows[i].listener->OnReopen(params);
            return NS_OK;
          }
        }
      }
    }
  }
      
  //Else, create a new one...
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (!wwatch)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupportsInterfacePointer> msgParamsWrapper =
    do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  msgParamsWrapper->SetData(params);
  msgParamsWrapper->SetDataIID(&NS_GET_IID(nsIMsgComposeParams));

  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = wwatch->OpenWindow(0, chrome && *chrome ? chrome : DEFAULT_CHROME,
                 "_blank", "all,chrome,dialog=no,status,toolbar", msgParamsWrapper,
                 getter_AddRefs(newWindow));

  return rv;
}

void nsMsgComposeService::CloseWindow(nsIDOMWindowInternal *domWindow)
{
  if (domWindow)
  {
    nsCOMPtr<nsIDocShell> docshell;
    nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(domWindow));
    if (globalObj)
    {
      nsCOMPtr<nsIDocShellTreeItem> treeItem =
        do_QueryInterface(globalObj->GetDocShell());

      if (treeItem)
      {
        nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
        treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
        if (treeOwner)
        {
          nsCOMPtr<nsIBaseWindow> baseWindow;
          baseWindow = do_QueryInterface(treeOwner);
          if (baseWindow)
            baseWindow->Destroy();
        }
      }
    }
  }
}

NS_IMETHODIMP
nsMsgComposeService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  if (!strcmp(aTopic,"profile-do-change") || !strcmp(aTopic,NS_XPCOM_SHUTDOWN_OBSERVER_ID))
  {
    DeleteCachedWindows();
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID))
  {
    nsDependentString prefName(someData);
    if (prefName.EqualsLiteral(PREF_MAIL_COMPOSE_MAXRECYCLEDWINDOWS))
      Reset();
    return NS_OK;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeService::DetermineComposeHTML(nsIMsgIdentity *aIdentity, MSG_ComposeFormat aFormat, PRBool *aComposeHTML)
{
  NS_ENSURE_ARG_POINTER(aComposeHTML);

  *aComposeHTML = PR_TRUE;
  switch (aFormat)
  {
    case nsIMsgCompFormat::HTML: 
      *aComposeHTML = PR_TRUE;					
      break;
    case nsIMsgCompFormat::PlainText:
      *aComposeHTML = PR_FALSE;					
      break;
      
    default:
      nsCOMPtr<nsIMsgIdentity> identity = aIdentity;
      if (!identity)
        GetDefaultIdentity(getter_AddRefs(identity));
      
      if (identity)
      {
        identity->GetComposeHtml(aComposeHTML);
        if (aFormat == nsIMsgCompFormat::OppositeOfDefault)
          *aComposeHTML = !*aComposeHTML;
      }
      else
      {
        // default identity not found.  Use the mail.html_compose pref to determine
        // message compose type (HTML or PlainText).
        nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
        if (prefs)
        {
          nsresult rv;
          PRBool useHTMLCompose;
          rv = prefs->GetBoolPref(MAIL_ROOT_PREF "html_compose", &useHTMLCompose);
          if (NS_SUCCEEDED(rv))        
            *aComposeHTML = useHTMLCompose;
        }
      }
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMsgComposeService::OpenComposeWindow(const char *msgComposeWindowURL, const char *originalMsgURI,
  MSG_ComposeType type, MSG_ComposeFormat format, nsIMsgIdentity * aIdentity, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;

  nsCOMPtr<nsIMsgIdentity> identity = aIdentity;
  if (!identity)
    GetDefaultIdentity(getter_AddRefs(identity));
  
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
            rv = pMsgDraft->OpenDraftMsg(uriToOpen.get(), originalMsgURI, identity, PR_TRUE, aMsgWindow);
          break;
        case nsIMsgCompType::Draft:
            rv = pMsgDraft->OpenDraftMsg(uriToOpen.get(), nsnull, identity, PR_FALSE, aMsgWindow);
          break;
        case nsIMsgCompType::Template:
            rv = pMsgDraft->OpenEditorTemplate(uriToOpen.get(), identity, aMsgWindow);
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
      {
        if (type == nsIMsgCompType::NewsPost) 
        {
          nsCAutoString newsURI(originalMsgURI);
          nsCAutoString group;
          nsCAutoString host;
          
          PRInt32 slashpos = newsURI.RFindChar('/');
          if (slashpos > 0 )
          {
            // uri is "[s]news://host[:port]/group"
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
      }

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

NS_IMETHODIMP nsMsgComposeService::GetParamsForMailto(nsIURI * aURI, nsIMsgComposeParams ** aParams)
{
  nsresult rv = NS_OK;
  if (aURI)
  { 
    nsCOMPtr<nsIMailtoUrl> aMailtoUrl;
    rv = aURI->QueryInterface(NS_GET_IID(nsIMailtoUrl), getter_AddRefs(aMailtoUrl));
    if (NS_SUCCEEDED(rv))
    {
       MSG_ComposeFormat requestedComposeFormat = nsIMsgCompFormat::Default;
       nsXPIDLCString aToPart;
       nsXPIDLCString aCcPart;
       nsXPIDLCString aBccPart;
       nsXPIDLCString aSubjectPart;
       nsXPIDLCString aBodyPart;
       nsXPIDLCString aNewsgroup;
       nsXPIDLCString aHTMLBodyPart;

       // we are explictly not allowing attachments to be specified in mailto: urls
       // as it's a potential security problem.
       // see bug #99055
       aMailtoUrl->GetMessageContents(getter_Copies(aToPart), getter_Copies(aCcPart), 
                                    getter_Copies(aBccPart), nsnull /* from part */,
                                    nsnull /* follow */, nsnull /* organization */, 
                                    nsnull /* reply to part */, getter_Copies(aSubjectPart),
                                    getter_Copies(aBodyPart), getter_Copies(aHTMLBodyPart) /* html part */, 
                                    nsnull /* a ref part */, nsnull /* attachment part, must always null, see #99055 */,
                                    nsnull /* priority */, getter_Copies(aNewsgroup), nsnull /* host */,
                                    &requestedComposeFormat);

      nsAutoString sanitizedBody;

      // Since there is a buffer for each of the body types ('body', 'html-body') and
      // only one can be used, we give precedence to 'html-body' in the case where
      // both 'body' and 'html-body' are found in the url.
      NS_ConvertUTF8toUTF16 rawBody(aHTMLBodyPart);
      if(rawBody.IsEmpty())
        CopyUTF8toUTF16(aBodyPart, rawBody);

      PRBool composeHTMLFormat;
      DetermineComposeHTML(NULL, requestedComposeFormat, &composeHTMLFormat);
      if (!rawBody.IsEmpty() && composeHTMLFormat)
      {
        //For security reason, we must sanitize the message body before accepting any html...

        // Create a parser
        nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID);

        // Create the appropriate output sink
        nsCOMPtr<nsIContentSink> sink = do_CreateInstance(MOZ_SANITIZINGHTMLSERIALIZER_CONTRACTID);
        
        nsXPIDLCString allowedTags;
        nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
        if (prefs)
          prefs->GetCharPref(MAILNEWS_ROOT_PREF "display.html_sanitizer.allowed_tags", getter_Copies(allowedTags));

        if (parser && sink)
        {
          nsCOMPtr<mozISanitizingHTMLSerializer> sanSink(do_QueryInterface(sink));
          if (sanSink)
          {
            sanSink->Initialize(&sanitizedBody, 0, NS_ConvertASCIItoUTF16(allowedTags));

            parser->SetContentSink(sink);
            nsCOMPtr<nsIDTD> dtd = do_CreateInstance(kNavDTDCID);
            if (dtd)
            {
              parser->RegisterDTD(dtd);

              rv = parser->Parse(rawBody, 0, NS_LITERAL_CSTRING("text/html"), PR_FALSE, PR_TRUE);
              if (NS_FAILED(rv))
                // Something went horribly wrong with parsing for html format
                // in the body.  Set composeHTMLFormat to PR_FALSE so we show the
                // plain text mail compose.
                composeHTMLFormat = PR_FALSE;
            }
          }
        }
      }

      nsCOMPtr<nsIMsgComposeParams> pMsgComposeParams (do_CreateInstance(NS_MSGCOMPOSEPARAMS_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv) && pMsgComposeParams)
      {
        pMsgComposeParams->SetType(nsIMsgCompType::MailToUrl);
        pMsgComposeParams->SetFormat(composeHTMLFormat ? nsIMsgCompFormat::HTML : nsIMsgCompFormat::PlainText);


        nsCOMPtr<nsIMsgCompFields> pMsgCompFields (do_CreateInstance(NS_MSGCOMPFIELDS_CONTRACTID, &rv));
        if (pMsgCompFields)
        {
          //ugghh more conversion work!!!!
          pMsgCompFields->SetTo(NS_ConvertUTF8toUTF16(aToPart));
          pMsgCompFields->SetCc(NS_ConvertUTF8toUTF16(aCcPart));
          pMsgCompFields->SetBcc(NS_ConvertUTF8toUTF16(aBccPart));
          pMsgCompFields->SetNewsgroups(aNewsgroup);
          pMsgCompFields->SetSubject(NS_ConvertUTF8toUTF16(aSubjectPart));
          pMsgCompFields->SetBody(composeHTMLFormat ? sanitizedBody : rawBody);
          pMsgComposeParams->SetComposeFields(pMsgCompFields);

          NS_ADDREF(*aParams = pMsgComposeParams);
          return NS_OK;
        }
      } // if we created msg compose params....
    } // if we had a mailto url
  } // if we had a url...

  // if we got here we must have encountered an error
  *aParams = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgComposeService::OpenComposeWindowWithURI(const char * aMsgComposeWindowURL, nsIURI * aURI)
{
  nsCOMPtr<nsIMsgComposeParams> pMsgComposeParams;
  nsresult rv = GetParamsForMailto(aURI, getter_AddRefs(pMsgComposeParams));
  if (NS_SUCCEEDED(rv))
    rv = OpenComposeWindowWithParams(aMsgComposeWindowURL, pMsgComposeParams);
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

// the following two Windows routines are used to ensure new compose windows
// come to the very front of the desktop, even if the mozilla app is not the application
// with focus. This situation happens when Simple MAPI is used to invoke the compose window.

#ifdef XP_WIN32 
// begin shameless copying from nsNativeAppSupportWin
HWND hwndForComposeDOMWindow( nsISupports *window ) 
{
  nsCOMPtr<nsIScriptGlobalObject> ppScriptGlobalObj( do_QueryInterface(window) );
  if ( !ppScriptGlobalObj )
      return 0;

  nsCOMPtr<nsIBaseWindow> ppBaseWindow =
    do_QueryInterface( ppScriptGlobalObj->GetDocShell() );
  if (!ppBaseWindow) return 0;

  nsCOMPtr<nsIWidget> ppWidget;
  ppBaseWindow->GetMainWidget( getter_AddRefs( ppWidget ) );

  return (HWND)( ppWidget->GetNativeData( NS_NATIVE_WIDGET ) );
}

static void activateComposeWindow( nsIDOMWindowInternal *win ) 
{
  // Try to get native window handle.
  HWND hwnd = hwndForComposeDOMWindow( win );
  if ( hwnd ) 
  {
    // Restore the window if it is minimized.
    if ( ::IsIconic( hwnd ) ) 
      ::ShowWindow( hwnd, SW_RESTORE );
    // Use the OS call, if possible.
    ::SetForegroundWindow( hwnd );
  } else // Use internal method.  
    win->Focus();
}
// end shameless copying from nsNativeAppWinSupport.cpp
#endif

NS_IMETHODIMP nsMsgComposeService::InitCompose(nsIDOMWindowInternal *aWindow,
                                          nsIMsgComposeParams *params,
                                          nsIMsgCompose **_retval)
{
  nsresult rv;
  
  /* we need to remove the window from the cache
  */
  PRInt32 i;
  for (i = 0; i < mMaxRecycledWindows; i ++)
    if (mCachedWindows[i].window == aWindow)
    {
      mCachedWindows[i].Clear();
      break;
    }

  nsCOMPtr <nsIMsgCompose> msgCompose = do_CreateInstance(NS_MSGCOMPOSE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
	
  rv = msgCompose->Initialize(aWindow, params);
  NS_ENSURE_SUCCESS(rv,rv);

#ifdef XP_WIN32
  // on windows, ensure the compose window comes up to the front
  activateComposeWindow(aWindow);
#endif

  NS_IF_ADDREF(*_retval = msgCompose);
 	return rv;
}

NS_IMETHODIMP
nsMsgComposeService::GetDefaultIdentity(nsIMsgIdentity **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  nsresult rv;
  nsCOMPtr<nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && accountManager)
  {
    nsCOMPtr<nsIMsgAccount> defaultAccount;
    rv = accountManager->GetDefaultAccount(getter_AddRefs(defaultAccount));
    if (NS_SUCCEEDED(rv) && defaultAccount)
      defaultAccount->GetDefaultIdentity(_retval);
  }
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
    PR_LOG(MsgComposeLogModule, PR_LOG_ALWAYS, ("\n[process]: [totalTime][deltaTime]\n--------------------\n"));

    mStartTime = PR_IntervalNow();
    mPreviousTime = mStartTime;
    now = mStartTime;
  }
  else
    now = PR_IntervalNow();

  PRIntervalTime totalTime = PR_IntervalToMilliseconds(now - mStartTime);
  PRIntervalTime deltaTime = PR_IntervalToMilliseconds(now - mPreviousTime);

#if defined(DEBUG_ducarroz)
  printf("XXX Time Stamp: [%5d][%5d] - %s\n", totalTime, deltaTime, label);
#endif
  PR_LOG(MsgComposeLogModule, PR_LOG_ALWAYS, ("[%3.2f][%3.2f] - %s\n",
((double)totalTime/1000.0) + 0.005, ((double)deltaTime/1000.0) + 0.005, label));

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
  PRInt32 sameTypeId = -1;
  PRInt32 oppositeTypeId = -1;
  
  for (i = 0; i < mMaxRecycledWindows; i ++)
  {
    if (!mCachedWindows[i].window)
    {
      rv = ShowCachedComposeWindow(aWindow, PR_FALSE);
      if (NS_SUCCEEDED(rv))
        mCachedWindows[i].Initialize(aWindow, aListener, aComposeHTML);
      
      return rv;
    }
    else
      if (mCachedWindows[i].htmlCompose == aComposeHTML)
      {
        if (sameTypeId == -1)
          sameTypeId = i;
      }
      else
      {
        if (oppositeTypeId == -1)
          oppositeTypeId = i;
      }
  }
  
  /* Looks like the cache is full. In the case we try to cache a type (html or plain text) of compose window which is not
     already cached, we should replace an opposite one with this new one. That would allow users to be able to take advantage
     of the cached compose window when they switch from one type to another one
  */
  if (sameTypeId == -1 && oppositeTypeId != -1)
  {
    CloseWindow(mCachedWindows[oppositeTypeId].window);
    mCachedWindows[oppositeTypeId].Clear();
    
    rv = ShowCachedComposeWindow(aWindow, PR_FALSE);
    if (NS_SUCCEEDED(rv))
      mCachedWindows[oppositeTypeId].Initialize(aWindow, aListener, aComposeHTML);
    
    return rv;
  }
  
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsMsgComposeService::ShowCachedComposeWindow(nsIDOMWindowInternal *aComposeWindow, PRBool aShow)
{
  nsresult rv = NS_OK;

  nsCOMPtr <nsIScriptGlobalObject> globalScript = do_QueryInterface(aComposeWindow, &rv);

  NS_ENSURE_SUCCESS(rv,rv);

  nsIDocShell *docShell = globalScript->GetDocShell();

  nsCOMPtr <nsIWebShell> webShell = do_QueryInterface(docShell, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIWebShellContainer> webShellContainer;
  rv = webShell->GetContainer(*getter_AddRefs(webShellContainer));
  NS_ENSURE_SUCCESS(rv,rv);
  
  if (webShellContainer) {

    // the window need to be sticky before we hide it.
    nsCOMPtr<nsIContentViewer> contentViewer;
    rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
    NS_ENSURE_SUCCESS(rv,rv);
    
    rv = contentViewer->SetSticky(!aShow);
    NS_ENSURE_SUCCESS(rv,rv);

    // disable (enable) the cached window
    nsCOMPtr <nsIWebShellWindow> webShellWindow = do_QueryInterface(webShellContainer, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIDocShellTreeItem>  treeItem(do_QueryInterface(docShell, &rv));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    rv = treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIBaseWindow> baseWindow;
    baseWindow = do_QueryInterface(treeOwner, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    baseWindow->SetEnabled(aShow);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr <nsIXULWindow> xulWindow = do_QueryInterface(webShellWindow, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIWindowMediator> windowMediator = 
	         do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    // if showing, reinstate the window with the mediator
    if (aShow) {
      rv = windowMediator->RegisterWindow(xulWindow);
      NS_ENSURE_SUCCESS(rv,rv);
    }

    // hide (show) the cached window
    rv = webShellWindow->Show(aShow);
    NS_ENSURE_SUCCESS(rv,rv);

    // if hiding, remove the window from the mediator,
    // so that it will be removed from the task list
    if (!aShow) {
      rv = windowMediator->UnregisterWindow(xulWindow);
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }
  else {
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

nsresult nsMsgComposeService::AddGlobalHtmlDomains()
{

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefs->GetBranch(MAILNEWS_ROOT_PREF, getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIPrefBranch> defaultsPrefBranch;
  rv = prefs->GetDefaultBranch(MAILNEWS_ROOT_PREF, getter_AddRefs(defaultsPrefBranch));
  NS_ENSURE_SUCCESS(rv,rv);

  /** 
   * Check to see if we need to add any global domains.
   * If so, make sure the following prefs are added to mailnews.js
   *
   * 1. pref("mailnews.global_html_domains.version", version number);
   * This pref registers the current version in the user prefs file. A default value is stored
   * in mailnews file. Depending the changes we plan to make we can move the default version number.
   * Comparing version number from user's prefs file and the default one from mailnews.js, we
   * can effect ppropriate changes.
   *
   * 2. pref("mailnews.global_html_domains", <comma separated domain list>);
   * This pref contains the list of html domains that ISP can add to make that user's contain all
   * of these under the HTML domains in the Mail&NewsGrpus|Send Format under global preferences.
   */
  PRInt32 htmlDomainListCurrentVersion, htmlDomainListDefaultVersion;
  rv = prefBranch->GetIntPref(HTMLDOMAINUPDATE_VERSION_PREF_NAME, &htmlDomainListCurrentVersion);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = defaultsPrefBranch->GetIntPref(HTMLDOMAINUPDATE_VERSION_PREF_NAME, &htmlDomainListDefaultVersion);
  NS_ENSURE_SUCCESS(rv,rv);
  
  // Update the list as needed
  if (htmlDomainListCurrentVersion <= htmlDomainListDefaultVersion) {
    // Get list of global domains need to be added
    nsXPIDLCString globalHtmlDomainList;
    rv = prefBranch->GetCharPref(HTMLDOMAINUPDATE_DOMAINLIST_PREF_NAME, getter_Copies(globalHtmlDomainList));

    if (NS_SUCCEEDED(rv) && !globalHtmlDomainList.IsEmpty()) {

      // Get user's current HTML domain set for send format
      nsXPIDLCString currentHtmlDomainList;
      rv = prefBranch->GetCharPref(USER_CURRENT_HTMLDOMAINLIST_PREF_NAME, getter_Copies(currentHtmlDomainList));
      NS_ENSURE_SUCCESS(rv,rv);

      nsCAutoString newHtmlDomainList(currentHtmlDomainList);
      // Get the current html domain list into new list var
      nsCStringArray htmlDomainArray;
      if (!currentHtmlDomainList.IsEmpty()) 
        htmlDomainArray.ParseString(currentHtmlDomainList.get(), DOMAIN_DELIMITER);

      // Get user's current Plaintext domain set for send format
      nsXPIDLCString currentPlaintextDomainList;
      rv = prefBranch->GetCharPref(USER_CURRENT_PLAINTEXTDOMAINLIST_PREF_NAME, getter_Copies(currentPlaintextDomainList));
      NS_ENSURE_SUCCESS(rv,rv);

      // Get the current plaintext domain list into new list var
      nsCStringArray plaintextDomainArray;
      if (!currentPlaintextDomainList.IsEmpty()) 
        plaintextDomainArray.ParseString(currentPlaintextDomainList.get(), DOMAIN_DELIMITER);
    
      if (htmlDomainArray.Count() || plaintextDomainArray.Count()) {
        // Tokenize the data and add each html domain if it is not alredy there in 
        // the user's current html or plaintext domain lists
        char *newData;
        char *globalData = ToNewCString(globalHtmlDomainList);
  
        char *token = nsCRT::strtok(globalData, DOMAIN_DELIMITER, &newData);

        nsCAutoString htmlDomain;
        while (token) {
          if (token && *token) {
            htmlDomain.Assign(token);
            htmlDomain.StripWhitespace();

            if (htmlDomainArray.IndexOf(htmlDomain) == -1  && 
                plaintextDomainArray.IndexOf(htmlDomain) == -1) {
              if (!newHtmlDomainList.IsEmpty())
                newHtmlDomainList += DOMAIN_DELIMITER;
              newHtmlDomainList += htmlDomain;
            }
          }
          token = nsCRT::strtok(newData, DOMAIN_DELIMITER, &newData);
        }
        PR_FREEIF(globalData);
      }
      else
      {
        // User has no domains listed either in html or plain text category.
        // Assign the global list to be the user's current html domain list
        newHtmlDomainList = globalHtmlDomainList;
      }

      // Set user's html domain pref with the updated list
      rv = prefBranch->SetCharPref(USER_CURRENT_HTMLDOMAINLIST_PREF_NAME, newHtmlDomainList.get());
      NS_ENSURE_SUCCESS(rv,rv);

      // Increase the version to avoid running the update code unless needed (based on default version)
      rv = prefBranch->SetIntPref(HTMLDOMAINUPDATE_VERSION_PREF_NAME, htmlDomainListCurrentVersion + 1);
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }
  return NS_OK;
}

CMDLINEHANDLER_IMPL(nsMsgComposeService, "-compose", "general.startup.messengercompose", DEFAULT_CHROME,
                    "Start with messenger compose.", NS_MSGCOMPOSESTARTUPHANDLER_CONTRACTID, "Messenger Compose Startup Handler",
                    PR_TRUE, "about:blank", PR_TRUE)

