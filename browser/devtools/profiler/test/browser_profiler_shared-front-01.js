/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler connection front relays console notifications.
 */

let test = Task.async(function*() {
  // This test seems to be a bit slow on debug builds.
  requestLongerTimeout(3);

  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let front = panel.panelWin.gFront;

  let SharedProfilerUtils = devtools.require("devtools/profiler/shared");
  let sharedProfilerConnection = SharedProfilerUtils.getProfilerConnection(panel._toolbox);

  let stackSize = 0;
  front.on("profile", () => stackSize++);
  front.on("profileEnd", () => stackSize--);

  for (let i = 0; i < 10; i++) {
    yield consoleProfile(sharedProfilerConnection, i);
    is(stackSize, i + 1,
      "The current stack size is correctly: " + (i + 1));
    is(front.pendingConsoleRecordings.length, i + 1,
      "The publicly exposed pending recordings array has the correct size.");
  }
  for (let i = 9; i >= 0; i--) {
    yield consoleProfileEnd(sharedProfilerConnection);
    is(stackSize, i,
      "The current stack size is correctly: " + i);
    is(front.pendingConsoleRecordings.length, i,
      "The publicly exposed pending recordings array has the correct size.");
    is(front.finishedConsoleRecordings.length, 10 - i,
      "The publicly exposed finished recordings array has the correct size.");
  }

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
