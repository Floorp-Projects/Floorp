/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EncodedBufferCache_h_
#define EncodedBufferCache_h_

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "mozilla/Mutex.h"

struct PRFileDesc;
class nsIDOMBlob;

namespace mozilla {

class ReentrantMonitor;
/**
 * Data is moved into a temporary file when it grows beyond
 * the maximal size passed in the Init function.
 * The AppendBuffer and ExtractBlob methods are thread-safe and can be called on
 * different threads at the same time.
 */
class EncodedBufferCache
{
public:
  explicit EncodedBufferCache(uint32_t aMaxMemoryStorage)
  : mFD(nullptr),
    mMutex("EncodedBufferCache.Data.Mutex"),
    mDataSize(0),
    mMaxMemoryStorage(aMaxMemoryStorage),
    mTempFileEnabled(false) { }
  ~EncodedBufferCache()
  {
  }
  // Append buffers in cache, check if the queue is too large then switch to write buffer to file system
  // aBuf will append to mEncodedBuffers or temporary File, aBuf also be cleared
  void AppendBuffer(nsTArray<uint8_t> & aBuf);
  // Read all buffer from memory or file System, also Remove the temporary file or clean the buffers in memory.
  already_AddRefed<nsIDOMBlob> ExtractBlob(const nsAString &aContentType);

private:
  //array for storing the encoded data.
  nsTArray<nsTArray<uint8_t> > mEncodedBuffers;
  // File handle for the temporary file
  PRFileDesc* mFD;
  // Used to protect the mEncodedBuffer for avoiding AppendBuffer/Consume on different thread at the same time.
  Mutex mMutex;
  // the current buffer size can be read
  uint64_t mDataSize;
  // The maximal buffer allowed in memory
  uint32_t mMaxMemoryStorage;
  // indicate the buffer is stored on temporary file or not
  bool mTempFileEnabled;
};

} // namespace mozilla

#endif
