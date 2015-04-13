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
#include "nsRefPtr.h"
#include "nsTArrayForwardDeclare.h"

class nsIThread;

namespace mozilla {
namespace dom {
namespace cache {

class CacheReadStream;
class CacheReadStreamOrVoid;
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
class ReadStream final : public nsIInputStream
{
public:
  // Interface that lets the StreamControl classes interact with
  // our private inner stream.
  class Controllable
  {
  public:
    // Closes the stream, notifies the stream control, and then forgets
    // the stream control.
    virtual void
    CloseStream() = 0;

    // Closes the stream and then forgets the stream control.  Does not
    // notify.
    virtual void
    CloseStreamWithoutReporting() = 0;

    virtual bool
    MatchId(const nsID& aId) const = 0;

    NS_IMETHOD_(MozExternalRefCountType)
    AddRef(void) = 0;

    NS_IMETHOD_(MozExternalRefCountType)
    Release(void) = 0;
  };

  static already_AddRefed<ReadStream>
  Create(const CacheReadStreamOrVoid& aReadStreamOrVoid);

  static already_AddRefed<ReadStream>
  Create(const CacheReadStream& aReadStream);

  static already_AddRefed<ReadStream>
  Create(PCacheStreamControlParent* aControl, const nsID& aId,
         nsIInputStream* aStream);

  void Serialize(CacheReadStreamOrVoid* aReadStreamOut);
  void Serialize(CacheReadStream* aReadStreamOut);

private:
  class Inner;

  explicit ReadStream(Inner* aInner);
  ~ReadStream();

  // Hold a strong ref to an inner class that actually implements the
  // majority of the stream logic.  Before releasing this ref the outer
  // ReadStream guarantees it will call Close() on the inner stream.
  // This is essential for the inner stream to avoid dealing with the
  // implicit close that can happen when a stream is destroyed.
  nsRefPtr<Inner> mInner;

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
