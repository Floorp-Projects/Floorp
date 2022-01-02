/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IPCStreamUtils_h
#define mozilla_ipc_IPCStreamUtils_h

#include "mozilla/ipc/IPCStream.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"

namespace mozilla {

namespace dom {
class ContentChild;
class ContentParent;
}  // namespace dom

namespace net {
class SocketProcessParent;
class SocketProcessChild;
}  // namespace net

namespace ipc {

class PBackgroundChild;
class PBackgroundParent;

// Deserialize an IPCStream received from an actor call.  These methods
// work in both the child and parent.
already_AddRefed<nsIInputStream> DeserializeIPCStream(const IPCStream& aValue);

already_AddRefed<nsIInputStream> DeserializeIPCStream(
    const Maybe<IPCStream>& aValue);

// RAII helper class that serializes an nsIInputStream into an IPCStream struct.
// Any file descriptor or PChildToParentStream actors are automatically managed
// correctly.
//
// Here is a simple example:
//
//  // in ipdl file
//  Protocol PMyStuff
//  {
//  parent:
//    async DoStuff(IPCStream aStream);
//  child:
//    async StuffDone(IPCStream aStream);
//  };
//
//  // in child c++ code
//  void CallDoStuff(PMyStuffChild* aActor, nsIInputStream* aStream)
//  {
//    AutoIPCStream autoStream;
//    autoStream.Serialize(aStream, aActor->Manager());
//    aActor->SendDoStuff(autoStream.TakeValue());
//  }
//
//  // in parent c++ code
//  bool
//  MyStuffParent::RecvDoStuff(const IPCStream& aIPCStream) {
//    nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(aIPCStream);
//    // Do something with stream...
//
//    // You can also serialize streams from parent-to-child as long as
//    // they don't require PChildToParentStream actor support.
//    AutoIPCStream anotherStream;
//    anotherStream.Serialize(mFileStream, Manager());
//    SendStuffDone(anotherStream.TakeValue());
//  }
//
// The AutoIPCStream RAII class may also be used if your stream is embedded
// in a more complex IPDL structure.  In this case you attach the AutoIPCStream
// to the embedded IPCStream and call TakeValue() after you pass the structure.
// For example:
//
//  // in ipdl file
//  struct Stuff
//  {
//    IPCStream stream;
//    nsCString name;
//  };
//
//  Protocol PMyStuff
//  {
//  parent:
//    async DoStuff(Stuff aStream);
//  };
//
//  // in child c++ code
//  void CallDoStuff(PMyStuffChild* aActor, nsIInputStream* aStream)
//  {
//    Stuff stuff;
//    AutoIPCStream autoStream(stuff.stream()); // attach to IPCStream here
//    autoStream.Serialize(aStream, aActor->Manager());
//    aActor->SendDoStuff(stuff);
//    autoStream.TakeValue();                   // call take value after send
//  }
//
//  // in parent c++ code
//  bool
//  MyStuffParent::RecvDoStuff(const Stuff& aStuff) {
//    nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(aStuff.stream());
//    /* do something with the nsIInputStream */
//  }
//
// Note: This example is about child-to-parent inputStream, but AutoIPCStream
// works also parent-to-child.
//
// The AutoIPCStream class also supports Maybe<IPCStream> values.  As long as
// you did not initialize the object with a non-optional IPCStream, you can call
// TakeOptionalValue() instead.
//
// The AutoIPCStream class can also be used to serialize nsIInputStream objects
// on the parent side to send to the child.  Currently, however, this only
// works for directly serializable stream types.  The PChildToParentStream actor
// mechanism is not supported in this direction yet.
//
// Like SerializeInputStream(), the AutoIPCStream will crash if
// serialization cannot be completed.
//
// NOTE: This is not a MOZ_STACK_CLASS so that it can be more easily integrated
//       with complex ipdl structures.  For example, you may want to create an
//       array of RAII AutoIPCStream objects or build your own wrapping
//       RAII object to handle other actors that need to be cleaned up.
class AutoIPCStream {
 public:
  // Implicitly create an Maybe<IPCStream> value.  Either
  // TakeValue() or TakeOptionalValue() can be used.
  explicit AutoIPCStream(bool aDelayedStart = false);

  // Wrap an existing IPCStream.  Only TakeValue() may be
  // used.  If a nullptr nsIInputStream is passed to SerializeOrSend() then
  // a crash will be forced.
  explicit AutoIPCStream(IPCStream& aTarget, bool aDelayedStart = false);

  // Wrap an existing Maybe<IPCStream>.  Either TakeValue()
  // or TakeOptionalValue can be used.
  explicit AutoIPCStream(Maybe<IPCStream>& aTarget, bool aDelayedStart = false);

  AutoIPCStream(const AutoIPCStream&) = delete;
  AutoIPCStream(AutoIPCStream&&) = delete;

  AutoIPCStream& operator=(const AutoIPCStream&) = delete;
  AutoIPCStream& operator=(AutoIPCStream&&) = delete;

  ~AutoIPCStream();

  // Serialize the input stream or create a SendStream actor using the PContent
  // manager.  If neither of these succeed, then crash.  This should only be
  // used on the main thread.
  bool Serialize(nsIInputStream* aStream, dom::ContentChild* aManager);

  // Serialize the input stream or create a SendStream actor using the
  // PBackground manager.  If neither of these succeed, then crash.  This can
  // be called on the main thread or Worker threads.
  bool Serialize(nsIInputStream* aStream, PBackgroundChild* aManager);

  // Serialize the input stream or create a SendStream actor using the
  // SocketProcess manager.  If neither of these succeed, then crash.  This
  // should only be used on the main thread.
  bool Serialize(nsIInputStream* aStream, net::SocketProcessChild* aManager);

  // Serialize the input stream.
  [[nodiscard]] bool Serialize(nsIInputStream* aStream,
                               dom::ContentParent* aManager);

  // Serialize the input stream.
  [[nodiscard]] bool Serialize(nsIInputStream* aStream,
                               PBackgroundParent* aManager);

  // Serialize the input stream.
  [[nodiscard]] bool Serialize(nsIInputStream* aStream,
                               net::SocketProcessParent* aManager);

  // Get the IPCStream as a non-optional value.  This will
  // assert if a stream has not been serialized or if it has already been taken.
  // This should only be called if the value is being, or has already been, sent
  // to the other side.
  IPCStream& TakeValue();

  // Get the Maybe<IPCStream> value. This will assert if the value has already
  // been taken. This should only be called if the value is being, or has
  // already been, sent to the other side.
  Maybe<IPCStream>& TakeOptionalValue();

 private:
  bool IsSet() const;

  Maybe<IPCStream> mInlineValue;
  IPCStream* const mValue = nullptr;
  Maybe<IPCStream>* const mOptionalValue = nullptr;
  bool mTaken = false;
  const bool mDelayedStart;
};

class HoldIPCStream final : public AutoIPCStream {
 public:
  NS_INLINE_DECL_REFCOUNTING(HoldIPCStream)

  using AutoIPCStream::AutoIPCStream;

 private:
  ~HoldIPCStream() = default;
};

template <>
struct IPDLParamTraits<nsIInputStream*> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    nsIInputStream* aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, RefPtr<nsIInputStream>* aResult);
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_IPCStreamUtils_h
