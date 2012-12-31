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

namespace mozilla {

typedef void (*FrameBuilder)(mozilla::layers::Image* aImage, void* aBuffer, uint32_t aWidth, uint32_t aHeight);

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
  bool ReceiveFrame(void* aBuffer, ImageFormat aFormat, mozilla::FrameBuilder aBuilder);
  bool HaveEnoughBuffered();

  NS_IMETHODIMP
  GetCurrentTime(double* aCurrentTime) {
    return nsDOMMediaStream::GetCurrentTime(aCurrentTime);
  }

  void Start();   // called by the MediaStreamListener to start preview
  void Started(); // called by the CameraControl when preview is started
  void StopPreview(); // called by the MediaStreamListener to stop preview
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

  // Helper function, used in conjunction with the macro below, to make
  //  it easy to track state changes, which must happen only on the main
  //  thread.
  void
  SetState(uint32_t aNewState, const char* aFileOrFunc, int aLine)
  {
#ifdef PR_LOGGING
    const char* states[] = { "stopped", "starting", "started", "stopping" };
    MOZ_ASSERT(mState < sizeof(states) / sizeof(states[0]));
    MOZ_ASSERT(aNewState < sizeof(states) / sizeof(states[0]));
    DOM_CAMERA_LOGI("SetState: (this=%p) '%s' --> '%s' : %s:%d\n", this, states[mState], states[aNewState], aFileOrFunc, aLine);
#endif

    NS_ASSERTION(NS_IsMainThread(), "Preview state set OFF OF main thread!");
    mState = aNewState;
  }

  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mFramesPerSecond;
  SourceMediaStream* mInput;
  nsRefPtr<mozilla::layers::ImageContainer> mImageContainer;
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

#define DOM_CAMERA_SETSTATE(newState)   SetState((newState), __func__, __LINE__)

#endif // DOM_CAMERA_DOMCAMERAPREVIEW_H
