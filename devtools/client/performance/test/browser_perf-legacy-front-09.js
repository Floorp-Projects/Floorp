/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that when using an older server (< Fx40) where the profiler actor does not
 * have the `getBufferInfo` method that nothing breaks and RecordingModels have null
 * `getBufferUsage()` values.
 */

const { LegacyPerformanceFront } = require("devtools/client/performance/legacy/front");

function* spawnTest() {
  let tab = yield getTab(SIMPLE_URL);
  let target = TargetFactory.forTab(tab);
  yield target.makeRemote();

  merge(target, {
    TEST_PROFILER_FILTER_STATUS: ["position", "totalSize", "generation"],
    TEST_PERFORMANCE_LEGACY_FRONT: true,
  });

  let front = new LegacyPerformanceFront(target);
  yield front.connect();
  front.setProfilerStatusInterval(10);

  front.on("profiler-status", () => ok(false, "profiler-status should not be called when not supported"));
  let model = yield front.startRecording();

  yield busyWait(100);
  is(front.getBufferUsageForRecording(model), null, "buffer usage for recording should be null");

  yield front.stopRecording(model);
  yield front.destroy();
  gBrowser.removeCurrentTab();
}

function getTab (url) {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let loaded = once(gBrowser.selectedBrowser, "load", true);

  content.location = url;
  return loaded.then(() => {
    return new Promise(resolve => {
      let isBlank = url == "about:blank";
      waitForFocus(() => resolve(tab), content, isBlank);
    });
  });
}
