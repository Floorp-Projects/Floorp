/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const TIMING_HISTOGRAM = "WEBEXT_BROWSERACTION_POPUP_OPEN_MS";
const TIMING_HISTOGRAM_KEYED = "WEBEXT_BROWSERACTION_POPUP_OPEN_MS_BY_ADDONID";
const RESULT_HISTOGRAM = "WEBEXT_BROWSERACTION_POPUP_PRELOAD_RESULT_COUNT";
const RESULT_HISTOGRAM_KEYED =
  "WEBEXT_BROWSERACTION_POPUP_PRELOAD_RESULT_COUNT_BY_ADDONID";

const EXTENSION_ID1 = "@test-extension1";
const EXTENSION_ID2 = "@test-extension2";

// Keep this in sync with the order in Histograms.json for
// WEBEXT_BROWSERACTION_POPUP_PRELOAD_RESULT_COUNT
const CATEGORIES = ["popupShown", "clearAfterHover", "clearAfterMousedown"];
const GLEAN_RESULT_LABELS = [...CATEGORIES, "__other__"];

function assertGleanPreloadResultLabelCounter(expectedLabelsValue) {
  for (const label of GLEAN_RESULT_LABELS) {
    const expectedLabelValue = expectedLabelsValue[label];
    Assert.deepEqual(
      Glean.extensionsCounters.browserActionPreloadResult[label].testGetValue(),
      expectedLabelValue,
      `Expect Glean browserActionPreloadResult metric label ${label} to be ${
        expectedLabelValue > 0 ? expectedLabelValue : "empty"
      }`
    );
  }
}

function assertGleanPreloadResultLabelCounterEmpty() {
  // All empty labels passed to the other helpers to make it
  // assert that all labels are empty.
  assertGleanPreloadResultLabelCounter({});
}

/**
 * Takes a Telemetry histogram snapshot and makes sure
 * that the index for that value (as defined by CATEGORIES)
 * has a count of 1, and that it's the only value that
 * has been incremented.
 *
 * @param {object} snapshot
 *        The Telemetry histogram snapshot to examine.
 * @param {string} category
 *        The category in CATEGORIES whose index we expect to have
 *        been set to 1.
 */
function assertOnlyOneTypeSet(snapshot, category) {
  let categoryIndex = CATEGORIES.indexOf(category);
  Assert.equal(
    snapshot.values[categoryIndex],
    1,
    `Should have seen the ${category} count increment.`
  );
  // Use Array.prototype.reduce to sum up all of the
  // snapshot.count entries
  Assert.equal(
    Object.values(snapshot.values).reduce((a, b) => a + b, 0),
    1,
    "Should only be 1 collected value."
  );
}

add_task(async function testBrowserActionTelemetryTiming() {
  let extensionOptions = {
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
        browser_style: true,
      },
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"></head><body><div></div></body></html>`,
    },
  };
  let extension1 = ExtensionTestUtils.loadExtension({
    ...extensionOptions,
    manifest: {
      ...extensionOptions.manifest,
      browser_specific_settings: {
        gecko: { id: EXTENSION_ID1 },
      },
    },
  });
  let extension2 = ExtensionTestUtils.loadExtension({
    ...extensionOptions,
    manifest: {
      ...extensionOptions.manifest,
      browser_specific_settings: {
        gecko: { id: EXTENSION_ID2 },
      },
    },
  });

  let histogram = Services.telemetry.getHistogramById(TIMING_HISTOGRAM);
  let histogramKeyed = Services.telemetry.getKeyedHistogramById(
    TIMING_HISTOGRAM_KEYED
  );

  histogram.clear();
  histogramKeyed.clear();
  Services.fog.testResetFOG();

  is(
    histogram.snapshot().sum,
    0,
    `No data recorded for histogram: ${TIMING_HISTOGRAM}.`
  );
  is(
    Object.keys(histogramKeyed).length,
    0,
    `No data recorded for histogram: ${TIMING_HISTOGRAM_KEYED}.`
  );
  Assert.deepEqual(
    Glean.extensionsTiming.browserActionPopupOpen.testGetValue(),
    undefined,
    "No data recorded for glean metric extensionsTiming.browserActionPopupOpen"
  );

  await extension1.startup();
  await extension2.startup();

  is(
    histogram.snapshot().sum,
    0,
    `No data recorded for histogram after startup: ${TIMING_HISTOGRAM}.`
  );
  is(
    Object.keys(histogramKeyed).length,
    0,
    `No data recorded for histogram after startup: ${TIMING_HISTOGRAM_KEYED}.`
  );
  Assert.deepEqual(
    Glean.extensionsTiming.browserActionPopupOpen.testGetValue(),
    undefined,
    "No data recorded for glean metric extensionsTiming.browserActionPopupOpen"
  );

  clickBrowserAction(extension1);
  await awaitExtensionPanel(extension1);
  let sumOld = histogram.snapshot().sum;
  ok(
    sumOld > 0,
    `Data recorded for first extension for histogram: ${TIMING_HISTOGRAM}.`
  );

  let oldKeyedSnapshot = histogramKeyed.snapshot();
  Assert.deepEqual(
    Object.keys(oldKeyedSnapshot),
    [EXTENSION_ID1],
    `Data recorded for first extension for histogram: ${TIMING_HISTOGRAM_KEYED}.`
  );
  ok(
    oldKeyedSnapshot[EXTENSION_ID1].sum > 0,
    `Data recorded for first extension for histogram: ${TIMING_HISTOGRAM_KEYED}.`
  );

  let gleanSumOld =
    Glean.extensionsTiming.browserActionPopupOpen.testGetValue()?.sum;
  ok(
    gleanSumOld > 0,
    "Data recorded for first extension on glean metric extensionsTiming.browserActionPopupOpen"
  );

  await closeBrowserAction(extension1);

  clickBrowserAction(extension2);
  await awaitExtensionPanel(extension2);
  let sumNew = histogram.snapshot().sum;
  ok(
    sumNew > sumOld,
    `Data recorded for second extension for histogram: ${TIMING_HISTOGRAM}.`
  );
  sumOld = sumNew;

  let gleanSumNew =
    Glean.extensionsTiming.browserActionPopupOpen.testGetValue()?.sum;
  ok(
    gleanSumNew > gleanSumOld,
    "Data recorded for second extension on glean metric extensionsTiming.browserActionPopupOpen"
  );
  gleanSumOld = gleanSumNew;

  let newKeyedSnapshot = histogramKeyed.snapshot();
  Assert.deepEqual(
    Object.keys(newKeyedSnapshot).sort(),
    [EXTENSION_ID1, EXTENSION_ID2],
    `Data recorded for second extension for histogram: ${TIMING_HISTOGRAM_KEYED}.`
  );
  ok(
    newKeyedSnapshot[EXTENSION_ID2].sum > 0,
    `Data recorded for second extension for histogram: ${TIMING_HISTOGRAM_KEYED}.`
  );
  is(
    newKeyedSnapshot[EXTENSION_ID1].sum,
    oldKeyedSnapshot[EXTENSION_ID1].sum,
    `Data recorded for first extension should not change for histogram: ${TIMING_HISTOGRAM_KEYED}.`
  );
  oldKeyedSnapshot = newKeyedSnapshot;

  await closeBrowserAction(extension2);

  clickBrowserAction(extension2);
  await awaitExtensionPanel(extension2);
  sumNew = histogram.snapshot().sum;
  ok(
    sumNew > sumOld,
    `Data recorded for second opening of popup for histogram: ${TIMING_HISTOGRAM}.`
  );
  sumOld = sumNew;

  gleanSumNew =
    Glean.extensionsTiming.browserActionPopupOpen.testGetValue()?.sum;
  ok(
    gleanSumNew > gleanSumOld,
    "Data recorded for second popup opening on glean metric extensionsTiming.browserActionPopupOpen"
  );
  gleanSumOld = gleanSumNew;

  newKeyedSnapshot = histogramKeyed.snapshot();
  ok(
    newKeyedSnapshot[EXTENSION_ID2].sum > oldKeyedSnapshot[EXTENSION_ID2].sum,
    `Data recorded for second opening of popup for histogram: ${TIMING_HISTOGRAM_KEYED}.`
  );
  is(
    newKeyedSnapshot[EXTENSION_ID1].sum,
    oldKeyedSnapshot[EXTENSION_ID1].sum,
    `Data recorded for first extension should not change for histogram: ${TIMING_HISTOGRAM_KEYED}.`
  );
  oldKeyedSnapshot = newKeyedSnapshot;

  await closeBrowserAction(extension2);

  clickBrowserAction(extension1);
  await awaitExtensionPanel(extension1);
  sumNew = histogram.snapshot().sum;
  ok(
    sumNew > sumOld,
    `Data recorded for third opening of popup for histogram: ${TIMING_HISTOGRAM}.`
  );

  gleanSumNew =
    Glean.extensionsTiming.browserActionPopupOpen.testGetValue()?.sum;
  ok(
    gleanSumNew > gleanSumOld,
    "Data recorded for third popup opening on glean metric extensionsTiming.browserActionPopupOpen"
  );

  newKeyedSnapshot = histogramKeyed.snapshot();
  ok(
    newKeyedSnapshot[EXTENSION_ID1].sum > oldKeyedSnapshot[EXTENSION_ID1].sum,
    `Data recorded for second opening of popup for histogram: ${TIMING_HISTOGRAM_KEYED}.`
  );
  is(
    newKeyedSnapshot[EXTENSION_ID2].sum,
    oldKeyedSnapshot[EXTENSION_ID2].sum,
    `Data recorded for second extension should not change for histogram: ${TIMING_HISTOGRAM_KEYED}.`
  );

  await closeBrowserAction(extension1);

  await extension1.unload();
  await extension2.unload();
});

add_task(async function testBrowserActionTelemetryResults() {
  let extensionOptions = {
    manifest: {
      browser_specific_settings: {
        gecko: { id: EXTENSION_ID1 },
      },
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
        browser_style: true,
      },
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"></head><body><div></div></body></html>`,
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionOptions);

  let histogram = Services.telemetry.getHistogramById(RESULT_HISTOGRAM);
  let histogramKeyed = Services.telemetry.getKeyedHistogramById(
    RESULT_HISTOGRAM_KEYED
  );

  histogram.clear();
  histogramKeyed.clear();
  Services.fog.testResetFOG();

  is(
    histogram.snapshot().sum,
    0,
    `No data recorded for histogram: ${RESULT_HISTOGRAM}.`
  );
  is(
    Object.keys(histogramKeyed).length,
    0,
    `No data recorded for histogram: ${RESULT_HISTOGRAM_KEYED}.`
  );
  assertGleanPreloadResultLabelCounterEmpty();

  await extension.startup();

  // Make sure the mouse isn't hovering over the browserAction widget to start.
  EventUtils.synthesizeMouseAtCenter(
    gURLBar.textbox,
    { type: "mouseover" },
    window
  );

  let widget = getBrowserActionWidget(extension).forWindow(window);

  // Hover the mouse over the browserAction widget and then move it away.
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseover", button: 0 },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseout", button: 0 },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    document.documentElement,
    { type: "mousemove" },
    window
  );

  assertOnlyOneTypeSet(histogram.snapshot(), "clearAfterHover");
  assertGleanPreloadResultLabelCounter({ clearAfterHover: 1 });

  let keyedSnapshot = histogramKeyed.snapshot();
  Assert.deepEqual(
    Object.keys(keyedSnapshot),
    [EXTENSION_ID1],
    `Data recorded for histogram: ${RESULT_HISTOGRAM_KEYED}.`
  );
  assertOnlyOneTypeSet(keyedSnapshot[EXTENSION_ID1], "clearAfterHover");

  histogram.clear();
  histogramKeyed.clear();
  Services.fog.testResetFOG();

  // TODO: Create a test for cancel after mousedown.
  // This is tricky because calling mouseout after mousedown causes a
  // "Hover" event to be added to the queue in ext-browserAction.js.

  clickBrowserAction(extension);
  await awaitExtensionPanel(extension);

  assertOnlyOneTypeSet(histogram.snapshot(), "popupShown");
  assertGleanPreloadResultLabelCounter({ popupShown: 1 });

  keyedSnapshot = histogramKeyed.snapshot();
  Assert.deepEqual(
    Object.keys(keyedSnapshot),
    [EXTENSION_ID1],
    `Data recorded for histogram: ${RESULT_HISTOGRAM_KEYED}.`
  );
  assertOnlyOneTypeSet(keyedSnapshot[EXTENSION_ID1], "popupShown");

  await closeBrowserAction(extension);

  await extension.unload();
});
