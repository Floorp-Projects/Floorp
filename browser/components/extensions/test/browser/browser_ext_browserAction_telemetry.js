/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const TIMING_HISTOGRAM = "WEBEXT_BROWSERACTION_POPUP_OPEN_MS";
const RESULT_HISTOGRAM = "WEBEXT_BROWSERACTION_POPUP_PRELOAD_RESULT_COUNT";

// Keep this in sync with the order in Histograms.json for
// WEBEXT_BROWSERACTION_POPUP_PRELOAD_RESULT_COUNT
const CATEGORIES = [
  "popupShown",
  "clearAfterHover",
  "clearAfterMousedown",
];

/**
 * Takes a Telemetry histogram snapshot and makes sure
 * that the index for that value (as defined by CATEGORIES)
 * has a count of 1, and that it's the only value that
 * has been incremented.
 *
 * @param {Object} snapshot
 *        The Telemetry histogram snapshot to examine.
 * @param {string} category
 *        The category in CATEGORIES whose index we expect to have
 *        been set to 1.
 */
function assertOnlyOneTypeSet(snapshot, category) {
  let categoryIndex = CATEGORIES.indexOf(category);
  Assert.equal(snapshot.counts[categoryIndex], 1,
               `Should have seen the ${category} count increment.`);
  // Use Array.prototype.reduce to sum up all of the
  // snapshot.count entries
  Assert.equal(snapshot.counts.reduce((a, b) => a + b), 1,
               "Should only be 1 collected value.");
}

add_task(async function testBrowserActionTelemetryTiming() {
  let extensionOptions = {
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"></head><body><div></div></body></html>`,
    },
  };
  let extension1 = ExtensionTestUtils.loadExtension(extensionOptions);
  let extension2 = ExtensionTestUtils.loadExtension(extensionOptions);

  let histogram = Services.telemetry.getHistogramById(TIMING_HISTOGRAM);

  histogram.clear();

  is(histogram.snapshot().sum, 0,
     `No data recorded for histogram: ${TIMING_HISTOGRAM}.`);

  await extension1.startup();
  await extension2.startup();

  is(histogram.snapshot().sum, 0,
     `No data recorded for histogram after startup: ${TIMING_HISTOGRAM}.`);

  clickBrowserAction(extension1);
  await awaitExtensionPanel(extension1);
  let sumOld = histogram.snapshot().sum;
  ok(sumOld > 0,
     `Data recorded for first extension for histogram: ${TIMING_HISTOGRAM}.`);
  await closeBrowserAction(extension1);

  clickBrowserAction(extension2);
  await awaitExtensionPanel(extension2);
  let sumNew = histogram.snapshot().sum;
  ok(sumNew > sumOld,
     `Data recorded for second extension for histogram: ${TIMING_HISTOGRAM}.`);
  sumOld = sumNew;
  await closeBrowserAction(extension2);

  clickBrowserAction(extension2);
  await awaitExtensionPanel(extension2);
  sumNew = histogram.snapshot().sum;
  ok(sumNew > sumOld,
     `Data recorded for second opening of popup for histogram: ${TIMING_HISTOGRAM}.`);
  sumOld = sumNew;
  await closeBrowserAction(extension2);

  clickBrowserAction(extension1);
  await awaitExtensionPanel(extension1);
  sumNew = histogram.snapshot().sum;
  ok(sumNew > sumOld,
     `Data recorded for second opening of popup for histogram: ${TIMING_HISTOGRAM}.`);

  await extension1.unload();
  await extension2.unload();
});

add_task(async function testBrowserActionTelemetryResults() {
  let extensionOptions = {
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"></head><body><div></div></body></html>`,
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionOptions);

  let histogram = Services.telemetry.getHistogramById(RESULT_HISTOGRAM);

  histogram.clear();

  is(histogram.snapshot().sum, 0,
     `No data recorded for histogram: ${TIMING_HISTOGRAM}.`);

  await extension.startup();

  // Make sure the mouse isn't hovering over the browserAction widget to start.
  EventUtils.synthesizeMouseAtCenter(gURLBar, {type: "mouseover"}, window);

  let widget = getBrowserActionWidget(extension).forWindow(window);

  // Hover the mouse over the browserAction widget and then move it away.
  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mouseover", button: 0}, window);
  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mouseout", button: 0}, window);
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"}, window);
  assertOnlyOneTypeSet(histogram.snapshot(), "clearAfterHover");
  histogram.clear();

  // TODO: Create a test for cancel after mousedown.
  // This is tricky because calling mouseout after mousedown causes a
  // "Hover" event to be added to the queue in ext-browserAction.js.

  clickBrowserAction(extension);
  await awaitExtensionPanel(extension);
  assertOnlyOneTypeSet(histogram.snapshot(), "popupShown");

  await extension.unload();
});
