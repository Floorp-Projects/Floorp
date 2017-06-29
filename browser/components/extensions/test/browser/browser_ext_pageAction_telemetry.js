/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const HISTOGRAM = "WEBEXT_PAGEACTION_POPUP_OPEN_MS";

function histogramCountsSum(histogram) {
  return histogram.snapshot().counts.reduce((a, b) => a + b, 0);
}

add_task(async function testPageActionTelemetry() {
  let extensionOptions = {
    manifest: {
      "page_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },
    background: function() {
      browser.tabs.query({active: true, currentWindow: true}, tabs => {
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
  let extension1 = ExtensionTestUtils.loadExtension(extensionOptions);
  let extension2 = ExtensionTestUtils.loadExtension(extensionOptions);

  let histogram = Services.telemetry.getHistogramById(HISTOGRAM);
  histogram.clear();
  is(histogramCountsSum(histogram), 0,
     `No data recorded for histogram: ${HISTOGRAM}.`);

  await extension1.startup();
  await extension1.awaitMessage("action-shown");
  await extension2.startup();
  await extension2.awaitMessage("action-shown");
  is(histogramCountsSum(histogram), 0,
     `No data recorded for histogram after PageAction shown: ${HISTOGRAM}.`);

  clickPageAction(extension1, window);
  await awaitExtensionPanel(extension1);
  is(histogramCountsSum(histogram), 1,
     `Data recorded for first extension for histogram: ${HISTOGRAM}.`);
  await closePageAction(extension1, window);

  clickPageAction(extension2, window);
  await awaitExtensionPanel(extension2);
  is(histogramCountsSum(histogram), 2,
     `Data recorded for second extension for histogram: ${HISTOGRAM}.`);
  await closePageAction(extension2, window);

  clickPageAction(extension2, window);
  await awaitExtensionPanel(extension2);
  is(histogramCountsSum(histogram), 3,
     `Data recorded for second opening of popup for histogram: ${HISTOGRAM}.`);
  await closePageAction(extension2, window);

  clickPageAction(extension1, window);
  await awaitExtensionPanel(extension1);
  is(histogramCountsSum(histogram), 4,
     `Data recorded for second opening of popup for histogram: ${HISTOGRAM}.`);

  await extension1.unload();
  await extension2.unload();
});
