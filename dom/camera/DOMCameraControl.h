/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_DOMCAMERACONTROL_H
#define DOM_CAMERA_DOMCAMERACONTROL_H

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/CameraControlBinding.h"
#include "mozilla/dom/Promise.h"
#include "ICameraControl.h"
#include "CameraCommon.h"
#include "DOMMediaStream.h"
#include "AudioChannelAgent.h"
#include "nsProxyRelease.h"
#include "nsHashPropertyBag.h"
#include "DeviceStorage.h"
#include "DOMCameraControlListener.h"
#ifdef MOZ_WIDGET_GONK
#include "nsITimer.h"
#endif

class nsDOMDeviceStorage;
class nsPIDOMWindow;
class nsIDOMBlob;

namespace mozilla {

namespace dom {
  class CameraCapabilities;
  struct CameraPictureOptions;
  struct CameraStartRecordingOptions;
  struct CameraRegion;
  struct CameraSize;
  template<typename T> class Optional;
}
class ErrorResult;
class StartRecordingHelper;

// Main camera control.
class nsDOMCameraControl MOZ_FINAL : public DOMMediaStream
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMCameraControl, DOMMediaStream)
  NS_DECL_ISUPPORTS_INHERITED

  // Because this header's filename doesn't match its C++ or DOM-facing
  // classname, we can't rely on the [Func="..."] WebIDL tag to implicitly
  // include the right header for us; instead we must explicitly include a
  // HasSupport() method in each header. We can get rid of these with the
  // Great Renaming proposed in bug 983177.
  static bool HasSupport(JSContext* aCx, JSObject* aGlobal);

  nsDOMCameraControl(uint32_t aCameraId,
                     const dom::CameraConfiguration& aInitialConfig,
                     dom::GetCameraCallback* aOnSuccess,
                     dom::CameraErrorCallback* aOnError,
                     dom::Promise* aPromise,
                     nsPIDOMWindow* aWindow);

  void Shutdown();

  nsPIDOMWindow* GetParentObject() const { return mWindow; }

  // Attributes.
  void GetEffect(nsString& aEffect, ErrorResult& aRv);
  void SetEffect(const nsAString& aEffect, ErrorResult& aRv);
  void GetWhiteBalanceMode(nsString& aMode, ErrorResult& aRv);
  void SetWhiteBalanceMode(const nsAString& aMode, ErrorResult& aRv);
  void GetSceneMode(nsString& aMode, ErrorResult& aRv);
  void SetSceneMode(const nsAString& aMode, ErrorResult& aRv);
  void GetFlashMode(nsString& aMode, ErrorResult& aRv);
  void SetFlashMode(const nsAString& aMode, ErrorResult& aRv);
  void GetFocusMode(nsString& aMode, ErrorResult& aRv);
  void SetFocusMode(const nsAString& aMode, ErrorResult& aRv);
  double GetZoom(ErrorResult& aRv);
  void SetZoom(double aZoom, ErrorResult& aRv);
  double GetFocalLength(ErrorResult& aRv);
  double GetFocusDistanceNear(ErrorResult& aRv);
  double GetFocusDistanceOptimum(ErrorResult& aRv);
  double GetFocusDistanceFar(ErrorResult& aRv);
  void SetExposureCompensation(double aCompensation, ErrorResult& aRv);
  double GetExposureCompensation(ErrorResult& aRv);
  int32_t SensorAngle();
  already_AddRefed<dom::CameraCapabilities> Capabilities();
  void GetIsoMode(nsString& aMode, ErrorResult& aRv);
  void SetIsoMode(const nsAString& aMode, ErrorResult& aRv);
  double GetPictureQuality(ErrorResult& aRv);
  void SetPictureQuality(double aQuality, ErrorResult& aRv);

  // Unsolicited event handlers.
  dom::CameraShutterCallback* GetOnShutter();
  void SetOnShutter(dom::CameraShutterCallback* aCb);
  dom::CameraClosedCallback* GetOnClosed();
  void SetOnClosed(dom::CameraClosedCallback* aCb);
  dom::CameraRecorderStateChange* GetOnRecorderStateChange();
  void SetOnRecorderStateChange(dom::CameraRecorderStateChange* aCb);
  dom::CameraPreviewStateChange* GetOnPreviewStateChange();
  void SetOnPreviewStateChange(dom::CameraPreviewStateChange* aCb);
  dom::CameraAutoFocusMovingCallback* GetOnAutoFocusMoving();
  void SetOnAutoFocusMoving(dom::CameraAutoFocusMovingCallback* aCb);
  dom::CameraAutoFocusCallback* GetOnAutoFocusCompleted();
  void SetOnAutoFocusCompleted(dom::CameraAutoFocusCallback* aCb);
  dom::CameraFaceDetectionCallback* GetOnFacesDetected();
  void SetOnFacesDetected(dom::CameraFaceDetectionCallback* aCb);

  // Methods.
  already_AddRefed<dom::Promise> SetConfiguration(const dom::CameraConfiguration& aConfiguration,
                                                  const dom::Optional<dom::OwningNonNull<dom::CameraSetConfigurationCallback> >& aOnSuccess,
                                                  const dom::Optional<dom::OwningNonNull<dom::CameraErrorCallback> >& aOnError,
                                                  ErrorResult& aRv);
  void GetMeteringAreas(nsTArray<dom::CameraRegion>& aAreas, ErrorResult& aRv);
  void SetMeteringAreas(const dom::Optional<dom::Sequence<dom::CameraRegion> >& aAreas, ErrorResult& aRv);
  void GetFocusAreas(nsTArray<dom::CameraRegion>& aAreas, ErrorResult& aRv);
  void SetFocusAreas(const dom::Optional<dom::Sequence<dom::CameraRegion> >& aAreas, ErrorResult& aRv);
  void GetPictureSize(dom::CameraSize& aSize, ErrorResult& aRv);
  void SetPictureSize(const dom::CameraSize& aSize, ErrorResult& aRv);
  void GetThumbnailSize(dom::CameraSize& aSize, ErrorResult& aRv);
  void SetThumbnailSize(const dom::CameraSize& aSize, ErrorResult& aRv);
  already_AddRefed<dom::Promise> AutoFocus(const dom::Optional<dom::OwningNonNull<dom::CameraAutoFocusCallback> >& aOnSuccess,
                                           const dom::Optional<dom::OwningNonNull<dom::CameraErrorCallback> >& aOnError,
                                           ErrorResult& aRv);
  void StartFaceDetection(ErrorResult& aRv);
  void StopFaceDetection(ErrorResult& aRv);
  already_AddRefed<dom::Promise> TakePicture(const dom::CameraPictureOptions& aOptions,
                                             const dom::Optional<dom::OwningNonNull<dom::CameraTakePictureCallback> >& aOnSuccess,
                                             const dom::Optional<dom::OwningNonNull<dom::CameraErrorCallback> >& aOnError,
                                             ErrorResult& aRv);
  already_AddRefed<dom::Promise> StartRecording(const dom::CameraStartRecordingOptions& aOptions,
                                                nsDOMDeviceStorage& storageArea,
                                                const nsAString& filename,
                                                const dom::Optional<dom::OwningNonNull<dom::CameraStartRecordingCallback> >& aOnSuccess,
                                                const dom::Optional<dom::OwningNonNull<dom::CameraErrorCallback> >& aOnError,
                                                ErrorResult& aRv);
  void StopRecording(ErrorResult& aRv);
  void ResumePreview(ErrorResult& aRv);
  already_AddRefed<dom::Promise> ReleaseHardware(const dom::Optional<dom::OwningNonNull<dom::CameraReleaseCallback> >& aOnSuccess,
                                                 const dom::Optional<dom::OwningNonNull<dom::CameraErrorCallback> >& aOnError,
                                                 ErrorResult& aRv);
  void ResumeContinuousFocus(ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

#ifdef MOZ_WIDGET_GONK
  static void PreinitCameraHardware();
  static void DiscardCachedCameraInstance(nsITimer* aTimer, void* aClosure);
#endif

  IMPL_EVENT_HANDLER(facesdetected)
  IMPL_EVENT_HANDLER(shutter)
  IMPL_EVENT_HANDLER(close)
  IMPL_EVENT_HANDLER(recorderstatechange)
  IMPL_EVENT_HANDLER(previewstatechange)
  IMPL_EVENT_HANDLER(focus)
  IMPL_EVENT_HANDLER(picture)
  IMPL_EVENT_HANDLER(configurationchange)

protected:
  virtual ~nsDOMCameraControl();

  class DOMCameraConfiguration MOZ_FINAL : public dom::CameraConfiguration
  {
  public:
    NS_INLINE_DECL_REFCOUNTING(DOMCameraConfiguration)

    DOMCameraConfiguration();
    explicit DOMCameraConfiguration(const dom::CameraConfiguration& aConfiguration);

    // Additional configuration options that aren't exposed to the DOM
    uint32_t mMaxFocusAreas;
    uint32_t mMaxMeteringAreas;

  private:
    // Private destructor, to discourage deletion outside of Release():
    ~DOMCameraConfiguration();
  };

  friend class DOMCameraControlListener;
  friend class mozilla::StartRecordingHelper;

  void OnCreatedFileDescriptor(bool aSucceeded);

  void OnAutoFocusComplete(bool aAutoFocusSucceeded);
  void OnAutoFocusMoving(bool aIsMoving);
  void OnTakePictureComplete(nsIDOMBlob* aPicture);
  void OnFacesDetected(const nsTArray<ICameraControl::Face>& aFaces);

  void OnHardwareStateChange(DOMCameraControlListener::HardwareState aState);
  void OnPreviewStateChange(DOMCameraControlListener::PreviewState aState);
  void OnRecorderStateChange(CameraControlListener::RecorderState aState, int32_t aStatus, int32_t aTrackNum);
  void OnConfigurationChange(DOMCameraConfiguration* aConfiguration);
  void OnShutter();
  void OnUserError(CameraControlListener::UserContext aContext, nsresult aError);

  bool IsWindowStillActive();

  nsresult NotifyRecordingStatusChange(const nsString& aMsg);

  already_AddRefed<dom::Promise> CreatePromise(ErrorResult& aRv);
  void AbortPromise(nsRefPtr<dom::Promise>& aPromise);
  virtual void EventListenerAdded(nsIAtom* aType) MOZ_OVERRIDE;
  void DispatchPreviewStateEvent(DOMCameraControlListener::PreviewState aState);
  void DispatchStateEvent(const nsString& aType, const nsString& aState);

  nsRefPtr<ICameraControl> mCameraControl; // non-DOM camera control

  // An agent used to join audio channel service.
  nsCOMPtr<nsIAudioChannelAgent> mAudioChannelAgent;

  nsresult Set(uint32_t aKey, const dom::Optional<dom::Sequence<dom::CameraRegion> >& aValue, uint32_t aLimit);
  nsresult Get(uint32_t aKey, nsTArray<dom::CameraRegion>& aValue);

  nsRefPtr<DOMCameraConfiguration>              mCurrentConfiguration;
  nsRefPtr<dom::CameraCapabilities>             mCapabilities;

  // camera control pending promises
  nsRefPtr<dom::Promise>                        mGetCameraPromise;
  nsRefPtr<dom::Promise>                        mAutoFocusPromise;
  nsRefPtr<dom::Promise>                        mTakePicturePromise;
  nsRefPtr<dom::Promise>                        mStartRecordingPromise;
  nsRefPtr<dom::Promise>                        mReleasePromise;
  nsRefPtr<dom::Promise>                        mSetConfigurationPromise;

  // solicited camera control event handlers
  nsRefPtr<dom::GetCameraCallback>              mGetCameraOnSuccessCb;
  nsRefPtr<dom::CameraErrorCallback>            mGetCameraOnErrorCb;
  nsRefPtr<dom::CameraAutoFocusCallback>        mAutoFocusOnSuccessCb;
  nsRefPtr<dom::CameraErrorCallback>            mAutoFocusOnErrorCb;
  nsRefPtr<dom::CameraTakePictureCallback>      mTakePictureOnSuccessCb;
  nsRefPtr<dom::CameraErrorCallback>            mTakePictureOnErrorCb;
  nsRefPtr<dom::CameraStartRecordingCallback>   mStartRecordingOnSuccessCb;
  nsRefPtr<dom::CameraErrorCallback>            mStartRecordingOnErrorCb;
  nsRefPtr<dom::CameraReleaseCallback>          mReleaseOnSuccessCb;
  nsRefPtr<dom::CameraErrorCallback>            mReleaseOnErrorCb;
  nsRefPtr<dom::CameraSetConfigurationCallback> mSetConfigurationOnSuccessCb;
  nsRefPtr<dom::CameraErrorCallback>            mSetConfigurationOnErrorCb;

  // unsolicited event handlers
  nsRefPtr<dom::CameraShutterCallback>          mOnShutterCb;
  nsRefPtr<dom::CameraClosedCallback>           mOnClosedCb;
  nsRefPtr<dom::CameraRecorderStateChange>      mOnRecorderStateChangeCb;
  nsRefPtr<dom::CameraPreviewStateChange>       mOnPreviewStateChangeCb;
  nsRefPtr<dom::CameraAutoFocusMovingCallback>  mOnAutoFocusMovingCb;
  nsRefPtr<dom::CameraAutoFocusCallback>        mOnAutoFocusCompletedCb;
  nsRefPtr<dom::CameraFaceDetectionCallback>    mOnFacesDetectedCb;

  // Camera event listener; we only need this weak reference so that
  //  we can remove the listener from the camera when we're done
  //  with it.
  DOMCameraControlListener* mListener;

  // our viewfinder stream
  nsRefPtr<CameraPreviewMediaStream> mInput;

  // set once when this object is created
  nsCOMPtr<nsPIDOMWindow>   mWindow;

  dom::CameraStartRecordingOptions mOptions;
  nsRefPtr<DeviceStorageFileDescriptor> mDSFileDescriptor;
  DOMCameraControlListener::PreviewState mPreviewState;

#ifdef MOZ_WIDGET_GONK
  // cached camera control, to improve start-up time
  static StaticRefPtr<ICameraControl> sCachedCameraControl;
  static nsresult sCachedCameraControlStartResult;
  static nsCOMPtr<nsITimer> sDiscardCachedCameraControlTimer;
#endif

private:
  nsDOMCameraControl(const nsDOMCameraControl&) MOZ_DELETE;
  nsDOMCameraControl& operator=(const nsDOMCameraControl&) MOZ_DELETE;
};

} // namespace mozilla

#endif // DOM_CAMERA_DOMCAMERACONTROL_H
