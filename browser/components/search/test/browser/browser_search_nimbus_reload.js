/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { NimbusFeatures } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { SearchService } = ChromeUtils.import(
  "resource://gre/modules/SearchService.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

add_task(async function test_engines_reloaded_nimbus() {
  let reloadSpy = sinon.spy(SearchService.prototype, "_maybeReloadEngines");
  let variableSpy = sinon.spy(NimbusFeatures.search, "getVariable");

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "search",
    value: { experiment: "nimbus-search-mochitest" },
  });

  Assert.equal(reloadSpy.callCount, 1, "Called by experiment enrollment");
  await BrowserTestUtils.waitForCondition(
    () => variableSpy.called,
    "Wait for SearchService update to run"
  );
  Assert.equal(
    variableSpy.callCount,
    1,
    "Called by update function to fetch engines"
  );
  Assert.equal(
    variableSpy.firstCall.args[0],
    "experiment",
    "Got `experiment` variable value"
  );
  Assert.equal(
    NimbusFeatures.search.getVariable("experiment"),
    "nimbus-search-mochitest",
    "Should have expected value"
  );

  await doExperimentCleanup();

  Assert.equal(reloadSpy.callCount, 2, "Called by experiment unenrollment");

  reloadSpy.restore();
  variableSpy.restore();
});
