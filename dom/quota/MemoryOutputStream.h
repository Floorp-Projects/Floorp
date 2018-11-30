/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_MemoryOutputStream_h
#define mozilla_dom_quota_MemoryOutputStream_h

#include "nsIOutputStream.h"

namespace mozilla {
namespace dom {
namespace quota {

// An output stream so you can read your potentially-async input stream into
// a contiguous buffer in the form of an nsCString using NS_AsyncCopy.
// Back when streams were more synchronous and people didn't know blocking I/O
// was bad, if you wanted to read a stream into a flat buffer, you could use
// NS_ReadInputStreamToString/NS_ReadInputStreamToBuffer.  But those don't work
// with async streams. This can be used to replace hand-rolled Read/AsyncWait()
// loops.  Because you specify the expected size up front, the nsCString buffer
// is pre-allocated so wasteful reallocations can be avoided.  However,
// nsCString currently may over-allocate and this can be problematic on 32-bit
// windows until we're rid of that build configuration.
class MemoryOutputStream final : public nsIOutputStream {
  nsCString mData;
  uint64_t mOffset;

 public:
  static already_AddRefed<MemoryOutputStream> Create(uint64_t aSize);

  const nsCString& Data() const { return mData; }

 private:
  MemoryOutputStream() : mOffset(0) {}

  virtual ~MemoryOutputStream() {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
};

}  // namespace quota
}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_quota_MemoryOutputStream_h */
