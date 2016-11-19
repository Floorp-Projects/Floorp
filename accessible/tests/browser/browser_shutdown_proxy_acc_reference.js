/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

add_task(function* () {
  // Making sure that the e10s is enabled on Windows for testing.
  yield setE10sPrefs();

  let a11yInit = initPromise();
  let accService = Cc['@mozilla.org/accessibilityService;1'].getService(
    Ci.nsIAccessibilityService);
  ok(accService, 'Service initialized');
  yield a11yInit;

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: `data:text/html,
      <html>
        <head>
          <meta charset="utf-8"/>
          <title>Accessibility Test</title>
        </head>
        <body><div id="div" style="visibility: hidden;"></div></body>
      </html>`
  }, function*(browser) {
    let onShow = waitForEvent(Ci.nsIAccessibleEvent.EVENT_SHOW, 'div');
    yield invokeSetStyle(browser, 'div', 'visibility', 'visible');
    let showEvent = yield onShow;
    let divAcc = showEvent.accessible;
    ok(divAcc, 'Accessible proxy is created');
    // Remove unnecessary dangling references
    onShow = null;
    showEvent = null;
    forceGC();

    let canShutdown = false;
    let a11yShutdown = new Promise((resolve, reject) =>
    shutdownPromise().then(flag => canShutdown ? resolve() :
      reject('Accessible service was shut down incorrectly')));

    accService = null;
    ok(!accService, 'Service is removed');
    // Force garbage collection that should not trigger shutdown because there
    // is a reference to an accessible proxy.
    forceGC();
    // Have some breathing room when removing a11y service references.
    yield new Promise(resolve => executeSoon(resolve));

    // Now allow a11y service to shutdown.
    canShutdown = true;
    // Remove a last reference to an accessible proxy.
    divAcc = null;
    ok(!divAcc, 'Accessible proxy is removed');

    // Force garbage collection that should now trigger shutdown.
    forceGC();
    yield a11yShutdown;
  });

  // Unsetting e10s related preferences.
  yield unsetE10sPrefs();
});
