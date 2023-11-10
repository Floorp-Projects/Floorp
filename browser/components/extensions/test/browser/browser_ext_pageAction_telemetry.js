/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const HISTOGRAM = "WEBEXT_PAGEACTION_POPUP_OPEN_MS";
const HISTOGRAM_KEYED = "WEBEXT_PAGEACTION_POPUP_OPEN_MS_BY_ADDONID";

const EXTENSION_ID1 = "@test-extension1";
const EXTENSION_ID2 = "@test-extension2";

function snapshotCountsSum(snapshot) {
  return Object.values(snapshot.values).reduce((a, b) => a + b, 0);
}

function histogramCountsSum(histogram) {
  return snapshotCountsSum(histogram.snapshot());
}

function gleanMetricSamplesCount(gleanMetric) {
  return snapshotCountsSum(gleanMetric.testGetValue() ?? { values: {} });
}

add_task(async function testPageActionTelemetry() {
  let extensionOptions = {
    manifest: {
      page_action: {
        default_popup: "popup.html",
        browser_style: true,
      },
    },
    background: function () {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        const tabId = tabs[0].id;

        browser.pageAction.show(tabId).then(() => {
          browser.test.sendMessage("action-shown");
        });
      });
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

  let histogram = Services.telemetry.getHistogramById(HISTOGRAM);
  let histogramKeyed =
    Services.telemetry.getKeyedHistogramById(HISTOGRAM_KEYED);

  histogram.clear();
  histogramKeyed.clear();
  Services.fog.testResetFOG();

  is(
    histogramCountsSum(histogram),
    0,
    `No data recorded for histogram: ${HISTOGRAM}.`
  );
  is(
    Object.keys(histogramKeyed).length,
    0,
    `No data recorded for histogram: ${HISTOGRAM_KEYED}.`
  );
  Assert.deepEqual(
    Glean.extensionsTiming.pageActionPopupOpen.testGetValue(),
    undefined,
    "No data recorded for glean metric extensionsTiming.pageActionPopupOpen"
  );

  await extension1.startup();
  await extension1.awaitMessage("action-shown");
  await extension2.startup();
  await extension2.awaitMessage("action-shown");

  is(
    histogramCountsSum(histogram),
    0,
    `No data recorded for histogram after PageAction shown: ${HISTOGRAM}.`
  );
  is(
    Object.keys(histogramKeyed).length,
    0,
    `No data recorded for histogram after PageAction shown: ${HISTOGRAM_KEYED}.`
  );
  is(
    gleanMetricSamplesCount(Glean.extensionsTiming.pageActionPopupOpen),
    0,
    "No data recorded for glean metric extensionsTiming.pageActionPopupOpen"
  );

  clickPageAction(extension1, window);
  await awaitExtensionPanel(extension1);

  is(
    histogramCountsSum(histogram),
    1,
    `Data recorded for first extension for histogram: ${HISTOGRAM}.`
  );
  is(
    gleanMetricSamplesCount(Glean.extensionsTiming.pageActionPopupOpen),
    1,
    `Data recorded for first extension on Glean metric extensionsTiming.pageActionPopupOpen`
  );

  let keyedSnapshot = histogramKeyed.snapshot();
  Assert.deepEqual(
    Object.keys(keyedSnapshot),
    [EXTENSION_ID1],
    `Data recorded for first extension histogram: ${HISTOGRAM_KEYED}.`
  );
  is(
    snapshotCountsSum(keyedSnapshot[EXTENSION_ID1]),
    1,
    `Data recorded for first extension for histogram: ${HISTOGRAM_KEYED}.`
  );

  await closePageAction(extension1, window);

  clickPageAction(extension2, window);
  await awaitExtensionPanel(extension2);

  is(
    histogramCountsSum(histogram),
    2,
    `Data recorded for second extension for histogram: ${HISTOGRAM}.`
  );
  is(
    gleanMetricSamplesCount(Glean.extensionsTiming.pageActionPopupOpen),
    2,
    `Data recorded for second extension on Glean metric extensionsTiming.pageActionPopupOpen`
  );

  keyedSnapshot = histogramKeyed.snapshot();
  Assert.deepEqual(
    Object.keys(keyedSnapshot).sort(),
    [EXTENSION_ID1, EXTENSION_ID2],
    `Data recorded for second extension histogram: ${HISTOGRAM_KEYED}.`
  );
  is(
    snapshotCountsSum(keyedSnapshot[EXTENSION_ID2]),
    1,
    `Data recorded for second extension for histogram: ${HISTOGRAM_KEYED}.`
  );
  is(
    snapshotCountsSum(keyedSnapshot[EXTENSION_ID1]),
    1,
    `Data recorded for first extension should not change for histogram: ${HISTOGRAM_KEYED}.`
  );

  await closePageAction(extension2, window);

  clickPageAction(extension2, window);
  await awaitExtensionPanel(extension2);

  is(
    histogramCountsSum(histogram),
    3,
    `Data recorded for second opening of popup for histogram: ${HISTOGRAM}.`
  );
  is(
    gleanMetricSamplesCount(Glean.extensionsTiming.pageActionPopupOpen),
    3,
    `Data recorded for second opening popup on Glean metric extensionsTiming.pageActionPopupOpen`
  );

  keyedSnapshot = histogramKeyed.snapshot();
  is(
    snapshotCountsSum(keyedSnapshot[EXTENSION_ID2]),
    2,
    `Data recorded for second opening of popup for histogram: ${HISTOGRAM_KEYED}.`
  );
  is(
    snapshotCountsSum(keyedSnapshot[EXTENSION_ID1]),
    1,
    `Data recorded for first extension should not change for histogram: ${HISTOGRAM_KEYED}.`
  );

  await closePageAction(extension2, window);

  clickPageAction(extension1, window);
  await awaitExtensionPanel(extension1);

  is(
    histogramCountsSum(histogram),
    4,
    `Data recorded for third opening of popup for histogram: ${HISTOGRAM}.`
  );
  is(
    gleanMetricSamplesCount(Glean.extensionsTiming.pageActionPopupOpen),
    4,
    `Data recorded for third opening popup on Glean metric extensionsTiming.pageActionPopupOpen`
  );

  keyedSnapshot = histogramKeyed.snapshot();
  is(
    snapshotCountsSum(keyedSnapshot[EXTENSION_ID1]),
    2,
    `Data recorded for second opening of popup for histogram: ${HISTOGRAM_KEYED}.`
  );
  is(
    snapshotCountsSum(keyedSnapshot[EXTENSION_ID1]),
    2,
    `Data recorded for second extension should not change for histogram: ${HISTOGRAM_KEYED}.`
  );

  await closePageAction(extension1, window);

  await extension1.unload();
  await extension2.unload();
});
