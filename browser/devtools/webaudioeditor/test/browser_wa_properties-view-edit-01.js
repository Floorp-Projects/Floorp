/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that properties are updated when modifying the VariablesView.
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

  click(panelWin, findGraphNode(panelWin, nodeIds[1]));
  // Wait for the node to be set as well as the inspector to come fully into the view
  yield Promise.all([
    once(panelWin, EVENTS.UI_INSPECTOR_NODE_SET),
    once(panelWin, EVENTS.UI_INSPECTOR_TOGGLED)
  ]);

  let setAndCheck = setAndCheckVariable(panelWin, gVars);

  checkVariableView(gVars, 0, {
    "type": "sine",
    "frequency": 440,
    "detune": 0
  }, "default loaded string");

  click(panelWin, findGraphNode(panelWin, nodeIds[2]));
  yield once(panelWin, EVENTS.UI_INSPECTOR_NODE_SET);
  checkVariableView(gVars, 0, {
    "gain": 0
  }, "default loaded number");

  click(panelWin, findGraphNode(panelWin, nodeIds[1]));
  yield once(panelWin, EVENTS.UI_INSPECTOR_NODE_SET);
  yield setAndCheck(0, "type", "square", "square", "sets string as string");

  click(panelWin, findGraphNode(panelWin, nodeIds[2]));
  yield once(panelWin, EVENTS.UI_INSPECTOR_NODE_SET);
  yield setAndCheck(0, "gain", "0.005", 0.005, "sets number as number");
  yield setAndCheck(0, "gain", "0.1", 0.1, "sets float as float");
  yield setAndCheck(0, "gain", ".2", 0.2, "sets float without leading zero as float");

  yield teardown(panel);
  finish();
}

function setAndCheckVariable (panelWin, gVars) {
  return Task.async(function (varNum, prop, value, expected, desc) {
    yield modifyVariableView(panelWin, gVars, varNum, prop, value);
    var props = {};
    props[prop] = expected;
    checkVariableView(gVars, varNum, props, desc);
  });
}

