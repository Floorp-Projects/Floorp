/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  info("Creating a service");
  // Create a11y service.
  let [a11yInitObserver, a11yInit] = initAccService();
  await a11yInitObserver;

  let accService = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService
  );
  await a11yInit;
  ok(accService, "Service initialized");

  info("Removing a service");
  // Remove the only reference to an a11y service.
  let [a11yShutdownObserver, a11yShutdown] = shutdownAccService();
  await a11yShutdownObserver;

  accService = null;
  ok(!accService, "Service is removed");
  // Force garbage collection that should trigger shutdown.
  forceGC();
  await a11yShutdown;

  info("Recreating a service");
  // Re-create a11y service.
  [a11yInitObserver, a11yInit] = initAccService();
  await a11yInitObserver;

  accService = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService
  );
  await a11yInit;
  ok(accService, "Service initialized again");

  info("Removing a service again");
  // Remove the only reference to an a11y service again.
  [a11yShutdownObserver, a11yShutdown] = shutdownAccService();
  await a11yShutdownObserver;

  accService = null;
  ok(!accService, "Service is removed again");
  // Force garbage collection that should trigger shutdown.
  forceGC();
  await a11yShutdown;
});
