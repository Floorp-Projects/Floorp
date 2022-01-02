/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WidevineUtils_h_
#define WidevineUtils_h_

#include "stddef.h"
#include "content_decryption_module.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "mozilla/Logging.h"

namespace mozilla {

#define ENSURE_TRUE(condition, rv)                                   \
  {                                                                  \
    if (!(condition)) {                                              \
      GMP_LOG_DEBUG("ENSURE_TRUE FAILED %s:%d", __FILE__, __LINE__); \
      return rv;                                                     \
    }                                                                \
  }

#define ENSURE_GMP_SUCCESS(err, rv)                                         \
  {                                                                         \
    if (GMP_FAILED(err)) {                                                  \
      GMP_LOG_DEBUG("ENSURE_GMP_SUCCESS FAILED %s:%d", __FILE__, __LINE__); \
      return rv;                                                            \
    }                                                                       \
  }

namespace gmp {
class CDMShmemBuffer;
}
class WidevineBuffer;

// Base class for our cdm::Buffer implementations, so we can tell at runtime
// whether the buffer is a Shmem or non-Shmem buffer.
class CDMBuffer : public cdm::Buffer {
 public:
  virtual WidevineBuffer* AsArrayBuffer() { return nullptr; }
  virtual gmp::CDMShmemBuffer* AsShmemBuffer() { return nullptr; }
};

class WidevineBuffer : public CDMBuffer {
 public:
  explicit WidevineBuffer(size_t aSize);
  ~WidevineBuffer() override;
  void Destroy() override;
  uint32_t Capacity() const override;
  uint8_t* Data() override;
  void SetSize(uint32_t aSize) override;
  uint32_t Size() const override;

  // Moves contents of buffer out into temporary.
  // Note: This empties the buffer.
  nsTArray<uint8_t> ExtractBuffer();

  WidevineBuffer* AsArrayBuffer() override { return this; }

 private:
  nsTArray<uint8_t> mBuffer;
  WidevineBuffer(const WidevineBuffer&);
  void operator=(const WidevineBuffer&);
};

class WidevineDecryptedBlock : public cdm::DecryptedBlock {
 public:
  WidevineDecryptedBlock();
  ~WidevineDecryptedBlock() override;
  void SetDecryptedBuffer(cdm::Buffer* aBuffer) override;
  cdm::Buffer* DecryptedBuffer() override;
  void SetTimestamp(int64_t aTimestamp) override;
  int64_t Timestamp() const override;

 private:
  cdm::Buffer* mBuffer;
  int64_t mTimestamp;
};

}  // namespace mozilla

#endif  // WidevineUtils_h_
