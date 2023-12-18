/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_EncryptingOutputStream_h
#define mozilla_dom_quota_EncryptingOutputStream_h

// Local includes
#include "EncryptedBlock.h"  // for EncryptedBlock

// Global includes
#include <cstddef>
#include <cstdint>
#include "ErrorList.h"
#include "mozilla/InitializedOnce.h"
#include "mozilla/Maybe.h"
#include "mozilla/NotNull.h"
#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nscore.h"

class nsIInputStream;
class nsIRandomGenerator;

namespace mozilla::dom::quota {
class EncryptingOutputStreamBase : public nsIOutputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Write(const char* aBuf, uint32_t aCount, uint32_t* _retval) final;
  NS_IMETHOD WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                       uint32_t* _retval) final;
  NS_IMETHOD IsNonBlocking(bool* _retval) final;

 protected:
  EncryptingOutputStreamBase(nsCOMPtr<nsIOutputStream> aBaseStream,
                             size_t aBlockSize);

  virtual ~EncryptingOutputStreamBase() = default;

  nsresult WriteAll(const char* aBuf, uint32_t aCount,
                    uint32_t* aBytesWrittenOut);

  InitializedOnce<const NotNull<nsCOMPtr<nsIOutputStream>>> mBaseStream;
  const size_t mBlockSize;
};

// Wraps another nsIOutputStream using the CipherStrategy to encrypt it a
// page-based manner. Essentially, the CipherStrategy is not actually
// necessarily doing encryption, but any transformation to a page requiring some
// fixed-size reserved size per page.
//
// Paired with DecryptingInputStream which can be used to read the data written
// to the underlying stream, using the same (or more generally, a compatible)
// CipherStrategy, when created with the same key (assuming a symmetric cipher
// is being used; in principle, an asymmetric cipher would probably also work).
template <typename CipherStrategy>
class EncryptingOutputStream final : public EncryptingOutputStreamBase {
 public:
  // Construct a new blocking output stream to encrypt data to
  // the given base stream.  The base stream must also be blocking.
  // The encryption block size may optionally be set to a value
  // up to kMaxBlockSize.
  explicit EncryptingOutputStream(nsCOMPtr<nsIOutputStream> aBaseStream,
                                  size_t aBlockSize,
                                  typename CipherStrategy::KeyType aKey);

 private:
  ~EncryptingOutputStream();

  nsresult FlushToBaseStream();

  bool EnsureBuffers();

  CipherStrategy mCipherStrategy;

  // Buffer holding copied plain data.  This must be copied here
  // so that the encryption can be performed on a single flat buffer.
  // XXX This is only necessary if the data written doesn't contain a portion of
  // effective block size at a block boundary.
  nsTArray<uint8_t> mBuffer;

  nsCOMPtr<nsIRandomGenerator> mRandomGenerator;

  // The next byte in the plain data to copy incoming data to.
  size_t mNextByte = 0;

  // Buffer holding the resulting encrypted data.
  using EncryptedBlockType = EncryptedBlock<CipherStrategy::BlockPrefixLength,
                                            CipherStrategy::BasicBlockSize>;
  Maybe<EncryptedBlockType> mEncryptedBlock;

 public:
  NS_IMETHOD Close() override;
  NS_IMETHOD Flush() override;
  NS_IMETHOD StreamStatus() override;
  NS_IMETHOD WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                           uint32_t aCount, uint32_t* _retval) override;
};

}  // namespace mozilla::dom::quota

#endif
