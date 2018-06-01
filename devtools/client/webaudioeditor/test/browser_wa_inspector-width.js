/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the WebAudioInspector's Width is saved as
 * a preference
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS, InspectorView } = panelWin;
  const gVars = InspectorView._propsView;

  const started = once(gFront, "start-context");

  let events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  let [actors] = await events;
  let nodeIds = actors.map(actor => actor.actorID);

  ok(!InspectorView.isVisible(), "InspectorView hidden on start.");

  // Open inspector pane
  $("#inspector-pane-toggle").click();
  await once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED);

  const newInspectorWidth = 500;

  // Setting width to new_inspector_width
  $("#web-audio-inspector").setAttribute("width", newInspectorWidth);

  // Width should be 500 after reloading
  events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  [actors] = await events;
  nodeIds = actors.map(actor => actor.actorID);

  // Open inspector pane
  $("#inspector-pane-toggle").click();
  await once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED);

  await clickGraphNode(panelWin, findGraphNode(panelWin, nodeIds[1]));

  // Getting the width of the audio inspector
  const width = $("#web-audio-inspector").getAttribute("width");

  is(width, newInspectorWidth, "WebAudioEditor's Inspector width should be saved as a preference");

  await teardown(target);
});
