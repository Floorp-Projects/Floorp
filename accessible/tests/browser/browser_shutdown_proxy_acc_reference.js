/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
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
        <body><div id="div" style="visibility: hidden;"></div></body>
      </html>`,
    },
    async function (browser) {
      let onShow = waitForEvent(Ci.nsIAccessibleEvent.EVENT_SHOW, "div");
      await invokeSetStyle(browser, "div", "visibility", "visible");
      let showEvent = await onShow;
      let divAcc = showEvent.accessible;
      ok(divAcc, "Accessible proxy is created");
      // Remove unnecessary dangling references
      onShow = null;
      showEvent = null;
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
      // Remove a last reference to an accessible proxy.
      divAcc = null;
      ok(!divAcc, "Accessible proxy is removed");

      // Force garbage collection that should now trigger shutdown.
      forceGC();
      await a11yShutdown;
    }
  );
});
