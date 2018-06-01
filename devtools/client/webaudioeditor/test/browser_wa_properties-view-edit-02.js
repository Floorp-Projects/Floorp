/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that properties are not updated when modifying the VariablesView.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(COMPLEX_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS, PropertiesView } = panelWin;
  const gVars = PropertiesView._propsView;

  const started = once(gFront, "start-context");

  const events = Promise.all([
    getN(gFront, "create-node", 8),
    waitForGraphRendered(panelWin, 8, 8)
  ]);
  reload(target);
  const [actors] = await events;
  const nodeIds = actors.map(actor => actor.actorID);

  click(panelWin, findGraphNode(panelWin, nodeIds[3]));
  // Wait for the node to be set as well as the inspector to come fully into the view
  await Promise.all([
    waitForInspectorRender(panelWin, EVENTS),
    once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED),
  ]);

  const errorEvent = once(panelWin, EVENTS.UI_SET_PARAM_ERROR);

  try {
    await modifyVariableView(panelWin, gVars, 0, "bufferSize", 2048);
  } catch (e) {
    // we except modifyVariableView to fail here, because bufferSize is not writable
  }

  await errorEvent;

  checkVariableView(gVars, 0, {bufferSize: 4096}, "check that unwritable variable is not updated");

  await teardown(target);
});
