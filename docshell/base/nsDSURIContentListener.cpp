/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#include "nsDocShell.h"
#include "nsDSURIContentListener.h"
#include "nsIChannel.h"

//*****************************************************************************
//***    nsDSURIContentListener: Object Management
//*****************************************************************************

nsDSURIContentListener::nsDSURIContentListener() : mDocShell(nsnull), 
   mParentContentListener(nsnull)
{
	NS_INIT_REFCNT();
}

nsDSURIContentListener::~nsDSURIContentListener()
{
}

//*****************************************************************************
// nsDSURIContentListener::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsDSURIContentListener)
NS_IMPL_RELEASE(nsDSURIContentListener)

NS_INTERFACE_MAP_BEGIN(nsDSURIContentListener)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsDSURIContentListener::nsIURIContentListener
//*****************************************************************************   

NS_IMETHODIMP nsDSURIContentListener::OnStartURIOpen(nsIURI* aURI, 
   const char* aWindowTarget, PRBool* aAbortOpen)
{
   if(mParentContentListener)
      return mParentContentListener->OnStartURIOpen(aURI, aWindowTarget, 
         aAbortOpen);

   return NS_OK;
}

NS_IMETHODIMP nsDSURIContentListener::GetProtocolHandler(nsIURI* aURI,
   nsIProtocolHandler** aProtocolHandler)
{
   NS_ENSURE_ARG_POINTER(aProtocolHandler);
   NS_ENSURE_ARG(aURI);
                                
   if(mParentContentListener) 
      return mParentContentListener->GetProtocolHandler(aURI, aProtocolHandler);
   else
      *aProtocolHandler = nsnull;

   return NS_OK;
}

NS_IMETHODIMP nsDSURIContentListener::DoContent(const char* aContentType, 
   nsURILoadCommand aCommand, const char* aWindowTarget, 
   nsIChannel* aOpenedChannel, nsIStreamListener** aContentHandler,
   PRBool* aAbortProcess)
{
   NS_ENSURE_ARG_POINTER(aContentHandler);
   if(aAbortProcess)
      *aAbortProcess = PR_FALSE;

   // determine if the channel has just been retargeted to us...
   nsLoadFlags loadAttribs = 0;
   aOpenedChannel->GetLoadAttributes(&loadAttribs);

   if(loadAttribs & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
      mDocShell->StopCurrentLoads();

   nsCOMPtr<nsIURI> aURI;
   aOpenedChannel->GetURI(getter_AddRefs(aURI));
   mDocShell->OnLoadingSite(aURI);

   NS_ENSURE_SUCCESS(mDocShell->CreateContentViewer(aContentType, aCommand, 
      aOpenedChannel, aContentHandler), NS_ERROR_FAILURE);

   if(loadAttribs & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
      mDocShell->SetFocus();

   return NS_OK;
}

NS_IMETHODIMP nsDSURIContentListener::IsPreferred(const char* aContentType,
   nsURILoadCommand aCommand, const char* aWindowTarget, char ** aDesiredContentType, PRBool* aCanHandle)
{
   NS_ENSURE_ARG_POINTER(aCanHandle);
   NS_ENSURE_ARG_POINTER(aDesiredContentType);

   // the docshell has no idea if it is the preferred content provider or not.
   // It needs to ask it's parent if it is the preferred content handler or not...

   if(mParentContentListener)
      return mParentContentListener->IsPreferred(aContentType, aCommand, 
         aWindowTarget, aDesiredContentType, aCanHandle);
   else
     *aCanHandle = PR_FALSE;

   return NS_OK;
}

NS_IMETHODIMP nsDSURIContentListener::CanHandleContent(const char* aContentType,
   nsURILoadCommand aCommand, const char* aWindowTarget, char ** aDesiredContentType, PRBool* aCanHandleContent)
{
   NS_ENSURE_ARG_POINTER(aCanHandleContent);
   NS_ENSURE_ARG_POINTER(aDesiredContentType);
  
   // this implementation should be the same for all webshell's so no need to pass it up the chain...
  // although I suspect if aWindowTarget has a value, we will need to pass it up the chain in order to find
  // the desired window target.

  // a webshell can handle the following types. Eventually I think we want to get this information
  // from the registry and in addition, we want to
  //    incoming Type                     Preferred type
  //      text/html
  //      text/xul
  //      text/rdf
  //      text/xml
  //      text/css
  //      image/gif
  //      image/jpeg
  //      image/png
  //      image/tiff
  //      application/http-index-format
  //      message/rfc822                    text/xul 

  if (aContentType)
  {
     // (1) list all content types we want to  be the primary handler for....
     // and suggest a desired content type if appropriate...
     if (nsCRT::strcasecmp(aContentType,  "text/html") == 0
       || nsCRT::strcasecmp(aContentType, "text/xul") == 0
       || nsCRT::strcasecmp(aContentType, "text/rdf") == 0 
       || nsCRT::strcasecmp(aContentType, "text/xml") == 0
       || nsCRT::strcasecmp(aContentType, "text/css") == 0
       || nsCRT::strcasecmp(aContentType, "image/gif") == 0
       || nsCRT::strcasecmp(aContentType, "image/jpeg") == 0
       || nsCRT::strcasecmp(aContentType, "image/png") == 0
       || nsCRT::strcasecmp(aContentType, "image/tiff") == 0
       || nsCRT::strcasecmp(aContentType, "application/http-index-format") == 0)
       *aCanHandleContent = PR_TRUE;
     
    if (nsCRT::strcasecmp(aContentType, "message/rfc822") == 0)
    {
      *aCanHandleContent = PR_TRUE;
      *aDesiredContentType = nsCRT::strdup("text/html");      
    }
  }
  else
    *aCanHandleContent = PR_FALSE;

  // we may need to ask the plugin manager for this webshell if it can handle the content type too...
  return NS_OK;
}

NS_IMETHODIMP nsDSURIContentListener::GetLoadCookie(nsISupports ** aLoadCookie)
{
  *aLoadCookie = mDocShell->mLoadCookie;
  NS_IF_ADDREF(*aLoadCookie);
  return NS_OK;
}

NS_IMETHODIMP nsDSURIContentListener::SetLoadCookie(nsISupports * aLoadCookie)
{
  mDocShell->mLoadCookie = aLoadCookie;
  return NS_OK;
}

NS_IMETHODIMP nsDSURIContentListener::GetParentContentListener(nsIURIContentListener**
   aParentListener)
{
   *aParentListener = mParentContentListener;
   NS_IF_ADDREF(*aParentListener);
   return NS_OK;
}

NS_IMETHODIMP nsDSURIContentListener::SetParentContentListener(nsIURIContentListener* 
   aParentListener)
{
   // Weak Reference, don't addref
   mParentContentListener = aParentListener;
   return NS_OK;
}

//*****************************************************************************
// nsDSURIContentListener: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsDSURIContentListener: Accessors
//*****************************************************************************   

void nsDSURIContentListener::DocShell(nsDocShell* aDocShell)
{
   mDocShell = aDocShell;
}

nsDocShell* nsDSURIContentListener::DocShell()
{
   return mDocShell;
}

void nsDSURIContentListener::GetPresContext(nsIPresContext** aPresContext)
{
   *aPresContext = mPresContext;
   NS_IF_ADDREF(*aPresContext);
}

void nsDSURIContentListener::SetPresContext(nsIPresContext* aPresContext)
{
   mPresContext = aPresContext;
}
