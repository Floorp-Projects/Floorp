/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests page view time recording for interactions.
 */

const TEST_REFERRER_URL = "https://example.org/browser";
const TEST_URL = "https://example.org/browser/browser";

add_task(async function test_interactions_referrer() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_REFERRER_URL, async browser => {
    let ReferrerInfo = Components.Constructor(
      "@mozilla.org/referrer-info;1",
      "nsIReferrerInfo",
      "init"
    );

    // Load a new URI with a specific referrer.
    let referrerInfo1 = new ReferrerInfo(
      Ci.nsIReferrerInfo.EMPTY,
      true,
      Services.io.newURI(TEST_REFERRER_URL)
    );
    browser.loadURI(TEST_URL, {
      referrerInfo: referrerInfo1,
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    });
    await BrowserTestUtils.browserLoaded(browser, true, TEST_URL);
  });
  await assertDatabaseValues([
    {
      url: TEST_REFERRER_URL,
      referrer_url: null,
    },
    {
      url: TEST_URL,
      referrer_url: TEST_REFERRER_URL,
    },
  ]);
});
