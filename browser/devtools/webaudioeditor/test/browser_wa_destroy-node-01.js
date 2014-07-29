/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the destruction node event is fired and that the nodes are no
 * longer stored internally in the tool, that the graph is updated properly, and
 * that selecting a soon-to-be dead node clears the inspector.
 *
 * All done in one test since this test takes a few seconds to clear GC.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(DESTROY_NODES_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  let started = once(gFront, "start-context");

  reload(target);

  let destroyed = getN(panelWin, EVENTS.DESTROY_NODE, 10);

  forceCC();

  let [created] = yield Promise.all([
    getNSpread(panelWin, EVENTS.CREATE_NODE, 13),
    waitForGraphRendered(panelWin, 13, 2)
  ]);

  // Since CREATE_NODE emits several arguments (eventName and actorID), let's
  // flatten it to just the actorIDs
  let actorIDs = created.map(ev => ev[1]);

  // Click a soon-to-be dead buffer node
  yield clickGraphNode(panelWin, actorIDs[5]);

  forceCC();

  // Wait for destruction and graph to re-render
  yield Promise.all([destroyed, waitForGraphRendered(panelWin, 3, 2)]);

  // Test internal storage
  is(panelWin.AudioNodes.length, 3, "All nodes should be GC'd except one gain, osc and dest node.");

  // Test graph rendering
  ok(findGraphNode(panelWin, actorIDs[0]), "dest should be in graph");
  ok(findGraphNode(panelWin, actorIDs[1]), "osc should be in graph");
  ok(findGraphNode(panelWin, actorIDs[2]), "gain should be in graph");

  let { nodes, edges } = countGraphObjects(panelWin);

  is(nodes, 3, "Only 3 nodes rendered in graph.");
  is(edges, 2, "Only 2 edges rendered in graph.");

  // Test that the inspector reset to no node selected
  ok(isVisible($("#web-audio-editor-details-pane-empty")),
    "InspectorView empty message should show if the currently selected node gets collected.");

  yield teardown(panel);
  finish();
}

