/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";
add_task(async function testPrefFirstRollout() {
  await setup();
  await setupRegion();
  let defaults = Services.prefs.getDefaultBranch("");

  is(
    DoHConfigController.currentConfig.enabled,
    false,
    "Rollout should not be enabled"
  );
  setPassingHeuristics();

  let configFlushedPromise = DoHTestUtils.waitForConfigFlush();
  defaults.setBoolPref(`${kRegionalPrefNamespace}.enabled`, true);
  await configFlushedPromise;

  is(
    DoHConfigController.currentConfig.enabled,
    true,
    "Rollout should be enabled"
  );
  await ensureTRRMode(2);

  is(
    Preferences.get("doh-rollout.home-region"),
    "DE",
    "Initial region should be DE"
  );
  Region._setHomeRegion("UK");
  await ensureTRRMode(2); // Mode shouldn't change.

  is(Preferences.get("doh-rollout.home-region-changed"), true);

  await DoHController._uninit();
  await DoHConfigController._uninit();

  // Check after controller gets reinitialized (or restart)
  // that the region gets set to UK
  await DoHConfigController.init();
  await DoHController.init();
  is(Preferences.get("doh-rollout.home-region"), "UK");

  is(
    DoHConfigController.currentConfig.enabled,
    false,
    "Rollout should not be enabled for new region"
  );
  await ensureTRRMode(undefined); // restart of the controller should change the region.

  // Reset state to initial values.
  await setupRegion();
  defaults.deleteBranch(`doh-rollout.de`);
  Preferences.reset("doh-rollout.home-region-changed");
  Preferences.reset("doh-rollout.home-region");
});
