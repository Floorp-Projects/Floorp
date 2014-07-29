/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OSXRUNLOOPSINGLETON_H_
#define OSXRUNLOOPSINGLETON_H_

#include <mozilla/Types.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* This function tells CoreAudio to use its own thread for device change
 * notifications, and can be called from any thread without external
 * synchronization. */
void MOZ_EXPORT
mozilla_set_coreaudio_notification_runloop_if_needed();

#if defined(__cplusplus)
}
#endif

#endif // OSXRUNLOOPSINGLETON_H_
