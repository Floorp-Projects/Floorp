/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIASESSION_MEDIASESSIONIPCUTILS_H_
#define DOM_MEDIA_MEDIASESSION_MEDIASESSIONIPCUTILS_H_

#include "ipc/EnumSerializer.h"
#include "mozilla/dom/BindingIPCUtils.h"
#include "MediaMetadata.h"
#include "mozilla/dom/MediaSession.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace dom {

typedef Maybe<MediaMetadataBase> MaybeMediaMetadataBase;

}  // namespace dom
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::MediaImage> {
  typedef mozilla::dom::MediaImage paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mSizes);
    WriteParam(aWriter, aParam.mSrc);
    WriteParam(aWriter, aParam.mType);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mSizes)) ||
        !ReadParam(aReader, &(aResult->mSrc)) ||
        !ReadParam(aReader, &(aResult->mType))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::MediaMetadataBase> {
  typedef mozilla::dom::MediaMetadataBase paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mTitle);
    WriteParam(aWriter, aParam.mArtist);
    WriteParam(aWriter, aParam.mAlbum);
    WriteParam(aWriter, aParam.mArtwork);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mTitle)) ||
        !ReadParam(aReader, &(aResult->mArtist)) ||
        !ReadParam(aReader, &(aResult->mAlbum)) ||
        !ReadParam(aReader, &(aResult->mArtwork))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::PositionState> {
  typedef mozilla::dom::PositionState paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mDuration);
    WriteParam(aWriter, aParam.mPlaybackRate);
    WriteParam(aWriter, aParam.mLastReportedPlaybackPosition);
    WriteParam(aWriter, aParam.mPositionUpdatedTime);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mDuration)) ||
        !ReadParam(aReader, &(aResult->mPlaybackRate)) ||
        !ReadParam(aReader, &(aResult->mLastReportedPlaybackPosition)) ||
        !ReadParam(aReader, &(aResult->mPositionUpdatedTime))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::MediaSessionPlaybackState>
    : public mozilla::dom::WebIDLEnumSerializer<
          mozilla::dom::MediaSessionPlaybackState> {};

template <>
struct ParamTraits<mozilla::dom::MediaSessionAction>
    : public mozilla::dom::WebIDLEnumSerializer<
          mozilla::dom::MediaSessionAction> {};

}  // namespace IPC

#endif  // DOM_MEDIA_MEDIASESSION_MEDIASESSIONIPCUTILS_H_
