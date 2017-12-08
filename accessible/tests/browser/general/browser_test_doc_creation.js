/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const tab1URL = `data:text/html,
  <html xmlns="http://www.w3.org/1999/xhtml">
    <head>
      <meta charset="utf-8"/>
      <title>First tab to be loaded</title>
    </head>
    <body>
      <butotn>JUST A BUTTON</butotn>
    </body>
  </html>`;

const tab2URL = `data:text/html,
  <html xmlns="http://www.w3.org/1999/xhtml">
    <head>
      <meta charset="utf-8"/>
      <title>Second tab to be loaded</title>
    </head>
    <body>
      <butotn>JUST A BUTTON</butotn>
    </body>
  </html>`;

// Checking that, if there are open windows before accessibility was started,
// root accessibles for open windows are created so that all root accessibles
// are stored in application accessible children array.
add_task(async function testDocumentCreation() {
  let tab1 = await openNewTab(tab1URL);
  let tab2 = await openNewTab(tab2URL);
  let accService = await initAccessibilityService(); // eslint-disable-line no-unused-vars

  info("Verifying that each tab content document is in accessible cache.");
  for (const browser of [...gBrowser.browsers]) {
    await ContentTask.spawn(browser, null, async () => {
      let accServiceContent =
        Cc["@mozilla.org/accessibilityService;1"].getService(
          Ci.nsIAccessibilityService);
      ok(!!accServiceContent.getAccessibleFromCache(content.document),
        "Document accessible is in cache.");
    });
  }

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);

  accService = null;
  await shutdownAccessibilityService();
});
