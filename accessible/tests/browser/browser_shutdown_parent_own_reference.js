/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `data:text/html,
      <html>
        <head>
          <meta charset="utf-8"/>
          <title>Accessibility Test</title>
        </head>
        <body></body>
      </html>`,
    },
    async function (browser) {
      info(
        "Creating a service in parent and waiting for service to be created " +
          "in content"
      );
      await loadContentScripts(browser, {
        script: "Common.sys.mjs",
        symbol: "CommonUtils",
      });
      // Create a11y service in the main process. This will trigger creating of
      // the a11y service in parent as well.
      const [parentA11yInitObserver, parentA11yInit] = initAccService();
      const [contentA11yInitObserver, contentA11yInit] =
        initAccService(browser);

      await Promise.all([parentA11yInitObserver, contentA11yInitObserver]);

      let accService = Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService
      );
      ok(accService, "Service initialized in parent");
      await Promise.all([parentA11yInit, contentA11yInit]);

      info(
        "Adding additional reference to accessibility service in content " +
          "process"
      );
      // Add a new reference to the a11y service inside the content process.
      await SpecialPowers.spawn(browser, [], () => {
        content.CommonUtils.accService;
      });

      info(
        "Trying to shut down a service in content and making sure it stays " +
          "alive as it was started by parent"
      );
      let contentCanShutdown = false;
      // This promise will resolve only if contentCanShutdown flag is set to true.
      // If 'a11y-init-or-shutdown' event with '0' flag (in content) comes before
      // it can be shut down, the promise will reject.
      const [contentA11yShutdownObserver, contentA11yShutdownPromise] =
        shutdownAccService(browser);
      await contentA11yShutdownObserver;
      const contentA11yShutdown = new Promise((resolve, reject) =>
        contentA11yShutdownPromise.then(flag =>
          contentCanShutdown
            ? resolve()
            : reject("Accessible service was shut down incorrectly")
        )
      );
      // Remove a11y service reference in content and force garbage collection.
      // This should not trigger shutdown since a11y was originally initialized by
      // the main process.
      await SpecialPowers.spawn(browser, [], () => {
        content.CommonUtils.clearAccService();
      });

      // Have some breathing room between a11y service shutdowns.
      await TestUtils.waitForTick();

      info("Removing a service in parent");
      // Now allow a11y service to shutdown in content.
      contentCanShutdown = true;
      // Remove the a11y service reference in the main process.
      const [parentA11yShutdownObserver, parentA11yShutdown] =
        shutdownAccService();
      await parentA11yShutdownObserver;

      accService = null;
      ok(!accService, "Service is removed in parent");
      // Force garbage collection that should trigger shutdown in both parent and
      // content.
      forceGC();
      await Promise.all([parentA11yShutdown, contentA11yShutdown]);
    }
  );
});
