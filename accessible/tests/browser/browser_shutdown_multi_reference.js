/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  info("Creating a service");
  // Create a11y service.
  const [a11yInitObserver, a11yInit] = initAccService();
  await a11yInitObserver;

  let accService1 = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService
  );
  await a11yInit;
  ok(accService1, "Service initialized");

  // Add another reference to a11y service. This will not trigger
  // 'a11y-init-or-shutdown' event
  let accService2 = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService
  );
  ok(accService2, "Service initialized");

  info("Removing all service references");
  let canShutdown = false;
  // This promise will resolve only if canShutdown flag is set to true. If
  // 'a11y-init-or-shutdown' event with '0' flag comes before it can be shut
  // down, the promise will reject.
  const [a11yShutdownObserver, a11yShutdownPromise] = shutdownAccService();
  await a11yShutdownObserver;
  const a11yShutdown = new Promise((resolve, reject) =>
    a11yShutdownPromise.then(flag =>
      canShutdown
        ? resolve()
        : reject("Accessible service was shut down incorrectly")
    )
  );
  // Remove first a11y service reference.
  accService1 = null;
  ok(!accService1, "Service is removed");
  // Force garbage collection that should not trigger shutdown because there is
  // another reference.
  forceGC();

  // Have some breathing room when removing a11y service references.
  await TestUtils.waitForTick();

  // Now allow a11y service to shutdown.
  canShutdown = true;
  // Remove last a11y service reference.
  accService2 = null;
  ok(!accService2, "Service is removed");
  // Force garbage collection that should trigger shutdown.
  forceGC();
  await a11yShutdown;
});
