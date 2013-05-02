/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CameraPreviewMediaStream.h"

using namespace mozilla::layers;
using namespace mozilla::dom;

namespace mozilla {

void
CameraPreviewMediaStream::AddAudioOutput(void* aKey)
{
}

void
CameraPreviewMediaStream::SetAudioOutputVolume(void* aKey, float aVolume)
{
}

void
CameraPreviewMediaStream::RemoveAudioOutput(void* aKey)
{
}

void
CameraPreviewMediaStream::AddVideoOutput(VideoFrameContainer* aContainer)
{
  MutexAutoLock lock(mMutex);
  nsRefPtr<VideoFrameContainer> container = aContainer;
  AddVideoOutputImpl(container.forget());

  if (mVideoOutputs.Length() > 1) {
    return;
  }
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  mIsConsumed = true;
  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    l->NotifyConsumptionChanged(gm, MediaStreamListener::CONSUMED);
  }
}

void
CameraPreviewMediaStream::RemoveVideoOutput(VideoFrameContainer* aContainer)
{
  MutexAutoLock lock(mMutex);
  RemoveVideoOutputImpl(aContainer);

  if (!mVideoOutputs.IsEmpty()) {
    return;
  }
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  mIsConsumed = false;
  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    l->NotifyConsumptionChanged(gm, MediaStreamListener::NOT_CONSUMED);
  }
}

void
CameraPreviewMediaStream::ChangeExplicitBlockerCount(int32_t aDelta)
{
}

void
CameraPreviewMediaStream::AddListener(MediaStreamListener* aListener)
{
  MutexAutoLock lock(mMutex);

  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  MediaStreamListener* listener = *mListeners.AppendElement() = aListener;
  listener->NotifyBlockingChanged(gm, MediaStreamListener::UNBLOCKED);
}

void
CameraPreviewMediaStream::RemoveListener(MediaStreamListener* aListener)
{
  MutexAutoLock lock(mMutex);

  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  nsRefPtr<MediaStreamListener> listener(aListener);
  mListeners.RemoveElement(aListener);
  listener->NotifyRemoved(gm);
}

void
CameraPreviewMediaStream::Destroy()
{
  MutexAutoLock lock(mMutex);
  DestroyImpl();
}

void
CameraPreviewMediaStream::SetCurrentFrame(const gfxIntSize& aIntrinsicSize, Image* aImage)
{
  MutexAutoLock lock(mMutex);

  TimeStamp now = TimeStamp::Now();
  for (uint32_t i = 0; i < mVideoOutputs.Length(); ++i) {
    VideoFrameContainer* output = mVideoOutputs[i];
    output->SetCurrentFrame(aIntrinsicSize, aImage, now);
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(output, &VideoFrameContainer::Invalidate);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }

  if (mFrameCallback) {
    mFrameCallback->OnNewFrame(aIntrinsicSize, aImage);
  }
}

}
