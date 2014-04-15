/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shader editor shows the appropriate UI when opened.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { gFront, $, $$, EVENTS, WebAudioParamView } = panel.panelWin;
  let gVars = WebAudioParamView._paramsView;

  let started = once(gFront, "start-context");

  reload(target);

  let [[dest, osc, gain], [[_, destID], [_, oscID], [_, gainID]]] = yield Promise.all([
    get3(gFront, "create-node"),
    get3Spread(panel.panelWin, EVENTS.UI_ADD_NODE_LIST)
  ]);


  checkVariableView(gVars, 1, {
    "type": "\"sine\"",
    "frequency": 440,
    "detune": 0
  });

  checkVariableView(gVars, 2, {
    "gain": 0
  });

  yield modifyVariableView(panel.panelWin, gVars, 1, "type", "square");

  checkVariableView(gVars, 1, {
    "type": "\"square\""
  });

  yield teardown(panel);
  finish();
}

