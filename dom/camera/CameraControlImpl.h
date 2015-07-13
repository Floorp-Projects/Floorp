/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERACONTROLIMPL_H
#define DOM_CAMERA_CAMERACONTROLIMPL_H

#include "nsTArray.h"
#include "nsWeakPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIFile.h"
#include "nsProxyRelease.h"
#include "AutoRwLock.h"
#include "ICameraControl.h"
#include "CameraCommon.h"
#include "DeviceStorage.h"
#include "DeviceStorageFileDescriptor.h"
#include "CameraControlListener.h"

namespace mozilla {

namespace dom {
  class BlobImpl;
} // namespace dom

namespace layers {
  class Image;
} // namespace layers

class CameraControlImpl : public ICameraControl
{
public:
  explicit CameraControlImpl();
  virtual void AddListener(CameraControlListener* aListener) override;
  virtual void RemoveListener(CameraControlListener* aListener) override;

  // See ICameraControl.h for these methods' return values.
  virtual nsresult Start(const Configuration* aConfig = nullptr) override;
  virtual nsresult Stop() override;
  virtual nsresult SetConfiguration(const Configuration& aConfig) override;
  virtual nsresult StartPreview() override;
  virtual nsresult StopPreview() override;
  virtual nsresult AutoFocus() override;
  virtual nsresult StartFaceDetection() override;
  virtual nsresult StopFaceDetection() override;
  virtual nsresult TakePicture() override;
  virtual nsresult StartRecording(DeviceStorageFileDescriptor* aFileDescriptor,
                                  const StartRecordingOptions* aOptions) override;
  virtual nsresult StopRecording() override;
  virtual nsresult ResumeContinuousFocus() override;

  // Event handlers called directly from outside this class.
  void OnShutter();
  void OnUserError(CameraControlListener::UserContext aContext, nsresult aError);
  void OnSystemError(CameraControlListener::SystemContext aContext, nsresult aError);
  void OnAutoFocusMoving(bool aIsMoving);

protected:
  // Event handlers.
  void OnAutoFocusComplete(bool aAutoFocusSucceeded);
  void OnFacesDetected(const nsTArray<Face>& aFaces);
  void OnTakePictureComplete(const uint8_t* aData, uint32_t aLength, const nsAString& aMimeType);
  void OnPoster(dom::BlobImpl* aBlobImpl);

  void OnRateLimitPreview(bool aLimit);
  bool OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight);
  void OnRecorderStateChange(CameraControlListener::RecorderState aState,
                             int32_t aStatus = -1, int32_t aTrackNumber = -1);
  void OnPreviewStateChange(CameraControlListener::PreviewState aState);
  void OnHardwareStateChange(CameraControlListener::HardwareState aState,
                             nsresult aReason);
  void OnConfigurationChange();

  // When we create a new CameraThread, we keep a static reference to it so
  // that multiple CameraControl instances can find and reuse it; but we
  // don't want that reference to keep the thread object around unnecessarily,
  // so we make it a weak reference. The strong dynamic references will keep
  // the thread object alive as needed.
  static StaticRefPtr<nsIThread> sCameraThread;
  nsCOMPtr<nsIThread> mCameraThread;

  virtual ~CameraControlImpl();

  virtual void BeginBatchParameterSet() override { }
  virtual void EndBatchParameterSet() override { }

  // Manage camera event listeners.
  void AddListenerImpl(already_AddRefed<CameraControlListener> aListener);
  void RemoveListenerImpl(CameraControlListener* aListener);
  nsTArray<nsRefPtr<CameraControlListener> > mListeners;
  PRRWLock* mListenerLock;

  class ControlMessage;
  class ListenerMessage;

  nsresult Dispatch(ControlMessage* aMessage);

  // Asynchronous method implementations, invoked on the Camera Thread.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if one or more arguments is invalid;
  //  - NS_ERROR_NOT_INITIALIZED if the underlying hardware is not initialized,
  //      failed to initialize (in the case of StartImpl()), or has been stopped;
  //      for StartRecordingImpl(), this indicates that no recorder has been
  //      configured (either by calling StartImpl() or SetConfigurationImpl());
  //  - NS_ERROR_ALREADY_INITIALIZED if the underlying hardware is already
  //      initialized;
  //  - NS_ERROR_NOT_IMPLEMENTED if the method is not implemented;
  //  - NS_ERROR_FAILURE on general failures.
  virtual nsresult StartImpl(const Configuration* aConfig = nullptr) = 0;
  virtual nsresult StopImpl() = 0;
  virtual nsresult SetConfigurationImpl(const Configuration& aConfig) = 0;
  virtual nsresult StartPreviewImpl() = 0;
  virtual nsresult StopPreviewImpl() = 0;
  virtual nsresult AutoFocusImpl() = 0;
  virtual nsresult StartFaceDetectionImpl() = 0;
  virtual nsresult StopFaceDetectionImpl() = 0;
  virtual nsresult TakePictureImpl() = 0;
  virtual nsresult StartRecordingImpl(DeviceStorageFileDescriptor* aFileDescriptor,
                                      const StartRecordingOptions* aOptions) = 0;
  virtual nsresult StopRecordingImpl() = 0;
  virtual nsresult ResumeContinuousFocusImpl() = 0;
  virtual nsresult PushParametersImpl() = 0;
  virtual nsresult PullParametersImpl() = 0;

  void OnShutterInternal();
  void OnClosedInternal();

  CameraControlListener::CameraListenerConfiguration mCurrentConfiguration;

  CameraControlListener::PreviewState   mPreviewState;
  CameraControlListener::HardwareState  mHardwareState;
  nsresult                              mHardwareStateChangeReason;

private:
  CameraControlImpl(const CameraControlImpl&) = delete;
  CameraControlImpl& operator=(const CameraControlImpl&) = delete;
};

} // namespace mozilla

#endif // DOM_CAMERA_CAMERACONTROLIMPL_H
