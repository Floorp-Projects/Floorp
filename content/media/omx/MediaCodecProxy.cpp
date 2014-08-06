/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaCodecProxy.h"

#include <string.h>

#include <binder/IPCThreadState.h>

namespace android {

sp<MediaCodecProxy>
MediaCodecProxy::CreateByType(sp<ALooper> aLooper,
                              const char *aMime,
                              bool aEncoder,
                              bool aAsync,
                              wp<CodecResourceListener> aListener)
{
  sp<MediaCodecProxy> codec = new MediaCodecProxy(aLooper, aMime, aEncoder, aAsync, aListener);
  if ((!aAsync && codec->allocated()) || codec->requestResource()) {
    return codec;
  }
  return nullptr;
}

MediaCodecProxy::MediaCodecProxy(sp<ALooper> aLooper,
                                 const char *aMime,
                                 bool aEncoder,
                                 bool aAsync,
                                 wp<CodecResourceListener> aListener)
  : mCodecLooper(aLooper)
  , mCodecMime(aMime)
  , mCodecEncoder(aEncoder)
  , mListener(aListener)
{
  MOZ_ASSERT(mCodecLooper != nullptr, "ALooper should not be nullptr.");
  if (aAsync) {
    mResourceHandler = new MediaResourceHandler(this);
  } else {
    allocateCodec();
  }
}

MediaCodecProxy::~MediaCodecProxy()
{
  releaseCodec();

  // Complete all pending Binder ipc transactions
  IPCThreadState::self()->flushCommands();

  cancelResource();
}

bool
MediaCodecProxy::requestResource()
{
  if (mResourceHandler == nullptr) {
    return false;
  }

  if (strncasecmp(mCodecMime.get(), "video/", 6) == 0) {
    mResourceHandler->requestResource(mCodecEncoder
        ? IMediaResourceManagerService::HW_VIDEO_ENCODER
        : IMediaResourceManagerService::HW_VIDEO_DECODER);
  } else if (strncasecmp(mCodecMime.get(), "audio/", 6) == 0) {
    mResourceHandler->requestResource(mCodecEncoder
        ? IMediaResourceManagerService::HW_AUDIO_ENCODER
        : IMediaResourceManagerService::HW_AUDIO_DECODER);
  } else {
    return false;
  }

  return true;
}

void
MediaCodecProxy::cancelResource()
{
  if (mResourceHandler == nullptr) {
    return;
  }

  mResourceHandler->cancelResource();
}

bool
MediaCodecProxy::allocateCodec()
{
  if (mCodecLooper == nullptr) {
    return false;
  }

  // Write Lock for mCodec
  RWLock::AutoWLock awl(mCodecLock);

  // Create MediaCodec
  mCodec = MediaCodec::CreateByType(mCodecLooper, mCodecMime.get(), mCodecEncoder);
  if (mCodec == nullptr) {
    return false;
  }

  return true;
}

void
MediaCodecProxy::releaseCodec()
{
  wp<MediaCodec> codec;

  {
    // Write Lock for mCodec
    RWLock::AutoWLock awl(mCodecLock);

    codec = mCodec;

    // Release MediaCodec
    if (mCodec != nullptr) {
      mCodec->release();
      mCodec = nullptr;
    }
  }

  while (codec.promote() != nullptr) {
    // this value come from stagefright's AwesomePlayer.
    usleep(1000);
  }
}

bool
MediaCodecProxy::allocated() const
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  return mCodec != nullptr;
}

status_t
MediaCodecProxy::configure(const sp<AMessage> &aFormat,
                           const sp<Surface> &aNativeWindow,
                           const sp<ICrypto> &aCrypto,
                           uint32_t aFlags)
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->configure(aFormat, aNativeWindow, aCrypto, aFlags);
}

status_t
MediaCodecProxy::start()
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->start();
}

status_t
MediaCodecProxy::stop()
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->stop();
}

status_t
MediaCodecProxy::release()
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->release();
}

status_t
MediaCodecProxy::flush()
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->flush();
}

status_t
MediaCodecProxy::queueInputBuffer(size_t aIndex,
                                  size_t aOffset,
                                  size_t aSize,
                                  int64_t aPresentationTimeUs,
                                  uint32_t aFlags,
                                  AString *aErrorDetailMessage)
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->queueInputBuffer(aIndex, aOffset, aSize,
      aPresentationTimeUs, aFlags, aErrorDetailMessage);
}

status_t
MediaCodecProxy::queueSecureInputBuffer(size_t aIndex,
                                        size_t aOffset,
                                        const CryptoPlugin::SubSample *aSubSamples,
                                        size_t aNumSubSamples,
                                        const uint8_t aKey[16],
                                        const uint8_t aIV[16],
                                        CryptoPlugin::Mode aMode,
                                        int64_t aPresentationTimeUs,
                                        uint32_t aFlags,
                                        AString *aErrorDetailMessage)
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->queueSecureInputBuffer(aIndex, aOffset,
      aSubSamples, aNumSubSamples, aKey, aIV, aMode,
      aPresentationTimeUs, aFlags, aErrorDetailMessage);
}

status_t
MediaCodecProxy::dequeueInputBuffer(size_t *aIndex,
                                    int64_t aTimeoutUs)
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->dequeueInputBuffer(aIndex, aTimeoutUs);
}

status_t
MediaCodecProxy::dequeueOutputBuffer(size_t *aIndex,
                                     size_t *aOffset,
                                     size_t *aSize,
                                     int64_t *aPresentationTimeUs,
                                     uint32_t *aFlags,
                                     int64_t aTimeoutUs)
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->dequeueOutputBuffer(aIndex, aOffset, aSize,
      aPresentationTimeUs, aFlags, aTimeoutUs);
}

status_t
MediaCodecProxy::renderOutputBufferAndRelease(size_t aIndex)
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->renderOutputBufferAndRelease(aIndex);
}

status_t
MediaCodecProxy::releaseOutputBuffer(size_t aIndex)
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->releaseOutputBuffer(aIndex);
}

status_t
MediaCodecProxy::signalEndOfInputStream()
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->signalEndOfInputStream();
}

status_t
MediaCodecProxy::getOutputFormat(sp<AMessage> *aFormat) const
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->getOutputFormat(aFormat);
}

status_t
MediaCodecProxy::getInputBuffers(Vector<sp<ABuffer>> *aBuffers) const
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->getInputBuffers(aBuffers);
}

status_t
MediaCodecProxy::getOutputBuffers(Vector<sp<ABuffer>> *aBuffers) const
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->getOutputBuffers(aBuffers);
}

void
MediaCodecProxy::requestActivityNotification(const sp<AMessage> &aNotify)
{
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return;
  }
  mCodec->requestActivityNotification(aNotify);
}

// Called on a Binder thread
void
MediaCodecProxy::resourceReserved()
{
  // Create MediaCodec
  releaseCodec();
  if (!allocateCodec()) {
    cancelResource();
    return;
  }

  // Notification
  sp<CodecResourceListener> listener = mListener.promote();
  if (listener != nullptr) {
    listener->codecReserved();
  }
}

// Called on a Binder thread
void
MediaCodecProxy::resourceCanceled()
{
  // Release MediaCodec
  releaseCodec();

  // Notification
  sp<CodecResourceListener> listener = mListener.promote();
  if (listener != nullptr) {
    listener->codecCanceled();
  }
}

} // namespace android
