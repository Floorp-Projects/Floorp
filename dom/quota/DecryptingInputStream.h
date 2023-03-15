/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_DecryptingInputStream_h
#define mozilla_dom_quota_DecryptingInputStream_h

// Local includes
#include "EncryptedBlock.h"

// Global includes
#include <cstddef>
#include <cstdint>
#include "ErrorList.h"
#include "mozilla/InitializedOnce.h"
#include "mozilla/Maybe.h"
#include "mozilla/NotNull.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "nsCOMPtr.h"
#include "nsICloneableInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIInputStream.h"
#include "nsISeekableStream.h"
#include "nsISupports.h"
#include "nsITellableStream.h"
#include "nsTArray.h"
#include "nscore.h"

template <class T>
class nsCOMPtr;

namespace mozilla::dom::quota {

class DecryptingInputStreamBase : public nsIInputStream,
                                  public nsISeekableStream,
                                  public nsICloneableInputStream,
                                  public nsIIPCSerializableInputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Read(char* aBuf, uint32_t aCount, uint32_t* _retval) final;
  NS_IMETHOD IsNonBlocking(bool* _retval) final;

  NS_IMETHOD SetEOF() final;

  using nsICloneableInputStream::GetCloneable;
  NS_IMETHOD GetCloneable(bool* aCloneable) final;

  void SerializedComplexity(uint32_t aMaxSize, uint32_t* aSizeUsed,
                            uint32_t* aPipes,
                            uint32_t* aTransferables) override;

 protected:
  DecryptingInputStreamBase(MovingNotNull<nsCOMPtr<nsIInputStream>> aBaseStream,
                            size_t aBlockSize);

  // For deserialization only.
  DecryptingInputStreamBase() = default;

  virtual ~DecryptingInputStreamBase() = default;

  void Init(MovingNotNull<nsCOMPtr<nsIInputStream>> aBaseStream,
            size_t aBlockSize);

  // Convenience routine to determine how many bytes of plain data
  // we currently have in our buffer.
  size_t PlainLength() const;

  size_t EncryptedBufferLength() const;

  LazyInitializedOnceEarlyDestructible<const NotNull<nsCOMPtr<nsIInputStream>>>
      mBaseStream;
  LazyInitializedOnce<const NotNull<nsISeekableStream*>> mBaseSeekableStream;
  LazyInitializedOnce<const NotNull<nsICloneableInputStream*>>
      mBaseCloneableInputStream;
  LazyInitializedOnce<const NotNull<nsIIPCSerializableInputStream*>>
      mBaseIPCSerializableInputStream;

  // Number of bytes of plain data in mBuffer.
  size_t mPlainBytes = 0;

  // Next byte of mBuffer to return in ReadSegments().
  size_t mNextByte = 0;

  LazyInitializedOnceNotNull<const size_t> mBlockSize;

  size_t mLastBlockLength = 0;
};

// Wraps another nsIInputStream which contains data written using
// EncryptingInputStream with a compatible CipherStategy and key. See the
// remarks on EncryptingOutputStream.
template <typename CipherStrategy>
class DecryptingInputStream final : public DecryptingInputStreamBase {
 public:
  // Construct a new blocking stream to decrypt the given base stream.  The
  // base stream must also be blocking.  The base stream does not have to be
  // buffered.
  DecryptingInputStream(MovingNotNull<nsCOMPtr<nsIInputStream>> aBaseStream,
                        size_t aBlockSize,
                        typename CipherStrategy::KeyType aKey);

  // For deserialization only.
  explicit DecryptingInputStream();

  NS_IMETHOD Close() override;
  NS_IMETHOD Available(uint64_t* _retval) override;
  NS_IMETHOD StreamStatus() override;
  NS_IMETHOD ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                          uint32_t aCount, uint32_t* _retval) override;

  NS_DECL_NSITELLABLESTREAM

  NS_IMETHOD Seek(int32_t aWhence, int64_t aOffset) override;

  NS_IMETHOD Clone(nsIInputStream** _retval) override;

  void Serialize(mozilla::ipc::InputStreamParams& aParams, uint32_t aMaxSize,
                 uint32_t* aSizeUsed) override;

  bool Deserialize(const mozilla::ipc::InputStreamParams& aParams) override;

 private:
  ~DecryptingInputStream();

  // Parse the next chunk of data.  This may populate mBuffer and set
  // mBufferFillSize.  This should not be called when mBuffer already
  // contains data.
  nsresult ParseNextChunk(uint32_t* aBytesReadOut);

  // Convenience routine to Read() from the base stream until we get
  // the given number of bytes or reach EOF.
  //
  // aBuf           - The buffer to write the bytes into.
  // aCount         - Max number of bytes to read. If the stream closes
  //                  fewer bytes my be read.
  // aMinValidCount - A minimum expected number of bytes.  If we find
  //                  fewer than this many bytes, then return
  //                  NS_ERROR_CORRUPTED_CONTENT.  If nothing was read due
  //                  due to EOF (aBytesReadOut == 0), then NS_OK is returned.
  // aBytesReadOut  - An out parameter indicating how many bytes were read.
  nsresult ReadAll(char* aBuf, uint32_t aCount, uint32_t aMinValidCount,
                   uint32_t* aBytesReadOut);

  bool EnsureBuffers();

  CipherStrategy mCipherStrategy;
  LazyInitializedOnce<const typename CipherStrategy::KeyType> mKey;

  // Buffer to hold encrypted data.  Must copy here since we need a
  // flat buffer to run the decryption process on.
  using EncryptedBlockType = EncryptedBlock<CipherStrategy::BlockPrefixLength,
                                            CipherStrategy::BasicBlockSize>;
  Maybe<EncryptedBlockType> mEncryptedBlock;

  // Buffer storing the resulting plain data.
  nsTArray<uint8_t> mPlainBuffer;
};

}  // namespace mozilla::dom::quota

#endif
