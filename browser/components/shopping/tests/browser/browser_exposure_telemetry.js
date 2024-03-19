/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ShoppingUtils: "resource:///modules/ShoppingUtils.sys.mjs",
});

// Tests in this file simulate exposure detection without actually loading the
// product pages. Instead, we call the `ShoppingUtils.maybeRecordExposure`
// method, passing in flags and URLs to simulate onLocationChange events.
// Bug 1853401 captures followup work to add integration tests.

const PRODUCT_PAGE = Services.io.newURI(
  "https://example.com/product/B09TJGHL5F"
);
const WALMART_PAGE = Services.io.newURI(
  "https://www.walmart.com/ip/Utz-Cheese-Balls-23-Oz/15543964"
);
const WALMART_OTHER_PAGE = Services.io.newURI(
  "https://www.walmart.com/ip/Utz-Gluten-Free-Cheese-Balls-23-0-OZ/10898644"
);

async function setup(pref) {
  await SpecialPowers.pushPrefEnv({
    set: [[`browser.shopping.experience2023.${pref}`, true]],
  });
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
}

async function teardown() {
  await SpecialPowers.popPrefEnv();
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  // Clear out the normally short-lived pushState navigation cache in between
  // runs, to avoid accidentally deduping when we shouldn't.
  ShoppingUtils.lastWalmartURI = null;
}

async function runTest({ aLocationURI, aFlags, expected }) {
  async function _run() {
    Assert.equal(undefined, Glean.shopping.productPageVisits.testGetValue());
    ShoppingUtils.onLocationChange(aLocationURI, aFlags);
    await Services.fog.testFlushAllChildren();
    Assert.equal(expected, Glean.shopping.productPageVisits.testGetValue());
  }

  await setup("enabled");
  await _run();
  await teardown("enabled");

  await setup("control");
  await _run();
  await teardown("control");
}

add_task(async function test_shopping_exposure_new_page() {
  await runTest({
    aLocationURI: PRODUCT_PAGE,
    aFlags: 0,
    expected: 1,
  });
});

add_task(async function test_shopping_exposure_reload_page() {
  await runTest({
    aLocationURI: PRODUCT_PAGE,
    aFlags: Ci.nsIWebProgressListener.LOCATION_CHANGE_RELOAD,
    expected: 1,
  });
});

add_task(async function test_shopping_exposure_session_restore_page() {
  await runTest({
    aLocationURI: PRODUCT_PAGE,
    aFlags: Ci.nsIWebProgressListener.LOCATION_CHANGE_SESSION_STORE,
    expected: 1,
  });
});

add_task(async function test_shopping_exposure_ignore_same_page() {
  await runTest({
    aLocationURI: PRODUCT_PAGE,
    aFlags: Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT,
    expected: undefined,
  });
});

add_task(async function test_shopping_exposure_count_same_page_pushstate() {
  await runTest({
    aLocationURI: WALMART_PAGE,
    aFlags: Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT,
    expected: 1,
  });
});

add_task(async function test_shopping_exposure_ignore_pushstate_repeats() {
  async function _run() {
    let aFlags = Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT;
    Assert.equal(undefined, Glean.shopping.productPageVisits.testGetValue());

    // Slightly different setup here: simulate deduping by setting the first
    // walmart page's URL as the `ShoppingUtils.lastWalmartURI`, then fire the
    // pushState for the first page, then twice for a second page. This seems
    // to be roughly the observed behavior when navigating between walmart
    // product pages.
    ShoppingUtils.lastWalmartURI = WALMART_PAGE;
    ShoppingUtils.onLocationChange(WALMART_PAGE, aFlags);
    ShoppingUtils.onLocationChange(WALMART_OTHER_PAGE, aFlags);
    ShoppingUtils.onLocationChange(WALMART_OTHER_PAGE, aFlags);
    await Services.fog.testFlushAllChildren();
    Assert.equal(1, Glean.shopping.productPageVisits.testGetValue());
  }
  await setup("enabled");
  await _run();
  await teardown("enabled");
  await setup("control");
  await _run();
  await teardown("control");
});
