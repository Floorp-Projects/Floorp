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
  mWebShell(nsnull),
  mWebShellWindow(nsnull),
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
  EndObserving();
}

//
// nsISupports
//
NS_IMPL_THREADSAFE_ISUPPORTS3(nsMsgStatusFeedback,
                              nsIMsgStatusFeedback,
                              nsIDocumentLoaderObserver,
                              nsIObserver)

// nsIDocumentLoaderObserver


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
		  setAttribute( rootWebshell, "Messenger:Status", "value", "Loading Document..." );

		  // Enable the Stop buton
		  setAttribute( rootWebshell, "canStop", "disabled", "" );
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
			setAttribute( rootWebshell, "Messenger:Status", "value", "Document: Done" );
		  // Disable the Stop buton
		  setAttribute( rootWebshell, "canStop", "disabled", "true" );
		}
	}
  return rv;
}

static const char *prefix = "component://netscape/appshell/component/browser/window";

void nsMsgStatusFeedback::BeginObserving() 
{
  // Get observer service.
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIObserverService, svc, NS_OBSERVERSERVICE_PROGID, &rv);
  if ( NS_SUCCEEDED( rv ) && svc ) 
  {
    // Add/Remove object as observer of web shell window topics.
    nsAutoString topic1(prefix);
    topic1 += ";status";
    rv = svc->AddObserver( this, topic1.GetUnicode() );
  }

  return;
}

void nsMsgStatusFeedback::EndObserving() 
{
  // Get observer service.
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIObserverService, svc, NS_OBSERVERSERVICE_PROGID, &rv);
  if ( NS_SUCCEEDED( rv ) && svc ) 
  {
    // Add/Remove object as observer of web shell window topics.
    nsAutoString topic1(prefix);
    topic1 += ";status";
    rv = svc->RemoveObserver( this, topic1.GetUnicode() );
   }

  return;
}

NS_IMETHODIMP nsMsgStatusFeedback::Observe( nsISupports *aSubject,
                                            const PRUnichar *aTopic,
                                            const PRUnichar *someData ) 
{
  nsresult rv = NS_OK;
  // We only are interested if aSubject is our web shell window.
  if ( aSubject && mWebShellWindow ) 
  {
    nsCOMPtr<nsIWebShellWindow> window = do_QueryInterface(aSubject, &rv);
    if ( NS_SUCCEEDED(rv) && window && (window.get() == mWebShellWindow) ) 
    {
      nsAutoString topic1 = prefix;
      topic1 += ";status";
      if ( topic1 == aTopic ) 
        rv = ShowStatusString(someData);
    } // if window matches our window
  } // if we have a window to worry about
    
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
	setAttribute( mWebShell, "Messenger:Status", "value", statusMsg );
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

	if (percentage >= 0)
		setAttribute(mWebShell, "Messenger:LoadingProgress", "mode","normal");

	strPercentage.Append(percentage, 10);
	setAttribute( mWebShell, "Messenger:LoadingProgress", "value", strPercentage);
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
  EndObserving();
  mWindow = nsnull;
  mWebShell = nsnull;
  mWebShellWindow = nsnull;

  return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::SetWebShell(nsIWebShell *shell, nsIDOMWindow *aWindow)
{
	if (aWindow)
	{
		nsCOMPtr<nsIScriptGlobalObject>
	  globalScript(do_QueryInterface(aWindow));
    nsCOMPtr<nsIDocShell> docShell;
		if (globalScript)
			globalScript->GetDocShell(getter_AddRefs(docShell));
		nsCOMPtr<nsIWebShell> webshell(do_QueryInterface(docShell));
    nsCOMPtr<nsIWebShell> rootWebshell;
		if (webshell)
		{
			webshell->GetRootWebShell(mWebShell);
			nsIWebShell *root = mWebShell;
			NS_RELEASE(root); // don't hold reference

      // get the webshell window too....
      nsCOMPtr<nsIWebShellContainer> topLevelWindow;
      webshell->GetTopLevelWindow(getter_AddRefs(topLevelWindow));
      if (topLevelWindow)
      {
        nsCOMPtr<nsIWebShellWindow> webWindow = do_QueryInterface(topLevelWindow);
        // do NOT!!! keep an owning reference on the webshell window..it owns us...
        mWebShellWindow = webWindow;
      }
		}
	}
	mWindow = aWindow;

  BeginObserving();

	return NS_OK;
}

static int debugSetAttr = 0;
nsresult nsMsgStatusFeedback::setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value ) 
{
    if (!mWebShell)
      return NS_OK;

    nsresult rv = NS_OK;

    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(shell));

    nsCOMPtr<nsIContentViewer> cv;
    rv = docShell ? docShell->GetContentViewer(getter_AddRefs(cv))
               : NS_ERROR_NULL_POINTER;
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                // Up-cast.
                nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
                if ( xulDoc ) 
				        {
                    // Find specified element.
                    nsCOMPtr<nsIDOMElement> elem;
                    rv = xulDoc->GetElementById( id, getter_AddRefs(elem) );
                    if ( elem ) {
                        // Set the text attribute.
                        rv = elem->SetAttribute( name, value );
                        if ( debugSetAttr ) {
                            char *p = value.ToNewCString();
							              printf("setting busy to %s\n", p);
                            delete [] p;
                        }
                        if ( rv != NS_OK ) {
                             if (debugSetAttr) printf("SetAttribute failed, rv=0x%X\n",(int)rv);
                        }
                    } else {
                        if (debugSetAttr) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
                    }
                } else {
                  if (debugSetAttr)   printf("Upcast to nsIDOMHTMLDocument failed\n");
                }
            } else {
                if (debugSetAttr) printf("GetDocument failed, rv=0x%X\n",(int)rv);
            }
        } else {
             if (debugSetAttr) printf("Upcast to nsIDocumentViewer failed\n");
        }
    } else {
        if (debugSetAttr) printf("GetContentViewer failed, rv=0x%X\n",(int)rv);
    }
    return rv;
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
  setAttribute(mWebShell, "Messenger:Throbber", "busy", "true");
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
  setAttribute( mWebShell, "Messenger:Throbber", "busy", "false" );
  m_meteorsSpinning = PR_FALSE;
}

