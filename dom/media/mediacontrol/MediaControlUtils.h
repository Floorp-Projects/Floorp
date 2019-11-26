/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLUTILS_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLUTILS_H_

#include "mozilla/Logging.h"
#include "MediaController.h"

extern mozilla::LazyLogModule gMediaControlLog;

namespace mozilla {
namespace dom {

inline const char* ToMediaControlActionsStr(MediaControlActions aAction) {
  switch (aAction) {
    case MediaControlActions::ePlay:
      return "Play";
    case MediaControlActions::ePause:
      return "Pause";
    case MediaControlActions::eStop:
      return "Stop";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid action.");
  }
  return "UNKNOWN";
}

void NotifyMediaStarted(uint64_t aWindowID);
void NotifyMediaStopped(uint64_t aWindowID);
void NotifyMediaAudibleChanged(uint64_t aWindowID, bool aAudible);

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_MEDIA_MEDIACONTROL_MEDIACONTROLUTILS_H_
