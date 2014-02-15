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
protected:
  nsMainThreadPtrHandle<nsDOMCameraControl> mDOMCameraControl;
  CameraPreviewMediaStream* mStream;

  class DOMCallback;

public:
  DOMCameraControlListener(nsDOMCameraControl* aDOMCameraControl, CameraPreviewMediaStream* aStream)
    : mDOMCameraControl(new nsMainThreadPtrHolder<nsDOMCameraControl>(aDOMCameraControl))
    , mStream(aStream)
  { }

  void OnAutoFocusComplete(bool aAutoFocusSucceeded);
  void OnTakePictureComplete(uint8_t* aData, uint32_t aLength, const nsAString& aMimeType);

  void OnHardwareStateChange(HardwareState aState);
  void OnPreviewStateChange(PreviewState aState);
  void OnRecorderStateChange(RecorderState aState, int32_t aStatus, int32_t aTrackNum);
  void OnConfigurationChange(const CameraListenerConfiguration& aConfiguration);
  void OnShutter();
  bool OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight);
  void OnError(CameraErrorContext aContext, CameraError aError);

private:
  DOMCameraControlListener(const DOMCameraControlListener&) MOZ_DELETE;
  DOMCameraControlListener& operator=(const DOMCameraControlListener&) MOZ_DELETE;
};

} // namespace mozilla

#endif // DOM_CAMERA_DOMCAMERACONTROLLISTENER_H
