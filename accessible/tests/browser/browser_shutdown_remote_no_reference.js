/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  // Making sure that the e10s is enabled on Windows for testing.
  await setE10sPrefs();

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: `data:text/html,
      <html>
        <head>
          <meta charset="utf-8"/>
          <title>Accessibility Test</title>
        </head>
        <body></body>
      </html>`
  }, async function(browser) {
    info("Creating a service in parent and waiting for service to be created " +
      "in content");
    // Create a11y service in the main process. This will trigger creating of
    // the a11y service in parent as well.
    let parentA11yInit = initPromise();
    let contentA11yInit = initPromise(browser);
    let accService = Cc["@mozilla.org/accessibilityService;1"].getService(
      Ci.nsIAccessibilityService);
    ok(accService, "Service initialized in parent");
    await Promise.all([parentA11yInit, contentA11yInit]);

    info("Removing a service in parent and waiting for service to be shut " +
      "down in content");
    // Remove a11y service reference in the main process.
    let parentA11yShutdown = shutdownPromise();
    let contentA11yShutdown = shutdownPromise(browser);
    accService = null;
    ok(!accService, "Service is removed in parent");
    // Force garbage collection that should trigger shutdown in both main and
    // content process.
    forceGC();
    await Promise.all([parentA11yShutdown, contentA11yShutdown]);
  });

  // Unsetting e10s related preferences.
  await unsetE10sPrefs();
});
