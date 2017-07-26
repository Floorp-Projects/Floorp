/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIONOTIFICATIONSENDER_H_
#define MOZILLA_AUDIONOTIFICATIONSENDER_H_

#include "nsError.h" // for nsresult

namespace mozilla {
namespace audio {

// Please see the architecture figure in AudioNotificationReceiver.h.
class AudioNotificationSender final
{
public:
  // Register the AudioNotification to get the device-changed event.
  static nsresult Init();

private:
  // Send the device-changed notification from the chrome processes
  // to the content processes.
  static void NotifyDefaultDeviceChanged();
}; // AudioNotificationSender

} // namespace audio
} // namespace mozilla

#endif // MOZILLA_AUDIONOTIFICATIONSENDER_H_
