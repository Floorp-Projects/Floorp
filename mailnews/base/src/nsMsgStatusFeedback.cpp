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
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsMsgStatusFeedback.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIObserverService.h"
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

//
// nsISupports
//
NS_IMPL_THREADSAFE_ISUPPORTS2(nsMsgStatusFeedback,
                              nsIMsgStatusFeedback,
                              nsIDocumentLoaderObserver)

// nsIDocumentLoaderObserver methods

NS_IMETHODIMP
nsMsgStatusFeedback::OnStartDocumentLoad(nsIDocumentLoader* aLoader, nsIURI* aURL, const char* aCommand)
{
	NS_PRECONDITION(aLoader != nsnull, "null ptr");
	if (! aLoader)
	    return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(aURL != nsnull, "null ptr");
	if (! aURL)
	    return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;

	if (mWindow)
	{
		nsIDOMWindow *aWindow = mWindow;
		nsCOMPtr<nsIScriptGlobalObject>
	  globalScript(do_QueryInterface(aWindow));
    nsCOMPtr<nsIDocShell> docShell;
		
    if (globalScript)
			globalScript->GetDocShell(getter_AddRefs(docShell));
		
    nsCOMPtr<nsIWebShell> webshell(do_QueryInterface(docShell));
    nsCOMPtr<nsIWebShell> rootWebshell;
		if (webshell)
			webshell->GetRootWebShell(*getter_AddRefs(rootWebshell));
		
    if (rootWebshell) 
		{
		  // Kick start the throbber
      StartMeteors();
      ShowStatusString(NS_ConvertASCIItoUCS2("Loading Document...").GetUnicode());
		  // Enable the Stop buton
		  // setAttribute( rootWebshell, "canStop", "disabled", "" );
		}
	}
	return rv;
}


NS_IMETHODIMP
nsMsgStatusFeedback::OnEndDocumentLoad(nsIDocumentLoader* aLoader, nsIChannel* channel, nsresult aStatus)
{
  NS_PRECONDITION(aLoader != nsnull, "null ptr");
  if (! aLoader)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(channel != nsnull, "null ptr");
  if (! channel)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;

	if (mWindow)
	{
		nsIDOMWindow *aWindow = mWindow;
		nsCOMPtr<nsIScriptGlobalObject>
		globalScript(do_QueryInterface(aWindow));
    nsCOMPtr<nsIDocShell> docShell;
		if (globalScript)
			globalScript->GetDocShell(getter_AddRefs(docShell));
		
    nsCOMPtr<nsIWebShell> webshell(do_QueryInterface(docShell));
    nsCOMPtr<nsIWebShell> rootWebshell;
		if (webshell)
			webshell->GetRootWebShell(*getter_AddRefs(rootWebshell));
		if (rootWebshell) 
		{
		  // stop the throbber
      StopMeteors();
      ShowStatusString(NS_ConvertASCIItoUCS2("Document: Done").GetUnicode());
		  // Disable the Stop buton
		  //setAttribute( rootWebshell, "canStop", "disabled", "true" );
		}
	}
  return rv;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* channel)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress, PRUint32 aProgressMax)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus)
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

	if (percentage == m_lastPercent)
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
	  rv = NS_NewTimer(getter_AddRefs(mStartTimer));
	  if (NS_FAILED(rv)) return rv;

	  rv = mStartTimer->Init(notifyStartMeteors, (void *)this,
							 MSGFEEDBACK_TIMER_INTERVAL);
	  if (NS_FAILED(rv)) return rv;

	  mQueuedMeteorStarts++;
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
	  rv = NS_NewTimer(getter_AddRefs(mStopTimer));
	  if (NS_FAILED(rv)) return rv;

	  rv = mStopTimer->Init(notifyStopMeteors, (void *)this,
							MSGFEEDBACK_TIMER_INTERVAL);
	  if (NS_FAILED(rv)) return rv;

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
