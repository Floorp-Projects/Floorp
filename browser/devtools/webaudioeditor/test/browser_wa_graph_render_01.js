/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shader editor shows the appropriate UI when opened.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, WebAudioParamView } = panelWin;
  let gVars = WebAudioParamView._paramsView;

  let started = once(gFront, "start-context");

  reload(target);

  let [[dest, osc, gain], [[_, destID], [_, oscID], [_, gainID]]] = yield Promise.all([
    get3(gFront, "create-node"),
    get3Spread(panelWin, EVENTS.UI_ADD_NODE_LIST),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  ok(findGraphNode(panelWin, oscID).classList.contains("type-OscillatorNode"), "found OscillatorNode with class");
  ok(findGraphNode(panelWin, gainID).classList.contains("type-GainNode"), "found GainNode with class");
  ok(findGraphNode(panelWin, destID).classList.contains("type-AudioDestinationNode"), "found AudioDestinationNode with class");
  is(findGraphEdge(panelWin, oscID, gainID).toString(), "[object SVGGElement]", "found edge for osc -> gain");
  is(findGraphEdge(panelWin, gainID, destID).toString(), "[object SVGGElement]", "found edge for gain -> dest");

  yield teardown(panel);
  finish();
}

