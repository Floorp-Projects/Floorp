/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the call tree up/down events work for js calltree and memory calltree.
 */
const { ThreadNode } = devtools.require("devtools/performance/tree-model");
const RecordingUtils = devtools.require("devtools/performance/recording-utils")

function* spawnTest() {
  let focus = 0;
  let focusEvent = () => focus++;

  Services.prefs.setBoolPref(MEMORY_PREF, true);

  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, DetailsView, JsCallTreeView, MemoryCallTreeView } = panel.panelWin;

  yield DetailsView.selectView("js-calltree");
  ok(DetailsView.isViewSelected(JsCallTreeView), "The call tree is now selected.");

  // Make a recording just so the performance tool is in the correct state
  yield startRecording(panel);
  let rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  // Mock the profile used so we can get a deterministic tree created
  let threadNode = new ThreadNode(gProfile.threads[0]);
  JsCallTreeView._populateCallTree(threadNode);
  JsCallTreeView.emit(EVENTS.JS_CALL_TREE_RENDERED);

  JsCallTreeView.on("focus", focusEvent);

  click(panel.panelWin, $("#js-calltree-view .call-tree-item"));
  fireKey("VK_DOWN");
  fireKey("VK_DOWN");
  fireKey("VK_DOWN");
  fireKey("VK_DOWN");

  JsCallTreeView.off("focus", focusEvent);

  is(focus, 4, "several focus events are fired for the js calltree.");

  yield teardown(panel);
  finish();
};

let gProfile = {
  meta: { version: 2 },
  threads: [{
    samples: [{
      time: 5,
      frames: [
        { category: 8,  location: "(root)" },
        { category: 8,  location: "A (http://foo/bar/baz:12)" },
        { category: 16, location: "B (http://foo/bar/baz:34)" },
        { category: 32, location: "C (http://foo/bar/baz:56)" }
      ]
    }, {
      time: 5 + 1,
      frames: [
        { category: 8,  location: "(root)" },
        { category: 8,  location: "A (http://foo/bar/baz:12)" },
        { category: 16, location: "B (http://foo/bar/baz:34)" },
        { category: 64, location: "D (http://foo/bar/baz:78)" }
      ]
    }, {
      time: 5 + 1 + 2,
      frames: [
        { category: 8,  location: "(root)" },
        { category: 8,  location: "A (http://foo/bar/baz:12)" },
        { category: 16, location: "B (http://foo/bar/baz:34)" },
        { category: 64, location: "D (http://foo/bar/baz:78)" }
      ]
    }, {
      time: 5 + 1 + 2 + 7,
      frames: [
        { category: 8,   location: "(root)" },
        { category: 8,   location: "A (http://foo/bar/baz:12)" },
        { category: 128, location: "E (http://foo/bar/baz:90)" },
        { category: 256, location: "F (http://foo/bar/baz:99)" }
      ]
    }],
    markers: []
  }]
};

RecordingUtils.deflateProfile(gProfile);
