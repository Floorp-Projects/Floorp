/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Tests if the performance tool gracefully handles loading bogus files.
 */

var test = async function () {
  let { target, panel, toolbox } = await initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController } = panel.panelWin;

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

  try {
    await PerformanceController.importRecording("", file);
    ok(false, "The recording succeeded unexpectedly.");
  } catch (e) {
    is(e.message, "Could not read recording data file.", "Message is correct.");
    ok(true, "The recording was cancelled.");
  }

  await teardown(panel);
  finish();
};
