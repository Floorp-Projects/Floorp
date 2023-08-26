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
      let [parentConsumersChangedObserver, parentConsumersChanged] =
        accConsumersChanged();
      let [contentConsumersChangedObserver, contentConsumersChanged] =
        accConsumersChanged(browser);

      await Promise.all([
        parentA11yInitObserver,
        contentA11yInitObserver,
        parentConsumersChangedObserver,
        contentConsumersChangedObserver,
      ]);

      let accService = Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService
      );
      ok(accService, "Service initialized in parent");
      await Promise.all([parentA11yInit, contentA11yInit]);
      await parentConsumersChanged.then(data =>
        Assert.deepEqual(
          data,
          {
            XPCOM: true,
            MainProcess: false,
            PlatformAPI: false,
          },
          "Accessibility service consumers in parent are correct."
        )
      );
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

      Assert.deepEqual(
        JSON.parse(accService.getConsumers()),
        {
          XPCOM: true,
          MainProcess: false,
          PlatformAPI: false,
        },
        "Accessibility service consumers in parent are correct."
      );

      info(
        "Removing a service in parent and waiting for service to be shut " +
          "down in content"
      );
      // Remove a11y service reference in the main process.
      const [parentA11yShutdownObserver, parentA11yShutdown] =
        shutdownAccService();
      const [contentA11yShutdownObserver, contentA11yShutdown] =
        shutdownAccService(browser);
      [parentConsumersChangedObserver, parentConsumersChanged] =
        accConsumersChanged();
      [contentConsumersChangedObserver, contentConsumersChanged] =
        accConsumersChanged(browser);

      await Promise.all([
        parentA11yShutdownObserver,
        contentA11yShutdownObserver,
        parentConsumersChangedObserver,
        contentConsumersChangedObserver,
      ]);

      accService = null;
      ok(!accService, "Service is removed in parent");
      // Force garbage collection that should trigger shutdown in both main and
      // content process.
      forceGC();
      await Promise.all([parentA11yShutdown, contentA11yShutdown]);
      await parentConsumersChanged.then(data =>
        Assert.deepEqual(
          data,
          {
            XPCOM: false,
            MainProcess: false,
            PlatformAPI: false,
          },
          "Accessibility service consumers are correct."
        )
      );
      await contentConsumersChanged.then(data =>
        Assert.deepEqual(
          data,
          {
            XPCOM: false,
            MainProcess: false,
            PlatformAPI: false,
          },
          "Accessibility service consumers are correct."
        )
      );
    }
  );
});
