/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  let docLoaded = waitForEvent(
    Ci.nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_COMPLETE,
    "body"
  );
  const [a11yInitObserver, a11yInit] = initAccService();
  await a11yInitObserver;

  let accService = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService
  );
  ok(accService, "Service initialized");
  await a11yInit;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `data:text/html,
      <html>
        <head>
          <meta charset="utf-8"/>
          <title>Accessibility Test</title>
        </head>
        <body id="body"></body>
      </html>`,
    },
    async function (browser) {
      let docLoadedEvent = await docLoaded;
      let docAcc = docLoadedEvent.accessibleDocument;
      ok(docAcc, "Accessible document proxy is created");
      // Remove unnecessary dangling references
      docLoaded = null;
      docLoadedEvent = null;
      forceGC();

      let canShutdown = false;
      const [a11yShutdownObserver, a11yShutdownPromise] = shutdownAccService();
      await a11yShutdownObserver;
      const a11yShutdown = new Promise((resolve, reject) =>
        a11yShutdownPromise.then(flag =>
          canShutdown
            ? resolve()
            : reject("Accessible service was shut down incorrectly")
        )
      );

      accService = null;
      ok(!accService, "Service is removed");
      // Force garbage collection that should not trigger shutdown because there
      // is a reference to an accessible proxy.
      forceGC();
      // Have some breathing room when removing a11y service references.
      await TestUtils.waitForTick();

      // Now allow a11y service to shutdown.
      canShutdown = true;
      // Remove a last reference to an accessible document proxy.
      docAcc = null;
      ok(!docAcc, "Accessible document proxy is removed");

      // Force garbage collection that should now trigger shutdown.
      forceGC();
      await a11yShutdown;
    }
  );
});
