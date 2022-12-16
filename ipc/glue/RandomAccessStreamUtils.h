/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_RandomAccessStreamUtils_h
#define mozilla_ipc_RandomAccessStreamUtils_h

template <class T>
class nsCOMPtr;

class nsIInterfaceRequestor;
class nsIRandomAccessStream;

namespace mozilla {

template <class T>
class Maybe;

template <typename T>
class MovingNotNull;

template <typename V, typename E>
class Result;

namespace ipc {

class RandomAccessStreamParams;

// Serialize an nsIRandomAccessStream to be sent over IPC infallibly.
RandomAccessStreamParams SerializeRandomAccessStream(
    MovingNotNull<nsCOMPtr<nsIRandomAccessStream>> aStream,
    nsIInterfaceRequestor* aCallbacks);

Maybe<RandomAccessStreamParams> SerializeRandomAccessStream(
    nsCOMPtr<nsIRandomAccessStream> aStream, nsIInterfaceRequestor* aCallbacks);

// Deserialize an nsIRandomAccessStream received from an actor call.  These
// methods work in both the child and parent.
Result<MovingNotNull<nsCOMPtr<nsIRandomAccessStream>>, bool>
DeserializeRandomAccessStream(RandomAccessStreamParams& aStreamParams);

Result<nsCOMPtr<nsIRandomAccessStream>, bool> DeserializeRandomAccessStream(
    Maybe<RandomAccessStreamParams>& aStreamParams);

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_RandomAccessStreamUtils_h
