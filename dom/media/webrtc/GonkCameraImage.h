/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GONKCAMERAIMAGE_H
#define GONKCAMERAIMAGE_H

#include "mozilla/ReentrantMonitor.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "GrallocImages.h"

namespace android {
class MOZ_EXPORT MediaBuffer;
}

namespace mozilla {

/**
 * GonkCameraImage has two parts. One is preview image which will be saved in
 * GrallocImage, another kind is the MediaBuffer keeps in mMediaBuffer
 * which is from gonk camera recording callback. The data in MediaBuffer is Gonk
 * shared memory based on android binder (IMemory), the actual format in IMemory
 * is platform dependent.
 * This instance is created in MediaEngine when the preview image arrives.
 * The MediaBuffer is attached to the current created GonkCameraImage via SetBuffer().
 * After sending this image to MediaStreamGraph by AppendToTrack(), ClearBuffer()
 * must be called to clear MediaBuffer to avoid MediaBuffer be kept in MSG thread.
 * The reason to keep MediaBuffer be accessed from MSG thread is MediaBuffer is
 * limited resource and it could cause frame rate jitter if MediaBuffer stay too
 * long in other threads.
 * So there will be 3 threads to accessed this class. First is camera preview
 * thread which creates an instance of this class and initialize the preview
 * image in the base class GrallocImage. Second is the camera recording
 * thread which attaches MediaBuffer and sends this image to MediaStreamDirectListener.
 * Third is the MSG thread via NotifyPull, the image should have preview image
 * only in NotifyPull.
 *
 * Note: SetBuffer() and GetBuffer() should be called from the same thread. It
 *       is forbidden to call GetBuffer() from other threads.
 */
class GonkCameraImage : public layers::GrallocImage
{
public:
  GonkCameraImage();

  // The returned aBuffer has called aBuffer->add_ref() already, so it is caller's
  // duty to release aBuffer. It should be called from the same thread which
  // called SetBuffer().
  nsresult GetBuffer(android::MediaBuffer** aBuffer);

  // Set MediaBuffer to image. It is caller's responsibility to call ClearBuffer()
  // after the MediaBuffer is sent via MediaStreamGraph.
  nsresult SetBuffer(android::MediaBuffer* aBuffer);

  // It should be called from the same thread which called SetBuffer().
  nsresult ClearBuffer();

  bool HasMediaBuffer();

protected:
  virtual ~GonkCameraImage();

  // mMonitor protects mMediaBuffer and mThread.
  ReentrantMonitor mMonitor;
  android::MediaBuffer* mMediaBuffer;
  // Check if current thread is the same one which called SetBuffer().
  // It doesn't need to hold reference count.
  DebugOnly<nsIThread*> mThread;
};

} // namespace mozilla

#endif /* GONKCAMERAIMAGE_H */
