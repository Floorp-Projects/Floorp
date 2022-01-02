/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test creates a large number of content processes as a
// regression test for bug 1635451.

const TEST_PAGE =
  "http://mochi.test:8888/browser/dom/ipc/tests/file_dummy.html";

const NUM_TABS = 256;

add_task(async () => {
  let promises = [];
  for (let i = 0; i < NUM_TABS; ++i) {
    promises.push(
      BrowserTestUtils.openNewForegroundTab({
        gBrowser,
        opening: TEST_PAGE,
        waitForLoad: true,
        forceNewProcess: true,
      })
    );
  }

  let tabs = [];
  for (const p of promises) {
    tabs.push(await p);
  }

  ok(true, "All of the tabs loaded");

  for (const t of tabs) {
    BrowserTestUtils.removeTab(t);
  }
});
