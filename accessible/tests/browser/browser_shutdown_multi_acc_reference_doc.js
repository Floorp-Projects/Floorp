/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

add_task(function* () {
  // Create a11y service.
  let a11yInit = initPromise();
  let accService = Cc['@mozilla.org/accessibilityService;1'].getService(
    Ci.nsIAccessibilityService);

  yield a11yInit;
  ok(accService, 'Service initialized');

  let docAcc = accService.getAccessibleFor(document);
  ok(docAcc, 'Accessible document is created');

  // Accessible object reference will live longer than the scope of this
  // function.
  let acc = yield new Promise(resolve => {
    let intervalId = setInterval(() => {
      let tabAcc = accService.getAccessibleFor(gBrowser.mCurrentTab);
      if (tabAcc) {
        clearInterval(intervalId);
        resolve(tabAcc);
      }
    }, 10);
  });
  ok(acc, 'Accessible object is created');

  let canShutdown = false;
  // This promise will resolve only if canShutdown flag is set to true. If
  // 'a11y-init-or-shutdown' event with '0' flag comes before it can be shut
  // down, the promise will reject.
  let a11yShutdown = new Promise((resolve, reject) =>
    shutdownPromise().then(flag => canShutdown ? resolve() :
      reject('Accessible service was shut down incorrectly')));

  accService = null;
  ok(!accService, 'Service is removed');

  // Force garbage collection that should not trigger shutdown because there are
  // references to accessible objects.
  forceGC();
  // Have some breathing room when removing a11y service references.
  yield new Promise(resolve => executeSoon(resolve));

  // Remove a reference to an accessible object.
  acc = null;
  ok(!acc, 'Accessible object is removed');
  // Force garbage collection that should not trigger shutdown because there is
  // a reference to an accessible document.
  forceGC();
  // Have some breathing room when removing a11y service references.
  yield new Promise(resolve => executeSoon(resolve));

  // Now allow a11y service to shutdown.
  canShutdown = true;
  // Remove a reference to an accessible document.
  docAcc = null;
  ok(!docAcc, 'Accessible document is removed');

  // Force garbage collection that should now trigger shutdown.
  forceGC();
  yield a11yShutdown;
});
