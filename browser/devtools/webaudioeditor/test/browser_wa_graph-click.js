/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the clicking on a node in the GraphView opens and sets
 * the correct node in the InspectorView
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(COMPLEX_CONTEXT_URL);
  let panelWin = panel.panelWin;
  let { gFront, $, $$, EVENTS, WebAudioInspectorView } = panelWin;

  let started = once(gFront, "start-context");

  reload(target);

  let [actors, _] = yield Promise.all([
    getN(gFront, "create-node", 8),
    waitForGraphRendered(panel.panelWin, 8, 8)
  ]);

  let nodeIds = actors.map(actor => actor.actorID);

  ok(!WebAudioInspectorView.isVisible(), "InspectorView hidden on start.");

  click(panel.panelWin, findGraphNode(panelWin, nodeIds[1]));
  yield once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED);

  ok(WebAudioInspectorView.isVisible(), "InspectorView visible after selecting a node.");
  is(WebAudioInspectorView.getCurrentNode().id, nodeIds[1], "InspectorView has correct node set.");

  click(panel.panelWin, findGraphNode(panelWin, nodeIds[2]));
  ok(WebAudioInspectorView.isVisible(), "InspectorView still visible after selecting another node.");
  is(WebAudioInspectorView.getCurrentNode().id, nodeIds[2], "InspectorView has correct node set on second node.");

  click(panel.panelWin, findGraphNode(panelWin, nodeIds[2]));
  is(WebAudioInspectorView.getCurrentNode().id, nodeIds[2], "Clicking the same node again works (idempotent).");

  click(panel.panelWin, $("rect", findGraphNode(panelWin, nodeIds[3])));
  is(WebAudioInspectorView.getCurrentNode().id, nodeIds[3], "Clicking on a <rect> works as expected.");

  click(panel.panelWin, $("tspan", findGraphNode(panelWin, nodeIds[4])));
  is(WebAudioInspectorView.getCurrentNode().id, nodeIds[4], "Clicking on a <tspan> works as expected.");

  yield teardown(panel);
  finish();
}

function isExpanded (view, index) {
  let scope = view.getScopeAtIndex(index);
  return scope.expanded;
}
