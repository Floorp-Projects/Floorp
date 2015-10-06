/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler's tree view implementation works properly and
 * correctly emits events when certain DOM nodes are clicked.
 */

function* spawnTest() {
  let { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
  let { CallView } = require("devtools/client/performance/modules/widgets/tree-view");

  let threadNode = new ThreadNode(gThread, { startTime: 0, endTime: 20 });
  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;
  let treeRoot = new CallView({ frame: threadNode });

  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  let A = treeRoot.getChild();
  let B = A.getChild();
  let D = B.getChild();

  let linkEvent = null;
  let handler = (_, e) => linkEvent = e;

  treeRoot.on("link", handler);

  // Fire the right click
  EventUtils.synthesizeMouseAtCenter(D.target.querySelector(".call-tree-url"), { button: 2, type: "mousedown" }, window);

  // Wait 200ms
  yield idleWait(50);

  // Ensure link was not called for right click
  ok(!linkEvent, "`link` event not fired for right click")

  // Fire left click
  EventUtils.sendMouseEvent({ type: "mousedown" }, D.target.querySelector(".call-tree-url"));

  // Wait until linkEvent is true, should occur quickly, if not synchronously
  yield waitUntil(() => linkEvent);
  is(linkEvent, D, "The 'link' event target is correct.");

  treeRoot.off("link", handler);
  finish();
}

var gThread = synthesizeProfileForTest([{
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
}]);
