/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Tests if the performance tool gracefully handles loading bogus files.
 */

var test = Task.async(function* () {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController } = panel.panelWin;

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

  try {
    yield PerformanceController.importRecording("", file);
    ok(false, "The recording succeeded unexpectedly.");
  } catch (e) {
    is(e.message, "Could not read recording data file.", "Message is correct.");
    ok(true, "The recording was cancelled.");
  }

  yield teardown(panel);
  finish();
});
