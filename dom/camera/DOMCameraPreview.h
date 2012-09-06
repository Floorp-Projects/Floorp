/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_DOMCAMERAPREVIEW_H
#define DOM_CAMERA_DOMCAMERAPREVIEW_H

#include "nsCycleCollectionParticipant.h"
#include "MediaStreamGraph.h"
#include "StreamBuffer.h"
#include "ICameraControl.h"
#include "nsDOMMediaStream.h"
#include "CameraCommon.h"

using namespace mozilla;
using namespace mozilla::layers;

namespace mozilla {

typedef void (*FrameBuilder)(Image* aImage, void* aBuffer, uint32_t aWidth, uint32_t aHeight);

/**
 * DOMCameraPreview is only exposed to the DOM as an nsDOMMediaStream,
 * which is a cycle-collection participant already.
 */
class DOMCameraPreview : public nsDOMMediaStream
{
protected:
  enum { TRACK_VIDEO = 1 };

public:
  DOMCameraPreview(ICameraControl* aCameraControl, uint32_t aWidth, uint32_t aHeight, uint32_t aFramesPerSecond = 30);
  void ReceiveFrame(void* aBuffer, ImageFormat aFormat, FrameBuilder aBuilder);
  bool HaveEnoughBuffered();

  NS_IMETHODIMP
  GetCurrentTime(double* aCurrentTime) {
    return nsDOMMediaStream::GetCurrentTime(aCurrentTime);
  }

  void Start();   // called by the MediaStreamListener to start preview
  void Started(); // called by the CameraControl when preview is started
  void Stop();    // called by the MediaStreamListener to stop preview
  void Stopped(bool aForced = false);
                  // called by the CameraControl when preview is stopped
  void Error();   // something went wrong, NS_RELEASE needed

  void SetStateStarted();
  void SetStateStopped();

protected:
  virtual ~DOMCameraPreview();

  enum {
    STOPPED,
    STARTING,
    STARTED,
    STOPPING
  };
  uint32_t mState;

  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mFramesPerSecond;
  SourceMediaStream* mInput;
  nsRefPtr<ImageContainer> mImageContainer;
  VideoSegment mVideoSegment;
  uint32_t mFrameCount;
  nsRefPtr<ICameraControl> mCameraControl;

  // Raw pointer; AddListener() keeps the reference for us
  MediaStreamListener* mListener;

private:
  DOMCameraPreview(const DOMCameraPreview&) MOZ_DELETE;
  DOMCameraPreview& operator=(const DOMCameraPreview&) MOZ_DELETE;
};

} // namespace mozilla

#endif // DOM_CAMERA_DOMCAMERAPREVIEW_H
