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

// Local Includes
#include "nsWebBrowser.h"
#include "nsWBURIContentListener.h"

// Interfaces Needed
#include "nsIChannel.h"

//*****************************************************************************
//***    nsWBURIContentListener: Object Management
//*****************************************************************************

nsWBURIContentListener::nsWBURIContentListener() : mWebBrowser(nsnull), 
   mParentContentListener(nsnull)
{
	NS_INIT_REFCNT();
}

nsWBURIContentListener::~nsWBURIContentListener()
{
}

//*****************************************************************************
// nsWBURIContentListener::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsWBURIContentListener)
NS_IMPL_RELEASE(nsWBURIContentListener)

NS_INTERFACE_MAP_BEGIN(nsWBURIContentListener)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsWBURIContentListener::nsIURIContentListener
//*****************************************************************************   

NS_IMETHODIMP nsWBURIContentListener::OnStartURIOpen(nsIURI* aURI, 
   const char* aWindowTarget, PRBool* aAbortOpen)
{
   if(mParentContentListener)
      {
      nsresult rv = mParentContentListener->OnStartURIOpen(aURI, aWindowTarget,
         aAbortOpen);

      if(NS_ERROR_NOT_IMPLEMENTED != rv)
         return rv;
      }

   return NS_OK;
}

NS_IMETHODIMP nsWBURIContentListener::GetProtocolHandler(nsIURI* aURI,
   nsIProtocolHandler** aProtocolHandler)
{
   NS_ENSURE_ARG_POINTER(aProtocolHandler);
   NS_ENSURE_ARG(aURI);
                                
   if(mParentContentListener)
      {
      nsresult rv = mParentContentListener->GetProtocolHandler(aURI,
         aProtocolHandler);

      if(NS_ERROR_NOT_IMPLEMENTED != rv)
         return rv;
      }
   *aProtocolHandler = nsnull;

   return NS_OK;
}

NS_IMETHODIMP nsWBURIContentListener::DoContent(const char* aContentType, 
   nsURILoadCommand aCommand, const char* aWindowTarget, 
   nsIRequest* request, nsIStreamListener** aContentHandler,
   PRBool* aAbortProcess)
{     
   NS_ERROR("Hmmmm, why is this getting called on this object?");
   return NS_OK;
}

NS_IMETHODIMP nsWBURIContentListener::IsPreferred(const char* aContentType,
   nsURILoadCommand aCommand, const char* aWindowTarget, char ** aDesiredContentType,
   PRBool* aCanHandle)
{
   NS_ENSURE_ARG_POINTER(aCanHandle);
   NS_ENSURE_ARG_POINTER(aDesiredContentType);

   if(mParentContentListener)
      {
      nsresult rv = mParentContentListener->IsPreferred(aContentType,
         aCommand, aWindowTarget, aDesiredContentType, aCanHandle);
      
      if(NS_ERROR_NOT_IMPLEMENTED != rv)
         return rv;
      }

   *aCanHandle = PR_FALSE;
   // the webbrowser object is the primary content handler for the following types:
   // If we are asked to handle any of these types, we will always say Yes!
   // regardlesss of the uri load command.
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

   if(aContentType)
      {
      // (1) list all content types we want to  be the primary handler for....
      // and suggest a desired content type if appropriate...
      if(nsCRT::strcasecmp(aContentType,  "text/html") == 0
         || nsCRT::strcasecmp(aContentType, "text/xul") == 0
         || nsCRT::strcasecmp(aContentType, "text/rdf") == 0 
         || nsCRT::strcasecmp(aContentType, "text/xml") == 0
         || nsCRT::strcasecmp(aContentType, "text/css") == 0
         || nsCRT::strcasecmp(aContentType, "image/gif") == 0
         || nsCRT::strcasecmp(aContentType, "image/jpeg") == 0
         || nsCRT::strcasecmp(aContentType, "image/png") == 0
         || nsCRT::strcasecmp(aContentType, "image/tiff") == 0
         || nsCRT::strcasecmp(aContentType, "application/http-index-format") == 0)
         *aCanHandle = PR_TRUE;
      }

   return NS_OK;
}

NS_IMETHODIMP nsWBURIContentListener::CanHandleContent(const char* aContentType,
   nsURILoadCommand aCommand, const char* aWindowTarget, char ** aDesiredContentType,
   PRBool* aCanHandleContent)
{
   NS_ERROR("Hmmmm, why is this getting called?");
   return NS_OK;
}

NS_IMETHODIMP nsWBURIContentListener::GetLoadCookie(nsISupports ** aLoadCookie)
{
   NS_ENSURE_ARG_POINTER(aLoadCookie);
   *aLoadCookie = nsnull;
   return NS_OK;
}

NS_IMETHODIMP nsWBURIContentListener::SetLoadCookie(nsISupports * aLoadCookie)
{
   NS_ERROR("Hmmmm, why is this getting called?");
   return NS_OK;
}

NS_IMETHODIMP nsWBURIContentListener::GetParentContentListener(nsIURIContentListener**
   aParentListener)
{
   NS_ENSURE_ARG_POINTER(aParentListener);

   *aParentListener = mParentContentListener;
   NS_IF_ADDREF(*aParentListener);
   return NS_OK;
}

NS_IMETHODIMP nsWBURIContentListener::SetParentContentListener(nsIURIContentListener* 
   aParentListener)
{
   // Weak Reference, don't addref
   mParentContentListener = aParentListener;
   return NS_OK;
}

//*****************************************************************************
// nsWBURIContentListener: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsWBURIContentListener: Accessors
//*****************************************************************************   

void nsWBURIContentListener::WebBrowser(nsWebBrowser* aWebBrowser)
{
   mWebBrowser = aWebBrowser;
}

nsWebBrowser* nsWBURIContentListener::WebBrowser()
{
   return mWebBrowser;
}
