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

//*****************************************************************************
//***    nsDSURIContentListener: Object Management
//*****************************************************************************

nsDSURIContentListener::nsDSURIContentListener() : mDocShell(nsnull)
{
	NS_INIT_REFCNT();
}

nsDSURIContentListener::~nsDSURIContentListener()
{
}

//*****************************************************************************
// nsDSURIContentListener::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(nsDSURIContentListener, nsIURIContentListener)

//*****************************************************************************
// nsDSURIContentListener::nsIURIContentListener
//*****************************************************************************   

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

NS_IMETHODIMP nsDSURIContentListener::DoContent(const char* aContentType, nsURILoadCommand aCommand, 
   const char* aWindowTarget, nsIChannel* aOpenedChannel,
   nsIStreamListener** aContentHandler, PRBool* aAbortProcess)
{
   NS_ENSURE_ARG_POINTER(aContentHandler && aAbortProcess);

   if(HandleInCurrentDocShell(aContentType, "view", aWindowTarget, 
      aOpenedChannel, aContentHandler))
      { 
      // In this condition content will start to be directed here.
      nsCOMPtr<nsIURI> uri;

      //XXXQ Do we want the original or the current URI?
      aOpenedChannel->GetOriginalURI(getter_AddRefs(uri));
      // XXXIMPL Session history set this page as a page that has been visited
      mDocShell->SetCurrentURI(uri);
      }
   else if(mParentContentListener)
      return mParentContentListener->DoContent(aContentType, aCommand, 
         aWindowTarget, aOpenedChannel, aContentHandler, aAbortProcess);

   return NS_OK;
}

NS_IMETHODIMP nsDSURIContentListener::CanHandleContent(const char* aContentType,
   nsURILoadCommand aCommand, const char* aWindowTarget, char ** aDesiredContentType, PRBool* aCanHandle)
{
   NS_ENSURE_ARG_POINTER(aCanHandle);
   NS_ENSURE_ARG_POINTER(aDesiredContentType);

   *aDesiredContentType = nsnull;

   *aCanHandle = PR_TRUE;  // Always say true and let DoContent decide.
   return NS_OK;
}

//*****************************************************************************
// nsDSURIContentListener: Helpers
//*****************************************************************************   

PRBool nsDSURIContentListener::HandleInCurrentDocShell(const char* aContentType,
   const char* aCommand, const char* aWindowTarget, nsIChannel* aOpenedChannel,
   nsIStreamListener** aContentHandler)
{
   NS_ENSURE_TRUE(mDocShell, PR_FALSE);

   //XXXQ Should we have to match the windowTarget???

   NS_ENSURE_SUCCESS(mDocShell->CreateContentViewer(aContentType, aCommand,
      aOpenedChannel, aContentHandler), PR_FALSE);

   return PR_TRUE;
}

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

void nsDSURIContentListener::GetParentContentListener(nsIURIContentListener**
   aParentListener)
{
   *aParentListener = mParentContentListener;
   NS_IF_ADDREF(*aParentListener);
}

void nsDSURIContentListener::SetParentContentListener(nsIURIContentListener* 
   aParentListener)
{
   mParentContentListener = aParentListener;
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
