/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry for quickactions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.sys.mjs",
});

let testActionCalled = 0;

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quickactions", true],
      ["browser.urlbar.quickactions.enabled", true],
    ],
  });

  UrlbarProviderQuickActions.addAction("testaction", {
    commands: ["testaction"],
    label: "quickactions-downloads2",
    onPick: () => testActionCalled++,
  });

  registerCleanupFunction(() => {
    UrlbarProviderQuickActions.removeAction("testaction");
  });
});

add_task(async function test() {
  const histograms = snapshotHistograms();

  // Do a search to show the quickaction.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "testaction",
    waitForFocus,
    fireInputEvent: true,
  });

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_Enter");
  });

  Assert.equal(testActionCalled, 1, "Test action was called");

  TelemetryTestUtils.assertHistogram(
    histograms.resultMethodHist,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.arrowEnterSelection,
    1
  );

  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    `urlbar.picked.quickaction`,
    1,
    1
  );

  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "quickaction.picked",
    "testaction-10",
    1
  );

  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "quickaction.impression",
    "testaction-10",
    1
  );

  // Clean up for subsequent tests.
  gURLBar.handleRevert();
});

add_task(async function test_impressions() {
  UrlbarProviderQuickActions.addAction("testaction2", {
    commands: ["testaction2"],
    label: "quickactions-downloads2",
    onPick: () => {},
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "testaction",
    waitForFocus,
    fireInputEvent: true,
  });

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_Enter");
  });

  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);

  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "quickaction.impression",
    `testaction-10`,
    1
  );
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "quickaction.impression",
    `testaction2-10`,
    1
  );

  UrlbarProviderQuickActions.removeAction("testaction2");
  gURLBar.handleRevert();
});

function snapshotHistograms() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  return {
    resultMethodHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_METHOD"
    ),
  };
}
