/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
// Bug 1235788, increase time out of this test
requestLongerTimeout(2);

/**
 * Tests that the JIT Optimizations view renders optimization data
 * if on, and displays selected frames on focus.
 */

Services.prefs.setBoolPref(INVERT_PREF, false);

function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, $$, window, PerformanceController } = panel.panelWin;
  let { OverviewView, DetailsView, OptimizationsListView, JsCallTreeView, RecordingsView } = panel.panelWin;

  let profilerData = { threads: [gThread] };

  is(Services.prefs.getBoolPref(JIT_PREF), false, "record JIT Optimizations pref off by default");
  Services.prefs.setBoolPref(JIT_PREF, true);
  is(Services.prefs.getBoolPref(JIT_PREF), true, "toggle on record JIT Optimizations");

  // Make two recordings, so we have one to switch to later, as the
  // second one will have fake sample data
  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);
  yield stopRecording(panel);

  yield DetailsView.selectView("js-calltree");

  yield injectAndRenderProfilerData();

  is($("#jit-optimizations-view").classList.contains("hidden"), true,
    "JIT Optimizations should be hidden when pref is on, but no frame selected");

  // A is never a leaf, so it's optimizations should not be shown.
  yield checkFrame(1);

  // gRawSite2 and gRawSite3 are both optimizations on B, so they'll have
  // indices in descending order of # of samples.
  yield checkFrame(2, true);

  // Leaf node (C) with no optimizations should not display any opts.
  yield checkFrame(3);

  // Select the node with optimizations and change to a new recording
  // to ensure the opts view is cleared
  let rendered = once(JsCallTreeView, "focus");
  mousedown(window, $$(".call-tree-item")[2]);
  yield rendered;
  let isHidden = $("#jit-optimizations-view").classList.contains("hidden");
  ok(!isHidden, "opts view should be visible when selecting a frame with opts");

  let select = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  RecordingsView.selectedIndex = 0;
  yield Promise.all([select, rendered]);

  isHidden = $("#jit-optimizations-view").classList.contains("hidden");
  ok(isHidden, "opts view is hidden when switching recordings");

  rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  RecordingsView.selectedIndex = 1;
  yield rendered;

  rendered = once(JsCallTreeView, "focus");
  mousedown(window, $$(".call-tree-item")[2]);
  yield rendered;
  isHidden = $("#jit-optimizations-view").classList.contains("hidden");
  ok(!isHidden, "opts view should be visible when selecting a frame with opts");

  rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  Services.prefs.setBoolPref(JIT_PREF, false);
  yield rendered;
  ok(true, "call tree rerendered when JIT pref changes");
  isHidden = $("#jit-optimizations-view").classList.contains("hidden");
  ok(isHidden, "opts view hidden when toggling off jit pref");

  rendered = once(JsCallTreeView, "focus");
  mousedown(window, $$(".call-tree-item")[2]);
  yield rendered;
  isHidden = $("#jit-optimizations-view").classList.contains("hidden");
  ok(isHidden, "opts view hidden when jit pref off and selecting a frame with opts");

  yield teardown(panel);
  finish();

  function* injectAndRenderProfilerData() {
    // Get current recording and inject our mock data
    info("Injecting mock profile data");
    let recording = PerformanceController.getCurrentRecording();
    recording._profile = profilerData;

    // Force a rerender
    let rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
    JsCallTreeView.render(OverviewView.getTimeInterval());
    yield rendered;
  }

  function* checkFrame(frameIndex, hasOpts) {
    info(`Checking frame ${frameIndex}`);
    // Click the frame
    let rendered = once(JsCallTreeView, "focus");
    mousedown(window, $$(".call-tree-item")[frameIndex]);
    yield rendered;

    let isHidden = $("#jit-optimizations-view").classList.contains("hidden");
    if (hasOpts) {
      ok(!isHidden, "JIT Optimizations view is not hidden if current frame has opts.");
    } else {
      ok(isHidden, "JIT Optimizations view is hidden if current frame does not have opts");
    }
  }
}

var gUniqueStacks = new RecordingUtils.UniqueStacks();

function uniqStr(s) {
  return gUniqueStacks.getOrAddStringIndex(s);
}

// Since deflateThread doesn't handle deflating optimization info, use
// placeholder names A_O1, B_O2, and B_O3, which will be used to manually
// splice deduped opts into the profile.
var gThread = RecordingUtils.deflateThread({
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
      { location: "B_O2" },
      { location: "C (http://foo/bar/baz:56)" }
    ]
  }, {
    time: 5 + 1,
    frames: [
      { location: "(root)" },
      { location: "A (http://foo/bar/baz:12)" },
      { location: "B_O2" },
    ]
  }, {
    time: 5 + 1 + 2,
    frames: [
      { location: "(root)" },
      { location: "A_O1" },
      { location: "B_O3" },
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
var gRawSite1 = {
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
      [uniqStr("Failure3"), uniqStr("SomeGetter3")]
    ]
  }
};

var gRawSite2 = {
  _testFrameInfo: { name: "B", line: "10", file: "@boo" },
  line: 40,
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
      [uniqStr("Inlined"), uniqStr("SomeGetter3")]
    ]
  }
};

var gRawSite3 = {
  _testFrameInfo: { name: "B", line: "10", file: "@boo" },
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
    case "B_O2":
      frame[LOCATION_SLOT] = uniqStr("B (http://foo/bar/boo:10)");
      frame[OPTIMIZATIONS_SLOT] = gRawSite2;
      break;
    case "B_O3":
      frame[LOCATION_SLOT] = uniqStr("B (http://foo/bar/boo:10)");
      frame[OPTIMIZATIONS_SLOT] = gRawSite3;
      break;
  }
});
/* eslint-enable */
