/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that inspector view opens on graph node click, and
 * loads the correct node inside the inspector.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, WebAudioInspectorView } = panelWin;
  let gVars = WebAudioInspectorView._propsView;

  let started = once(gFront, "start-context");

  reload(target);

  let [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  let nodeIds = actors.map(actor => actor.actorID);

  ok(!WebAudioInspectorView.isVisible(), "InspectorView hidden on start.");
  ok(isVisible($("#web-audio-editor-details-pane-empty")),
    "InspectorView empty message should show when no node's selected.");
  ok(!isVisible($("#web-audio-editor-tabs")),
    "InspectorView tabs view should be hidden when no node's selected.");
  is($("#web-audio-inspector-title").value, "AudioNode Inspector",
    "Inspector should have default title when empty.");

  click(panelWin, findGraphNode(panelWin, nodeIds[1]));
  // Wait for the node to be set as well as the inspector to come fully into the view
  yield Promise.all([
    once(panelWin, EVENTS.UI_INSPECTOR_NODE_SET),
    once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED)
  ]);

  ok(WebAudioInspectorView.isVisible(), "InspectorView shown once node selected.");
  ok(!isVisible($("#web-audio-editor-details-pane-empty")),
    "InspectorView empty message hidden when node selected.");
  ok(isVisible($("#web-audio-editor-tabs")),
    "InspectorView tabs view visible when node selected.");

  is($("#web-audio-inspector-title").value, "OscillatorNode (" + nodeIds[1] + ")",
    "Inspector should have the node title when a node is selected.");

  is($("#web-audio-editor-tabs").selectedIndex, 0,
    "default tab selected should be the parameters tab.");

  click(panelWin, findGraphNode(panelWin, nodeIds[2]));
  yield once(panelWin, EVENTS.UI_INSPECTOR_NODE_SET);

  is($("#web-audio-inspector-title").value, "GainNode (" + nodeIds[2] + ")",
    "Inspector title updates when a new node is selected.");

  yield teardown(panel);
  finish();
}
