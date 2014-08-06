/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if there's only one shared profiler connection instance per toolbox.
 */

let gProfilerConnections = 0;
Services.obs.addObserver(profilerConnectionObserver, "profiler-connection-created", false);

let test = Task.async(function*() {
  let firstTab = yield addTab(SIMPLE_URL);
  let firstTarget = TargetFactory.forTab(firstTab);
  yield firstTarget.makeRemote();

  yield gDevTools.showToolbox(firstTarget, "webconsole");
  is(gProfilerConnections, 1,
    "A shared profiler connection should have been created.");

  yield gDevTools.showToolbox(firstTarget, "jsprofiler");
  is(gProfilerConnections, 1,
    "No new profiler connections should have been created.");

  let secondTab = yield addTab(SIMPLE_URL);
  let secondTarget = TargetFactory.forTab(secondTab);
  yield secondTarget.makeRemote();

  yield gDevTools.showToolbox(secondTarget, "jsprofiler");
  is(gProfilerConnections, 2,
    "Only one new profiler connection should have been created.");

  yield removeTab(firstTab);
  yield removeTab(secondTab);

  finish();
});

function profilerConnectionObserver(subject, topic, data) {
  is(topic, "profiler-connection-created", "The correct topic was observed.");
  gProfilerConnections++;
}

registerCleanupFunction(() => {
  Services.obs.removeObserver(profilerConnectionObserver, "profiler-connection-created");
});
