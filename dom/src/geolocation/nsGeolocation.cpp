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

#ifdef NS_OSSO
#include "MaemoLocationProvider.h"
#endif

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
////////////////////////////////////////////////////
// nsGeolocationRequest
////////////////////////////////////////////////////

nsGeolocationRequest::nsGeolocationRequest(nsGeolocator* locator, nsIDOMGeolocationCallback* callback)
  : mAllowed(PR_FALSE), mCleared(PR_FALSE), mFuzzLocation(PR_FALSE), mCallback(callback), mLocator(locator)
{
}

nsGeolocationRequest::~nsGeolocationRequest()
{
}

NS_INTERFACE_MAP_BEGIN(nsGeolocationRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGeolocationRequest)
  NS_INTERFACE_MAP_ENTRY(nsIGeolocationRequest)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsGeolocationRequest)
NS_IMPL_RELEASE(nsGeolocationRequest)

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
  // pass a null back.
  mCallback->OnRequest(nsnull);

  // remove ourselves from the locators callback lists.
  mLocator->RemoveRequest(this);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::Allow()
{
  // Kick off the geo device, if it isn't already running
  nsRefPtr<nsGeolocatorService> geoService = nsGeolocatorService::GetInstance();
  geoService->StartDevice();

  mAllowed = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::AllowButFuzz()
{
  mFuzzLocation = PR_TRUE;
  return Allow();
}

void
nsGeolocationRequest::MarkCleared()
{
  mCleared = PR_TRUE;
}

void
nsGeolocationRequest::SendLocation(nsIDOMGeolocation* location)
{
  if (mCleared)
    return;

  //TODO mFuzzLocation.  Needs to be defined what we do here.
  if (mFuzzLocation)
  {
    // need to make a copy because nsIDOMGeolocation is
    // readonly, and we are not sure of its implementation.

    double lat, lon, alt, herror, verror;
    DOMTimeStamp time;
    location->GetLatitude(&lat);
    location->GetLongitude(&lon);
    location->GetAltitude(&alt);
    location->GetHorizontalAccuracy(&herror);
    location->GetVerticalAccuracy(&verror);
    location->GetTimestamp(&time); 

    // do something to the numbers...  TODO.
    
    // mask out any location until we figure out how to fuzz locations;
    lat = 0; lon = 0; alt = 0; herror = 100000; verror = 100000;

    nsRefPtr<nsGeolocation> somewhere = new nsGeolocation(lat,
                                                          lon,
                                                          alt,
                                                          herror,
                                                          verror,
                                                          time);
    mCallback->OnRequest(somewhere);
    return;
  }
  
  mCallback->OnRequest(location);
}

void
nsGeolocationRequest::Shutdown()
{
  mCleared = PR_TRUE;
  mCallback = nsnull;
}

////////////////////////////////////////////////////
// nsGeolocation
////////////////////////////////////////////////////
NS_INTERFACE_MAP_BEGIN(nsGeolocation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeolocation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeolocation)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Geolocation)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsGeolocation)
NS_IMPL_THREADSAFE_RELEASE(nsGeolocation)

NS_IMETHODIMP
nsGeolocation::GetLatitude(double *aLatitude)
{
  *aLatitude = mLat;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetLongitude(double *aLongitude)
{
  *aLongitude = mLong;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetAltitude(double *aAltitude)
{
  *aAltitude = mAlt;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetHorizontalAccuracy(double *aHorizontalAccuracy)
{
  *aHorizontalAccuracy = mHError;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetVerticalAccuracy(double *aVerticalAccuracy)
{
  *aVerticalAccuracy = mVError;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::GetTimestamp(DOMTimeStamp* aTimestamp)
{
  *aTimestamp = mTimestamp;
  return NS_OK;
}

////////////////////////////////////////////////////
// nsGeolocatorService
////////////////////////////////////////////////////
NS_INTERFACE_MAP_BEGIN(nsGeolocatorService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGeolocationUpdate)
  NS_INTERFACE_MAP_ENTRY(nsIGeolocationUpdate)
  NS_INTERFACE_MAP_ENTRY(nsIGeolocationService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsGeolocatorService)
NS_IMPL_THREADSAFE_RELEASE(nsGeolocatorService)

nsGeolocatorService::nsGeolocatorService()
{
  nsCOMPtr<nsIObserverService> obs = do_GetService("@mozilla.org/observer-service;1");
  if (obs) {
    obs->AddObserver(this, "quit-application", false);
  }

  mTimeout = nsContentUtils::GetIntPref("geo.timeout", 6000);
}

nsGeolocatorService::~nsGeolocatorService()
{
}

NS_IMETHODIMP
nsGeolocatorService::Observe(nsISupports* aSubject, const char* aTopic,
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

    // Remove our reference to any prompt that may have been set.
    mPrompt = nsnull;
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
nsGeolocatorService::GetPrompt(nsIGeolocationPrompt * *aPrompt)
{
  NS_ENSURE_ARG_POINTER(aPrompt);
  *aPrompt = mPrompt;
  NS_IF_ADDREF(*aPrompt);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocatorService::SetPrompt(nsIGeolocationPrompt * aPrompt)
{
  mPrompt = aPrompt;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocatorService::Update(nsIDOMGeolocation *somewhere)
{
  for (PRUint32 i = 0; i< mGeolocators.Length(); i++)
    mGeolocators[i]->Update(somewhere);
  return NS_OK;
}

already_AddRefed<nsIDOMGeolocation>
nsGeolocatorService::GetLastKnownPosition()
{
  nsIDOMGeolocation* location = nsnull;
  if (mProvider)
    mProvider->GetCurrentLocation(&location);

  return location;
}

PRBool
nsGeolocatorService::IsDeviceReady()
{
  PRBool ready = PR_FALSE;
  if (mProvider)
    mProvider->IsReady(&ready);

  return ready;
}

nsresult
nsGeolocatorService::StartDevice()
{
  if (!mProvider)
  {
    // Check to see if there is an override in place. if so, use it.
    mProvider = do_GetService(NS_GEOLOCATION_PROVIDER_CONTRACTID);

    // if NS_OSSO, see if we should try the MAEMO location provider
#ifdef NS_OSSO
    if (!mProvider)
    {
      // guess not, lets try a default one:  
      mProvider = new MaemoLocationProvider();
    }
#endif

    if (!mProvider)
      return NS_ERROR_NOT_AVAILABLE;
    
    // if we have one, start it up.
    nsresult rv = mProvider->Startup();
    if (NS_FAILED(rv)) 
      return NS_ERROR_NOT_AVAILABLE;;
    
    // lets monitor it for any changes.
    mProvider->Watch(this);
    
    // we do not want to keep the geolocation devices online
    // indefinitely.  Close them down after a reasonable period of
    // inactivivity
    SetDisconnectTimer();
  }

  return NS_OK;
}

void
nsGeolocatorService::SetDisconnectTimer()
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
nsGeolocatorService::StopDevice()
{
  if (mProvider) {
    mProvider->Shutdown();
    mProvider = nsnull;
  }

  if(mDisconnectTimer) {
    mDisconnectTimer->Cancel();
    mDisconnectTimer = nsnull;
  }
}

nsGeolocatorService* nsGeolocatorService::gService = nsnull;

nsGeolocatorService*
nsGeolocatorService::GetInstance()
{
  if (!nsGeolocatorService::gService) {
    nsGeolocatorService::gService = new nsGeolocatorService();
  }
  return nsGeolocatorService::gService;
}

nsGeolocatorService*
nsGeolocatorService::GetGeolocationService()
{
  nsGeolocatorService* inst = nsGeolocatorService::GetInstance();
  NS_ADDREF(inst);
  return inst;
}

void
nsGeolocatorService::AddLocator(nsGeolocator* locator)
{
  mGeolocators.AppendElement(locator);
}

void
nsGeolocatorService::RemoveLocator(nsGeolocator* locator)
{
  mGeolocators.RemoveElement(locator);
}

////////////////////////////////////////////////////
// nsGeolocator
////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN(nsGeolocator)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeolocator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeolocator)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Geolocator)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsGeolocator)
NS_IMPL_RELEASE(nsGeolocator)

nsGeolocator::nsGeolocator(nsIDOMWindow* contentDom) 
: mUpdateInProgress(PR_FALSE)
{
  // Remember the window
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(contentDom);
  if (window)
    mOwner = window->GetCurrentInnerWindow();

  // Grab the uri of the document
  nsCOMPtr<nsIDOMDocument> domdoc;
  contentDom->GetDocument(getter_AddRefs(domdoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
  if (doc)
    doc->NodePrincipal()->GetURI(getter_AddRefs(mURI));

  mService = nsGeolocatorService::GetInstance();
  if (mService)
    mService->AddLocator(this);
}

nsGeolocator::~nsGeolocator()
{
}

void
nsGeolocator::Shutdown()
{
  // Shutdown and release all callbacks
  for (PRInt32 i = 0; i< mPendingCallbacks.Count(); i++)
    mPendingCallbacks[i]->Shutdown();
  mPendingCallbacks.Clear();

  for (PRInt32 i = 0; i< mWatchingCallbacks.Count(); i++)
    mWatchingCallbacks[i]->Shutdown();
  mWatchingCallbacks.Clear();

  if (mService)
    mService->RemoveLocator(this);

  mService = nsnull;
  mOwner = nsnull;
  mURI = nsnull;
}

PRBool
nsGeolocator::HasActiveCallbacks()
{
  return (PRBool) mWatchingCallbacks.Count();
}

void
nsGeolocator::RemoveRequest(nsGeolocationRequest* request)
{
  mPendingCallbacks.RemoveObject(request);

  // if it is in the mWatchingCallbacks, we can't do much
  // since we passed back the position in the array to who
  // ever called WatchPosition() and we do not want to mess
  // around with the ordering of the array.  Instead, just
  // mark the request as "cleared".

  request->MarkCleared();
}

void
nsGeolocator::Update(nsIDOMGeolocation *somewhere)
{
  // This method calls out to objects which may spin and
  // event loop which may add new location objects into
  // mPendingCallbacks, and mWatchingCallbacks.  Since this
  // function can only be called on the primary thread, we
  // can lock this method with a member var.

  if (mUpdateInProgress)
    return;

  mUpdateInProgress = PR_TRUE;
  if (!OwnerStillExists())
  {
    Shutdown();
    return;
  }

  // notify anyone that has been waiting
  for (PRInt32 i = 0; i< mPendingCallbacks.Count(); i++)
    mPendingCallbacks[i]->SendLocation(somewhere);
  mPendingCallbacks.Clear();

  // notify everyone that is watching
  for (PRInt32 i = 0; i< mWatchingCallbacks.Count(); i++)
      mWatchingCallbacks[i]->SendLocation(somewhere);

  mUpdateInProgress = PR_FALSE;
}

NS_IMETHODIMP
nsGeolocator::GetLastPosition(nsIDOMGeolocation * *aLastPosition)
{
  // we are advocating that this method be removed.
  NS_ENSURE_ARG_POINTER(aLastPosition);
  *aLastPosition = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocator::GetCurrentPosition(nsIDOMGeolocationCallback *callback)
{
  nsIGeolocationPrompt* prompt = mService->GetPrompt();
  if (prompt == nsnull)
    return NS_ERROR_NOT_AVAILABLE;

  nsRefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(this, callback);
  prompt->Prompt(request);

  // What if you have a location provider that only sends a location once, then stops.?  fix.
  mPendingCallbacks.AppendObject(request);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocator::WatchPosition(nsIDOMGeolocationCallback *callback, PRUint16 *_retval NS_OUTPARAM)
{
  nsIGeolocationPrompt* prompt = mService->GetPrompt();
  if (prompt == nsnull)
    return NS_ERROR_NOT_AVAILABLE;
    
  nsRefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(this, callback);
  prompt->Prompt(request);

  // need to hand back an index/reference.
  mWatchingCallbacks.AppendObject(request);
  *_retval = mWatchingCallbacks.Count() - 1;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocator::ClearWatch(PRUint16 watchId)
{
  mWatchingCallbacks[watchId]->MarkCleared();
  return NS_OK;
}

PRBool
nsGeolocator::OwnerStillExists()
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
