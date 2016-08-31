/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

add_task(function* () {
  // Create a11y service inside of the function scope. Its reference should be
  // released once the anonimous function is called.
  let a11yInitThenShutdown = initPromise().then(shutdownPromise);

  (function() {
    let accService = Cc['@mozilla.org/accessibilityService;1'].getService(
      Ci.nsIAccessibilityService);
    ok(accService, 'Service initialized');
  })();

  // Force garbage collection that should trigger shutdown.
  forceGC();
  yield a11yInitThenShutdown;
});
