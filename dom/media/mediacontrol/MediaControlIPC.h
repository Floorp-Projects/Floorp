/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_MediaControlIPC_h
#define ipc_MediaControlIPC_h

#include "ipc/EnumSerializer.h"

#include "mozilla/dom/MediaControllerBinding.h"
#include "mozilla/dom/MediaControlKeySource.h"
#include "mozilla/dom/MediaPlaybackStatus.h"

namespace IPC {
template <>
struct ParamTraits<mozilla::dom::MediaControlKey>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::MediaControlKey, mozilla::dom::MediaControlKey::Focus,
          mozilla::dom::MediaControlKey::Stop> {};

template <>
struct ParamTraits<mozilla::dom::MediaPlaybackState>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::MediaPlaybackState,
          mozilla::dom::MediaPlaybackState::eStarted,
          mozilla::dom::MediaPlaybackState::eStopped> {};

template <>
struct ParamTraits<mozilla::dom::MediaAudibleState>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::MediaAudibleState,
          mozilla::dom::MediaAudibleState::eInaudible,
          mozilla::dom::MediaAudibleState::eAudible> {};

template <>
struct ParamTraits<mozilla::dom::SeekDetails> {
  typedef mozilla::dom::SeekDetails paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mSeekTime);
    WriteParam(aWriter, aParam.mFastSeek);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mSeekTime) ||
        !ReadParam(aReader, &aResult->mFastSeek)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::MediaControlAction> {
  typedef mozilla::dom::MediaControlAction paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mKey);
    WriteParam(aWriter, aParam.mDetails);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mKey) ||
        !ReadParam(aReader, &aResult->mDetails)) {
      return false;
    }
    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_MediaControlIPC_hh
