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

NS_IMETHODIMP nsDSURIContentListener::DoContent(const char* aContentType,
   const char* aCommand, const char* aWindowTarget, nsIChannel* aOpenedChannel,
   nsIStreamListener** aContentHandler, PRBool* aAbortProcess)
{
   NS_ENSURE_ARG_POINTER(aContentHandler && aAbortProcess);

  /* The URIDispatcher will give the content listener a shot at handling
     the content before it tries other means. If the content listener
     wants to handle the content then it should return a stream listener
     the data should be pushed into. 
     aContentType --> the content type we need to handle
     aCommand --> verb for the action (this comes from layout???)
     aWindowTarget --> name of the target window if any
     aStreamListener --> the content viewer the content should be displayed in
                        You should return null for this out parameter if you do
                        not want to handle this content type.
	 aAbortProcess --> If you want to handle the content yourself and you don't
					   want the dispatcher to do anything else, return TRUE for
					   this parameter. 
  */

   if(HandleInCurrentDocShell(aContentType, aCommand, aWindowTarget))
      {
      //XXX Start Load here....
      //XXX mDocShell->SetCurrentURI(nsIChannel::GetURI()));

      }
   else
      return mParentContentListener->DoContent(aContentType, aCommand, 
         aWindowTarget, aOpenedChannel, aContentHandler, aAbortProcess);

   return NS_OK;
}

NS_IMETHODIMP nsDSURIContentListener::CanHandleContent(const char* aContentType,
   const char* aCommand, const char* aWindowTarget, PRBool* aCanHandle)
{
   NS_ENSURE_ARG_POINTER(aCanHandle);

   *aCanHandle = PR_TRUE;  // Always say true and let DoContent decide.
   return NS_OK;
}

//*****************************************************************************
// nsDSURIContentListener: Helpers
//*****************************************************************************   

PRBool nsDSURIContentListener::HandleInCurrentDocShell(const char* aContentType,
   const char* aCommand, const char* aWindowTarget)
{
   nsAutoString contentType(aContentType);
   PRBool fCanHandle = PR_FALSE;
   /* XXX Remove this
   NS_ENSURE_SUCCESS(mDocShell->CanHandleContentType(contentType.GetUnicode(),
      &fCanHandle), PR_FALSE);*/

   NS_ENSURE_TRUE(fCanHandle, PR_FALSE);

   /*  XXX Implement
   1.) Check content type
   2.)  See if we can handle the command
   3.)  See if the target is for us  
   */

   return PR_FALSE;
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