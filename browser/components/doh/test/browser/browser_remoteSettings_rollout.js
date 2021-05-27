/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(setup);
add_task(setupRegion);

add_task(async function testPrefFirstRollout() {
  is(
    DoHConfigController.currentConfig.enabled,
    false,
    "Rollout should not be enabled"
  );

  let prefPromise = TestUtils.waitForPrefChange(prefs.BREADCRUMB_PREF);
  Preferences.set(`${kRegionalPrefNamespace}.enabled`, true);

  await prefPromise;
  is(Preferences.get(prefs.BREADCRUMB_PREF), true, "Breadcrumb saved.");
  is(
    Preferences.get(prefs.TRR_SELECT_URI_PREF),
    "https://example.com/dns-query",
    "TRR selection complete."
  );
  await checkTRRSelectionTelemetry();

  await ensureTRRMode(2);
  await checkHeuristicsTelemetry("enable_doh", "startup");

  is(
    DoHConfigController.currentConfig.enabled,
    true,
    "Rollout should be enabled"
  );

  await DoHTestUtils.loadRemoteSettingsProviders([
    {
      id: "provider1",
      uri: "https://example.org/1",
      autoDefault: true,
    },
  ]);

  await DoHTestUtils.loadRemoteSettingsConfig({
    id: kTestRegion.toLowerCase(),
    rolloutEnabled: true,
    providers: "provider1",
  });

  is(
    DoHConfigController.currentConfig.enabled,
    true,
    "Rollout should still be enabled"
  );

  let configUpdatedPromise = DoHTestUtils.waitForConfigUpdate();
  Preferences.reset(`${kRegionalPrefNamespace}.enabled`);
  await configUpdatedPromise;

  is(
    DoHConfigController.currentConfig.enabled,
    true,
    "Rollout should still be enabled"
  );

  await DoHTestUtils.resetRemoteSettingsConfig();

  is(
    DoHConfigController.currentConfig.enabled,
    false,
    "Rollout should not be enabled"
  );
});
