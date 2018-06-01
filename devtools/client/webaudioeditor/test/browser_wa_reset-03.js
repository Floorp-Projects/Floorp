/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests reloading a tab with the tools open properly cleans up
 * the inspector and selected node.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, InspectorView } = panelWin;

  let events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  let [actors] = await events;
  let nodeIds = actors.map(actor => actor.actorID);

  await clickGraphNode(panelWin, nodeIds[1], true);
  ok(InspectorView.isVisible(), "InspectorView visible after selecting a node.");
  is(InspectorView.getCurrentAudioNode().id, nodeIds[1], "InspectorView has correct node set.");

  /**
   * Reload
   */

  events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  [actors] = await events;
  nodeIds = actors.map(actor => actor.actorID);

  ok(!InspectorView.isVisible(), "InspectorView hidden on start.");
  is(InspectorView.getCurrentAudioNode(), null,
    "InspectorView has no current node set on reset.");

  await clickGraphNode(panelWin, nodeIds[2], true);
  ok(InspectorView.isVisible(),
    "InspectorView visible after selecting a node after a reset.");
  is(InspectorView.getCurrentAudioNode().id, nodeIds[2], "InspectorView has correct node set upon clicking graph node after a reset.");

  await teardown(target);
});
