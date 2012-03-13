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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Doug Turner <dougt@dougt.org>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsDeviceMotion.h"

#include "nsAutoPtr.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIServiceManager.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMDeviceOrientationEvent.h"
#include "nsIDOMDeviceMotionEvent.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsDOMDeviceMotionEvent.h"

static const nsTArray<nsIDOMWindow*>::index_type NoIndex =
    nsTArray<nsIDOMWindow*>::NoIndex;

class nsDeviceMotionData : public nsIDeviceMotionData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDEVICEMOTIONDATA

  nsDeviceMotionData(unsigned long type, double x, double y, double z);

private:
  ~nsDeviceMotionData();

protected:
  unsigned long mType;
  double mX, mY, mZ;
};

nsDeviceMotionData::nsDeviceMotionData(unsigned long type, double x, double y, double z)
  : mType(type), mX(x), mY(y), mZ(z)
{
}

NS_INTERFACE_MAP_BEGIN(nsDeviceMotionData)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDeviceMotionData)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDeviceMotionData)
NS_IMPL_RELEASE(nsDeviceMotionData)

nsDeviceMotionData::~nsDeviceMotionData()
{
}

NS_IMETHODIMP nsDeviceMotionData::GetType(PRUint32 *aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceMotionData::GetX(double *aX)
{
  NS_ENSURE_ARG_POINTER(aX);
  *aX = mX;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceMotionData::GetY(double *aY)
{
  NS_ENSURE_ARG_POINTER(aY);
  *aY = mY;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceMotionData::GetZ(double *aZ)
{
  NS_ENSURE_ARG_POINTER(aZ);
  *aZ = mZ;
  return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsDeviceMotion, nsIDeviceMotion, nsIDeviceMotionUpdate)

nsDeviceMotion::nsDeviceMotion()
: mStarted(false),
  mUpdateInterval(50), /* default to 50 ms */
  mEnabled(true)
{
  nsCOMPtr<nsIPrefBranch> prefSrv = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefSrv) {
    PRInt32 value;
    nsresult rv = prefSrv->GetIntPref("device.motion.update.interval", &value);
    if (NS_SUCCEEDED(rv))
      mUpdateInterval = value;

    bool bvalue;
    rv = prefSrv->GetBoolPref("device.motion.enabled", &bvalue);
    if (NS_SUCCEEDED(rv) && bvalue == false)
      mEnabled = false;
  }
}

nsDeviceMotion::~nsDeviceMotion()
{
  if (mTimeoutTimer)
    mTimeoutTimer->Cancel();
}

void
nsDeviceMotion::StartDisconnectTimer()
{
  if (mTimeoutTimer)
    mTimeoutTimer->Cancel();

  mTimeoutTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mTimeoutTimer)
    mTimeoutTimer->InitWithFuncCallback(TimeoutHandler,
                                        this,
                                        2000, 
                                        nsITimer::TYPE_ONE_SHOT);  
}

void
nsDeviceMotion::TimeoutHandler(nsITimer *aTimer, void *aClosure)
{
  // the reason that we use self, instead of just using nsITimerCallback or nsIObserver
  // is so that subclasses are free to use timers without worry about the base classes's
  // usage.
  nsDeviceMotion *self = reinterpret_cast<nsDeviceMotion *>(aClosure);
  if (!self) {
    NS_ERROR("no self");
    return;
  }
  
  // what about listeners that don't clean up properly?  they will leak
  if (self->mListeners.Count() == 0 && self->mWindowListeners.Length() == 0) {
    self->Shutdown();
    self->mStarted = false;
  }
}

NS_IMETHODIMP nsDeviceMotion::AddListener(nsIDeviceMotionListener *aListener)
{
  if (mListeners.IndexOf(aListener) != -1)
    return NS_OK; // already exists

  if (mStarted == false) {
    mStarted = true;
    Startup();
  }

  mListeners.AppendObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceMotion::RemoveListener(nsIDeviceMotionListener *aListener)
{
  if (mListeners.IndexOf(aListener) == -1)
    return NS_OK; // doesn't exist

  mListeners.RemoveObject(aListener);
  StartDisconnectTimer();
  return NS_OK;
}

NS_IMETHODIMP nsDeviceMotion::AddWindowListener(nsIDOMWindow *aWindow)
{
  if (mWindowListeners.IndexOf(aWindow) != NoIndex)
      return NS_OK;

  if (mStarted == false) {
    mStarted = true;
    Startup();
  }

  mWindowListeners.AppendElement(aWindow);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceMotion::RemoveWindowListener(nsIDOMWindow *aWindow)
{
  if (mWindowListeners.IndexOf(aWindow) == NoIndex)
    return NS_OK;

  mWindowListeners.RemoveElement(aWindow);
  StartDisconnectTimer();
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceMotion::DeviceMotionChanged(PRUint32 type, double x, double y, double z)
{
  if (!mEnabled)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMArray<nsIDeviceMotionListener> listeners = mListeners;
  for (PRUint32 i = listeners.Count(); i > 0 ; ) {
    --i;
    nsRefPtr<nsDeviceMotionData> a = new nsDeviceMotionData(type, x, y, z);
    listeners[i]->OnMotionChange(a);
  }

  nsCOMArray<nsIDOMWindow> windowListeners;
  for (PRUint32 i = 0; i < mWindowListeners.Length(); i++) {
    windowListeners.AppendObject(mWindowListeners[i]);
  }

  for (PRUint32 i = windowListeners.Count(); i > 0 ; ) {
    --i;

    // check to see if this window is in the background.  if
    // it is, don't send any device motion to it.
    nsCOMPtr<nsPIDOMWindow> pwindow = do_QueryInterface(windowListeners[i]);
    if (!pwindow ||
        !pwindow->GetOuterWindow() ||
        pwindow->GetOuterWindow()->IsBackground())
      continue;

    nsCOMPtr<nsIDOMDocument> domdoc;
    windowListeners[i]->GetDocument(getter_AddRefs(domdoc));

    if (domdoc) {
      nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(windowListeners[i]);
      if (type == nsIDeviceMotionData::TYPE_ACCELERATION)
        FireDOMMotionEvent(domdoc, target, x, y, z);
      else if (type == nsIDeviceMotionData::TYPE_ORIENTATION)
        FireDOMOrientationEvent(domdoc, target, x, y, z);
    }
  }
  return NS_OK;
}

void
nsDeviceMotion::FireDOMOrientationEvent(nsIDOMDocument *domdoc,
                                        nsIDOMEventTarget *target,
                                        double alpha,
                                        double beta,
                                        double gamma)
{
  nsCOMPtr<nsIDOMEvent> event;
  bool defaultActionEnabled = true;
  domdoc->CreateEvent(NS_LITERAL_STRING("DeviceOrientationEvent"), getter_AddRefs(event));

  nsCOMPtr<nsIDOMDeviceOrientationEvent> oe = do_QueryInterface(event);

  if (!oe) {
    return;
  }

  oe->InitDeviceOrientationEvent(NS_LITERAL_STRING("deviceorientation"),
                                 true,
                                 false,
                                 alpha,
                                 beta,
                                 gamma,
                                 true);

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(event);
  if (privateEvent)
    privateEvent->SetTrusted(true);
  
  target->DispatchEvent(event, &defaultActionEnabled);
}


void
nsDeviceMotion::FireDOMMotionEvent(nsIDOMDocument *domdoc,
                                   nsIDOMEventTarget *target,
                                   double x,
                                   double y,
                                   double z) {
  nsCOMPtr<nsIDOMEvent> event;
  bool defaultActionEnabled = true;
  domdoc->CreateEvent(NS_LITERAL_STRING("DeviceMotionEvent"), getter_AddRefs(event));

  nsCOMPtr<nsIDOMDeviceMotionEvent> me = do_QueryInterface(event);

  if (!me) {
    return;
}

  // Currently acceleration as determined includes gravity.
  nsRefPtr<nsDOMDeviceAcceleration> acceleration = new nsDOMDeviceAcceleration(x, y, z);

  me->InitDeviceMotionEvent(NS_LITERAL_STRING("devicemotion"),
                            true,
                            false,
                            nsnull,
                            acceleration,
                            nsnull,
                            0);

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(event);
  if (privateEvent)
    privateEvent->SetTrusted(true);
  
  target->DispatchEvent(event, &defaultActionEnabled);
}

