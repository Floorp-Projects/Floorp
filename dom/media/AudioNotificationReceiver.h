/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIONOTIFICATIONRECEIVER_H_
#define MOZILLA_AUDIONOTIFICATIONRECEIVER_H_

/*
 * Architecture to send/receive default device-changed notification:
 *
 *  Chrome Process                       ContentProcess 1
 *  ------------------                   ------------------
 *
 *       AudioNotification               DeviceChangeListener 1   DeviceChangeListener N
 *             |      ^                             |      ^         ^
 *          (4)|      |(2)                          |(3)   |(8)      .
 *             v      |                             v      |         v
 *   AudioNotificationSender                  AudioNotificationReceiver
 *     ^       |      ^                                ^
 *     .    (5)|      |(1)                             |(7)
 *     .       v      |                                |
 *     .  (P)ContentParent 1                   (P)ContentChild 1
 *     .          |                                    ^
 *     .       (6)|                                    |
 *     .          |                                    |
 *     .          |                                    |
 *     .          +------------------------------------+
 *     .                      PContent IPC
 *     .
 *     .                                 Content Process M
 *     .                                 ------------------
 *     .                                          .
 *     v                                          .
 *   (P)ContentParent M  < . . . . . . . . .  > (P)ContentChild M
 *                            PContent IPC
 *
 * Steps
 * --------
 *  1) Initailize the AudioNotificationSender when ContentParent is created.
 *  2) Create an AudioNotification to get the device-changed signal
 *     from the system.
 *  3) Register the DeviceChangeListener to AudioNotificationReceiver when it's created.
 *  4) When the default device is changed, AudioNotification get the signal and
 *  5) Pass this message by AudioNotificationSender.
 *  6) The AudioNotificationSender sends the device-changed notification via
 *     the PContent.
 *  7) The ContentChild will call AudioNotificationReceiver to
 *  8) Notify all the registered audio streams to reconfigure the output devices.
 *
 * Notes
 * --------
 * a) There is only one AudioNotificationSender and AudioNotification
 *    in a chrome process.
 * b) There is only one AudioNotificationReceiver and might be many
 *    DeviceChangeListeners in a content process.
 * c) There might be many ContentParent in a chrome process.
 * d) There is only one ContentChild in a content process.
 * e) All the DeviceChangeListeners are registered in the AudioNotificationReceiver.
 * f) All the ContentParents are registered in the AudioNotificationSender.
 */

namespace mozilla {
namespace audio {

// The base class that provides a ResetDefaultDevice interface that
// will be called in AudioNotificationReceiver::NotifyDefaultDeviceChanged
// when it receives device-changed notification from the chrome process.
class DeviceChangeListener
{
protected:
  virtual ~DeviceChangeListener() {};
public:
  // The subclass shoule provide its own implementation switching the
  // audio stream to the new default output device.
  virtual void ResetDefaultDevice() = 0;
};

class AudioNotificationReceiver final
{
public:
  // Add the DeviceChangeListener into the subscribers list.
  static void Register(DeviceChangeListener* aDeviceChangeListener);

  // Remove the DeviceChangeListener from the subscribers list.
  static void Unregister(DeviceChangeListener* aDeviceChangeListener);

  // Notify all the streams that the default device has been changed.
  static void NotifyDefaultDeviceChanged();
}; // AudioNotificationReceiver

} // namespace audio
} // namespace mozilla

#endif // MOZILLA_AUDIONOTIFICATIONRECEIVER_H_