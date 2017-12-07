/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOCK_MEDIA_RESOURCE_H_
#define MOCK_MEDIA_RESOURCE_H_

#include "MediaResource.h"
#include "nsTArray.h"
#include "mozilla/Atomics.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(MockMediaResource, MediaResource);

class MockMediaResource
  : public MediaResource
  , public DecoderDoctorLifeLogger<MockMediaResource>
{
public:
  explicit MockMediaResource(const char* aFileName);
  nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount,
                  uint32_t* aBytes) override;
  // Data stored in file, caching recommended.
  bool ShouldCacheReads() override { return true; }
  void Pin() override {}
  void Unpin() override {}
  int64_t GetLength() override;
  int64_t GetNextCachedData(int64_t aOffset) override;
  int64_t GetCachedDataEnd(int64_t aOffset) override;
  bool IsDataCachedToEndOfResource(int64_t aOffset) override
  {
    return false;
  }
  nsresult ReadFromCache(char* aBuffer, int64_t aOffset,
                         uint32_t aCount) override
  {
    uint32_t bytesRead = 0;
    nsresult rv = ReadAt(aOffset, aBuffer, aCount, &bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);
    return bytesRead == aCount ? NS_OK : NS_ERROR_FAILURE;
  }

  nsresult Open();
  nsresult GetCachedRanges(MediaByteRangeSet& aRanges) override;

  void MockClearBufferedRanges();
  void MockAddBufferedRange(int64_t aStart, int64_t aEnd);

protected:
  virtual ~MockMediaResource();

private:
  FILE* mFileHandle;
  const char* mFileName;
  MediaByteRangeSet mRanges;
  Atomic<int> mEntry;
};

} // namespace mozilla

#endif
