/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the recording utility functions work as expected.
 */

const WAIT_TIME = 100; // ms

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { RecordingsListView, RecordingUtils } = panel.panelWin;

  yield startRecording(panel);
  yield idleWait(WAIT_TIME); // allow refresh driver ticks to accumulate
  busyWait(WAIT_TIME); // allow the profiler module to sample more cpu activity
  yield stopRecording(panel, { waitForDisplay: true });

  let recordingData = RecordingsListView.selectedItem.attachment;
  ok(recordingData, "The recording data was successfully retrieved.");

  // Test plotting categories...

  let profilerData = recordingData.profilerData;
  let categoriesData = RecordingUtils.plotCategoriesFor(profilerData, 0, Infinity);

  for (let category of categoriesData) {
    is(Object.keys(category).toSource(), '["delta", "values"]',
      "The correct keys are in the cateogries data.");
    is(typeof category.delta, "number",
      "The delta property is a number, as expected.");
    is(typeof category.values, "object",
      "The values property is a object, as expected.");
    ok("length" in category.values,
      "The values property is an array, as expected.");
  }

  // Test plotting framerate...

  let ticksData = recordingData.ticksData;
  let framerateData = RecordingUtils.plotFramerateFor(ticksData, 0, Infinity);

  ok("length" in framerateData,
    "The framerate data is an array, as expected.");

  for (let rate of framerateData) {
    is(typeof rate.delta, "number",
      "The delta property is a number, as expected.");
    is(typeof rate.value, "number",
      "The value property is a number, as expected.");
  }

  // Test categories and framerate syncing...

  RecordingUtils.syncCategoriesWithFramerate(categoriesData, framerateData);
  info("Total framerate data: " + framerateData.length);
  info("Total categories data: " + categoriesData.length);

  if (framerateData.length >= 2 && categoriesData.length >= 2) {
    is(categoriesData[0].delta, framerateData[0].delta,
      "The categories and framerate data now start at the exact same time.");
    is(categoriesData.pop().delta, framerateData.pop().delta,
      "The categories and framerate data now end at the exact same time.");
  }

  yield teardown(panel);
  finish();
});
