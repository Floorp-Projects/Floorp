/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//#define DONT_INFORM_WEBSHELL

#include "nsIServiceManager.h"
#include "nsIDocumentLoader.h"
#include "nsIWebShellServices.h"
#include "nsIContentViewerContainer.h"
#include "nsCURILoader.h"
#include "nsObserverBase.h"
#include "nsIParser.h"
#include "nsString.h"
#include "nsIDocShell.h"
#include "nsIHttpChannel.h"
#include "nsXPIDLString.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_IID(kIDocumentLoaderIID, NS_IDOCUMENTLOADER_IID);
static NS_DEFINE_IID(kIWebShellServicesIID, NS_IWEB_SHELL_SERVICES_IID);

//-------------------------------------------------------------------------
NS_IMETHODIMP nsObserverBase::NotifyWebShell(
  nsISupports* aParserBundle, const char* charset, nsCharsetSource source)
{
   
   nsresult rv  = NS_OK;

   nsCOMPtr<nsISupportsParserBundle> bundle=do_QueryInterface(aParserBundle);

   if (bundle) {
     nsresult res = NS_OK;
     
     // XXX - Make sure not to reload POST data to prevent bugs such as 27006
     nsAutoString theChannelKey;
     theChannelKey.AssignWithConversion("channel");

     nsCOMPtr<nsIChannel> channel=nsnull;
     res=bundle->GetDataFromBundle(theChannelKey,getter_AddRefs(channel));
     if(NS_SUCCEEDED(res)) {
       nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel,&res));
       if(NS_SUCCEEDED(res)) {
         nsXPIDLCString method;
         httpChannel->GetRequestMethod(getter_Copies(method));
         if(method) {
           if(!PL_strcasecmp(method, "POST"))
             return NS_OK; 
         }
       }
     }
     
     nsAutoString theDocShellKey;
     theDocShellKey.AssignWithConversion("docshell"); // Key to find docshell from the bundle. 
     
     nsCOMPtr<nsIDocShell> docshell=nsnull;
 	     
     res=bundle->GetDataFromBundle(theDocShellKey,getter_AddRefs(docshell));
     if(NS_SUCCEEDED(res)) {
       nsCOMPtr<nsIWebShellServices> wss=nsnull;
       wss=do_QueryInterface(docshell,&res);  // Query webshell service through docshell.
       if(NS_SUCCEEDED(res)) {

#ifndef DONT_INFORM_WEBSHELL
         // ask the webshellservice to load the URL
         if(NS_FAILED( res = wss->SetRendering(PR_FALSE) ))
           rv=res;

         // XXX nisheeth, uncomment the following two line to see the reent problem

         else if(NS_FAILED(res = wss->StopDocumentLoad())){
	         rv = wss->SetRendering(PR_TRUE); // turn on the rendering so at least we will see something.
         }

         else if(NS_FAILED(res = wss->ReloadDocument(charset, source))) {
	         rv = wss->SetRendering(PR_TRUE); // turn on the rendering so at least we will see something.
         }
         else
           rv = NS_ERROR_HTMLPARSER_STOPPARSING; // We're reloading a new document...stop loading the current.
#endif

       }
     }
  }
   return rv;
}
