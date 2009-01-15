/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Geolocation.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGeolocation.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsDOMClassInfo.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsIPermissionManager.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsIJSContextStack.h"

#include <math.h>

#ifdef NS_MAEMO_LOCATION
#include "MaemoLocationProvider.h"
#endif

#include "nsIDOMDocument.h"
#include "nsIDocument.h"

// Some limit to the number of get or watch geolocation requests
// that a window can make.
#define MAX_GEO_REQUESTS_PER_WINDOW  1500

////////////////////////////////////////////////////
// nsDOMGeoPositionError
////////////////////////////////////////////////////

class nsDOMGeoPositionError : public nsIDOMGeoPositionError
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOPOSITIONERROR

  nsDOMGeoPositionError(PRInt16 aCode);
  void NotifyCallback(nsIDOMGeoPositionErrorCallback* callback);

private:
  ~nsDOMGeoPositionError();
  PRInt16 mCode;
};

NS_INTERFACE_MAP_BEGIN(nsDOMGeoPositionError)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionError)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionError)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoPositionError)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsDOMGeoPositionError)
NS_IMPL_THREADSAFE_RELEASE(nsDOMGeoPositionError)

nsDOMGeoPositionError::nsDOMGeoPositionError(PRInt16 aCode)
  : mCode(aCode)
{
}

nsDOMGeoPositionError::~nsDOMGeoPositionError(){}


NS_IMETHODIMP
nsDOMGeoPositionError::GetCode(PRInt16 *aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  *aCode = mCode;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMGeoPositionError::GetMessage(nsAString & aMessage)
{
  aMessage.Truncate();
  return NS_OK;
}

void
nsDOMGeoPositionError::NotifyCallback(nsIDOMGeoPositionErrorCallback* aCallback)
{
  if (!aCallback)
    return;
  
  // Ensure that the proper context is on the stack (bug 452762)
  nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
  if (!stack || NS_FAILED(stack->Push(nsnull)))
    return;
  
  aCallback->HandleEvent(this);
  
  // remove the stack
  JSContext* cx;
  stack->Pop(&cx);
}
////////////////////////////////////////////////////
// nsGeolocationRequest
////////////////////////////////////////////////////

nsGeolocationRequest::nsGeolocationRequest(nsGeolocation* aLocator,
                                           nsIDOMGeoPositionCallback* aCallback,
                                           nsIDOMGeoPositionErrorCallback* aErrorCallback,
                                           nsIDOMGeoPositionOptions* aOptions)
  : mAllowed(PR_FALSE),
    mCleared(PR_FALSE),
    mHasSentData(PR_FALSE),
    mCallback(aCallback),
    mErrorCallback(aErrorCallback),
    mOptions(aOptions),
    mLocator(aLocator)
{
}

nsGeolocationRequest::~nsGeolocationRequest()
{
}

nsresult
nsGeolocationRequest::Init()
{
  // This method is called before the user has given permission for this request.

  // check to see if we have a geolocation provider, if not, notify an error and bail.
  nsRefPtr<nsGeolocationService> geoService = nsGeolocationService::GetInstance();
  if (!geoService->HasGeolocationProvider()) {
    NotifyError(nsIDOMGeoPositionError::POSITION_UNAVAILABLE);
    return NS_ERROR_FAILURE;;
  }

  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGeolocationRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGeolocationRequest)
  NS_INTERFACE_MAP_ENTRY(nsIGeolocationRequest)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGeolocationRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGeolocationRequest)

NS_IMPL_CYCLE_COLLECTION_3(nsGeolocationRequest, mCallback, mErrorCallback, mOptions)


void
nsGeolocationRequest::NotifyError(PRInt16 errorCode)
{
  nsRefPtr<nsDOMGeoPositionError> positionError = new nsDOMGeoPositionError(errorCode);
  if (!positionError)
    return;
  
  positionError->NotifyCallback(mErrorCallback);
}


NS_IMETHODIMP
nsGeolocationRequest::Notify(nsITimer* aTimer)
{
  // If we haven't gotten an answer from the geolocation
  // provider yet, cancel the request.  Same logic as
  // ::Cancel, just a different error
  
  if (!mHasSentData) {
    NotifyError(nsIDOMGeoPositionError::TIMEOUT);
    // remove ourselves from the locator's callback lists.
    mLocator->RemoveRequest(this);
  }

  mTimeoutTimer = nsnull;
  return NS_OK;
}
 
NS_IMETHODIMP
nsGeolocationRequest::GetRequestingURI(nsIURI * *aRequestingURI)
{
  NS_ENSURE_ARG_POINTER(aRequestingURI);
  *aRequestingURI = mLocator->GetURI();
  NS_IF_ADDREF(*aRequestingURI);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::GetRequestingWindow(nsIDOMWindow * *aRequestingWindow)
{
  NS_ENSURE_ARG_POINTER(aRequestingWindow);
  *aRequestingWindow = mLocator->GetOwner();
  NS_IF_ADDREF(*aRequestingWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::Cancel()
{
  NotifyError(nsIDOMGeoPositionError::PERMISSION_DENIED);

  // remove ourselves from the locators callback lists.
  mLocator->RemoveRequest(this);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::Allow()
{
  nsRefPtr<nsGeolocationService> geoService = nsGeolocationService::GetInstance();

  // Kick off the geo device, if it isn't already running
  nsresult rv = geoService->StartDevice();
  
  if (NS_FAILED(rv)) {
    // Location provider error
    NotifyError(nsIDOMGeoPositionError::POSITION_UNAVAILABLE);
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMGeoPosition> lastPosition = geoService->GetCachedPosition();
  DOMTimeStamp cachedPositionTime;
  if (lastPosition)
    lastPosition->GetTimestamp(&cachedPositionTime);

  // check to see if we can use a cached value
  //
  // either:
  // a) the user has specified a maximumAge which allows us to return a cached value,
  // -or-
  // b) the cached position time is some reasonable value to return to the user (<30s)
  
  PRUint32 maximumAge = 30 * PR_MSEC_PER_SEC;
  if (mOptions) {
    PRInt32 tempAge;
    nsresult rv = mOptions->GetMaximumAge(&tempAge);
    if (NS_SUCCEEDED(rv)) {
      if (tempAge > 0)
        maximumAge = tempAge;
    }
  }

  if (lastPosition && maximumAge > 0 && ( (PR_Now() / PR_USEC_PER_MSEC ) - maximumAge <= cachedPositionTime) ) {
    // okay, we can return a cached position
    mAllowed = PR_TRUE;
    
    // send the cached location
    SendLocation(lastPosition);
    
    // remove ourselves from the locators callback lists.
    mLocator->RemoveRequest(this);
  }

  PRInt32 timeout;
  if (mOptions && NS_SUCCEEDED(mOptions->GetTimeout(&timeout)) && timeout > 0) {
    
    if (timeout < 10)
      timeout = 10;

    mTimeoutTimer = do_CreateInstance("@mozilla.org/timer;1");
    mTimeoutTimer->InitWithCallback(this, timeout, nsITimer::TYPE_ONE_SHOT);
  }

  mAllowed = PR_TRUE;
  return NS_OK;
}

void
nsGeolocationRequest::MarkCleared()
{
  mCleared = PR_TRUE;
}

void
nsGeolocationRequest::SendLocation(nsIDOMGeoPosition* aPosition)
{
  if (mCleared || !mAllowed)
    return;

  // we should not pass null back to the DOM.
  if (!aPosition) {
    NotifyError(nsIDOMGeoPositionError::POSITION_UNAVAILABLE);
    return;
  }

  // Ensure that the proper context is on the stack (bug 452762)
  nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
  if (!stack || NS_FAILED(stack->Push(nsnull)))
    return; // silently fail
  
  mCallback->HandleEvent(aPosition);

  // remove the stack
  JSContext* cx;
  stack->Pop(&cx);

  mHasSentData = PR_TRUE;
}

void
nsGeolocationRequest::Shutdown()
{
  mCleared = PR_TRUE;
  mCallback = nsnull;
  mErrorCallback = nsnull;
}

////////////////////////////////////////////////////
// nsGeolocationService
////////////////////////////////////////////////////
NS_INTERFACE_MAP_BEGIN(nsGeolocationService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGeolocationUpdate)
  NS_INTERFACE_MAP_ENTRY(nsIGeolocationUpdate)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsGeolocationService)
NS_IMPL_THREADSAFE_RELEASE(nsGeolocationService)

nsGeolocationService::nsGeolocationService()
 : mProviderStarted(PR_FALSE)
{
  nsCOMPtr<nsIObserverService> obs = do_GetService("@mozilla.org/observer-service;1");
  if (obs) {
    obs->AddObserver(this, "quit-application", false);
  }

  mTimeout = nsContentUtils::GetIntPref("geo.timeout", 6000);

  mProvider = do_GetService(NS_GEOLOCATION_PROVIDER_CONTRACTID);
  
  // if NS_MAEMO_LOCATION, see if we should try the MAEMO location provider
#ifdef NS_MAEMO_LOCATION
  if (!mProvider)
    mProvider = new MaemoLocationProvider();
#endif

}

nsGeolocationService::~nsGeolocationService()
{
}

NS_IMETHODIMP
nsGeolocationService::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const PRUnichar* aData)
{
  if (!strcmp("quit-application", aTopic))
  {
    nsCOMPtr<nsIObserverService> obs = do_GetService("@mozilla.org/observer-service;1");
    if (obs) {
      obs->RemoveObserver(this, "quit-application");
    }

    for (PRUint32 i = 0; i< mGeolocators.Length(); i++)
      mGeolocators[i]->Shutdown();

    StopDevice();

    return NS_OK;
  }
  
  if (!strcmp("timer-callback", aTopic))
  {
    // decide if we can close down the service.
    for (PRUint32 i = 0; i< mGeolocators.Length(); i++)
      if (mGeolocators[i]->HasActiveCallbacks())
      {
        SetDisconnectTimer();
        return NS_OK;
      }
    
    // okay to close up.
    StopDevice();
    Update(nsnull);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGeolocationService::Update(nsIDOMGeoPosition *aSomewhere)
{
  for (PRUint32 i = 0; i< mGeolocators.Length(); i++)
    mGeolocators[i]->Update(aSomewhere);
  return NS_OK;
}


void
nsGeolocationService::SetCachedPosition(nsIDOMGeoPosition* aPosition)
{
  mLastPosition = aPosition;
}

nsIDOMGeoPosition*
nsGeolocationService::GetCachedPosition()
{
  return mLastPosition;
}

PRBool
nsGeolocationService::HasGeolocationProvider()
{
  return (mProvider != nsnull);
}

nsresult
nsGeolocationService::StartDevice()
{
  if (!mProvider)
    return NS_ERROR_NOT_AVAILABLE;
  
  if (!mProviderStarted) {

    // if we have one, start it up.
    nsresult rv = mProvider->Startup();
    if (NS_FAILED(rv)) 
      return NS_ERROR_NOT_AVAILABLE;
  
    // lets monitor it for any changes.
    mProvider->Watch(this);

    // remember that we are started up
    mProviderStarted = PR_TRUE;

    // we do not want to keep the geolocation devices online
    // indefinitely.  Close them down after a reasonable period of
    // inactivivity
    SetDisconnectTimer();

  }

  return NS_OK;
}

void
nsGeolocationService::SetDisconnectTimer()
{
  if (!mDisconnectTimer)
    mDisconnectTimer = do_CreateInstance("@mozilla.org/timer;1");
  else
    mDisconnectTimer->Cancel();

  mDisconnectTimer->Init(this,
                         mTimeout,
                         nsITimer::TYPE_ONE_SHOT);
}

void 
nsGeolocationService::StopDevice()
{
  if (mProvider) {
    mProvider->Shutdown();
    mProviderStarted = PR_FALSE;
  }

  if(mDisconnectTimer) {
    mDisconnectTimer->Cancel();
    mDisconnectTimer = nsnull;
  }
}

nsGeolocationService* nsGeolocationService::gService = nsnull;

nsGeolocationService*
nsGeolocationService::GetInstance()
{
  if (!nsGeolocationService::gService) {
    nsGeolocationService::gService = new nsGeolocationService();
    NS_ASSERTION(nsGeolocationService::gService, "null nsGeolocationService.");
  }
  return nsGeolocationService::gService;
}

nsGeolocationService*
nsGeolocationService::GetGeolocationService()
{
  nsGeolocationService* inst = nsGeolocationService::GetInstance();
  NS_ADDREF(inst);
  return inst;
}

void
nsGeolocationService::AddLocator(nsGeolocation* aLocator)
{
  mGeolocators.AppendElement(aLocator);
}

void
nsGeolocationService::RemoveLocator(nsGeolocation* aLocator)
{
  mGeolocators.RemoveElement(aLocator);
}

////////////////////////////////////////////////////
// nsGeolocation
////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGeolocation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoGeolocation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoGeolocation)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoGeolocation)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGeolocation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGeolocation)
NS_IMPL_CYCLE_COLLECTION_CLASS(nsGeolocation)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGeolocation)
  tmp->mPendingCallbacks.Clear();
  tmp->mWatchingCallbacks.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsGeolocation)
  PRUint32 i; 
  for (i = 0; i < tmp->mPendingCallbacks.Length(); ++i)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mPendingCallbacks[i], nsIGeolocationRequest)

  for (i = 0; i < tmp->mWatchingCallbacks.Length(); ++i)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mWatchingCallbacks[i], nsIGeolocationRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsGeolocation::nsGeolocation(nsIDOMWindow* aContentDom) 
: mUpdateInProgress(PR_FALSE)
{
  // Remember the window
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aContentDom);
  if (window)
    mOwner = window->GetCurrentInnerWindow();

  // Grab the uri of the document
  nsCOMPtr<nsIDOMDocument> domdoc;
  aContentDom->GetDocument(getter_AddRefs(domdoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
  if (doc)
    doc->NodePrincipal()->GetURI(getter_AddRefs(mURI));

  mService = nsGeolocationService::GetInstance();
  if (mService)
    mService->AddLocator(this);
}

nsGeolocation::~nsGeolocation()
{
}

void
nsGeolocation::Shutdown()
{
  // Shutdown and release all callbacks
  for (PRUint32 i = 0; i< mPendingCallbacks.Length(); i++)
    mPendingCallbacks[i]->Shutdown();
  mPendingCallbacks.Clear();

  for (PRUint32 i = 0; i< mWatchingCallbacks.Length(); i++)
    mWatchingCallbacks[i]->Shutdown();
  mWatchingCallbacks.Clear();

  if (mService)
    mService->RemoveLocator(this);

  mService = nsnull;
  mOwner = nsnull;
  mURI = nsnull;
}

PRBool
nsGeolocation::HasActiveCallbacks()
{
  return mWatchingCallbacks.Length() != 0;
}

void
nsGeolocation::RemoveRequest(nsGeolocationRequest* aRequest)
{
  mPendingCallbacks.RemoveElement(aRequest);

  // if it is in the mWatchingCallbacks, we can't do much
  // since we passed back the position in the array to who
  // ever called WatchPosition() and we do not want to mess
  // around with the ordering of the array.  Instead, just
  // mark the request as "cleared".

  aRequest->MarkCleared();
}

void
nsGeolocation::Update(nsIDOMGeoPosition *aSomewhere)
{
  // This method calls out to objects which may spin and
  // event loop which may add new location objects into
  // mPendingCallbacks, and mWatchingCallbacks.  Since this
  // function can only be called on the primary thread, we
  // can lock this method with a member var.

  if (mUpdateInProgress)
    return;

  mUpdateInProgress = PR_TRUE;

  if (aSomewhere)
  {
    nsRefPtr<nsGeolocationService> geoService = nsGeolocationService::GetInstance();
    geoService->SetCachedPosition(aSomewhere);
  }


  if (!OwnerStillExists())
  {
    Shutdown();
    return;
  }

  // notify anyone that has been waiting
  for (PRUint32 i = 0; i< mPendingCallbacks.Length(); i++)
    mPendingCallbacks[i]->SendLocation(aSomewhere);
  mPendingCallbacks.Clear();

  // notify everyone that is watching
  for (PRUint32 i = 0; i< mWatchingCallbacks.Length(); i++)
    mWatchingCallbacks[i]->SendLocation(aSomewhere);

  mUpdateInProgress = PR_FALSE;
}

NS_IMETHODIMP
nsGeolocation::GetCurrentPosition(nsIDOMGeoPositionCallback *callback,
                                  nsIDOMGeoPositionErrorCallback *errorCallback,
                                  nsIDOMGeoPositionOptions *options)
{
  nsCOMPtr<nsIGeolocationPrompt> prompt = do_GetService(NS_GEOLOCATION_PROMPT_CONTRACTID);
  if (prompt == nsnull)
    return NS_ERROR_NOT_AVAILABLE;

  if (mPendingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW)
    return NS_ERROR_NOT_AVAILABLE;

  nsRefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(this, callback, errorCallback, options);
  if (!request)
    return NS_ERROR_OUT_OF_MEMORY;

  if (NS_FAILED(request->Init()))
    return NS_OK;

  prompt->Prompt(request);

  // What if you have a location provider that only sends a location once, then stops.?  fix.
  mPendingCallbacks.AppendElement(request);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::WatchPosition(nsIDOMGeoPositionCallback *aCallback,
                             nsIDOMGeoPositionErrorCallback *aErrorCallback,
                             nsIDOMGeoPositionOptions *aOptions, 
                             PRInt32 *_retval NS_OUTPARAM)
{
  nsCOMPtr<nsIGeolocationPrompt> prompt = do_GetService(NS_GEOLOCATION_PROMPT_CONTRACTID);
  if (prompt == nsnull)
    return NS_ERROR_NOT_AVAILABLE;

  if (mWatchingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW)
    return NS_ERROR_NOT_AVAILABLE;

  nsRefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(this, aCallback, aErrorCallback, aOptions);
  if (!request)
    return NS_ERROR_OUT_OF_MEMORY;

  if (NS_FAILED(request->Init()))
    return NS_OK;

  prompt->Prompt(request);

  // need to hand back an index/reference.
  mWatchingCallbacks.AppendElement(request);
  *_retval = mWatchingCallbacks.Length() - 1;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::ClearWatch(PRInt32 aWatchId)
{
  PRUint32 count = mWatchingCallbacks.Length();
  if (aWatchId < 0 || count == 0 || aWatchId > count)
    return NS_ERROR_FAILURE;

  mWatchingCallbacks[aWatchId]->MarkCleared();
  return NS_OK;
}

PRBool
nsGeolocation::OwnerStillExists()
{
  if (!mOwner)
    return PR_FALSE;

  nsCOMPtr<nsIDOMWindowInternal> domWindow(mOwner);
  if (domWindow)
  {
    PRBool closed = PR_FALSE;
    domWindow->GetClosed(&closed);
    if (closed)
      return PR_FALSE;
  }

  nsPIDOMWindow* outer = mOwner->GetOuterWindow();
  if (!outer || outer->GetCurrentInnerWindow() != mOwner)
    return PR_FALSE;

  return PR_TRUE;
}
