/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BUFFER_DECODER_H_
#define BUFFER_DECODER_H_

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/TaskQueue.h"

#include "AbstractMediaDecoder.h"

namespace mozilla {

/**
 * This class provides a decoder object which decodes a media file that lives in
 * a memory buffer.
 */
class BufferDecoder final : public AbstractMediaDecoder
{
public:
  // This class holds a weak pointer to MediaResource.  It's the responsibility
  // of the caller to manage the memory of the MediaResource object.
  explicit BufferDecoder(MediaResource* aResource);

  NS_DECL_THREADSAFE_ISUPPORTS

  // This has to be called before decoding begins
  void BeginDecoding(TaskQueue* aTaskQueueIdentity);

  virtual MediaResource* GetResource() const final override;

  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded,
                                   uint32_t aDropped) final override;

  virtual VideoFrameContainer* GetVideoFrameContainer() final override;
  virtual layers::ImageContainer* GetImageContainer() final override;

  virtual MediaDecoderOwner* GetOwner() final override;

private:
  virtual ~BufferDecoder();
  RefPtr<TaskQueue> mTaskQueueIdentity;
  RefPtr<MediaResource> mResource;
};

} // namespace mozilla

#endif /* BUFFER_DECODER_H_ */
