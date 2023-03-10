/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function runTests(browser, accDoc) {
  let getAcc = id => findAccessibleChildByID(accDoc, id);

  // a: no traversed state
  testStates(getAcc("link_traversed"), 0, 0, STATE_TRAVERSED);

  let onStateChanged = waitForEvent(EVENT_STATE_CHANGE, "link_traversed");
  let newTabOpened = BrowserTestUtils.waitForNewTab(gBrowser);

  await BrowserTestUtils.synthesizeMouse(
    "#link_traversed",
    1,
    1,
    { ctrlKey: !MAC, metaKey: MAC },
    browser
  );

  await onStateChanged;
  testStates(getAcc("link_traversed"), STATE_TRAVERSED);

  let newTab = await newTabOpened;
  gBrowser.removeTab(newTab);
}

/**
 * Test caching of accessible object states
 */
addAccessibleTask(
  // The URL doesn't really matter, just the fact that it isn't in the history
  // initially. We append ms since epoch to the URL so it will never be visited
  // initially, regardless of other tests (even this one) that ran before.
  `
  <a id="link_traversed"
      href="https://www.example.com/${Date.now()}" target="_top">
    example.com
  </a>`,
  runTests
);
