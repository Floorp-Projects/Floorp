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
      info("Creating a service in content");
      await loadContentScripts(browser, {
        script: "Common.sys.mjs",
        symbol: "CommonUtils",
      });
      // Create a11y service in the content process.
      const [a11yInitObserver, a11yInit] = initAccService(browser);
      await a11yInitObserver;
      await SpecialPowers.spawn(browser, [], () => {
        content.CommonUtils.accService;
      });
      await a11yInit;
      ok(
        true,
        "Accessibility service is started in content process correctly."
      );

      info("Removing a service in content");
      // Remove a11y service reference from the content process.
      const [a11yShutdownObserver, a11yShutdown] = shutdownAccService(browser);
      await a11yShutdownObserver;
      // Force garbage collection that should trigger shutdown.
      await SpecialPowers.spawn(browser, [], () => {
        content.CommonUtils.clearAccService();
      });
      await a11yShutdown;
      ok(
        true,
        "Accessibility service is shutdown in content process correctly."
      );
    }
  );
});
