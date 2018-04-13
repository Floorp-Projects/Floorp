/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the destruction node event is fired and that the nodes are no
 * longer stored internally in the tool, that the graph is updated properly, and
 * that selecting a soon-to-be dead node clears the inspector.
 *
 * All done in one test since this test takes a few seconds to clear GC.
 */

add_task(async function() {
  let { target, panel } = await initWebAudioEditor(DESTROY_NODES_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, gAudioNodes } = panelWin;

  let started = once(gFront, "start-context");

  let events = Promise.all([
    getNSpread(gAudioNodes, "add", 13),
    waitForGraphRendered(panelWin, 13, 2)
  ]);
  reload(target);
  let [created] = await events;

  // Flatten arrays of event arguments and take the first (AudioNodeModel)
  // and get its ID.
  let actorIDs = created.map(ev => ev[0].id);

  // Click a soon-to-be dead buffer node
  await clickGraphNode(panelWin, actorIDs[5]);

  let destroyed = getN(gAudioNodes, "remove", 10);

  // Force a CC in the child process to collect the orphaned nodes.
  forceNodeCollection();

  // Wait for destruction and graph to re-render
  await Promise.all([destroyed, waitForGraphRendered(panelWin, 3, 2)]);

  // Test internal storage
  is(panelWin.gAudioNodes.length, 3, "All nodes should be GC'd except one gain, osc and dest node.");

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

  await teardown(target);
});
