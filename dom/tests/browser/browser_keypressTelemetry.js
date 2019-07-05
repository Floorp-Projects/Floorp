/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EventUtils = {};
var PaintListener = {};
Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
  EventUtils
);
const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);

function getRecordedKeypressCount() {
  let snapshot = Services.telemetry.getSnapshotForHistograms("main", false);

  var totalCount = 0;
  for (var prop in snapshot) {
    if (snapshot[prop].KEYPRESS_PRESENT_LATENCY) {
      dump("found snapshot");
      totalCount += Object.values(
        snapshot[prop].KEYPRESS_PRESENT_LATENCY.values
      ).reduce((a, b) => a + b, 0);
    }
  }

  return totalCount;
}

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["toolkit.telemetry.ipcBatchTimeout", 10]],
  });
  let histogram = Services.telemetry.getHistogramById(
    "KEYPRESS_PRESENT_LATENCY"
  );
  histogram.clear();

  waitForExplicitFinish();

  gURLBar.focus();
  await SimpleTest.promiseFocus(window);
  EventUtils.sendChar("x");

  await ContentTaskUtils.waitForCondition(
    () => {
      return getRecordedKeypressCount() > 0;
    },
    "waiting for telemetry",
    200,
    600
  );
  let result = getRecordedKeypressCount();
  ok(result == 1, "One keypress recorded");

  gURLBar.focus();
  await SimpleTest.promiseFocus(window);
  EventUtils.sendChar("x");

  await ContentTaskUtils.waitForCondition(
    () => {
      return getRecordedKeypressCount() > 1;
    },
    "waiting for telemetry",
    200,
    600
  );
  result = getRecordedKeypressCount();
  ok(result == 2, "Two keypresses recorded");
});
