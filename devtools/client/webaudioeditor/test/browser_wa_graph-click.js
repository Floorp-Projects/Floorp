/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the clicking on a node in the GraphView opens and sets
 * the correct node in the InspectorView
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(COMPLEX_CONTEXT_URL);
  const panelWin = panel.panelWin;
  const { gFront, $, $$, InspectorView } = panelWin;

  const started = once(gFront, "start-context");

  const events = Promise.all([
    getN(gFront, "create-node", 8),
    waitForGraphRendered(panel.panelWin, 8, 8)
  ]);
  reload(target);
  const [actors, _] = await events;
  const nodeIds = actors.map(actor => actor.actorID);

  ok(!InspectorView.isVisible(), "InspectorView hidden on start.");

  await clickGraphNode(panelWin, nodeIds[1], true);

  ok(InspectorView.isVisible(), "InspectorView visible after selecting a node.");
  is(InspectorView.getCurrentAudioNode().id, nodeIds[1], "InspectorView has correct node set.");

  await clickGraphNode(panelWin, nodeIds[2]);

  ok(InspectorView.isVisible(), "InspectorView still visible after selecting another node.");
  is(InspectorView.getCurrentAudioNode().id, nodeIds[2], "InspectorView has correct node set on second node.");

  await clickGraphNode(panelWin, nodeIds[2]);
  is(InspectorView.getCurrentAudioNode().id, nodeIds[2], "Clicking the same node again works (idempotent).");

  await clickGraphNode(panelWin, $("rect", findGraphNode(panelWin, nodeIds[3])));
  is(InspectorView.getCurrentAudioNode().id, nodeIds[3], "Clicking on a <rect> works as expected.");

  await clickGraphNode(panelWin, $("tspan", findGraphNode(panelWin, nodeIds[4])));
  is(InspectorView.getCurrentAudioNode().id, nodeIds[4], "Clicking on a <tspan> works as expected.");

  ok(InspectorView.isVisible(),
    "InspectorView still visible after several nodes have been clicked.");

  await teardown(target);
});
