/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the settings component is rendered as expected.
 */
add_task(async function test_shopping_settings() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.telemetry.testing.overridePreRelease", true],
      ["browser.shopping.experience2023.optedIn", 0],
    ],
  });

  let opt_in_status = Services.prefs.getIntPref(
    "browser.shopping.experience2023.optedIn",
    undefined
  );
  // Values that match how we're defining the metrics
  let component_opted_out = opt_in_status === 2;
  let onboarded_status = opt_in_status > 0;

  Assert.equal(
    component_opted_out,
    Glean.shoppingSettings.componentOptedOut.testGetValue(),
    "Component Opted Out metric should correctly reflect the preference value"
  );
  Assert.equal(
    onboarded_status,
    Glean.shoppingSettings.hasOnboarded.testGetValue(),
    "Has Onboarded metric should correctly reflect the preference value"
  );

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_shopping_setting_update() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.telemetry.testing.overridePreRelease", true],
      ["browser.shopping.experience2023.optedIn", 2],
    ],
  });

  Assert.equal(
    true,
    Glean.shoppingSettings.componentOptedOut.testGetValue(),
    "Component Opted Out metric should return True as we've set the value of the preference"
  );

  await SpecialPowers.popPrefEnv();
});
