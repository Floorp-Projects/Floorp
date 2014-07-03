/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_DOMCAMERACONTROLLISTENER_H
#define DOM_CAMERA_DOMCAMERACONTROLLISTENER_H

#include "nsProxyRelease.h"
#include "CameraControlListener.h"

namespace mozilla {

class nsDOMCameraControl;
class CameraPreviewMediaStream;

class DOMCameraControlListener : public CameraControlListener
{
public:
  DOMCameraControlListener(nsDOMCameraControl* aDOMCameraControl, CameraPreviewMediaStream* aStream);

  virtual void OnAutoFocusComplete(bool aAutoFocusSucceeded) MOZ_OVERRIDE;
  virtual void OnAutoFocusMoving(bool aIsMoving) MOZ_OVERRIDE;
  virtual void OnFacesDetected(const nsTArray<ICameraControl::Face>& aFaces) MOZ_OVERRIDE;
  virtual void OnTakePictureComplete(uint8_t* aData, uint32_t aLength, const nsAString& aMimeType) MOZ_OVERRIDE;

  virtual void OnHardwareStateChange(HardwareState aState) MOZ_OVERRIDE;
  virtual void OnPreviewStateChange(PreviewState aState) MOZ_OVERRIDE;
  virtual void OnRecorderStateChange(RecorderState aState, int32_t aStatus, int32_t aTrackNum) MOZ_OVERRIDE;
  virtual void OnConfigurationChange(const CameraListenerConfiguration& aConfiguration) MOZ_OVERRIDE;
  virtual void OnShutter() MOZ_OVERRIDE;
  virtual void OnRateLimitPreview(bool aLimit) MOZ_OVERRIDE;
  virtual bool OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight) MOZ_OVERRIDE;
  virtual void OnUserError(UserContext aContext, nsresult aError) MOZ_OVERRIDE;

protected:
  virtual ~DOMCameraControlListener();

  nsMainThreadPtrHandle<nsDOMCameraControl> mDOMCameraControl;
  CameraPreviewMediaStream* mStream;

  class DOMCallback;

private:
  DOMCameraControlListener(const DOMCameraControlListener&) MOZ_DELETE;
  DOMCameraControlListener& operator=(const DOMCameraControlListener&) MOZ_DELETE;
};

} // namespace mozilla

#endif // DOM_CAMERA_DOMCAMERACONTROLLISTENER_H
