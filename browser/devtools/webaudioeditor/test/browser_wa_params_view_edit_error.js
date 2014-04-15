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

  yield modifyVariableView(panel.panelWin, gVars, 1, "frequency", "invalid-frequency").then(null, (message) => {
    ok(true, "Correctly fires EVENTS.UI_SET_PARAM_ERROR");
  });

  checkVariableView(gVars, 1, {
    "frequency": 1000
  });

  yield teardown(panel);
  finish();
}

