/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERAPREVIEWMEDIASTREAM_H
#define DOM_CAMERA_CAMERAPREVIEWMEDIASTREAM_H

#include "VideoFrameContainer.h"
#include "MediaStreamGraph.h"
#include "mozilla/Mutex.h"

namespace mozilla {

/**
 * This is a stream for camere preview.
 *
 * XXX It is a temporary fix of SourceMediaStream.
 * A camera preview requests no delay and no buffering stream.
 * But the SourceMediaStream do not support it.
 */
class CameraPreviewMediaStream : public MediaStream {
  typedef mozilla::layers::Image Image;

public:
  CameraPreviewMediaStream(DOMMediaStream* aWrapper) :
    MediaStream(aWrapper),
    mMutex("mozilla::camera::CameraPreviewMediaStream")
  {
    mIsConsumed = false;
  }

  virtual void AddAudioOutput(void* aKey);
  virtual void SetAudioOutputVolume(void* aKey, float aVolume);
  virtual void RemoveAudioOutput(void* aKey);
  virtual void AddVideoOutput(VideoFrameContainer* aContainer);
  virtual void RemoveVideoOutput(VideoFrameContainer* aContainer);
  virtual void ChangeExplicitBlockerCount(int32_t aDelta);
  virtual void AddListener(MediaStreamListener* aListener);
  virtual void RemoveListener(MediaStreamListener* aListener);
  virtual void Destroy();

  // Call these on any thread.
  void SetCurrentFrame(const gfxIntSize& aIntrinsicSize, Image* aImage);

protected:
  // mMutex protects all the class' fields.
  // This class is not registered to MediaStreamGraph.
  // It needs to protect all the fields.
  Mutex mMutex;
};


}

#endif // DOM_CAMERA_CAMERAPREVIEWMEDIASTREAM_H
