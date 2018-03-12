/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests more edge rendering for complex graphs.
 */

add_task(function* () {
  let { target, panel } = yield initWebAudioEditor(COMPLEX_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$ } = panelWin;

  let started = once(gFront, "start-context");

  let events = Promise.all([
    getN(gFront, "create-node", 8),
    waitForGraphRendered(panelWin, 8, 8)
  ]);
  reload(target);
  let [actors] = yield events;
  let nodeIDs = actors.map(actor => actor.actorID);

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

  yield teardown(target);
});
