/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const ROOT_URL = "http://example.com/browser/dom/tests/browser/perfmetrics";
const PAGE_URL = ROOT_URL + "/unresponsive.html";

add_task(async function test() {
  waitForExplicitFinish();

  await BrowserTestUtils.withNewTab({ gBrowser, url: PAGE_URL }, async function(
    browser
  ) {
    let dataBack = 0;
    let tabId = gBrowser.selectedBrowser.outerWindowID;

    function exploreResults(data, filterByWindowId) {
      for (let entry of data) {
        if (entry.windowId == tabId && entry.host != "about:blank") {
          dataBack += 1;
        }
      }
    }
    let results = await ChromeUtils.requestPerformanceMetrics();
    exploreResults(results);
    Assert.ok(dataBack == 0);
  });
});
