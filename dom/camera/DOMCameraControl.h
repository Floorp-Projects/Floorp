/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_DOMCAMERACONTROL_H
#define DOM_CAMERA_DOMCAMERACONTROL_H

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "DictionaryHelpers.h"
#include "ICameraControl.h"
#include "DOMCameraPreview.h"
#include "nsIDOMCameraManager.h"
#include "CameraCommon.h"
#include "AudioChannelAgent.h"

namespace mozilla {


// Main camera control.
class nsDOMCameraControl : public nsICameraControl
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMCameraControl)
  NS_DECL_NSICAMERACONTROL

  nsDOMCameraControl(uint32_t aCameraId, nsIThread* aCameraThread, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, uint64_t aWindowId);
  nsresult Result(nsresult aResult, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, uint64_t aWindowId);

  void Shutdown();

protected:
  virtual ~nsDOMCameraControl();

private:
  nsDOMCameraControl(const nsDOMCameraControl&) MOZ_DELETE;
  nsDOMCameraControl& operator=(const nsDOMCameraControl&) MOZ_DELETE;

protected:
  /* additional members */
  nsRefPtr<ICameraControl>        mCameraControl; // non-DOM camera control
  nsCOMPtr<nsICameraCapabilities> mDOMCapabilities;
  // An agent used to join audio channel service.
  nsCOMPtr<nsIAudioChannelAgent>  mAudioChannelAgent;
};

} // namespace mozilla

#endif // DOM_CAMERA_DOMCAMERACONTROL_H
