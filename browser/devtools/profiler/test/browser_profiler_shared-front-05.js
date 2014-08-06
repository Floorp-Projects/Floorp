/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the framerate front is kept recording only when required.
 */

let test = Task.async(function*() {
  let [,, panel] = yield initFrontend(SIMPLE_URL);
  let front = panel.panelWin.gFront;

  let SharedProfilerUtils = devtools.require("devtools/profiler/shared");
  let sharedProfilerConnection = SharedProfilerUtils.getProfilerConnection(panel._toolbox);

  ok(!(yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should not be recording yet.");

  yield front.startRecording();
  ok((yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should now be recording.");

  yield consoleProfile(sharedProfilerConnection, "1");
  ok((yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should still be recording (1).");

  yield front.startRecording();
  ok((yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should still be recording (2).");

  yield consoleProfile(sharedProfilerConnection, "2");
  ok((yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should still be recording (3).");

  yield front.stopRecording();
  ok((yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should still be recording (4).");

  yield consoleProfileEnd(sharedProfilerConnection);
  ok((yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should still be recording (5).");

  yield front.stopRecording();
  ok((yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should still be recording (6).");

  yield consoleProfileEnd(sharedProfilerConnection);
  ok(!(yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should finally have stopped recording.");

  yield teardown(panel);
  finish();
});

function* consoleProfile(connection, label) {
  let notified = connection.once("profile");
  console.profile(label);
  yield notified;
}

function* consoleProfileEnd(connection) {
  let notified = connection.once("profileEnd");
  console.profileEnd();
  yield notified;
}
