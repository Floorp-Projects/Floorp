/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineSource.h"

#include "mozilla/dom/MediaTrackSettingsBinding.h"

namespace mozilla {

using dom::MediaSourceEnum;
using dom::MediaTrackSettings;

// These need a definition somewhere because template
// code is allowed to take their address, and they aren't
// guaranteed to have one without this.
const unsigned int MediaEngineSource::kMaxDeviceNameLength;
const unsigned int MediaEngineSource::kMaxUniqueIdLength;

/* static */
bool MediaEngineSource::IsVideo(MediaSourceEnum aSource) {
  switch (aSource) {
    case MediaSourceEnum::Camera:
    case MediaSourceEnum::Screen:
    case MediaSourceEnum::Window:
    case MediaSourceEnum::Browser:
      return true;
    case MediaSourceEnum::Microphone:
    case MediaSourceEnum::AudioCapture:
    case MediaSourceEnum::Other:
      return false;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown type");
      return false;
  }
}

bool MediaEngineSource::IsFake() const { return false; }

bool MediaEngineSource::GetScary() const { return false; }

nsresult MediaEngineSource::FocusOnSelectedSource() {
  return NS_ERROR_NOT_AVAILABLE;
}

void MediaEngineSource::Shutdown() {}

nsresult MediaEngineSource::TakePhoto(MediaEnginePhotoCallback* aCallback) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void MediaEngineSource::GetSettings(MediaTrackSettings& aOutSettings) const {
  MediaTrackSettings empty;
  aOutSettings = empty;
}

MediaEngineSource::~MediaEngineSource() = default;

}  // namespace mozilla
