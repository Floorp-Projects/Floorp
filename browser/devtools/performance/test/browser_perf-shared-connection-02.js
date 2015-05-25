/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shared PerformanceActorsConnection is only opened once.
 */

let gProfilerConnectionsOpened = 0;
Services.obs.addObserver(profilerConnectionObserver, "performance-actors-connection-opened", false);

function* spawnTest() {
  let { target, panel } = yield initPerformance(SIMPLE_URL);

  is(gProfilerConnectionsOpened, 1,
    "Only one profiler connection was opened.");

  let sharedConnection = getPerformanceActorsConnection(target);

  ok(sharedConnection,
    "A shared profiler connection for the current toolbox was retrieved.");
  is(panel.panelWin.gFront._connection, sharedConnection,
    "The same shared profiler connection is used by the panel's front.");

  yield sharedConnection.open();

  is(gProfilerConnectionsOpened, 1,
    "No additional profiler connections were opened.");

  yield teardown(panel);
  finish();
}

function profilerConnectionObserver(subject, topic, data) {
  is(topic, "performance-actors-connection-opened", "The correct topic was observed.");
  gProfilerConnectionsOpened++;
}

registerCleanupFunction(() => {
  Services.obs.removeObserver(profilerConnectionObserver, "performance-actors-connection-opened");
});
