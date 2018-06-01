/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the inspector toggle button shows and hides
 * the inspector panel as intended.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS, InspectorView } = panelWin;
  const gVars = InspectorView._propsView;

  const started = once(gFront, "start-context");

  const events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  const [actors] = await events;
  const nodeIds = actors.map(actor => actor.actorID);

  ok(!InspectorView.isVisible(), "InspectorView hidden on start.");

  // Open inspector pane
  $("#inspector-pane-toggle").click();
  await once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED);

  ok(InspectorView.isVisible(), "InspectorView shown after toggling.");

  ok(isVisible($("#web-audio-editor-details-pane-empty")),
    "InspectorView empty message should still be visible.");
  ok(!isVisible($("#web-audio-editor-tabs")),
    "InspectorView tabs view should still be hidden.");

  // Close inspector pane
  $("#inspector-pane-toggle").click();
  await once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED);

  ok(!InspectorView.isVisible(), "InspectorView back to being hidden.");

  // Open again to test node loading while open
  $("#inspector-pane-toggle").click();
  await once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED);

  ok(InspectorView.isVisible(), "InspectorView being shown.");
  ok(!isVisible($("#web-audio-editor-tabs")),
    "InspectorView tabs are still hidden.");

  await clickGraphNode(panelWin, findGraphNode(panelWin, nodeIds[1]));

  ok(!isVisible($("#web-audio-editor-details-pane-empty")),
    "Empty message hides even when loading node while open.");
  ok(isVisible($("#web-audio-editor-tabs")),
    "Switches to tab view when loading node while open.");

  await teardown(target);
});
