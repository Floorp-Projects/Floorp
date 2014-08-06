/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shared profiler connection is only opened once.
 */

let gProfilerConnectionsOpened = 0;
Services.obs.addObserver(profilerConnectionObserver, "profiler-connection-opened", false);

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);

  is(gProfilerConnectionsOpened, 1,
    "Only one profiler connection was opened.");

  let SharedProfilerUtils = devtools.require("devtools/profiler/shared");
  let sharedProfilerConnection = SharedProfilerUtils.getProfilerConnection(panel._toolbox);

  ok(sharedProfilerConnection,
    "A shared profiler connection for the current toolbox was retrieved.");
  is(sharedProfilerConnection._request, panel.panelWin.gFront._request,
    "The same shared profiler connection is used by the panel's front.");

  ok(sharedProfilerConnection._target,
    "A target exists for the current profiler connection.");
  ok(sharedProfilerConnection._client,
    "A target exists for the current profiler connection.");
  is(sharedProfilerConnection._pendingConsoleRecordings.length, 0,
    "There shouldn't be any pending console recordings yet.");
  is(sharedProfilerConnection._finishedConsoleRecordings.length, 0,
    "There shouldn't be any finished console recordings yet.");

  yield sharedProfilerConnection.open();
  is(gProfilerConnectionsOpened, 1,
    "No additional profiler connections were opened.");

  yield teardown(panel);
  finish();
});

function profilerConnectionObserver(subject, topic, data) {
  is(topic, "profiler-connection-opened", "The correct topic was observed.");
  gProfilerConnectionsOpened++;
}

registerCleanupFunction(() => {
  Services.obs.removeObserver(profilerConnectionObserver, "profiler-connection-opened");
});
