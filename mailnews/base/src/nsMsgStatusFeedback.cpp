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
#include "nsIWebProgress.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsMsgStatusFeedback.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocShell.h"


#define MSGFEEDBACK_TIMER_INTERVAL 500

nsMsgStatusFeedback::nsMsgStatusFeedback() :
  m_meteorsSpinning(PR_FALSE),
  m_lastPercent(0),
  mQueuedMeteorStarts(0),
  mQueuedMeteorStops(0)
{
	NS_INIT_REFCNT();
	LL_I2L(m_lastProgressTime, 0);
}

nsMsgStatusFeedback::~nsMsgStatusFeedback()
{
}

NS_IMPL_THREADSAFE_ADDREF(nsMsgStatusFeedback);
NS_IMPL_THREADSAFE_RELEASE(nsMsgStatusFeedback);

NS_INTERFACE_MAP_BEGIN(nsMsgStatusFeedback)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgStatusFeedback)
   NS_INTERFACE_MAP_ENTRY(nsIMsgStatusFeedback)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener) 
NS_INTERFACE_MAP_END

//////////////////////////////////////////////////////////////////////////////////
// nsMsgStatusFeedback::nsIWebProgressListener
//////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsMsgStatusFeedback::OnProgressChange(nsIChannel* aChannel,
   PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, 
   PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
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
      
NS_IMETHODIMP nsMsgStatusFeedback::OnChildProgressChange(nsIChannel* aChannel,
   PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress)
{
   return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnStatusChange(nsIChannel* aChannel,
   PRInt32 aProgressStatusFlags)
{
  if (aProgressStatusFlags & nsIWebProgress::flag_net_start)
  {
    m_lastPercent = 0;
    StartMeteors();
    ShowStatusString(NS_ConvertASCIItoUCS2("Loading Document...").GetUnicode());
  }
  if (aProgressStatusFlags & nsIWebProgress::flag_net_stop)
  {
    StopMeteors();
    ShowStatusString(NS_ConvertASCIItoUCS2("Document: Done").GetUnicode());
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnChildStatusChange(nsIChannel* aChannel,
   PRInt32 aProgressStatusFlags)
{
   return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnLocationChange(nsIURI* aLocation)
{
   return NS_OK;
}

NS_IMETHODIMP
nsMsgStatusFeedback::ShowStatusString(const PRUnichar *status)
{
	nsAutoString statusMsg = status;
  if (mStatusFeedback)
    mStatusFeedback->ShowStatusString(status);
	return NS_OK;
}

NS_IMETHODIMP
nsMsgStatusFeedback::ShowProgress(PRInt32 percentage)
{
	nsAutoString strPercentage;

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
  if (mQueuedMeteorStarts <= 0)
    mQueuedMeteorStarts++;
	return NS_OK;
}

NS_IMETHODIMP
nsMsgStatusFeedback::StartMeteors()
{
  nsresult rv;

  // cancel outstanding starts
  if (mQueuedMeteorStarts>0) {
    mQueuedMeteorStarts--;
    NS_ASSERTION(mQueuedMeteorStarts == 0, "destroying unfired/uncanceled start timer");
	if(mStartTimer)
	  mStartTimer->Cancel();
  }

    //If there is an outstanding stop timer still then we might as well cancel it since we are
  //just going to start it.  This will prevent there being a start and stop timer outstanding in
  //which case the start could go before the stop and cause the meteors to never start.
  if(mQueuedMeteorStops > 0) {
    mQueuedMeteorStops--;
	if(mStopTimer)
	  mStopTimer->Cancel();
  }

  //only run the start timer if the meteors aren't spinning.
  if(!m_meteorsSpinning)
  {
    NotifyStartMeteors(nsnull);
#if 0
	  rv = NS_NewTimer(getter_AddRefs(mStartTimer));
	  if (NS_FAILED(rv)) return rv;

	  rv = mStartTimer->Init(notifyStartMeteors, (void *)this,
							 MSGFEEDBACK_TIMER_INTERVAL);
	  if (NS_FAILED(rv)) return rv;
	  m_meteorsSpinning = PR_TRUE;
#endif
  }
  return NS_OK;
}


NS_IMETHODIMP
nsMsgStatusFeedback::StopMeteors()
{
  nsresult rv;

  // cancel outstanding stops
  if (mQueuedMeteorStops>0) {
    mQueuedMeteorStops--;
    NS_ASSERTION(mQueuedMeteorStops == 0, "destroying unfired/uncanceled stop");
    if(mStopTimer)
	  mStopTimer->Cancel();
  }
  
  //If there is an outstanding start timer still then we might as well cancel it since we are
  //just going to stop it.  This will prevent there being a start and stop timer outstanding in
  //which case the stop could go before the start and cause the meteors to never stop.
  if(mQueuedMeteorStarts > 0) {
    mQueuedMeteorStarts--;
	if(mStartTimer)
	  mStartTimer->Cancel();
  }


  //only run the stop timer if the meteors are actually spinning.
  if(m_meteorsSpinning)
  {
    NotifyStopMeteors(nsnull);
#if 0
	  rv = NS_NewTimer(getter_AddRefs(mStopTimer));
	  if (NS_FAILED(rv)) return rv;

	  rv = mStopTimer->Init(notifyStopMeteors, (void *)this,
							MSGFEEDBACK_TIMER_INTERVAL);
	  if (NS_FAILED(rv)) return rv;
#endif
	  mQueuedMeteorStops++;
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::CloseWindow()
{
  mWindow = nsnull;
  mStatusFeedback = nsnull;

  return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::SetWebShell(nsIWebShell *shell, nsIDOMWindow *aWindow)
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

//
// timer callbacks that resolve closure
//
void
nsMsgStatusFeedback::notifyStartMeteors(nsITimer *aTimer, void *aClosure)
{
  NS_ASSERTION(aClosure, "Start meteors: bad nsIMsgStatusFeedback!\n");
  if (!aClosure) return;
  ((nsMsgStatusFeedback*)aClosure)->NotifyStartMeteors(aTimer);
}

void
nsMsgStatusFeedback::notifyStopMeteors(nsITimer *aTimer, void *aClosure)
{
  NS_ASSERTION(aClosure, "Stop meteors: bad nsMsgStatusFeedback!\n");
  if (!aClosure) return;
  ((nsMsgStatusFeedback*)aClosure)->NotifyStopMeteors(aTimer);
}

//
// actual timer callbacks
//
void
nsMsgStatusFeedback::NotifyStartMeteors(nsITimer *aTimer)
{
  mQueuedMeteorStarts--;
  
  // meteors already spinning, so noop
  if (m_meteorsSpinning) return;

  // we'll be stopping them soon, don't bother starting.
  if (mQueuedMeteorStops > 0) return;
  
  // actually start the meteors
  if (mStatusFeedback)
    mStatusFeedback->StartMeteors();
  m_meteorsSpinning = PR_TRUE;
}

void
nsMsgStatusFeedback::NotifyStopMeteors(nsITimer* aTimer)
{
  mQueuedMeteorStops--;
  
  // meteors not spinning
  if (!m_meteorsSpinning) return;

  // there is at least one more timer firing soon
  if (mQueuedMeteorStarts > 0) return;

  // actually stop the meteors
  if (mStatusFeedback)
    mStatusFeedback->StopMeteors();
  m_meteorsSpinning = PR_FALSE;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnProgress(nsIChannel* channel, nsISupports* ctxt, 
                                          PRUint32 aProgress, PRUint32 aProgressMax)
{
  return OnProgressChange(channel, aProgress, aProgressMax, 
                          aProgress /* current total progress */, aProgressMax /* max total progress */);
}

NS_IMETHODIMP nsMsgStatusFeedback::OnStatus(nsIChannel* channel, nsISupports* ctxt, const PRUnichar* aMsg)
{
  return ShowStatusString(aMsg);
}
