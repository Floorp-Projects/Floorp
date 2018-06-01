/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that properties are updated when modifying the VariablesView.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS, PropertiesView } = panelWin;
  const gVars = PropertiesView._propsView;

  const started = once(gFront, "start-context");

  const events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  const [actors] = await events;
  const nodeIds = actors.map(actor => actor.actorID);

  click(panelWin, findGraphNode(panelWin, nodeIds[1]));
  // Wait for the node to be set as well as the inspector to come fully into the view
  await Promise.all([
    waitForInspectorRender(panelWin, EVENTS),
    once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED)
  ]);

  const setAndCheck = setAndCheckVariable(panelWin, gVars);

  checkVariableView(gVars, 0, {
    "type": "sine",
    "frequency": 440,
    "detune": 0
  }, "default loaded string");

  click(panelWin, findGraphNode(panelWin, nodeIds[2]));
  await waitForInspectorRender(panelWin, EVENTS);
  checkVariableView(gVars, 0, {
    "gain": 0
  }, "default loaded number");

  click(panelWin, findGraphNode(panelWin, nodeIds[1]));
  await waitForInspectorRender(panelWin, EVENTS);
  await setAndCheck(0, "type", "square", "square", "sets string as string");

  click(panelWin, findGraphNode(panelWin, nodeIds[2]));
  await waitForInspectorRender(panelWin, EVENTS);
  await setAndCheck(0, "gain", "0.005", 0.005, "sets number as number");
  await setAndCheck(0, "gain", "0.1", 0.1, "sets float as float");
  await setAndCheck(0, "gain", ".2", 0.2, "sets float without leading zero as float");

  await teardown(target);
});

function setAndCheckVariable(panelWin, gVars) {
  return async function(varNum, prop, value, expected, desc) {
    await modifyVariableView(panelWin, gVars, varNum, prop, value);
    var props = {};
    props[prop] = expected;
    checkVariableView(gVars, varNum, props, desc);
  };
}
