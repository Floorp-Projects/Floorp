/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that inspector view opens on graph node click, and
 * loads the correct node inside the inspector.
 */

add_task(function* () {
  let { target, panel } = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, InspectorView } = panelWin;
  let gVars = InspectorView._propsView;

  let started = once(gFront, "start-context");

  reload(target);

  let [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  let nodeIds = actors.map(actor => actor.actorID);

  ok(!InspectorView.isVisible(), "InspectorView hidden on start.");
  ok(isVisible($("#web-audio-editor-details-pane-empty")),
    "InspectorView empty message should show when no node's selected.");
  ok(!isVisible($("#web-audio-editor-tabs")),
    "InspectorView tabs view should be hidden when no node's selected.");

  // Wait for the node to be set as well as the inspector to come fully into the view
  yield clickGraphNode(panelWin, findGraphNode(panelWin, nodeIds[1]), true);

  ok(InspectorView.isVisible(), "InspectorView shown once node selected.");
  ok(!isVisible($("#web-audio-editor-details-pane-empty")),
    "InspectorView empty message hidden when node selected.");
  ok(isVisible($("#web-audio-editor-tabs")),
    "InspectorView tabs view visible when node selected.");

  is($("#web-audio-editor-tabs").selectedIndex, 0,
    "default tab selected should be the parameters tab.");

  yield clickGraphNode(panelWin, findGraphNode(panelWin, nodeIds[2]));

  yield teardown(target);
});
