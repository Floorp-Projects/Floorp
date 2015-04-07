/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the JIT Optimizations view renders optimization data
 * if on, and displays selected frames on focus.
 */

Services.prefs.setBoolPref(INVERT_PREF, false);

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

  yield checkFrame(1, [0, 1]);
  yield checkFrame(2, [2]);
  yield checkFrame(3);

  let select = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  let reset = once(JITOptimizationsView, EVENTS.OPTIMIZATIONS_RESET);
  RecordingsView.selectedIndex = 0;
  yield Promise.all([select, reset]);
  ok(true, "JITOptimizations view correctly reset when switching recordings.");

  yield teardown(panel);
  finish();

  function *injectAndRenderProfilerData() {
    // Get current recording and inject our mock data
    info("Injecting mock profile data");
    let recording = PerformanceController.getCurrentRecording();
    recording._profile = profilerData;

    is($("#jit-optimizations-view").hidden, true, "JIT Optimizations panel is hidden when pref off.");

    // Force a rerender
    let rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
    JsCallTreeView.render();
    yield rendered;

    is($("#jit-optimizations-view").hidden, true, "JIT Optimizations panel still hidden when rerendered");
    Services.prefs.setBoolPref(JIT_PREF, true);
    is($("#jit-optimizations-view").hidden, false, "JIT Optimizations should be visible when pref is on");
    ok($("#jit-optimizations-view").classList.contains("empty"),
      "JIT Optimizations view has empty message when no frames selected.");

     Services.prefs.setBoolPref(JIT_PREF, false);
  }

  function *checkFrame (frameIndex, expectedOptsIndex=[]) {
    // Click the frame
    let rendered = once(JITOptimizationsView, EVENTS.OPTIMIZATIONS_RENDERED);
    mousedown(window, $$(".call-tree-item")[frameIndex]);
    Services.prefs.setBoolPref(JIT_PREF, true);
    yield rendered;
    ok(true, "JITOptimizationsView rendered when enabling with the current frame node selected");

    let isEmpty = $("#jit-optimizations-view").classList.contains("empty");
    if (expectedOptsIndex.length === 0) {
      ok(isEmpty, "JIT Optimizations view has an empty message when selecting a frame without opt data.");
      return;
    } else {
      ok(!isEmpty, "JIT Optimizations view has no empty message.");
    }

    // Get the frame info for the first opt site, since all opt sites
    // share the same frame info
    let frameInfo = gOpts[expectedOptsIndex[0]]._testFrameInfo;

    let { $headerName, $headerLine, $headerFile } = JITOptimizationsView;
    ok(!$headerName.hidden, "header function name should be shown");
    ok(!$headerLine.hidden, "header line should be shown");
    ok(!$headerFile.hidden, "header file should be shown");
    is($headerName.textContent, frameInfo.name, "correct header function name.");
    is($headerLine.textContent, frameInfo.line, "correct header line");
    is($headerFile.textContent, frameInfo.file, "correct header file");

    // Need the value of the optimizations in its array, as its
    // an index used internally by the view to uniquely ID the opt
    for (let i of expectedOptsIndex) {
      let opt = gOpts[i];
      let { types: ionTypes, attempts } = opt;

      // Check attempts
      is($$(`.tree-widget-container li[data-id='["${i}","${i}-attempts"]'] .tree-widget-children .tree-widget-item`).length, attempts.length,
        `found ${attempts.length} attempts`);

      for (let j = 0; j < ionTypes.length; j++) {
        ok($(`.tree-widget-container li[data-id='["${i}","${i}-types","${i}-types-${j}"]']`),
          "found an ion type row");
      }

      // The second and third optimization should display optimization failures.
      let warningIcon = $(`.tree-widget-container li[data-id='["${i}"]'] .opt-icon[severity=warning]`);
      if (i === 1 || i === 2) {
        ok(warningIcon, "did find a warning icon for all strategies failing.");
      } else {
        ok(!warningIcon, "did not find a warning icon for no successful strategies");
      }
    }
  }
}

let gSamples = [{
  time: 5,
  frames: [
    { location: "(root)" },
    { location: "A (http://foo/bar/baz:12)", optsIndex: 0 },
    { location: "B (http://foo/bar/boo:34)", optsIndex: 2 },
    { location: "C (http://foo/bar/baz:56)" }
  ]
}, {
  time: 5 + 1,
  frames: [
    { location: "(root)" },
    { location: "A (http://foo/bar/baz:12)" },
    { location: "B (http://foo/bar/boo:34)" },
  ]
}, {
  time: 5 + 1 + 2,
  frames: [
    { location: "(root)" },
    { location: "A (http://foo/bar/baz:12)", optsIndex: 1 },
    { location: "B (http://foo/bar/boo:34)" },
  ]
}, {
  time: 5 + 1 + 2 + 7,
  frames: [
    { location: "(root)" },
    { location: "A (http://foo/bar/baz:12)", optsIndex: 0 },
    { location: "E (http://foo/bar/baz:90)" },
    { location: "F (http://foo/bar/baz:99)" }
  ]
}];

// Array of OptimizationSites
let gOpts = [{
  _testFrameInfo: { name: "A", line: "12", file: "@baz" },
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
  _testFrameInfo: { name: "A", line: "12", file: "@baz" },
  line: 12,
  types: [{ mirType: "Int32", site: "Receiver" }], // use no types
  attempts: [
    { outcome: "Failure1", strategy: "SomeGetter1" },
    { outcome: "Failure2", strategy: "SomeGetter2" },
    { outcome: "Failure3", strategy: "SomeGetter3" },
  ]
}, {
  _testFrameInfo: { name: "B", line: "34", file: "@boo" },
  line: 34,
  types: [{ mirType: "Int32", site: "Receiver" }], // use no types
  attempts: [
    { outcome: "Failure1", strategy: "SomeGetter1" },
    { outcome: "Failure2", strategy: "SomeGetter2" },
    { outcome: "Failure3", strategy: "SomeGetter3" },
  ]
}];
