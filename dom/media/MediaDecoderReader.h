/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaDecoderReader_h_)
#define MediaDecoderReader_h_

#include "AbstractMediaDecoder.h"
#include "AudioCompactor.h"
#include "Intervals.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "MediaMetadataManager.h"
#include "MediaQueue.h"
#include "MediaResult.h"
#include "MediaTimer.h"
#include "SeekTarget.h"
#include "TimeUnits.h"
#include "mozilla/EnumSet.h"
#include "mozilla/MozPromise.h"
#include "nsAutoPtr.h"

namespace mozilla {

class CDMProxy;
class GMPCrashHelper;
class MediaDecoderReader;
class TaskQueue;
class VideoFrameContainer;

// Encapsulates the decoding and reading of media data. Reading can either
// synchronous and done on the calling "decode" thread, or asynchronous and
// performed on a background thread, with the result being returned by
// callback.
// Unless otherwise specified, methods and fields of this class can only
// be accessed on the decode task queue.
class MediaDecoderReader
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderReader)

  // The caller must ensure that Shutdown() is called before aDecoder is
  // destroyed.
  MediaDecoderReader();

protected:
  virtual ~MediaDecoderReader();
};

} // namespace mozilla

#endif
