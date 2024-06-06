/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const { SearchService } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchService.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_task(async function test_engines_reloaded_nimbus() {
  let reloadSpy = sinon.spy(SearchService.prototype, "_maybeReloadEngines");
  let getVariableSpy = sinon.spy(
    NimbusFeatures.searchConfiguration,
    "getVariable"
  );

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "searchConfiguration",
    value: { experiment: "nimbus-search-mochitest" },
  });

  Assert.equal(reloadSpy.callCount, 1, "Called by experiment enrollment");
  await BrowserTestUtils.waitForCondition(
    () => getVariableSpy.calledWith("experiment"),
    "Wait for SearchService update to run"
  );
  Assert.equal(
    getVariableSpy.callCount,
    3,
    "Called by update function to fetch engines"
  );
  Assert.ok(
    getVariableSpy.calledWith("experiment"),
    "Called by search service observer"
  );
  Assert.equal(
    NimbusFeatures.searchConfiguration.getVariable("experiment"),
    "nimbus-search-mochitest",
    "Should have expected value"
  );

  doExperimentCleanup();

  Assert.equal(reloadSpy.callCount, 2, "Called by experiment unenrollment");

  reloadSpy.restore();
  getVariableSpy.restore();
});
