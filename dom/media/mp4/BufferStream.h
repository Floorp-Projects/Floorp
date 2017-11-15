/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BUFFER_STREAM_H_
#define BUFFER_STREAM_H_

#include "mp4_demuxer/Stream.h"
#include "nsTArray.h"
#include "MediaResource.h"

namespace mozilla {
class MediaByteBuffer;
}

namespace mp4_demuxer {

class BufferStream : public Stream
{
public:
  /* BufferStream does not take ownership of aData nor does it make a copy.
   * Therefore BufferStream shouldn't get used after aData is destroyed.
   */
  BufferStream();
  explicit BufferStream(mozilla::MediaByteBuffer* aBuffer);

  virtual bool ReadAt(int64_t aOffset, void* aData, size_t aLength,
                      size_t* aBytesRead) override;
  virtual bool CachedReadAt(int64_t aOffset, void* aData, size_t aLength,
                            size_t* aBytesRead) override;
  virtual bool Length(int64_t* aLength) override;

  virtual void DiscardBefore(int64_t aOffset) override;

  bool AppendBytes(const uint8_t* aData, size_t aLength);

  mozilla::MediaByteRange GetByteRange();

private:
  ~BufferStream();
  int64_t mStartOffset;
  RefPtr<mozilla::MediaByteBuffer> mData;
};
}

#endif
