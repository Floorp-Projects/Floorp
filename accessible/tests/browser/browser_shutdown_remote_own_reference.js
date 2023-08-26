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
      let [contentConsumersChangedObserver, contentConsumersChanged] =
        accConsumersChanged(browser);

      await Promise.all([
        parentA11yInitObserver,
        contentA11yInitObserver,
        contentConsumersChangedObserver,
      ]);

      let accService = Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService
      );
      ok(accService, "Service initialized in parent");
      await Promise.all([parentA11yInit, contentA11yInit]);
      await contentConsumersChanged.then(data =>
        Assert.deepEqual(
          data,
          {
            XPCOM: false,
            MainProcess: true,
            PlatformAPI: false,
          },
          "Accessibility service consumers in content are correct."
        )
      );

      info(
        "Adding additional reference to accessibility service in content " +
          "process"
      );
      [contentConsumersChangedObserver, contentConsumersChanged] =
        accConsumersChanged(browser);
      await contentConsumersChangedObserver;
      // Add a new reference to the a11y service inside the content process.
      await SpecialPowers.spawn(browser, [], () => {
        content.CommonUtils.accService;
      });
      await contentConsumersChanged.then(data =>
        Assert.deepEqual(
          data,
          {
            XPCOM: true,
            MainProcess: true,
            PlatformAPI: false,
          },
          "Accessibility service consumers in content are correct."
        )
      );

      const contentConsumers = await SpecialPowers.spawn(browser, [], () =>
        content.CommonUtils.accService.getConsumers()
      );
      Assert.deepEqual(
        JSON.parse(contentConsumers),
        {
          XPCOM: true,
          MainProcess: true,
          PlatformAPI: false,
        },
        "Accessibility service consumers in parent are correct."
      );

      info(
        "Shutting down a service in parent and making sure the one in " +
          "content stays alive"
      );
      let contentCanShutdown = false;
      const [parentA11yShutdownObserver, parentA11yShutdown] =
        shutdownAccService();
      [contentConsumersChangedObserver, contentConsumersChanged] =
        accConsumersChanged(browser);
      // This promise will resolve only if contentCanShutdown flag is set to true.
      // If 'a11y-init-or-shutdown' event with '0' flag (in content) comes before
      // it can be shut down, the promise will reject.
      const [contentA11yShutdownObserver, contentA11yShutdownPromise] =
        shutdownAccService(browser);
      const contentA11yShutdown = new Promise((resolve, reject) =>
        contentA11yShutdownPromise.then(flag =>
          contentCanShutdown
            ? resolve()
            : reject("Accessible service was shut down incorrectly")
        )
      );

      await Promise.all([
        parentA11yShutdownObserver,
        contentA11yShutdownObserver,
        contentConsumersChangedObserver,
      ]);
      // Remove a11y service reference in the main process and force garbage
      // collection. This should not trigger shutdown in content since a11y
      // service is used by XPCOM.
      accService = null;
      ok(!accService, "Service is removed in parent");
      // Force garbage collection that should not trigger shutdown because there
      // is a reference in a content process.
      forceGC();
      await SpecialPowers.spawn(browser, [], () => {
        SpecialPowers.Cu.forceGC();
      });
      await parentA11yShutdown;
      await contentConsumersChanged.then(data =>
        Assert.deepEqual(
          data,
          {
            XPCOM: true,
            MainProcess: false,
            PlatformAPI: false,
          },
          "Accessibility service consumers in content are correct."
        )
      );

      // Have some breathing room between a11y service shutdowns.
      await TestUtils.waitForTick();

      info("Removing a service in content");
      // Now allow a11y service to shutdown in content.
      contentCanShutdown = true;
      [contentConsumersChangedObserver, contentConsumersChanged] =
        accConsumersChanged(browser);
      await contentConsumersChangedObserver;
      // Remove last reference to a11y service in content and force garbage
      // collection that should trigger shutdown.
      await SpecialPowers.spawn(browser, [], () => {
        content.CommonUtils.clearAccService();
      });
      await contentA11yShutdown;
      await contentConsumersChanged.then(data =>
        Assert.deepEqual(
          data,
          {
            XPCOM: false,
            MainProcess: false,
            PlatformAPI: false,
          },
          "Accessibility service consumers in content are correct."
        )
      );
    }
  );
});
