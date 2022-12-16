/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RandomAccessStreamUtils.h"

#include "mozilla/NotNull.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/ipc/RandomAccessStreamParams.h"
#include "nsFileStreams.h"
#include "nsIInterfaceRequestor.h"
#include "nsIRandomAccessStream.h"

namespace mozilla::ipc {

RandomAccessStreamParams SerializeRandomAccessStream(
    MovingNotNull<nsCOMPtr<nsIRandomAccessStream>> aStream,
    nsIInterfaceRequestor* aCallbacks) {
  NotNull<nsCOMPtr<nsIRandomAccessStream>> stream = std::move(aStream);

  RandomAccessStreamParams streamParams = stream->Serialize(aCallbacks);

  MOZ_ASSERT(streamParams.type() != RandomAccessStreamParams::T__None);

  return streamParams;
}

Maybe<RandomAccessStreamParams> SerializeRandomAccessStream(
    nsCOMPtr<nsIRandomAccessStream> aStream,
    nsIInterfaceRequestor* aCallbacks) {
  if (!aStream) {
    return Nothing();
  }

  return Some(SerializeRandomAccessStream(
      WrapMovingNotNullUnchecked(std::move(aStream)), aCallbacks));
}

Result<MovingNotNull<nsCOMPtr<nsIRandomAccessStream>>, bool>
DeserializeRandomAccessStream(RandomAccessStreamParams& aStreamParams) {
  nsCOMPtr<nsIRandomAccessStream> stream;

  switch (aStreamParams.type()) {
    case RandomAccessStreamParams::TFileRandomAccessStreamParams:
      nsFileRandomAccessStream::Create(NS_GET_IID(nsIFileRandomAccessStream),
                                       getter_AddRefs(stream));
      break;

    case RandomAccessStreamParams::TLimitingFileRandomAccessStreamParams:
      stream = new dom::quota::FileRandomAccessStream();
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unknown params!");
      return Err(false);
  }

  MOZ_ASSERT(stream);

  if (!stream->Deserialize(aStreamParams)) {
    MOZ_ASSERT_UNREACHABLE("Deserialize failed!");
    return Err(false);
  }

  return WrapMovingNotNullUnchecked(std::move(stream));
}

Result<nsCOMPtr<nsIRandomAccessStream>, bool> DeserializeRandomAccessStream(
    Maybe<RandomAccessStreamParams>& aStreamParams) {
  if (aStreamParams.isNothing()) {
    return nsCOMPtr<nsIRandomAccessStream>();
  }

  auto res = DeserializeRandomAccessStream(aStreamParams.ref());
  if (res.isErr()) {
    return res.propagateErr();
  }

  MovingNotNull<nsCOMPtr<nsIRandomAccessStream>> stream = res.unwrap();

  return std::move(stream).unwrapBasePtr();
}

}  // namespace mozilla::ipc
