/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shader editor shows the appropriate UI when opened.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(COMPLEX_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, WebAudioParamView } = panelWin;
  let gVars = WebAudioParamView._paramsView;

  let started = once(gFront, "start-context");

  reload(target);

  let [[dest, osc, gain], nodeIDs ]= yield Promise.all([
    getN(gFront, "create-node", 8),
    getNSpread(panelWin, EVENTS.UI_ADD_NODE_LIST, 8),
    waitForGraphRendered(panelWin, 8, 8)
  ]);

  // Map result to only have ID, since we don't need the event name
  nodeIDs = nodeIDs.map(eventResult => eventResult[1]);
  let types = ["AudioDestinationNode", "OscillatorNode", "GainNode", "ScriptProcessorNode",
               "OscillatorNode", "GainNode", "AudioBufferSourceNode", "BiquadFilterNode"];


  types.forEach((type, i) => {
    ok(findGraphNode(panelWin, nodeIDs[i]).classList.contains("type-" + type), "found " + type + " with class");
  });

  let edges = [
    [1, 2, "osc1 -> gain1"],
    [1, 3, "osc1 -> proc"],
    [2, 0, "gain1 -> dest"],
    [4, 5, "osc2 -> gain2"],
    [5, 0, "gain2 -> dest"],
    [6, 7, "buf -> filter"],
    [4, 7, "osc2 -> filter"],
    [7, 0, "filter -> dest"],
  ];

  edges.forEach(([source, target, msg], i) => {
    is(findGraphEdge(panelWin, nodeIDs[source], nodeIDs[target]).toString(), "[object SVGGElement]",
      "found edge for " + msg);
  });

  yield teardown(panel);
  finish();
}

