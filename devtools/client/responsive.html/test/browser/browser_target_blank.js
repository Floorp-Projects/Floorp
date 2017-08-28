/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure target="_blank" link opens a new tab

const TAB_URL = "http://example.com/";
const TEST_URL =
  `data:text/html,<a href="${TAB_URL}" target="_blank">Click me</a>`
  .replace(/ /g, "%20");

addRDMTask(TEST_URL, function* ({ ui }) {
  let store = ui.toolWindow.store;

  // Wait until the viewport has been added
  yield waitUntilState(store, state => state.viewports.length == 1);

  // Click the target="_blank" link and wait for a new tab
  yield waitForFrameLoad(ui, TEST_URL);
  let newTab = BrowserTestUtils.waitForNewTab(gBrowser, TAB_URL);
  spawnViewportTask(ui, {}, function* () {
    content.document.querySelector("a").click(); // eslint-disable-line
  });
  ok(yield newTab, "New tab opened from link");
});
