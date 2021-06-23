/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(setup);
add_task(setupRegion);

add_task(async function testNewProfile() {
  is(
    DoHConfigController.currentConfig.enabled,
    false,
    "Rollout should not be enabled"
  );

  let provider1 = {
    id: "provider1",
    uri: "https://example.org/1",
    autoDefault: true,
  };
  let provider2 = {
    id: "provider2",
    uri: "https://example.org/2",
    canonicalName: "https://example.org/cname",
  };
  let provider3 = {
    id: "provider3",
    uri: "https://example.org/3",
    autoDefault: true,
  };

  await DoHTestUtils.loadRemoteSettingsProviders([
    provider1,
    provider2,
    provider3,
  ]);

  await DoHTestUtils.loadRemoteSettingsConfig({
    id: kTestRegion.toLowerCase(),
    rolloutEnabled: true,
    providers: "provider1, provider3",
    steeringEnabled: true,
    steeringProviders: "provider2",
    autoDefaultEnabled: true,
    autoDefaultProviders: "provider1, provider3",
  });

  is(
    DoHConfigController.currentConfig.enabled,
    true,
    "Rollout should be enabled"
  );
  await ensureTRRMode(2);
  Assert.deepEqual(
    DoHConfigController.currentConfig.providerList,
    [provider1, provider3],
    "Provider list should be loaded"
  );
  is(
    DoHConfigController.currentConfig.providerSteering.enabled,
    true,
    "Steering should be enabled"
  );
  Assert.deepEqual(
    DoHConfigController.currentConfig.providerSteering.providerList,
    [provider2],
    "Steering provider list should be loaded"
  );
  is(
    DoHConfigController.currentConfig.trrSelection.enabled,
    true,
    "TRR Selection should be enabled"
  );
  Assert.deepEqual(
    DoHConfigController.currentConfig.trrSelection.providerList,
    [provider1, provider3],
    "TRR Selection provider list should be loaded"
  );
  is(
    DoHConfigController.currentConfig.fallbackProviderURI,
    provider1.uri,
    "Fallback provider URI should be that of the first one"
  );

  await DoHTestUtils.resetRemoteSettingsConfig();

  is(
    DoHConfigController.currentConfig.enabled,
    false,
    "Rollout should be disabled"
  );
  await ensureTRRMode(undefined);
});
