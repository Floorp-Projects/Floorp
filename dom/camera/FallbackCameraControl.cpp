/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCameraControl.h"
#include "CameraControlImpl.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
class RecorderProfileManager;
}

/**
 * Fallback camera control subclass.  Can be used as a template for the
 * definition of new camera support classes.
 */
class nsFallbackCameraControl : public CameraControlImpl
{
public:
  nsFallbackCameraControl(uint32_t aCameraId, nsIThread* aCameraThread, nsDOMCameraControl* aDOMCameraControl, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, uint64_t aWindowId);

  const char* GetParameter(const char* aKey);
  const char* GetParameterConstChar(uint32_t aKey);
  double GetParameterDouble(uint32_t aKey);
  void GetParameter(uint32_t aKey, nsTArray<idl::CameraRegion>& aRegions);
  void GetParameter(uint32_t aKey, idl::CameraSize& aSize);
  void SetParameter(const char* aKey, const char* aValue);
  void SetParameter(uint32_t aKey, const char* aValue);
  void SetParameter(uint32_t aKey, double aValue);
  void SetParameter(uint32_t aKey, const nsTArray<idl::CameraRegion>& aRegions);
  void SetParameter(uint32_t aKey, const idl::CameraSize& aSize);
  nsresult GetVideoSizes(nsTArray<idl::CameraSize>& aVideoSizes);
  nsresult PushParameters();

protected:
  ~nsFallbackCameraControl();

  nsresult GetPreviewStreamImpl(GetPreviewStreamTask* aGetPreviewStream);
  nsresult StartPreviewImpl(StartPreviewTask* aStartPreview);
  nsresult StopPreviewImpl(StopPreviewTask* aStopPreview);
  nsresult AutoFocusImpl(AutoFocusTask* aAutoFocus);
  nsresult TakePictureImpl(TakePictureTask* aTakePicture);
  nsresult StartRecordingImpl(StartRecordingTask* aStartRecording);
  nsresult StopRecordingImpl(StopRecordingTask* aStopRecording);
  nsresult PushParametersImpl();
  nsresult PullParametersImpl();
  already_AddRefed<RecorderProfileManager> GetRecorderProfileManagerImpl();

private:
  nsFallbackCameraControl(const nsFallbackCameraControl&) MOZ_DELETE;
  nsFallbackCameraControl& operator=(const nsFallbackCameraControl&) MOZ_DELETE;
};

/**
 * Stub implementation of the DOM-facing camera control constructor.
 *
 * This should never get called--it exists to keep the linker happy; if
 * implemented, it should construct (e.g.) nsFallbackCameraControl and
 * store a reference in the 'mCameraControl' member (which is why it is
 * defined here).
 */
nsDOMCameraControl::nsDOMCameraControl(uint32_t aCameraId, nsIThread* aCameraThread, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, nsPIDOMWindow* aWindow) :
  mWindow(aWindow)
{
  MOZ_ASSERT(aWindow, "shouldn't be created with null window!");
  SetIsDOMBinding();
}

/**
 * Stub implemetations of the fallback camera control.
 *
 * None of these should ever get called--they exist to keep the linker happy,
 * and may be used as templates for new camera support classes.
 */
nsFallbackCameraControl::nsFallbackCameraControl(uint32_t aCameraId, nsIThread* aCameraThread, nsDOMCameraControl* aDOMCameraControl, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, uint64_t aWindowId)
  : CameraControlImpl(aCameraId, aCameraThread, aWindowId)
{
}

nsFallbackCameraControl::~nsFallbackCameraControl()
{
}

const char*
nsFallbackCameraControl::GetParameter(const char* aKey)
{
  return nullptr;
}

const char*
nsFallbackCameraControl::GetParameterConstChar(uint32_t aKey)
{
  return nullptr;
}

double
nsFallbackCameraControl::GetParameterDouble(uint32_t aKey)
{
  return NAN;
}

void
nsFallbackCameraControl::GetParameter(uint32_t aKey, nsTArray<idl::CameraRegion>& aRegions)
{
}

void
nsFallbackCameraControl::GetParameter(uint32_t aKey, idl::CameraSize& aSize)
{
}

void
nsFallbackCameraControl::SetParameter(const char* aKey, const char* aValue)
{
}

void
nsFallbackCameraControl::SetParameter(uint32_t aKey, const char* aValue)
{
}

void
nsFallbackCameraControl::SetParameter(uint32_t aKey, double aValue)
{
}

void
nsFallbackCameraControl::SetParameter(uint32_t aKey, const nsTArray<idl::CameraRegion>& aRegions)
{
}

void
nsFallbackCameraControl::SetParameter(uint32_t aKey, const idl::CameraSize& aSize)
{
}

nsresult
nsFallbackCameraControl::PushParameters()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFallbackCameraControl::GetPreviewStreamImpl(GetPreviewStreamTask* aGetPreviewStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFallbackCameraControl::StartPreviewImpl(StartPreviewTask* aGetPreviewStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFallbackCameraControl::StopPreviewImpl(StopPreviewTask* aGetPreviewStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFallbackCameraControl::AutoFocusImpl(AutoFocusTask* aAutoFocus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFallbackCameraControl::TakePictureImpl(TakePictureTask* aTakePicture)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFallbackCameraControl::StartRecordingImpl(StartRecordingTask* aStartRecording)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFallbackCameraControl::StopRecordingImpl(StopRecordingTask* aStopRecording)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFallbackCameraControl::PushParametersImpl()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFallbackCameraControl::PullParametersImpl()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFallbackCameraControl::GetVideoSizes(nsTArray<idl::CameraSize>& aVideoSizes)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

already_AddRefed<RecorderProfileManager> 
nsFallbackCameraControl::GetRecorderProfileManagerImpl()
{
  return nullptr;
}
