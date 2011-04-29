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

#include "nsAccelerometer.h"

#include "nsAutoPtr.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIServiceManager.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMDeviceOrientationEvent.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"

class nsAcceleration : public nsIAcceleration
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCELERATION

  nsAcceleration(double alpha, double beta, double gamma);

private:
  ~nsAcceleration();

protected:
  /* additional members */
  double mAlpha, mBeta, mGamma;
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsAcceleration, nsIAcceleration)

nsAcceleration::nsAcceleration(double alpha, double beta, double gamma)
:mAlpha(alpha), mBeta(beta), mGamma(gamma)
{
}

nsAcceleration::~nsAcceleration()
{
}

NS_IMETHODIMP nsAcceleration::GetAlpha(double *aAlpha)
{
  NS_ENSURE_ARG_POINTER(aAlpha);
  *aAlpha = mAlpha;
  return NS_OK;
}

NS_IMETHODIMP nsAcceleration::GetBeta(double *aBeta)
{
  NS_ENSURE_ARG_POINTER(aBeta);
  *aBeta = mBeta;
  return NS_OK;
}

NS_IMETHODIMP nsAcceleration::GetGamma(double *aGamma)
{
  NS_ENSURE_ARG_POINTER(aGamma);
  *aGamma = mGamma;
  return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsAccelerometer, nsIAccelerometer, nsIAccelerometerUpdate)

nsAccelerometer::nsAccelerometer()
: mLastAlpha(-200), /* initialize to values that can't be possible */
  mLastBeta(-200),
  mLastGamma(-200),
  mStarted(PR_FALSE),
  mNewListener(PR_FALSE),
  mUpdateInterval(50), /* default to 50 ms */
  mEnabled(PR_TRUE)
{
  nsCOMPtr<nsIPrefBranch> prefSrv = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefSrv) {
    PRInt32 value;
    nsresult rv = prefSrv->GetIntPref("accelerometer.update.interval", &value);
    if (NS_SUCCEEDED(rv))
      mUpdateInterval = value;

    PRBool bvalue;
    rv = prefSrv->GetBoolPref("accelerometer.enabled", &bvalue);
    if (NS_SUCCEEDED(rv) && bvalue == PR_FALSE)
      mEnabled = PR_FALSE;
  }
}

nsAccelerometer::~nsAccelerometer()
{
  if (mTimeoutTimer)
    mTimeoutTimer->Cancel();
}

void
nsAccelerometer::StartDisconnectTimer()
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
nsAccelerometer::TimeoutHandler(nsITimer *aTimer, void *aClosure)
{
  // the reason that we use self, instead of just using nsITimerCallback or nsIObserver
  // is so that subclasses are free to use timers without worry about the base classes's
  // usage.
  nsAccelerometer *self = reinterpret_cast<nsAccelerometer *>(aClosure);
  if (!self) {
    NS_ERROR("no self");
    return;
  }
  
  // what about listeners that don't clean up properly?  they will leak
  if (self->mListeners.Count() == 0 && self->mWindowListeners.Count() == 0) {
    self->Shutdown();
    self->mStarted = PR_FALSE;
  }
}

NS_IMETHODIMP nsAccelerometer::AddListener(nsIAccelerationListener *aListener)
{
  if (mListeners.IndexOf(aListener) >= 0)
    return NS_OK; // already exists

  if (mStarted == PR_FALSE) {
    mStarted = PR_TRUE;
    mNewListener = PR_TRUE;
    Startup();
  }

  mListeners.AppendObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP nsAccelerometer::RemoveListener(nsIAccelerationListener *aListener)
{
  if (mListeners.IndexOf(aListener) < 0)
    return NS_OK; // doesn't exist

  mListeners.RemoveObject(aListener);
  StartDisconnectTimer();
  return NS_OK;
}

NS_IMETHODIMP nsAccelerometer::AddWindowListener(nsIDOMWindow *aWindow)
{
  if (mWindowListeners.IndexOf(aWindow) >= 0)
    return NS_OK; // already exists

  if (mStarted == PR_FALSE) {
    mStarted = PR_TRUE;
    mNewListener = PR_TRUE;
    Startup();
  }

  mWindowListeners.AppendObject(aWindow);
  return NS_OK;
}

NS_IMETHODIMP nsAccelerometer::RemoveWindowListener(nsIDOMWindow *aWindow)
{
  if (mWindowListeners.IndexOf(aWindow) < 0)
    return NS_OK; // doesn't exist

  mWindowListeners.RemoveObject(aWindow);
  StartDisconnectTimer();

  return NS_OK;
}

NS_IMETHODIMP
nsAccelerometer::AccelerationChanged(double alpha, double beta, double gamma)
{
  if (!mEnabled)
    return NS_ERROR_NOT_INITIALIZED;

  if (alpha > 360)
    alpha = 360;
  if (alpha < 0)
    alpha = 0;

  if (beta > 180)
    beta = 180;
  if (beta < -180)
    beta = -180;

  if (gamma > 90)
    gamma = 90;
  if (gamma < -90)
    gamma = -90;

  if (!mNewListener) {
    if (PR_ABS(mLastAlpha - alpha) < 1 &&
        PR_ABS(mLastBeta - beta) < 1 &&
        PR_ABS(mLastGamma - gamma) < 1)
      return NS_OK;
  }

  mLastAlpha = alpha;
  mLastBeta = beta;
  mLastGamma = gamma;
  mNewListener = PR_FALSE;

  for (PRUint32 i = mListeners.Count(); i > 0 ; ) {
    --i;
    nsRefPtr<nsIAcceleration> a = new nsAcceleration(alpha, beta, gamma);
    mListeners[i]->OnAccelerationChange(a);
  }

  for (PRUint32 i = mWindowListeners.Count(); i > 0 ; ) {
    --i;

    nsCOMPtr<nsIDOMDocument> domdoc;
    mWindowListeners[i]->GetDocument(getter_AddRefs(domdoc));

    nsCOMPtr<nsIDOMDocumentEvent> docevent(do_QueryInterface(domdoc));
    nsCOMPtr<nsIDOMEvent> event;

    PRBool defaultActionEnabled = PR_TRUE;

    if (docevent) {
      docevent->CreateEvent(NS_LITERAL_STRING("DeviceOrientationEvent"), getter_AddRefs(event));

      nsCOMPtr<nsIDOMDeviceOrientationEvent> oe = do_QueryInterface(event);

      if (event) {
        oe->InitDeviceOrientationEvent(NS_LITERAL_STRING("deviceorientation"),
                                       PR_TRUE,
                                       PR_FALSE,
                                       alpha,
                                       beta,
                                       gamma,
                                       PR_TRUE);

        nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(event);
        if (privateEvent)
          privateEvent->SetTrusted(PR_TRUE);
        
        nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mWindowListeners[i]);
        target->DispatchEvent(event, &defaultActionEnabled);
      }
    }
  }
  return NS_OK;
}
