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

#include "mozilla/Hal.h"
#include "mozilla/HalSensor.h"

#include "nsDeviceMotion.h"

#include "nsAutoPtr.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIServiceManager.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"

using namespace mozilla;
using namespace hal;

// also see sDefaultSensorHint in mobile/android/base/GeckoAppShell.java
#define DEFAULT_SENSOR_POLL 100

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

NS_IMPL_ISUPPORTS1(nsDeviceMotion, nsIDeviceMotion)

nsDeviceMotion::nsDeviceMotion()
: mEnabled(true)
{
  mLastDOMMotionEventTime = TimeStamp::Now();

  nsCOMPtr<nsIPrefBranch> prefSrv = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefSrv) {
    bool bvalue;
    nsresult rv = prefSrv->GetBoolPref("device.motion.enabled", &bvalue);
    if (NS_SUCCEEDED(rv) && bvalue == false)
      mEnabled = false;
  }

  for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
    nsTArray<nsIDOMWindow*> *windows = new nsTArray<nsIDOMWindow*>();
    mWindowListeners.AppendElement(windows);
  }

  mLastDOMMotionEventTime = TimeStamp::Now();
}

nsDeviceMotion::~nsDeviceMotion()
{
  for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
    if (IsSensorEnabled(i))  
      UnregisterSensorObserver((SensorType)i, this);
  }

  for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
    delete mWindowListeners[i];
  }
}

NS_IMETHODIMP nsDeviceMotion::AddWindowListener(PRUint32 aType, nsIDOMWindow *aWindow)
{
  if (mWindowListeners[aType]->IndexOf(aWindow) != NoIndex)
    return NS_OK;

  if (!IsSensorEnabled(aType)) {
    RegisterSensorObserver((SensorType)aType, this);
  }

  mWindowListeners[aType]->AppendElement(aWindow);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceMotion::RemoveWindowListener(PRUint32 aType, nsIDOMWindow *aWindow)
{
  if (mWindowListeners[aType]->IndexOf(aWindow) == NoIndex)
    return NS_OK;

  mWindowListeners[aType]->RemoveElement(aWindow);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceMotion::RemoveWindowAsListener(nsIDOMWindow *aWindow)
{
  for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
    if (IsSensorEnabled(i))
        RemoveWindowListener((SensorType)i, aWindow);
  }
  return NS_OK;
}

void 
nsDeviceMotion::Notify(const mozilla::hal::SensorData& aSensorData)
{
  if (!mEnabled)
    return;

  PRUint32 type = aSensorData.sensor();

  double x = aSensorData.values()[0];
  double y = aSensorData.values()[1];
  double z = aSensorData.values()[2];

  nsCOMArray<nsIDOMWindow> windowListeners;
  for (PRUint32 i = 0; i < mWindowListeners[type]->Length(); i++) {
    windowListeners.AppendObject(mWindowListeners[type]->SafeElementAt(i));
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
      if (type == nsIDeviceMotionData::TYPE_ACCELERATION || 
        type == nsIDeviceMotionData::TYPE_LINEAR_ACCELERATION || 
        type == nsIDeviceMotionData::TYPE_GYROSCOPE)
        FireDOMMotionEvent(domdoc, target, type, x, y, z);
      else if (type == nsIDeviceMotionData::TYPE_ORIENTATION)
        FireDOMOrientationEvent(domdoc, target, x, y, z);
    }
  }
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
                                   PRUint32 type,
                                   double x,
                                   double y,
                                   double z) {
  // Attempt to coalesce events
  bool fireEvent = TimeStamp::Now() > mLastDOMMotionEventTime + TimeDuration::FromMilliseconds(DEFAULT_SENSOR_POLL);

  switch (type) {
  case nsIDeviceMotionData::TYPE_LINEAR_ACCELERATION:
    mLastAcceleration = new nsDOMDeviceAcceleration(x, y, z);
    break;
  case nsIDeviceMotionData::TYPE_ACCELERATION:
    mLastAccelerationIncluduingGravity = new nsDOMDeviceAcceleration(x, y, z);
    break;
  case nsIDeviceMotionData::TYPE_GYROSCOPE:
    mLastRotationRate = new nsDOMDeviceRotationRate(x, y, z);
    break;
  }

  if (!fireEvent && (!mLastAcceleration || !mLastAccelerationIncluduingGravity || !mLastRotationRate)) {
    return;
  }

  nsCOMPtr<nsIDOMEvent> event;
  domdoc->CreateEvent(NS_LITERAL_STRING("DeviceMotionEvent"), getter_AddRefs(event));

  nsCOMPtr<nsIDOMDeviceMotionEvent> me = do_QueryInterface(event);

  if (!me)
    return;

  me->InitDeviceMotionEvent(NS_LITERAL_STRING("devicemotion"),
                            true,
                            false,
                            mLastAcceleration,
                            mLastAccelerationIncluduingGravity,
                            mLastRotationRate,
                            DEFAULT_SENSOR_POLL);

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(event);
  if (privateEvent)
    privateEvent->SetTrusted(true);

  bool defaultActionEnabled = true;
  target->DispatchEvent(event, &defaultActionEnabled);

  mLastRotationRate = nsnull;
  mLastAccelerationIncluduingGravity = nsnull;
  mLastAcceleration = nsnull;
  mLastDOMMotionEventTime = TimeStamp::Now();
}
