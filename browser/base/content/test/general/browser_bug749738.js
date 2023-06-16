/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DUMMY_PAGE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.org/browser/browser/base/content/test/general/dummy_page.html";

/**
 * This test checks that if you search for something on one tab, then close
 * that tab and have the find bar open on the next tab you get switched to,
 * closing the find bar in that tab works without exceptions.
 */
add_task(async function test_bug749738() {
  // Open find bar on initial tab.
  await gFindBarPromise;

  await BrowserTestUtils.withNewTab(DUMMY_PAGE, async function () {
    await gFindBarPromise;
    gFindBar.onFindCommand();
    EventUtils.sendString("Dummy");
  });

  try {
    gFindBar.close();
    ok(true, "findbar.close should not throw an exception");
  } catch (e) {
    ok(false, "findbar.close threw exception: " + e);
  }
});
