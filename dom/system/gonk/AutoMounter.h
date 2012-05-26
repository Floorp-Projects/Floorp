/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_automounter_h__
#define mozilla_system_automounter_h__

#include "mozilla/StandardInteger.h"

namespace mozilla {
namespace system {

// AutoMounter modes
#define AUTOMOUNTER_DISABLE                 0
#define AUTOMOUNTER_ENABLE                  1
#define AUTOMOUNTER_DISABLE_WHEN_UNPLUGGED  2

/**
 * Initialize the automounter. This causes some of the phone's
 * directories to show up on the host when the phone is plugged
 * into the host via USB.
 *
 * When the AutoMounter starts, it will poll the current state
 * of affairs (usb cable plugged in, automounter enabled, etc)
 * and try to make the state of the volumes match.
 */
void InitAutoMounter();

/**
 * Sets the enabled state of the automounter.
 *
 * This will in turn cause the automounter to re-evaluate
 * whether it should mount/unmount/share/unshare volumes.
 */
void SetAutoMounterMode(int32_t aMode);

/**
 * Shuts down the automounter.
 *
 * This leaves the volumes in whatever state they're in.
 */
void ShutdownAutoMounter();

} // system
} // mozilla

#endif  // mozilla_system_automounter_h__
