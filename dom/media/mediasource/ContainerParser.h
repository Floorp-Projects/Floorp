/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_CONTAINERPARSER_H_
#define MOZILLA_CONTAINERPARSER_H_

#include "mozilla/RefPtr.h"
#include "nsString.h"
#include "MediaResource.h"
#include "MediaResult.h"

namespace mozilla {

class MediaByteBuffer;
class SourceBufferResource;

class ContainerParser {
public:
  explicit ContainerParser(const nsACString& aType);
  virtual ~ContainerParser();

  // Return true if aData starts with an initialization segment.
  // The base implementation exists only for debug logging and is expected
  // to be called first from the overriding implementation.
  // Return NS_OK if segment is present, NS_ERROR_NOT_AVAILABLE if no sufficient
  // data is currently available to make a determination. Any other value
  // indicates an error.
  virtual MediaResult IsInitSegmentPresent(MediaByteBuffer* aData);

  // Return true if aData starts with a media segment.
  // The base implementation exists only for debug logging and is expected
  // to be called first from the overriding implementation.
  // Return NS_OK if segment is present, NS_ERROR_NOT_AVAILABLE if no sufficient
  // data is currently available to make a determination. Any other value
  // indicates an error.
  virtual MediaResult IsMediaSegmentPresent(MediaByteBuffer* aData);

  // Parse aData to extract the start and end frame times from the media
  // segment.  aData may not start on a parser sync boundary.  Return true
  // if aStart and aEnd have been updated.
  virtual bool ParseStartAndEndTimestamps(MediaByteBuffer* aData,
                                          int64_t& aStart, int64_t& aEnd);

  // Compare aLhs and rHs, considering any error that may exist in the
  // timestamps from the format's base representation.  Return true if aLhs
  // == aRhs within the error epsilon.
  bool TimestampsFuzzyEqual(int64_t aLhs, int64_t aRhs);

  virtual int64_t GetRoundingError();

  MediaByteBuffer* InitData();

  bool HasInitData()
  {
    return mHasInitData;
  }

  // Return true if a complete initialization segment has been passed
  // to ParseStartAndEndTimestamps(). The calls below to retrieve
  // MediaByteRanges will be valid from when this call first succeeds.
  bool HasCompleteInitData();
  // Returns the byte range of the first complete init segment, or an empty
  // range if not complete.
  MediaByteRange InitSegmentRange();
  // Returns the byte range of the first complete media segment header,
  // or an empty range if not complete.
  MediaByteRange MediaHeaderRange();
  // Returns the byte range of the first complete media segment or an empty
  // range if not complete.
  MediaByteRange MediaSegmentRange();

  static ContainerParser* CreateForMIMEType(const nsACString& aType);

protected:
  RefPtr<MediaByteBuffer> mInitData;
  RefPtr<SourceBufferResource> mResource;
  bool mHasInitData;
  MediaByteRange mCompleteInitSegmentRange;
  MediaByteRange mCompleteMediaHeaderRange;
  MediaByteRange mCompleteMediaSegmentRange;
  const nsCString mType;
};

} // namespace mozilla

#endif /* MOZILLA_CONTAINERPARSER_H_ */
