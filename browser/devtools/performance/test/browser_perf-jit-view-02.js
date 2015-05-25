/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the JIT Optimizations view does not display information
 * for meta nodes when viewing "content only".
 */

const { CATEGORY_MASK } = devtools.require("devtools/performance/global");
const RecordingUtils = devtools.require("devtools/performance/recording-utils");

Services.prefs.setBoolPref(INVERT_PREF, false);
Services.prefs.setBoolPref(PLATFORM_DATA_PREF, false);

function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, $$, window, PerformanceController } = panel.panelWin;
  let { OverviewView, DetailsView, JITOptimizationsView, JsCallTreeView, RecordingsView } = panel.panelWin;

  let profilerData = { threads: [gThread] };

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

let gUniqueStacks = new RecordingUtils.UniqueStacks();

function uniqStr(s) {
  return gUniqueStacks.getOrAddStringIndex(s);
}

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
      { location: "A (http://foo/bar/baz:12)" }
    ]
  }, {
    time: 5 + 1,
    frames: [
      { location: "(root)" },
      { location: "A (http://foo/bar/baz:12)" },
      { location: "JS", category: CATEGORY_MASK("js") },
    ]
  }],
  markers: []
}, gUniqueStacks);

// 3 RawOptimizationSites
let gRawSite1 = {
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
  line: 22,
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
  case "A (http://foo/bar/baz:12)":
    frame[OPTIMIZATIONS_SLOT] = gRawSite1;
    break;
  case "JS":
    frame[OPTIMIZATIONS_SLOT] = gRawSite2;
    break;
  }
});
