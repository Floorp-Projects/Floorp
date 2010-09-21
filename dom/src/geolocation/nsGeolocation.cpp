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
 * The Initial Developer of the Original Code is Mozilla Foundation
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

#ifdef MOZ_IPC
#include "nsContentPermissionHelper.h"
#include "nsXULAppAPI.h"

#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/ContentChild.h"
#include "nsNetUtil.h"

#include "nsFrameManager.h"
#include "nsFrameLoader.h"
#include "nsIFrameLoader.h"

#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebProgressListener2.h"

#include "nsDOMEventTargetHelper.h"
#include "TabChild.h"
#endif

#include "nsGeolocation.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsDOMClassInfo.h"
#include "nsComponentManagerUtils.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsIPermissionManager.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsIJSContextStack.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"

#include <math.h>

#ifdef WINCE_WINDOWS_MOBILE
#include "WinMobileLocationProvider.h"
#endif

#ifdef MOZ_MAEMO_LIBLOCATION
#include "MaemoLocationProvider.h"
#endif

#ifdef ANDROID
#include "AndroidLocationProvider.h"
#endif

#include "nsIDOMDocument.h"
#include "nsIDocument.h"

// Some limit to the number of get or watch geolocation requests
// that a window can make.
#define MAX_GEO_REQUESTS_PER_WINDOW  1500

using mozilla::unused;          // <snicker>
using namespace mozilla::dom;

class RequestPromptEvent : public nsRunnable
{
public:
  RequestPromptEvent(nsGeolocationRequest* request)
    : mRequest(request)
  {
  }

  NS_IMETHOD Run() {
    nsCOMPtr<nsIContentPermissionPrompt> prompt = do_GetService(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
    if (prompt) {
      prompt->Prompt(mRequest);
    }
    return NS_OK;
  }

private:
  nsRefPtr<nsGeolocationRequest> mRequest;
};

class RequestAllowEvent : public nsRunnable
{
public:
  RequestAllowEvent(int allow, nsGeolocationRequest* request)
    : mAllow(allow),
      mRequest(request)
  {
  }

  NS_IMETHOD Run() {
    if (mAllow)
      mRequest->Allow();
    else
      mRequest->Cancel();
    return NS_OK;
  }

private:
  PRBool mAllow;
  nsRefPtr<nsGeolocationRequest> mRequest;
};

class RequestSendLocationEvent : public nsRunnable
{
public:
  // a bit funky.  if locator is passed, that means this
  // event should remove the request from it.  If we ever
  // have to do more, then we can change this around.
  RequestSendLocationEvent(nsIDOMGeoPosition* aPosition,
                           nsGeolocationRequest* aRequest,
                           nsGeolocation* aLocator = nsnull)
    : mPosition(aPosition),
      mRequest(aRequest),
      mLocator(aLocator)
  {
  }

  NS_IMETHOD Run() {
    mRequest->SendLocation(mPosition);
    if (mLocator)
      mLocator->RemoveRequest(mRequest);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMGeoPosition>    mPosition;
  nsRefPtr<nsGeolocationRequest> mRequest;

  nsRefPtr<nsGeolocation>        mLocator;
};

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

DOMCI_DATA(GeoPositionError, nsDOMGeoPositionError)

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
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGeolocationRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGeolocationRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGeolocationRequest)

NS_IMPL_CYCLE_COLLECTION_4(nsGeolocationRequest, mCallback, mErrorCallback, mOptions, mLocator)


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
  
  NotifyError(nsIDOMGeoPositionError::TIMEOUT);
  // remove ourselves from the locator's callback lists.
  mLocator->RemoveRequest(this);

  mTimeoutTimer = nsnull;
  return NS_OK;
}
 
NS_IMETHODIMP
nsGeolocationRequest::GetUri(nsIURI * *aRequestingURI)
{
  NS_ENSURE_ARG_POINTER(aRequestingURI);

  nsCOMPtr<nsIURI> uri = mLocator->GetURI();
  uri.forget(aRequestingURI);

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::GetType(nsACString & aType)
{
  aType = "geolocation";
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::GetWindow(nsIDOMWindow * *aRequestingWindow)
{
  NS_ENSURE_ARG_POINTER(aRequestingWindow);

  nsCOMPtr<nsIDOMWindow> window = do_QueryReferent(mLocator->GetOwner());
  window.forget(aRequestingWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::GetElement(nsIDOMElement * *aRequestingElement)
{
  NS_ENSURE_ARG_POINTER(aRequestingElement);
  *aRequestingElement = nsnull;
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
      if (tempAge >= 0)
        maximumAge = tempAge;
    }
  }

  if (lastPosition && maximumAge > 0 &&
      ( PRTime(PR_Now() / PR_USEC_PER_MSEC) - maximumAge <=
        PRTime(cachedPositionTime) )) {
    // okay, we can return a cached position
    mAllowed = PR_TRUE;
    
    nsCOMPtr<nsIRunnable> ev = new RequestSendLocationEvent(lastPosition, this, mLocator);
    NS_DispatchToMainThread(ev);
  }

  SetTimeoutTimer();

  mAllowed = PR_TRUE;
  return NS_OK;
}

void
nsGeolocationRequest::SetTimeoutTimer()
{
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nsnull;
  }
  PRInt32 timeout;
  if (mOptions && NS_SUCCEEDED(mOptions->GetTimeout(&timeout)) && timeout > 0) {
    
    if (timeout < 10)
      timeout = 10;

    mTimeoutTimer = do_CreateInstance("@mozilla.org/timer;1");
    mTimeoutTimer->InitWithCallback(this, timeout, nsITimer::TYPE_ONE_SHOT);
  }
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

  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nsnull;
  }

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

  SetTimeoutTimer();
}

void
nsGeolocationRequest::Shutdown()
{
  mCleared = PR_TRUE;
  mCallback = nsnull;
  mErrorCallback = nsnull;
}

#ifdef MOZ_IPC
bool nsGeolocationRequest::Recv__delete__(const bool& allow)
{
  if (allow)
    (void) Allow();
  else
    (void) Cancel();
  return true;
}
#endif
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


static PRBool sGeoEnabled = PR_TRUE;
static PRBool sGeoIgnoreLocationFilter = PR_FALSE;

static int
GeoEnabledChangedCallback(const char *aPrefName, void *aClosure)
{
  sGeoEnabled = nsContentUtils::GetBoolPref("geo.enabled", PR_TRUE);
  return 0;
}

static int
GeoIgnoreLocationFilterChangedCallback(const char *aPrefName, void *aClosure)
{
  sGeoIgnoreLocationFilter = nsContentUtils::GetBoolPref("geo.ignore.location_filter",
                                                         PR_TRUE);
  return 0;
}


nsresult nsGeolocationService::Init()
{
  mTimeout = nsContentUtils::GetIntPref("geo.timeout", 6000);

  nsContentUtils::RegisterPrefCallback("geo.ignore.location_filter",
                                       GeoIgnoreLocationFilterChangedCallback,
                                       nsnull);

  GeoIgnoreLocationFilterChangedCallback("geo.ignore.location_filter", nsnull);


  nsContentUtils::RegisterPrefCallback("geo.enabled",
                                       GeoEnabledChangedCallback,
                                       nsnull);

  GeoEnabledChangedCallback("geo.enabled", nsnull);

  if (!sGeoEnabled)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIGeolocationProvider> provider = do_GetService(NS_GEOLOCATION_PROVIDER_CONTRACTID);
  if (provider)
    mProviders.AppendObject(provider);

  // look up any providers that were registered via the category manager
  nsCOMPtr<nsICategoryManager> catMan(do_GetService("@mozilla.org/categorymanager;1"));
  if (!catMan)
    return NS_ERROR_FAILURE;

  // geolocation service can be enabled -> now register observer
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs)
    return NS_ERROR_FAILURE;

  obs->AddObserver(this, "quit-application", false);

  nsCOMPtr<nsISimpleEnumerator> geoproviders;
  catMan->EnumerateCategory("geolocation-provider", getter_AddRefs(geoproviders));
  if (geoproviders) {

    PRBool hasMore;
    while (NS_SUCCEEDED(geoproviders->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> elem;
      geoproviders->GetNext(getter_AddRefs(elem));

      nsCOMPtr<nsISupportsCString> elemString = do_QueryInterface(elem);
      
      nsCAutoString name;
      elemString->GetData(name);

      nsXPIDLCString spec;
      catMan->GetCategoryEntry("geolocation-provider", name.get(), getter_Copies(spec));

      provider = do_GetService(spec);
      if (provider)
        mProviders.AppendObject(provider);
    }
  }

  // we should move these providers outside of this file! dft

#ifdef WINCE_WINDOWS_MOBILE
  provider = new WinMobileLocationProvider();
  if (provider)
    mProviders.AppendObject(provider);
#endif

#ifdef MOZ_MAEMO_LIBLOCATION
  provider = new MaemoLocationProvider();
  if (provider)
    mProviders.AppendObject(provider);
#endif

#ifdef ANDROID
  provider = new AndroidLocationProvider();
  if (provider)
    mProviders.AppendObject(provider);
#endif
  return NS_OK;
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
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
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
  // here we have to determine this aSomewhere is a "better"
  // position than any previously recv'ed.

  if (!IsBetterPosition(aSomewhere))
    return NS_OK;

  SetCachedPosition(aSomewhere);

  for (PRUint32 i = 0; i< mGeolocators.Length(); i++)
    mGeolocators[i]->Update(aSomewhere);
  return NS_OK;
}

PRBool
nsGeolocationService::IsBetterPosition(nsIDOMGeoPosition *aSomewhere)
{
  if (!aSomewhere)
    return PR_FALSE;

  nsRefPtr<nsGeolocationService> geoService = nsGeolocationService::GetInstance();
  if (!geoService)
    return PR_FALSE;

  nsCOMPtr<nsIDOMGeoPosition> lastPosition = geoService->GetCachedPosition();
  if (!lastPosition)
    return PR_TRUE;
  
  nsresult rv;
  DOMTimeStamp oldTime;
  rv = lastPosition->GetTimestamp(&oldTime);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  nsCOMPtr<nsIDOMGeoPositionCoords> coords;
  lastPosition->GetCoords(getter_AddRefs(coords));
  if (!coords)
    return PR_FALSE;

  double oldAccuracy;
  rv = coords->GetAccuracy(&oldAccuracy);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  double oldLat, oldLon;
  rv = coords->GetLongitude(&oldLon);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  rv = coords->GetLatitude(&oldLat);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);


  DOMTimeStamp newTime;
  rv = aSomewhere->GetTimestamp(&newTime);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  aSomewhere->GetCoords(getter_AddRefs(coords));
  if (!coords)
    return PR_FALSE;

  double newAccuracy;
  rv = coords->GetAccuracy(&newAccuracy);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  double newLat, newLon;
  rv = coords->GetLongitude(&newLon);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  rv = coords->GetLatitude(&newLat);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  // check to see if there has been a large movement
  // Use spherical law of cosines to calculate difference
  // Not quite as correct as the Haversine but simpler and cheaper
  double radsInDeg = 3.14159265 / 180.0;

  double rNewLat = newLat * radsInDeg;
  double rNewLon = newLon * radsInDeg;
  double rOldLat = oldLat * radsInDeg;
  double rOldLon = oldLon * radsInDeg;

  // WGS84 equatorial radius of earth = 6378137m
  double delta = acos( (sin(rNewLat) * sin(rOldLat)) + (cos(rNewLat) * cos(rOldLat) * cos(rOldLon - rNewLon)) ) * 6378137; 

  // The threshold is when the distance between the two positions exceeds the
  // worse (larger value) of the two accuracies.
  double max_accuracy = NS_MAX(oldAccuracy, newAccuracy);
  if (delta > max_accuracy)
    return PR_TRUE;

  // check to see if the aSomewhere position is more accurate
  if (oldAccuracy >= newAccuracy)
    return PR_TRUE;

  return PR_FALSE;
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
  return mProviders.Count() > 0;
}

nsresult
nsGeolocationService::StartDevice()
{
  if (!sGeoEnabled)
    return NS_ERROR_NOT_AVAILABLE;

  if (!HasGeolocationProvider())
    return NS_ERROR_NOT_AVAILABLE;
  
  // if we have one, start it up.

  // Start them up!
  nsresult rv = NS_ERROR_NOT_AVAILABLE;
  for (PRUint32 i = mProviders.Count() - 1; i != PRUint32(-1); --i) {
    // If any provder gets started without error, go ahead
    // and proceed without error
    nsresult temp = mProviders[i]->Startup();
    if (NS_SUCCEEDED(temp)) {
      rv = NS_OK;

      mProviders[i]->Watch(this);
    }
  }
  
  if (NS_FAILED(rv)) 
    return NS_ERROR_NOT_AVAILABLE;
  
  // we do not want to keep the geolocation devices online
  // indefinitely.  Close them down after a reasonable period of
  // inactivivity
  SetDisconnectTimer();

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
  for (PRUint32 i = mProviders.Count() - 1; i != PRUint32(-1); --i) {
    mProviders[i]->Shutdown();
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

    if (nsGeolocationService::gService) {
      if (NS_FAILED(nsGeolocationService::gService->Init())) {
        delete nsGeolocationService::gService;
        nsGeolocationService::gService = nsnull;
      }        
    }
  }
  return nsGeolocationService::gService;
}

nsGeolocationService*
nsGeolocationService::GetGeolocationService()
{
  nsGeolocationService* inst = nsGeolocationService::GetInstance();
  NS_IF_ADDREF(inst);
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

DOMCI_DATA(GeoGeolocation, nsGeolocation)

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
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mPendingCallbacks[i], nsIContentPermissionRequest)

  for (i = 0; i < tmp->mWatchingCallbacks.Length(); ++i)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mWatchingCallbacks[i], nsIContentPermissionRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsGeolocation::nsGeolocation() 
{
}

nsGeolocation::~nsGeolocation()
{
  if (mService)
    Shutdown();
}

nsresult
nsGeolocation::Init(nsIDOMWindow* aContentDom)
{
  // Remember the window
  if (aContentDom) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aContentDom);
    if (!window)
      return NS_ERROR_FAILURE;

    mOwner = do_GetWeakReference(window->GetCurrentInnerWindow());
    if (!mOwner)
      return NS_ERROR_FAILURE;

    // Grab the uri of the document
    nsCOMPtr<nsIDOMDocument> domdoc;
    aContentDom->GetDocument(getter_AddRefs(domdoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
    if (!doc)
      return NS_ERROR_FAILURE;

    doc->NodePrincipal()->GetURI(getter_AddRefs(mURI));
    
    if (!mURI)
      return NS_ERROR_FAILURE;
  }

  // If no aContentDom was passed into us, we are being used
  // by chrome/c++ and have no mOwner, no mURI, and no need
  // to prompt.
  mService = nsGeolocationService::GetInstance();
  if (mService)
    mService->AddLocator(this);

  return NS_OK;
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
  if (!WindowOwnerStillExists())
    return Shutdown();

  for (PRUint32 i = 0; i< mPendingCallbacks.Length(); i++) {
    nsCOMPtr<nsIRunnable> ev  = new RequestSendLocationEvent(aSomewhere,
                                                             mPendingCallbacks[i]);
    NS_DispatchToMainThread(ev);
  }
  mPendingCallbacks.Clear();

  // notify everyone that is watching
  for (PRUint32 i = 0; i< mWatchingCallbacks.Length(); i++) {
    nsCOMPtr<nsIRunnable> ev  = new RequestSendLocationEvent(aSomewhere, mWatchingCallbacks[i]);
    NS_DispatchToMainThread(ev);
  }
}

NS_IMETHODIMP
nsGeolocation::GetCurrentPosition(nsIDOMGeoPositionCallback *callback,
                                  nsIDOMGeoPositionErrorCallback *errorCallback,
                                  nsIDOMGeoPositionOptions *options)
{
  NS_ENSURE_ARG_POINTER(callback);

  if (!sGeoEnabled)
    return NS_ERROR_NOT_AVAILABLE;

  if (mPendingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW)
    return NS_ERROR_NOT_AVAILABLE;

  nsRefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(this, callback, errorCallback, options);
  if (!request)
    return NS_ERROR_OUT_OF_MEMORY;

  if (NS_FAILED(request->Init()))
    return NS_ERROR_FAILURE; // this as OKAY.  not sure why we wouldn't throw. xxx dft

  if (mOwner) {
    RegisterRequestWithPrompt(request);
    mPendingCallbacks.AppendElement(request);
    return NS_OK;
  }

  if (!nsContentUtils::IsCallerChrome())
    return NS_ERROR_FAILURE;

  mPendingCallbacks.AppendElement(request);

  nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(true, request);
  NS_DispatchToMainThread(ev);

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::WatchPosition(nsIDOMGeoPositionCallback *callback,
                             nsIDOMGeoPositionErrorCallback *errorCallback,
                             nsIDOMGeoPositionOptions *options,
                             PRInt32 *_retval NS_OUTPARAM)
{

  NS_ENSURE_ARG_POINTER(callback);

  if (!sGeoEnabled)
    return NS_ERROR_NOT_AVAILABLE;

  if (mPendingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW)
    return NS_ERROR_NOT_AVAILABLE;

  nsRefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(this, callback, errorCallback, options);
  if (!request)
    return NS_ERROR_OUT_OF_MEMORY;

  if (NS_FAILED(request->Init()))
    return NS_ERROR_FAILURE; // this as OKAY.  not sure why we wouldn't throw. xxx dft

  if (mOwner) {
    RegisterRequestWithPrompt(request);

    // need to hand back an index/reference.
    mWatchingCallbacks.AppendElement(request);
    *_retval = mWatchingCallbacks.Length() - 1;

    return NS_OK;
  }

  if (!nsContentUtils::IsCallerChrome())
    return NS_ERROR_FAILURE;

  request->Allow();

  // need to hand back an index/reference.
  mWatchingCallbacks.AppendElement(request);
  *_retval = mWatchingCallbacks.Length() - 1;

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::ClearWatch(PRInt32 aWatchId)
{
  PRUint32 count = mWatchingCallbacks.Length();
  if (aWatchId < 0 || count == 0 || PRUint32(aWatchId) > count)
    return NS_OK;

  mWatchingCallbacks[aWatchId]->MarkCleared();
  return NS_OK;
}

PRBool
nsGeolocation::WindowOwnerStillExists()
{
  // an owner was never set when nsGeolocation
  // was created, which means that this object
  // is being used without a window.
  if (mOwner == nsnull)
    return PR_TRUE;

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mOwner);

  if (window)
  {
    PRBool closed = PR_FALSE;
    window->GetClosed(&closed);
    if (closed)
      return PR_FALSE;

    nsPIDOMWindow* outer = window->GetOuterWindow();
    if (!outer || outer->GetCurrentInnerWindow() != window)
      return PR_FALSE;
  }

  return PR_TRUE;
}

void
nsGeolocation::RegisterRequestWithPrompt(nsGeolocationRequest* request)
{
#ifdef MOZ_IPC
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mOwner);
    if (!window)
      return;

    // because owner implements nsITabChild, we can assume that it is
    // the one and only TabChild.
    TabChild* child = GetTabChildFrom(window->GetDocShell());
    
    // Retain a reference so the object isn't deleted without IPDL's knowledge.
    // Corresponding release occurs in DeallocPContentPermissionRequest.
    request->AddRef();

    nsCString type = NS_LITERAL_CSTRING("geolocation");
    child->SendPContentPermissionRequestConstructor(request, type, IPC::URI(mURI));
    
    request->Sendprompt();
    return;
  }
#endif

  if (nsContentUtils::GetBoolPref("geo.prompt.testing", PR_FALSE))
  {
    nsCOMPtr<nsIRunnable> ev  = new RequestAllowEvent(nsContentUtils::GetBoolPref("geo.prompt.testing.allow", PR_FALSE), request);
    NS_DispatchToMainThread(ev);
    return;
  }

  nsCOMPtr<nsIRunnable> ev  = new RequestPromptEvent(request);
  NS_DispatchToMainThread(ev);
}

#if !defined(WINCE_WINDOWS_MOBILE) && !defined(MOZ_MAEMO_LIBLOCATION) && !defined(ANDROID)
DOMCI_DATA(GeoPositionCoords, void)
DOMCI_DATA(GeoPosition, void)
#endif

