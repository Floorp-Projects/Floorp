#include "stdafx.h"
#include "CCommandObserver.h"


NS_IMPL_ADDREF(CCommandObserver)
NS_IMPL_RELEASE(CCommandObserver)
NS_IMPL_QUERY_INTERFACE1(CCommandObserver, nsIObserver)



CCommandObserver::CCommandObserver()
{
  NS_INIT_REFCNT();
  mFrame = 0;
}


/* void observe (in nsISupports aSubject, in string aTopic, in wstring aData); */
NS_IMETHODIMP
CCommandObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  if (!mFrame)
    return NS_ERROR_NOT_INITIALIZED;
  mFrame->KillTimer(mTimerId);
  mFrame->SetTimer(mTimerId,mDelay,0);//reset delay on update.
  return NS_OK;
}

void
CCommandObserver::SetFrame(CFrameWnd *frame,UINT timerId,UINT delay) //update if 100 ticks goes by and no more changes
{
  mFrame = frame;
  mDelay = delay;
  mTimerId = timerId;
}
