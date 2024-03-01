/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLUTILS_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLUTILS_H_

#include "imgIEncoder.h"
#include "imgITools.h"
#include "MediaController.h"
#include "mozilla/dom/ChromeUtilsBinding.h"
#include "mozilla/dom/MediaControllerBinding.h"
#include "mozilla/Logging.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"

extern mozilla::LazyLogModule gMediaControlLog;

namespace mozilla::dom {

inline const char* ToMediaControlKeyStr(MediaControlKey aKey) {
  switch (aKey) {
    case MediaControlKey::Focus:
      return "Focus";
    case MediaControlKey::Pause:
      return "Pause";
    case MediaControlKey::Play:
      return "Play";
    case MediaControlKey::Playpause:
      return "Play & pause";
    case MediaControlKey::Previoustrack:
      return "Previous track";
    case MediaControlKey::Nexttrack:
      return "Next track";
    case MediaControlKey::Seekbackward:
      return "Seek backward";
    case MediaControlKey::Seekforward:
      return "Seek forward";
    case MediaControlKey::Skipad:
      return "Skip Ad";
    case MediaControlKey::Seekto:
      return "Seek to";
    case MediaControlKey::Stop:
      return "Stop";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid action.");
      return "Unknown";
  }
}

inline const char* ToMediaSessionActionStr(MediaSessionAction aAction) {
  switch (aAction) {
    case MediaSessionAction::Play:
      return "play";
    case MediaSessionAction::Pause:
      return "pause";
    case MediaSessionAction::Seekbackward:
      return "seek backward";
    case MediaSessionAction::Seekforward:
      return "seek forward";
    case MediaSessionAction::Previoustrack:
      return "previous track";
    case MediaSessionAction::Nexttrack:
      return "next track";
    case MediaSessionAction::Skipad:
      return "skip ad";
    case MediaSessionAction::Seekto:
      return "Seek to";
    default:
      MOZ_ASSERT(aAction == MediaSessionAction::Stop);
      return "stop";
  }
}

inline MediaControlKey ConvertMediaSessionActionToControlKey(
    MediaSessionAction aAction) {
  switch (aAction) {
    case MediaSessionAction::Play:
      return MediaControlKey::Play;
    case MediaSessionAction::Pause:
      return MediaControlKey::Pause;
    case MediaSessionAction::Seekbackward:
      return MediaControlKey::Seekbackward;
    case MediaSessionAction::Seekforward:
      return MediaControlKey::Seekforward;
    case MediaSessionAction::Previoustrack:
      return MediaControlKey::Previoustrack;
    case MediaSessionAction::Nexttrack:
      return MediaControlKey::Nexttrack;
    case MediaSessionAction::Skipad:
      return MediaControlKey::Skipad;
    case MediaSessionAction::Seekto:
      return MediaControlKey::Seekto;
    default:
      MOZ_ASSERT(aAction == MediaSessionAction::Stop);
      return MediaControlKey::Stop;
  }
}

inline MediaSessionAction ConvertToMediaSessionAction(uint8_t aActionValue) {
  MOZ_DIAGNOSTIC_ASSERT(aActionValue < uint8_t(MediaSessionAction::EndGuard_));
  return static_cast<MediaSessionAction>(aActionValue);
}

inline const char* ToMediaPlaybackStateStr(MediaPlaybackState aState) {
  switch (aState) {
    case MediaPlaybackState::eStarted:
      return "started";
    case MediaPlaybackState::ePlayed:
      return "played";
    case MediaPlaybackState::ePaused:
      return "paused";
    case MediaPlaybackState::eStopped:
      return "stopped";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid media state.");
      return "Unknown";
  }
}

inline const char* ToMediaAudibleStateStr(MediaAudibleState aState) {
  switch (aState) {
    case MediaAudibleState::eInaudible:
      return "inaudible";
    case MediaAudibleState::eAudible:
      return "audible";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid audible state.");
      return "Unknown";
  }
}

inline const char* ToMediaSessionPlaybackStateStr(
    const MediaSessionPlaybackState& aState) {
  switch (aState) {
    case MediaSessionPlaybackState::None:
      return "none";
    case MediaSessionPlaybackState::Paused:
      return "paused";
    case MediaSessionPlaybackState::Playing:
      return "playing";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid MediaSessionPlaybackState.");
      return "Unknown";
  }
}

BrowsingContext* GetAliveTopBrowsingContext(BrowsingContext* aBC);

inline bool IsImageIn(const nsTArray<MediaImage>& aArtwork,
                      const nsAString& aImageUrl) {
  for (const MediaImage& image : aArtwork) {
    if (image.mSrc == aImageUrl) {
      return true;
    }
  }
  return false;
}

// The image buffer would be allocated in aStream whose size is aSize and the
// buffer head is aBuffer
inline nsresult GetEncodedImageBuffer(imgIContainer* aImage,
                                      const nsACString& aMimeType,
                                      nsIInputStream** aStream, uint32_t* aSize,
                                      char** aBuffer) {
  MOZ_ASSERT(aImage);

  nsCOMPtr<imgITools> imgTools = do_GetService("@mozilla.org/image/tools;1");
  if (!imgTools) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  nsresult rv = imgTools->EncodeImage(aImage, aMimeType, u""_ns,
                                      getter_AddRefs(inputStream));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!inputStream) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<imgIEncoder> encoder = do_QueryInterface(inputStream);
  if (!encoder) {
    return NS_ERROR_FAILURE;
  }

  rv = encoder->GetImageBufferUsed(aSize);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = encoder->GetImageBuffer(aBuffer);
  if (NS_FAILED(rv)) {
    return rv;
  }

  encoder.forget(aStream);
  return NS_OK;
}

inline bool IsValidImageUrl(const nsAString& aUrl) {
  return StringBeginsWith(aUrl, u"http://"_ns) ||
         StringBeginsWith(aUrl, u"https://"_ns);
}

inline uint32_t GetMediaKeyMask(mozilla::dom::MediaControlKey aKey) {
  return 1 << static_cast<uint8_t>(aKey);
}

}  // namespace mozilla::dom

#endif  // DOM_MEDIA_MEDIACONTROL_MEDIACONTROLUTILS_H_
