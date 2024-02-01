/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ShoppingUtils: "resource:///modules/ShoppingUtils.sys.mjs",
});

const ACTIVE_PREF = "browser.shopping.experience2023.active";
const AUTO_OPEN_ENABLED_PREF =
  "browser.shopping.experience2023.autoOpen.enabled";
const AUTO_OPEN_USER_ENABLED_PREF =
  "browser.shopping.experience2023.autoOpen.userEnabled";
const PRODUCT_PAGE = "https://example.com/product/B09TJGHL5F";
const productURI = Services.io.newURI(PRODUCT_PAGE);

async function trigger_auto_open_flow(expectedActivePrefValue) {
  // Set the active pref to false, which triggers ShoppingUtils.onActiveUpdate.
  Services.prefs.setBoolPref(ACTIVE_PREF, false);

  // Call onLocationChange with a product URL, triggering auto-open to flip the
  // active pref back to true, if the auto-open conditions are satisfied.
  ShoppingUtils.onLocationChange(productURI, 0);

  // Wait a turn for the change to propagate...
  await TestUtils.waitForTick();

  // Finally, assert the active pref has the expected state.
  Assert.equal(
    expectedActivePrefValue,
    Services.prefs.getBoolPref(ACTIVE_PREF, false)
  );
}

add_task(async function test_auto_open() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  await trigger_auto_open_flow(true);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_auto_open_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoOpen.enabled", false],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  await trigger_auto_open_flow(false);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_auto_open_user_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", false],
    ],
  });

  await trigger_auto_open_flow(false);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_auto_open_not_opted_in() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 0],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  await trigger_auto_open_flow(false);

  await SpecialPowers.popPrefEnv();
});
