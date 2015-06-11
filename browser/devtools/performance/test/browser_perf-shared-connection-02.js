/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shared PerformanceActorsConnection is only opened once.
 */

let gProfilerConnectionsOpened = 0;
Services.obs.addObserver(profilerConnectionObserver, "performance-tools-connection-opened", false);

function* spawnTest() {
  let { target, panel } = yield initPerformance(SIMPLE_URL);

  is(gProfilerConnectionsOpened, 1,
    "Only one profiler connection was opened.");

  let front = getPerformanceFront(target);

  ok(front, "A front for the current toolbox was retrieved.");
  is(panel.panelWin.gFront, front, "The same front is used by the panel's front");

  yield front.open();

  is(gProfilerConnectionsOpened, 1,
    "No additional profiler connections were opened.");

  yield teardown(panel);
  finish();
}

function profilerConnectionObserver(subject, topic, data) {
  is(topic, "performance-tools-connection-opened", "The correct topic was observed.");
  gProfilerConnectionsOpened++;
}

registerCleanupFunction(() => {
  Services.obs.removeObserver(profilerConnectionObserver, "performance-tools-connection-opened");
});
