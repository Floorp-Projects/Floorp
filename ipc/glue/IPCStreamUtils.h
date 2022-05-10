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

namespace mozilla::ipc {

// Serialize an IPCStream to be sent over IPC fallibly.
//
// If |aAllowLazy| is true the stream may be serialized as a
// RemoteLazyInputStream when being sent from child to parent.
//
// ParamTraits<nsIInputStream> may be used instead if serialization cannot be
// fallible.
[[nodiscard]] bool SerializeIPCStream(
    already_AddRefed<nsIInputStream> aInputStream, IPCStream& aValue,
    bool aAllowLazy);

// If serialization fails, `aValue` will be initialized to `Nothing()`, so this
// return value is safe to ignore.
bool SerializeIPCStream(already_AddRefed<nsIInputStream> aInputStream,
                        Maybe<IPCStream>& aValue, bool aAllowLazy);

// Deserialize an IPCStream received from an actor call.  These methods
// work in both the child and parent.
already_AddRefed<nsIInputStream> DeserializeIPCStream(const IPCStream& aValue);

already_AddRefed<nsIInputStream> DeserializeIPCStream(
    const Maybe<IPCStream>& aValue);

}  // namespace mozilla::ipc

namespace IPC {

template <>
struct ParamTraits<nsIInputStream*> {
  static void Write(MessageWriter* aWriter, nsIInputStream* aParam);
  static bool Read(MessageReader* aReader, RefPtr<nsIInputStream>* aResult);
};

}  // namespace IPC

#endif  // mozilla_ipc_IPCStreamUtils_h
