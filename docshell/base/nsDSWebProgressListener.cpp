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
#if 0 /* This file is now longer used... */

// Local Includes
#include "nsDocShell.h"
#include "nsDSWebProgressListener.h"

// Interfaces Needed
#include "nsIChannel.h"

//*****************************************************************************
//***    nsDSWebProgressListener: Object Management
//*****************************************************************************

nsDSWebProgressListener::nsDSWebProgressListener() : mDocShell(nsnull),
   mProgressStatusFlags(0), mCurSelfProgress(0), mMaxSelfProgress(0),
   mCurTotalProgress(0), mMaxTotalProgress(0)
 
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
   mCurSelfProgress = aCurSelfProgress;
   mMaxSelfProgress = aMaxSelfProgress;
   mCurTotalProgress = aCurTotalProgress;
   mMaxTotalProgress = aMaxTotalProgress;

   if(mDocShell)
      mDocShell->FireOnProgressChange(aChannel, aCurSelfProgress, 
      aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);

   return NS_OK;
}
      
NS_IMETHODIMP nsDSWebProgressListener::OnChildProgressChange(nsIChannel* aChannel,
   PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress)
{
   if(mDocShell)
      mDocShell->FireOnChildProgressChange(aChannel, aCurSelfProgress, 
      aMaxSelfProgress);

   return NS_OK;
}

NS_IMETHODIMP nsDSWebProgressListener::OnStatusChange(nsIChannel* aChannel,
   PRInt32 aProgressStatusFlags)
{
   mProgressStatusFlags = aProgressStatusFlags;
   //XXX Need to mask in flag_windowActivity when animation is occuring in the
   // window
   if(mDocShell)
      {
      if(aProgressStatusFlags & nsIWebProgress::flag_net_start &&
         PR_TRUE /*XXX Eventually check some flag to make sure there is not
         already window activity.  If there is don't add the window start*/)
         aProgressStatusFlags |= nsIWebProgress::flag_win_start;
      else if(aProgressStatusFlags & nsIWebProgress::flag_net_stop &&
         PR_TRUE /*XXX Eventually check some flag to see if there is animation
         going if there is don't add in the window stop flag*/)
         aProgressStatusFlags |= nsIWebProgress::flag_win_stop;
         
      mDocShell->FireOnStatusChange(aChannel, aProgressStatusFlags);
      }

   return NS_OK;
}

NS_IMETHODIMP nsDSWebProgressListener::OnChildStatusChange(nsIChannel* aChannel,
   PRInt32 aProgressStatusFlags)
{
   if(mDocShell)
      mDocShell->FireOnChildStatusChange(aChannel, aProgressStatusFlags);

   return NS_OK;
}

NS_IMETHODIMP nsDSWebProgressListener::OnLocationChange(nsIURI* aLocation)
{
   NS_ERROR("DocShell should be the only one generating this message");
   return NS_OK;
}

NS_IMETHODIMP 
nsDSWebProgressListener::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                          nsIRequest *aRequest, 
                                          PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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

#endif /* 0 */
