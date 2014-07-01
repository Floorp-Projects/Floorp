/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the clicking on a node in the GraphView opens and sets
 * the correct node in the InspectorView
 */

let EVENTS = null;

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(COMPLEX_CONTEXT_URL);
  let panelWin = panel.panelWin;
  let { gFront, $, $$, WebAudioInspectorView } = panelWin;
  EVENTS = panelWin.EVENTS;

  let started = once(gFront, "start-context");

  reload(target);

  let [actors, _] = yield Promise.all([
    getN(gFront, "create-node", 8),
    waitForGraphRendered(panel.panelWin, 8, 8)
  ]);

  let nodeIds = actors.map(actor => actor.actorID);

  ok(!WebAudioInspectorView.isVisible(), "InspectorView hidden on start.");

  yield clickGraphNode(panelWin, nodeIds[1], true);

  ok(WebAudioInspectorView.isVisible(), "InspectorView visible after selecting a node.");
  is(WebAudioInspectorView.getCurrentAudioNode().id, nodeIds[1], "InspectorView has correct node set.");

  yield clickGraphNode(panelWin, nodeIds[2]);

  ok(WebAudioInspectorView.isVisible(), "InspectorView still visible after selecting another node.");
  is(WebAudioInspectorView.getCurrentAudioNode().id, nodeIds[2], "InspectorView has correct node set on second node.");

  yield clickGraphNode(panelWin, nodeIds[2]);
  is(WebAudioInspectorView.getCurrentAudioNode().id, nodeIds[2], "Clicking the same node again works (idempotent).");

  yield clickGraphNode(panelWin, $("rect", findGraphNode(panelWin, nodeIds[3])));
  is(WebAudioInspectorView.getCurrentAudioNode().id, nodeIds[3], "Clicking on a <rect> works as expected.");

  yield clickGraphNode(panelWin, $("tspan", findGraphNode(panelWin, nodeIds[4])));
  is(WebAudioInspectorView.getCurrentAudioNode().id, nodeIds[4], "Clicking on a <tspan> works as expected.");

  ok(WebAudioInspectorView.isVisible(),
    "InspectorView still visible after several nodes have been clicked.");

  yield teardown(panel);
  finish();
}
