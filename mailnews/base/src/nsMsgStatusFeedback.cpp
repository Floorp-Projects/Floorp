/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "msgCore.h"

#include "nsXPIDLString.h"

#include "nsIWebProgress.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"

#include "nsMsgStatusFeedback.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIChannel.h"

#define MSGFEEDBACK_TIMER_INTERVAL 500

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

nsrefcnt nsMsgStatusFeedback::gInstanceCount = 0;
nsCOMPtr<nsIStringBundle> nsMsgStatusFeedback::mBundle;

nsMsgStatusFeedback::nsMsgStatusFeedback() :
  m_lastPercent(0)
{
	NS_INIT_REFCNT();
	LL_I2L(m_lastProgressTime, 0);

    if (gInstanceCount++ == 0) {
      nsresult rv;
      nsCOMPtr<nsIStringBundleService> bundleService =
        do_GetService(kStringBundleServiceCID, &rv);

      if (NS_SUCCEEDED(rv))
        bundleService->CreateBundle("chrome://messenger/locale/messenger.properties",
                                    nsnull,
                                    getter_AddRefs(mBundle));
    }
}

nsMsgStatusFeedback::~nsMsgStatusFeedback()
{
  if (gInstanceCount-- == 0) {
    mBundle = nsnull;
  }
}

NS_IMPL_THREADSAFE_ADDREF(nsMsgStatusFeedback);
NS_IMPL_THREADSAFE_RELEASE(nsMsgStatusFeedback);

NS_INTERFACE_MAP_BEGIN(nsMsgStatusFeedback)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgStatusFeedback)
   NS_INTERFACE_MAP_ENTRY(nsIMsgStatusFeedback)
   NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink) 
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener) 
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//////////////////////////////////////////////////////////////////////////////////
// nsMsgStatusFeedback::nsIWebProgressListener
//////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsMsgStatusFeedback::OnProgressChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest,
                                      PRInt32 aCurSelfProgress,
                                      PRInt32 aMaxSelfProgress, 
                                      PRInt32 aCurTotalProgress,
                                      PRInt32 aMaxTotalProgress)
{
  PRInt32 percentage = 0;
  if (aMaxTotalProgress > 0)
  {
    percentage =  (aCurTotalProgress * 100) / aMaxTotalProgress;
    if (percentage)
      ShowProgress(percentage);
  }

   return NS_OK;
}
      
NS_IMETHODIMP
nsMsgStatusFeedback::OnStateChange(nsIWebProgress* aWebProgress,
                                   nsIRequest* aRequest,
                                   PRInt32 aProgressStateFlags,
                                   nsresult aStatus)
{
  nsresult rv;
  if (aProgressStateFlags & STATE_IS_NETWORK)
  {
    if (aProgressStateFlags & STATE_START)
    {
      m_lastPercent = 0;
      StartMeteors();
      nsXPIDLString loadingDocument;
      rv = mBundle->GetStringFromName(NS_ConvertASCIItoUCS2("documentLoading").GetUnicode(),
                                      getter_Copies(loadingDocument));
      if (NS_SUCCEEDED(rv))
        ShowStatusString(loadingDocument);
    }
    else if (aProgressStateFlags & STATE_STOP)
    {
      StopMeteors();
      nsXPIDLString documentDone;
      rv = mBundle->GetStringFromName(NS_ConvertASCIItoUCS2("documentDone").GetUnicode(),
                                      getter_Copies(documentDone));
      if (NS_SUCCEEDED(rv))
        ShowStatusString(documentDone);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnLocationChange(nsIWebProgress* aWebProgress,
                                                    nsIRequest* aRequest,
                                                    nsIURI* aLocation)
{
   return NS_OK;
}

NS_IMETHODIMP 
nsMsgStatusFeedback::OnStatusChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsresult aStatus,
                                    const PRUnichar* aMessage)
{
    return NS_OK;
}


NS_IMETHODIMP 
nsMsgStatusFeedback::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsMsgStatusFeedback::ShowStatusString(const PRUnichar *status)
{
  if (mStatusFeedback)
    mStatusFeedback->ShowStatusString(status);
	return NS_OK;
}

NS_IMETHODIMP
nsMsgStatusFeedback::ShowProgress(PRInt32 percentage)
{
  // if the percentage hasn't changed...OR if we are going from 0 to 100% in one step
  // then don't bother....just fall out....
	if (percentage == m_lastPercent || (m_lastPercent == 0 && percentage >= 100))
		return NS_OK;
  
  m_lastPercent = percentage;

	PRInt64 nowMS;
	LL_I2L(nowMS, 0);
	if (percentage < 100)	// always need to do 100%
	{
		int64 minIntervalBetweenProgress;

		LL_I2L(minIntervalBetweenProgress, 250);
		int64 diffSinceLastProgress;
		LL_I2L(nowMS, PR_IntervalToMilliseconds(PR_IntervalNow()));
		LL_SUB(diffSinceLastProgress, nowMS, m_lastProgressTime); // r = a - b
		LL_SUB(diffSinceLastProgress, diffSinceLastProgress, minIntervalBetweenProgress); // r = a - b
		if (!LL_GE_ZERO(diffSinceLastProgress))
			return NS_OK;
	}

	m_lastProgressTime = nowMS;
  
  if (mStatusFeedback)
    mStatusFeedback->ShowProgress(percentage);
	return NS_OK;
}

NS_IMETHODIMP
nsMsgStatusFeedback::StartMeteors()
{
  if (mStatusFeedback)
    mStatusFeedback->StartMeteors();
  return NS_OK;
}

NS_IMETHODIMP
nsMsgStatusFeedback::StopMeteors()
{
  if (mStatusFeedback)
    mStatusFeedback->StopMeteors();
  return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::CloseWindow()
{
  mWindow = nsnull;
  mStatusFeedback = nsnull;

  return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::SetDocShell(nsIDocShell *shell, nsIDOMWindowInternal *aWindow)
{

  if (aWindow)
  {
     nsCOMPtr<nsISupports> xpConnectObj;
     nsCOMPtr<nsPIDOMWindow> piDOMWindow(do_QueryInterface(aWindow));
     if (piDOMWindow)
     {
        nsAutoString msgStatusFeedbackWinId; msgStatusFeedbackWinId.AssignWithConversion("MsgStatusFeedback");
        piDOMWindow->GetObjectProperty(msgStatusFeedbackWinId.GetUnicode(), getter_AddRefs(xpConnectObj));
        mStatusFeedback = do_QueryInterface(xpConnectObj);
     }
  }

	mWindow = aWindow;
	return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnProgress(nsIChannel* channel, nsISupports* ctxt, 
                                          PRUint32 aProgress, PRUint32 aProgressMax)
{
  // XXX: What should the nsIWebProgress be?
  return OnProgressChange(nsnull, channel, aProgress, aProgressMax, 
                          aProgress /* current total progress */, aProgressMax /* max total progress */);
}

NS_IMETHODIMP nsMsgStatusFeedback::OnStatus(nsIChannel* channel, nsISupports* ctxt, 
                                            nsresult aStatus, const PRUnichar* aStatusArg)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> sbs = do_GetService(kStringBundleServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsXPIDLString str;
  rv = sbs->FormatStatusMessage(aStatus, aStatusArg, getter_Copies(str));
  if (NS_FAILED(rv)) return rv;
  nsAutoString msg(NS_STATIC_CAST(const PRUnichar*, str));
  return ShowStatusString(msg.GetUnicode());
}
