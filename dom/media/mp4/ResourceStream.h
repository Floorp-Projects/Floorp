/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RESOURCESTREAM_H_
#define RESOURCESTREAM_H_

#include "MediaResource.h"
#include "ByteStream.h"
#include "mozilla/RefPtr.h"

namespace mozilla
{

DDLoggedTypeDeclNameAndBase(ResourceStream, ByteStream);

class ResourceStream
  : public ByteStream
  , public DecoderDoctorLifeLogger<ResourceStream>
{
public:
  explicit ResourceStream(mozilla::MediaResource* aResource);

  virtual bool ReadAt(int64_t offset, void* aBuffer, size_t aCount,
                      size_t* aBytesRead) override;
  virtual bool CachedReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                            size_t* aBytesRead) override;
  virtual bool Length(int64_t* size) override;

  void Pin()
  {
    mResource.GetResource()->Pin();
    ++mPinCount;
  }

  void Unpin()
  {
    mResource.GetResource()->Unpin();
    MOZ_ASSERT(mPinCount);
    --mPinCount;
  }

protected:
  virtual ~ResourceStream();

private:
  mozilla::MediaResourceIndex mResource;
  uint32_t mPinCount;
};

}

#endif // RESOURCESTREAM_H_
