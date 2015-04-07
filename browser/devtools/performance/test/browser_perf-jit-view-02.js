/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the JIT Optimizations view does not display information
 * for meta nodes when viewing "content only".
 */

let { CATEGORY_MASK } = devtools.require("devtools/shared/profiler/global");
Services.prefs.setBoolPref(INVERT_PREF, false);
Services.prefs.setBoolPref(PLATFORM_DATA_PREF, false);

function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, $$, window, PerformanceController } = panel.panelWin;
  let { OverviewView, DetailsView, JITOptimizationsView, JsCallTreeView, RecordingsView } = panel.panelWin;

  let profilerData = { threads: [{samples: gSamples, optimizations: gOpts}] };

  is(Services.prefs.getBoolPref(JIT_PREF), false, "show JIT Optimizations pref off by default");

  // Make two recordings, so we have one to switch to later, as the
  // second one will have fake sample data
  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);
  yield stopRecording(panel);

  yield DetailsView.selectView("js-calltree");

  yield injectAndRenderProfilerData();

  Services.prefs.setBoolPref(JIT_PREF, true);
  // Click the frame
  let rendered = once(JITOptimizationsView, EVENTS.OPTIMIZATIONS_RENDERED);
  mousedown(window, $$(".call-tree-item")[2]);
  yield rendered;

  ok($("#jit-optimizations-view").classList.contains("empty"),
    "platform meta frame shows as empty");

  let { $headerName, $headerLine, $headerFile } = JITOptimizationsView;
  ok(!$headerName.hidden, "header function name should be shown");
  ok($headerLine.hidden, "header line should be hidden");
  ok($headerFile.hidden, "header file should be hidden");
  is($headerName.textContent, "JIT", "correct header function name.");
  is($headerLine.textContent, "", "correct header line (empty string).");
  is($headerFile.textContent, "", "correct header file (empty string).");

  yield teardown(panel);
  finish();

  function *injectAndRenderProfilerData() {
    // Get current recording and inject our mock data
    info("Injecting mock profile data");
    let recording = PerformanceController.getCurrentRecording();
    recording._profile = profilerData;

    // Force a rerender
    let rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
    JsCallTreeView.render();
    yield rendered;

    Services.prefs.setBoolPref(JIT_PREF, true);
    ok($("#jit-optimizations-view").classList.contains("empty"),
      "JIT Optimizations view has empty message when no frames selected.");

     Services.prefs.setBoolPref(JIT_PREF, false);
  }
}

let gSamples = [{
  time: 5,
  frames: [
    { location: "(root)" },
    { location: "A (http://foo/bar/baz:12)", optsIndex: 0 }
  ]
}, {
  time: 5 + 1,
  frames: [
    { location: "(root)" },
    { location: "A (http://foo/bar/baz:12)", optsIndex: 0 },
    { location: "JS", optsIndex: 1, category: CATEGORY_MASK("js") },
  ]
}];

// Array of OptimizationSites
let gOpts = [{
  line: 12,
  column: 2,
  types: [{ mirType: "Object", site: "A (http://foo/bar/bar:12)", types: [
    { keyedBy: "constructor", name: "Foo", location: "A (http://foo/bar/baz:12)" },
    { keyedBy: "primitive", location: "self-hosted" }
  ]}],
  attempts: [
    { outcome: "Failure1", strategy: "SomeGetter1" },
    { outcome: "Failure2", strategy: "SomeGetter2" },
    { outcome: "Inlined", strategy: "SomeGetter3" },
  ]
}, {
  line: 22,
  types: [{ mirType: "Int32", site: "Receiver" }], // use no types
  attempts: [
    { outcome: "Failure1", strategy: "SomeGetter1" },
    { outcome: "Failure2", strategy: "SomeGetter2" },
    { outcome: "Failure3", strategy: "SomeGetter3" },
  ]
}];
