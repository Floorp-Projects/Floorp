/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the clicking on a node in the GraphView opens and sets
 * the correct node in the InspectorView
 */

add_task(function*() {
  let { target, panel } = yield initWebAudioEditor(COMPLEX_CONTEXT_URL);
  let panelWin = panel.panelWin;
  let { gFront, $, $$, InspectorView } = panelWin;

  let started = once(gFront, "start-context");

  reload(target);

  let [actors, _] = yield Promise.all([
    getN(gFront, "create-node", 8),
    waitForGraphRendered(panel.panelWin, 8, 8)
  ]);

  let nodeIds = actors.map(actor => actor.actorID);

  ok(!InspectorView.isVisible(), "InspectorView hidden on start.");

  yield clickGraphNode(panelWin, nodeIds[1], true);

  ok(InspectorView.isVisible(), "InspectorView visible after selecting a node.");
  is(InspectorView.getCurrentAudioNode().id, nodeIds[1], "InspectorView has correct node set.");

  yield clickGraphNode(panelWin, nodeIds[2]);

  ok(InspectorView.isVisible(), "InspectorView still visible after selecting another node.");
  is(InspectorView.getCurrentAudioNode().id, nodeIds[2], "InspectorView has correct node set on second node.");

  yield clickGraphNode(panelWin, nodeIds[2]);
  is(InspectorView.getCurrentAudioNode().id, nodeIds[2], "Clicking the same node again works (idempotent).");

  yield clickGraphNode(panelWin, $("rect", findGraphNode(panelWin, nodeIds[3])));
  is(InspectorView.getCurrentAudioNode().id, nodeIds[3], "Clicking on a <rect> works as expected.");

  yield clickGraphNode(panelWin, $("tspan", findGraphNode(panelWin, nodeIds[4])));
  is(InspectorView.getCurrentAudioNode().id, nodeIds[4], "Clicking on a <tspan> works as expected.");

  ok(InspectorView.isVisible(),
    "InspectorView still visible after several nodes have been clicked.");

  yield teardown(target);
});
