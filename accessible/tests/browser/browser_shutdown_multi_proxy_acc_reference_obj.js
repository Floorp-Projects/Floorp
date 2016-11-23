/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

add_task(function* () {
  // Making sure that the e10s is enabled on Windows for testing.
  yield setE10sPrefs();

  let docLoaded = waitForEvent(
    Ci.nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_COMPLETE, 'body');
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
        <body id="body"><div id="div"></div></body>
      </html>`
  }, function*(browser) {
    let docLoadedEvent = yield docLoaded;
    let docAcc = docLoadedEvent.accessibleDocument;
    ok(docAcc, 'Accessible document proxy is created');
    // Remove unnecessary dangling references
    docLoaded = null;
    docLoadedEvent = null;
    forceGC();

    let acc = docAcc.getChildAt(0);
    ok(acc, 'Accessible proxy is created');

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

    // Remove a reference to an accessible document proxy.
    docAcc = null;
    ok(!docAcc, 'Accessible document proxy is removed');
    // Force garbage collection that should not trigger shutdown because there is
    // a reference to an accessible proxy.
    forceGC();
    // Have some breathing room when removing a11y service references.
    yield new Promise(resolve => executeSoon(resolve));

    // Now allow a11y service to shutdown.
    canShutdown = true;
    // Remove a last reference to an accessible proxy.
    acc = null;
    ok(!acc, 'Accessible proxy is removed');

    // Force garbage collection that should now trigger shutdown.
    forceGC();
    yield a11yShutdown;
  });

  // Unsetting e10s related preferences.
  yield unsetE10sPrefs();
});
