/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

registerCleanupFunction(async () => {
  // When the test completes, make sure we cleanup with a populated cache,
  // since this is the default starting state for these tests.
  await withFullyLoadedAboutHome(async browser => {
    await simulateRestart(browser);
  });
});

/**
 * Tests that the ExperimentsAPI mechanism can be used to remotely
 * enable and disable the about:home startup cache.
 */
add_task(async function test_experiments_api_control() {
  // First, the disabled case.
  await withFullyLoadedAboutHome(async browser => {
    let doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
      featureId: "abouthomecache",
      value: { enabled: false },
    });

    Assert.ok(
      !NimbusFeatures.abouthomecache.getVariable("enabled"),
      "NimbusFeatures should tell us that the about:home startup cache " +
        "is disabled"
    );

    await simulateRestart(browser);

    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.DISABLED
    );

    await doEnrollmentCleanup();
  });

  // Now the enabled case.
  await withFullyLoadedAboutHome(async browser => {
    let doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
      featureId: "abouthomecache",
      value: { enabled: true },
    });

    Assert.ok(
      NimbusFeatures.abouthomecache.getVariable("enabled"),
      "NimbusFeatures should tell us that the about:home startup cache " +
        "is enabled"
    );

    await simulateRestart(browser);
    await ensureCachedAboutHome(browser);
    await doEnrollmentCleanup();
  });
});
