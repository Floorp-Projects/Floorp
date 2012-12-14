/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */

/* This Source Code Form Is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DASH - Dynamic Adaptive Streaming over HTTP
 *
 * DASH is an adaptive bitrate streaming technology where a multimedia file is
 * partitioned into one or more segments and delivered to a client using HTTP.
 *
 * see DASHDecoder.cpp for comments on DASH object interaction
 */

#if !defined(DASHRepReader_h_)
#define DASHRepReader_h_

#include "VideoUtils.h"
#include "MediaDecoderReader.h"
#include "DASHReader.h"

namespace mozilla {

class DASHReader;

class DASHRepReader : public MediaDecoderReader
{
public:
  DASHRepReader(AbstractMediaDecoder* aDecoder)
    : MediaDecoderReader(aDecoder) { }
  virtual ~DASHRepReader() { }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DASHRepReader)

  virtual void SetMainReader(DASHReader *aMainReader) = 0;

  // Sets range for initialization bytes; used by DASH.
  virtual void SetInitByteRange(MediaByteRange &aByteRange) = 0;

  // Sets range for index frame bytes; used by DASH.
  virtual void SetIndexByteRange(MediaByteRange &aByteRange) = 0;

  // Returns the index of the subsegment which contains the seek time (usecs).
  virtual int64_t GetSubsegmentForSeekTime(int64_t aSeekToTime) = 0;

  // Returns list of ranges for index frame start/end offsets. Used by DASH.
  virtual nsresult GetSubsegmentByteRanges(nsTArray<MediaByteRange>& aByteRanges) = 0;

  // Returns true if the reader has reached a DASH switch access point.
  virtual bool HasReachedSubsegment(uint32_t aSubsegmentIndex) = 0;

  // Requests a seek to the start of a particular DASH subsegment.
  virtual void RequestSeekToSubsegment(uint32_t aIdx) = 0;

  // Reader should stop reading at the start of the specified subsegment, and
  // should prepare for the next reader to add data to the video queue.
  // Should be implemented by a sub-reader, e.g. |nsDASHWebMReader|.
  virtual void RequestSwitchAtSubsegment(int32_t aCluster,
                                         MediaDecoderReader* aNextReader) = 0;
};

}// namespace mozilla

#endif /*DASHRepReader*/
