/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ShutdownTracker is an imagelib-global service that allows callers to check
 * whether shutdown has started.
 */

#ifndef mozilla_image_ShutdownTracker_h
#define mozilla_image_ShutdownTracker_h

namespace mozilla {
namespace image {

/**
 * ShutdownTracker is an imagelib-global service that allows callers to check
 * whether shutdown has started. It exists to avoid the need for registering
 * many 'xpcom-shutdown' notification observers on short-lived objects, which
 * would have an unnecessary performance cost.
 */
struct ShutdownTracker
{
  /**
   * Initialize static data. Called during imagelib module initialization.
   */
  static void Initialize();

  /**
   * Check whether shutdown has started. Callers can use this to check whether
   * it's safe to access XPCOM services; if shutdown has started, such calls
   * must be avoided.
   *
   * @return true if shutdown has already started.
   */
  static bool ShutdownHasStarted();

private:
  virtual ~ShutdownTracker() = 0;  // Forbid instantiation.
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_ShutdownTracker_h
