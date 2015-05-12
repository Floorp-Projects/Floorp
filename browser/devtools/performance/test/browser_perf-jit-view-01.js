/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the JIT Optimizations view renders optimization data
 * if on, and displays selected frames on focus.
 */

const { RecordingUtils } = devtools.require("devtools/performance/recording-utils");

Services.prefs.setBoolPref(INVERT_PREF, false);

function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, $$, window, PerformanceController } = panel.panelWin;
  let { OverviewView, DetailsView, JITOptimizationsView, JsCallTreeView, RecordingsView } = panel.panelWin;

  let profilerData = { threads: [gThread] }

  is(Services.prefs.getBoolPref(JIT_PREF), false, "show JIT Optimizations pref off by default");

  // Make two recordings, so we have one to switch to later, as the
  // second one will have fake sample data
  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);
  yield stopRecording(panel);

  yield DetailsView.selectView("js-calltree");

  yield injectAndRenderProfilerData();

  // gRawSite1 and gRawSite2 are both optimizations on A, so they'll have
  // indices in descending order of # of samples.
  yield checkFrame(1, [{ i: 0, opt: gRawSite1 }, { i: 1, opt: gRawSite2 }]);

  // gRawSite3 is the only optimization on B, so it'll have index 0.
  yield checkFrame(2, [{ i: 0, opt: gRawSite3 }]);
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

  function *checkFrame (frameIndex, expectedOpts=[]) {
    // Click the frame
    let rendered = once(JITOptimizationsView, EVENTS.OPTIMIZATIONS_RENDERED);
    mousedown(window, $$(".call-tree-item")[frameIndex]);
    Services.prefs.setBoolPref(JIT_PREF, true);
    yield rendered;
    ok(true, "JITOptimizationsView rendered when enabling with the current frame node selected");

    let isEmpty = $("#jit-optimizations-view").classList.contains("empty");
    if (expectedOpts.length === 0) {
      ok(isEmpty, "JIT Optimizations view has an empty message when selecting a frame without opt data.");
      return;
    } else {
      ok(!isEmpty, "JIT Optimizations view has no empty message.");
    }

    // Get the frame info for the first opt site, since all opt sites
    // share the same frame info
    let frameInfo = expectedOpts[0].opt._testFrameInfo;

    let { $headerName, $headerLine, $headerFile } = JITOptimizationsView;
    ok(!$headerName.hidden, "header function name should be shown");
    ok(!$headerLine.hidden, "header line should be shown");
    ok(!$headerFile.hidden, "header file should be shown");
    is($headerName.textContent, frameInfo.name, "correct header function name.");
    is($headerLine.textContent, frameInfo.line, "correct header line");
    is($headerFile.textContent, frameInfo.file, "correct header file");

    // Need the value of the optimizations in its array, as its
    // an index used internally by the view to uniquely ID the opt
    for (let { i, opt } of expectedOpts) {
      let { types: ionTypes, attempts } = opt;

      // Check attempts
      is($$(`.tree-widget-container li[data-id='["${i}","${i}-attempts"]'] .tree-widget-children .tree-widget-item`).length, attempts.data.length,
        `found ${attempts.data.length} attempts`);

      for (let j = 0; j < ionTypes.length; j++) {
        ok($(`.tree-widget-container li[data-id='["${i}","${i}-types","${i}-types-${j}"]']`),
          "found an ion type row");
      }

      // The second and third optimization should display optimization failures.
      let warningIcon = $(`.tree-widget-container li[data-id='["${i}"]'] .opt-icon[severity=warning]`);
      if (opt === gRawSite2 || opt === gRawSite3) {
        ok(warningIcon, "did find a warning icon for all strategies failing.");
      } else {
        ok(!warningIcon, "did not find a warning icon for no successful strategies");
      }
    }
  }
}

let gUniqueStacks = new RecordingUtils.UniqueStacks();

function uniqStr(s) {
  return gUniqueStacks.getOrAddStringIndex(s);
}

// Since deflateThread doesn't handle deflating optimization info, use
// placeholder names A_O1, B_O3, and A_O2, which will be used to manually
// splice deduped opts into the profile.
let gThread = RecordingUtils.deflateThread({
  samples: [{
    time: 0,
    frames: [
      { location: "(root)" }
    ]
  }, {
    time: 5,
    frames: [
      { location: "(root)" },
      { location: "A_O1" },
      { location: "B_O3" },
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
      { location: "A_O2" },
      { location: "B (http://foo/bar/boo:34)" },
    ]
  }, {
    time: 5 + 1 + 2 + 7,
    frames: [
      { location: "(root)" },
      { location: "A_O1" },
      { location: "E (http://foo/bar/baz:90)" },
      { location: "F (http://foo/bar/baz:99)" }
    ]
  }],
  markers: []
}, gUniqueStacks);

// 3 RawOptimizationSites
let gRawSite1 = {
  _testFrameInfo: { name: "A", line: "12", file: "@baz" },
  line: 12,
  column: 2,
  types: [{
    mirType: uniqStr("Object"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
        keyedBy: uniqStr("constructor"),
        name: uniqStr("Foo"),
        location: uniqStr("A (http://foo/bar/baz:12)")
    }, {
        keyedBy: uniqStr("primitive"),
        location: uniqStr("self-hosted")
    }]
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("Inlined"), uniqStr("SomeGetter3")]
    ]
  }
};

let gRawSite2 = {
  _testFrameInfo: { name: "A", line: "12", file: "@baz" },
  line: 12,
  types: [{
    mirType: uniqStr("Int32"),
    site: uniqStr("Receiver")
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("Failure3"), uniqStr("SomeGetter3")]
    ]
  }
};

let gRawSite3 = {
  _testFrameInfo: { name: "B", line: "34", file: "@boo" },
  line: 34,
  types: [{
    mirType: uniqStr("Int32"),
    site: uniqStr("Receiver")
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("Failure3"), uniqStr("SomeGetter3")]
    ]
  }
};

gThread.frameTable.data.forEach((frame) => {
  const LOCATION_SLOT = gThread.frameTable.schema.location;
  const OPTIMIZATIONS_SLOT = gThread.frameTable.schema.optimizations;

  let l = gThread.stringTable[frame[LOCATION_SLOT]];
  switch (l) {
  case "A_O1":
    frame[LOCATION_SLOT] = uniqStr("A (http://foo/bar/baz:12)");
    frame[OPTIMIZATIONS_SLOT] = gRawSite1;
    break;
  case "A_O2":
    frame[LOCATION_SLOT] = uniqStr("A (http://foo/bar/baz:12)");
    frame[OPTIMIZATIONS_SLOT] = gRawSite2;
    break;
  case "B_O3":
    frame[LOCATION_SLOT] = uniqStr("B (http://foo/bar/boo:34)");
    frame[OPTIMIZATIONS_SLOT] = gRawSite3;
    break;
  }
});
