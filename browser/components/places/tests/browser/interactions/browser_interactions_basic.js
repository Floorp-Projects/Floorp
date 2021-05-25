/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Interactions } = ChromeUtils.import(
  "resource:///modules/Interactions.jsm"
);

const TEST_URL = "https://example.com/";

add_task(async function test_interactions_basic() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  let docInfo;
  await BrowserTestUtils.waitForCondition(() => {
    docInfo = Interactions._getInteractionFor(tab.linkedBrowser);
    return !!docInfo;
  });

  Assert.equal(docInfo.url, TEST_URL, "Should have saved an interaction");

  BrowserTestUtils.removeTab(tab);
});
