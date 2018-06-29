/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaCapabilities.h"
#include "DecoderTraits.h"
#include "MediaRecorder.h"
#include "mozilla/Move.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/dom/MediaCapabilitiesBinding.h"
#include "mozilla/dom/MediaSource.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {

already_AddRefed<Promise>
MediaCapabilities::DecodingInfo(
  const MediaDecodingConfiguration& aConfiguration,
  ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(mParent, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // If configuration is not a valid MediaConfiguration, return a Promise
  // rejected with a TypeError.
  if (!aConfiguration.IsAnyMemberPresent() ||
      (!aConfiguration.mVideo.IsAnyMemberPresent() &&
       !aConfiguration.mAudio.IsAnyMemberPresent())) {
    aRv.ThrowTypeError<MSG_MISSING_REQUIRED_DICTIONARY_MEMBER>(
      NS_LITERAL_STRING("'audio' or 'video'"));
    return nullptr;
  }

  bool supported = true;

  // If configuration.video is present and is not a valid video configuration,
  // return a Promise rejected with a TypeError.
  if (aConfiguration.mVideo.IsAnyMemberPresent()) {
    if (!CheckVideoConfiguration(aConfiguration.mVideo)) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_VIDEO_CONFIGURATION>();
      return nullptr;
    }

    // We have a video configuration and it is valid. Check if it is supported.
    supported &=
      aConfiguration.mType == MediaDecodingType::File
        ? CheckTypeForFile(aConfiguration.mVideo.mContentType)
        : CheckTypeForMediaSource(aConfiguration.mVideo.mContentType);
  }
  if (aConfiguration.mAudio.IsAnyMemberPresent()) {
    if (!CheckAudioConfiguration(aConfiguration.mAudio)) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_AUDIO_CONFIGURATION>();
      return nullptr;
    }
    // We have an audio configuration and it is valid. Check if it is supported.
    supported &=
      aConfiguration.mType == MediaDecodingType::File
        ? CheckTypeForFile(aConfiguration.mAudio.mContentType)
        : CheckTypeForMediaSource(aConfiguration.mAudio.mContentType);
  }

  auto info =
    MakeUnique<MediaCapabilitiesInfo>(supported, supported, supported);
  promise->MaybeResolve(std::move(info));

  return promise.forget();
}

already_AddRefed<Promise>
MediaCapabilities::EncodingInfo(
  const MediaEncodingConfiguration& aConfiguration,
  ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(mParent, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // If configuration is not a valid MediaConfiguration, return a Promise
  // rejected with a TypeError.
  if (!aConfiguration.IsAnyMemberPresent() ||
      (!aConfiguration.mVideo.IsAnyMemberPresent() &&
       !aConfiguration.mAudio.IsAnyMemberPresent())) {
    aRv.ThrowTypeError<MSG_MISSING_REQUIRED_DICTIONARY_MEMBER>(
      NS_LITERAL_STRING("'audio' or 'video'"));
    return nullptr;
  }

  bool supported = true;

  // If configuration.video is present and is not a valid video configuration,
  // return a Promise rejected with a TypeError.
  if (aConfiguration.mVideo.IsAnyMemberPresent()) {
    if (!CheckVideoConfiguration(aConfiguration.mVideo)) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_VIDEO_CONFIGURATION>();
      return nullptr;
    }
    // We have a video configuration and it is valid. Check if it is supported.
    supported &= CheckTypeForEncoder(aConfiguration.mVideo.mContentType);
  }
  if (aConfiguration.mAudio.IsAnyMemberPresent()) {
    if (!CheckAudioConfiguration(aConfiguration.mAudio)) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_AUDIO_CONFIGURATION>();
      return nullptr;
    }
    // We have an audio configuration and it is valid. Check if it is supported.
    supported &= CheckTypeForEncoder(aConfiguration.mAudio.mContentType);
  }

  auto info = MakeUnique<MediaCapabilitiesInfo>(supported, supported, false);
  promise->MaybeResolve(std::move(info));

  return promise.forget();
}

bool
MediaCapabilities::CheckContentType(const nsAString& aMIMEType,
                                    Maybe<MediaContainerType>& aContainer) const
{
  if (aMIMEType.IsEmpty()) {
    return false;
  }

  aContainer = MakeMediaContainerType(aMIMEType);
  return aContainer.isSome();
}

bool
MediaCapabilities::CheckVideoConfiguration(
  const VideoConfiguration& aConfig) const
{
  Maybe<MediaContainerType> container;
  // A valid video MIME type is a string that is a valid media MIME type and for
  // which the type per [RFC7231] is either video or application.
  if (!CheckContentType(aConfig.mContentType, container)) {
    return false;
  }
  if (!container->Type().HasVideoMajorType() &&
      !container->Type().HasApplicationMajorType()) {
    return false;
  }

  return true;
}

bool
MediaCapabilities::CheckAudioConfiguration(
  const AudioConfiguration& aConfig) const
{
  Maybe<MediaContainerType> container;
  // A valid audio MIME type is a string that is valid media MIME type and for
  // which the type per [RFC7231] is either audio or application.
  if (!CheckContentType(aConfig.mContentType, container)) {
    return false;
  }
  if (!container->Type().HasAudioMajorType() &&
      !container->Type().HasApplicationMajorType()) {
    return false;
  }

  return true;
}

bool
MediaCapabilities::CheckTypeForMediaSource(const nsAString& aType)
{
  return NS_SUCCEEDED(MediaSource::IsTypeSupported(
    aType, nullptr /* DecoderDoctorDiagnostics */));
}

bool
MediaCapabilities::CheckTypeForFile(const nsAString& aType)
{
  Maybe<MediaContainerType> containerType = MakeMediaContainerType(aType);
  if (!containerType) {
    return false;
  }

  return DecoderTraits::CanHandleContainerType(
           *containerType, nullptr /* DecoderDoctorDiagnostics */) !=
         CANPLAY_NO;
}

bool
MediaCapabilities::CheckTypeForEncoder(const nsAString& aType)
{
  return MediaRecorder::IsTypeSupported(aType);
}

bool
MediaCapabilities::Enabled(JSContext* aCx, JSObject* aGlobal)
{
  return StaticPrefs::MediaCapabilitiesEnabled();
}

JSObject*
MediaCapabilities::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaCapabilities_Binding::Wrap(aCx, this, aGivenProto);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaCapabilities)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaCapabilities)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaCapabilities)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaCapabilities, mParent)

// MediaCapabilitiesInfo
bool
MediaCapabilitiesInfo::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto,
                                  JS::MutableHandle<JSObject*> aReflector)
{
  return MediaCapabilitiesInfo_Binding::Wrap(
    aCx, this, aGivenProto, aReflector);
}

} // namespace dom
} // namespace mozilla
