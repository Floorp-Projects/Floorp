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
#include "nsDocShell.h"
#include "nsDSWebProgressListener.h"

// Interfaces Needed
#include "nsIChannel.h"

//*****************************************************************************
//***    nsDSWebProgressListener: Object Management
//*****************************************************************************

nsDSWebProgressListener::nsDSWebProgressListener() : mDocShell(nsnull) 
{
	NS_INIT_REFCNT();
}

nsDSWebProgressListener::~nsDSWebProgressListener()
{
}

//*****************************************************************************
// nsDSWebProgressListener::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsDSWebProgressListener)
NS_IMPL_RELEASE(nsDSWebProgressListener)

NS_INTERFACE_MAP_BEGIN(nsDSWebProgressListener)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsDSWebProgressListener::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP nsDSWebProgressListener::OnProgressChange(nsIChannel* aChannel,
   PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, 
   PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
   //XXXTAB Implement
   NS_ERROR("Not yet Implemented");
   return NS_OK;
}
      
NS_IMETHODIMP nsDSWebProgressListener::OnChildProgressChange(nsIChannel* aChannel,
   PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress)
{
   //XXXTAB Implement
   NS_ERROR("Not yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsDSWebProgressListener::OnStatusChange(nsIChannel* aChannel,
   PRInt32 aProgressStatusFlags)
{
   //XXXTAB Implement
   NS_ERROR("Not yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsDSWebProgressListener::OnChildStatusChange(nsIChannel* aChannel,
   PRInt32 aProgressStatusFlags)
{
   //XXXTAB Implement
   NS_ERROR("Not yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsDSWebProgressListener::OnLocationChange(nsIURI* aLocation)
{
   //XXXTAB Implement
   NS_ERROR("Not yet Implemented");
   return NS_OK;
}

//*****************************************************************************
// nsDSWebProgressListener: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsDSWebProgressListener: Accessors
//*****************************************************************************   

void nsDSWebProgressListener::DocShell(nsDocShell* aDocShell)
{
   mDocShell = aDocShell;
}

nsDocShell* nsDSWebProgressListener::DocShell()
{
   return mDocShell;
}
