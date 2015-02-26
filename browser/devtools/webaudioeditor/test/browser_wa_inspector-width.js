/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the WebAudioInspector's Width is saved as
 * a preference
 */

add_task(function*() {
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

  // Open inspector pane
  $("#inspector-pane-toggle").click();
  yield once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED);

  let newInspectorWidth = 500;

  // Setting width to new_inspector_width
  $("#web-audio-inspector").setAttribute("width", newInspectorWidth);
  reload(target);

  //Width should be 500 after reloading
  [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  nodeIds = actors.map(actor => actor.actorID);

  // Open inspector pane
  $("#inspector-pane-toggle").click();
  yield once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED);

  yield clickGraphNode(panelWin, findGraphNode(panelWin, nodeIds[1]));
 
  // Getting the width of the audio inspector
  let width = $("#web-audio-inspector").getAttribute("width");

  is(width, newInspectorWidth, "WebAudioEditor's Inspector width should be saved as a preference");

  yield teardown(target);
});
