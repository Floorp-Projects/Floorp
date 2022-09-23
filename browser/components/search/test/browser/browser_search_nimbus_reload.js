/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { NimbusFeatures } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { SearchService } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchService.sys.mjs"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

add_task(async function test_engines_reloaded_nimbus() {
  let reloadSpy = sinon.spy(SearchService.prototype, "_maybeReloadEngines");
  let getVariableSpy = sinon.spy(NimbusFeatures.search, "getVariable");

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "search",
    value: { experiment: "nimbus-search-mochitest" },
  });

  Assert.equal(reloadSpy.callCount, 1, "Called by experiment enrollment");
  await BrowserTestUtils.waitForCondition(
    () => getVariableSpy.calledWith("experiment"),
    "Wait for SearchService update to run"
  );
  Assert.equal(
    getVariableSpy.callCount,
    2,
    "Called by update function to fetch engines and by ParamPreferenceCache"
  );
  Assert.ok(
    getVariableSpy.calledWith("extraParams"),
    "Called by ParamPreferenceCache listener"
  );
  Assert.ok(
    getVariableSpy.calledWith("experiment"),
    "Called by search service observer"
  );
  Assert.equal(
    NimbusFeatures.search.getVariable("experiment"),
    "nimbus-search-mochitest",
    "Should have expected value"
  );

  await doExperimentCleanup();

  Assert.equal(reloadSpy.callCount, 2, "Called by experiment unenrollment");

  reloadSpy.restore();
  getVariableSpy.restore();
});
