/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_ReadStream_h
#define mozilla_dom_cache_ReadStream_h

#include "mozilla/ipc/FileDescriptor.h"
#include "nsCOMPtr.h"
#include "nsID.h"
#include "nsIInputStream.h"
#include "nsISupportsImpl.h"
#include "nsTArrayForwardDeclare.h"

class nsIThread;

namespace mozilla {
namespace dom {
namespace cache {

class PCacheReadStream;
class PCacheReadStreamOrVoid;
class PCacheStreamControlParent;

// IID for the dom::cache::ReadStream interface
#define NS_DOM_CACHE_READSTREAM_IID \
{0x8e5da7c9, 0x0940, 0x4f1d, \
  {0x97, 0x25, 0x5c, 0x59, 0x38, 0xdd, 0xb9, 0x9f}}

// Custom stream class for Request and Response bodies being read from
// a Cache.  The main purpose of this class is to report back to the
// Cache's Manager when the stream is closed.  This allows the Cache to
// accurately determine when the underlying body file can be deleted,
// etc.
//
// The ReadStream class also provides us with a convenient QI'able
// interface that we can use to pass additional meta-data with the
// stream channel.  For example, Cache.put() can detect that the content
// script is passing a Cache-originated-stream back into the Cache
// again.  This enables certain optimizations.
class ReadStream : public nsIInputStream
{
public:
  static already_AddRefed<ReadStream>
  Create(const PCacheReadStreamOrVoid& aReadStreamOrVoid);

  static already_AddRefed<ReadStream>
  Create(const PCacheReadStream& aReadStream);

  static already_AddRefed<ReadStream>
  Create(PCacheStreamControlParent* aControl, const nsID& aId,
         nsIInputStream* aStream);

  void Serialize(PCacheReadStreamOrVoid* aReadStreamOut);
  void Serialize(PCacheReadStream* aReadStreamOut);

  // methods called from the child and parent CacheStreamControl actors
  void CloseStream();
  void CloseStreamWithoutReporting();
  bool MatchId(const nsID& aId) const;

protected:
  class NoteClosedRunnable;
  class ForgetRunnable;

  ReadStream(const nsID& aId, nsIInputStream* aStream);
  virtual ~ReadStream();

  void NoteClosed();
  void Forget();

  virtual void NoteClosedOnOwningThread() = 0;
  virtual void ForgetOnOwningThread() = 0;
  virtual void SerializeControl(PCacheReadStream* aReadStreamOut) = 0;

  virtual void
  SerializeFds(PCacheReadStream* aReadStreamOut,
               const nsTArray<mozilla::ipc::FileDescriptor>& fds) = 0;

  const nsID mId;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIInputStream> mSnappyStream;
  nsCOMPtr<nsIThread> mOwningThread;
  bool mClosed;

public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOM_CACHE_READSTREAM_IID);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
};

NS_DEFINE_STATIC_IID_ACCESSOR(ReadStream, NS_DOM_CACHE_READSTREAM_IID);

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_ReadStream_h
