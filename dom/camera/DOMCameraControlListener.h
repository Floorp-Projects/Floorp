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

  virtual void OnAutoFocusComplete(bool aAutoFocusSucceeded) override;
  virtual void OnAutoFocusMoving(bool aIsMoving) override;
  virtual void OnFacesDetected(const nsTArray<ICameraControl::Face>& aFaces) override;
  virtual void OnTakePictureComplete(const uint8_t* aData, uint32_t aLength, const nsAString& aMimeType) override;

  virtual void OnHardwareStateChange(HardwareState aState, nsresult aReason) override;
  virtual void OnPreviewStateChange(PreviewState aState) override;
  virtual void OnRecorderStateChange(RecorderState aState, int32_t aStatus, int32_t aTrackNum) override;
  virtual void OnConfigurationChange(const CameraListenerConfiguration& aConfiguration) override;
  virtual void OnShutter() override;
  virtual void OnRateLimitPreview(bool aLimit) override;
  virtual bool OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight) override;
  virtual void OnUserError(UserContext aContext, nsresult aError) override;
  virtual void OnPoster(dom::BlobImpl* aBlobImpl) override;

protected:
  virtual ~DOMCameraControlListener();

  nsMainThreadPtrHandle<nsISupports> mDOMCameraControl;
  CameraPreviewMediaStream* mStream;

  class DOMCallback;

private:
  DOMCameraControlListener(const DOMCameraControlListener&) = delete;
  DOMCameraControlListener& operator=(const DOMCameraControlListener&) = delete;
};

} // namespace mozilla

#endif // DOM_CAMERA_DOMCAMERACONTROLLISTENER_H
