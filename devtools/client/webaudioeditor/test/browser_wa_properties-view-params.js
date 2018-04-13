/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that params view correctly displays all properties for nodes
 * correctly, with default values and correct types.
 */

add_task(async function() {
  let { target, panel } = await initWebAudioEditor(SIMPLE_NODES_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, PropertiesView } = panelWin;
  let gVars = PropertiesView._propsView;

  let started = once(gFront, "start-context");

  await loadFrameScriptUtils();

  let events = Promise.all([
    getN(gFront, "create-node", 15),
    waitForGraphRendered(panelWin, 15, 0)
  ]);
  reload(target);
  let [actors] = await events;
  let nodeIds = actors.map(actor => actor.actorID);

  let types = [
    "AudioDestinationNode", "AudioBufferSourceNode", "ScriptProcessorNode",
    "AnalyserNode", "GainNode", "DelayNode", "BiquadFilterNode", "WaveShaperNode",
    "PannerNode", "ConvolverNode", "ChannelSplitterNode", "ChannelMergerNode",
    "DynamicsCompressorNode", "OscillatorNode"
  ];

  let defaults = await Promise.all(types.map(type => nodeDefaultValues(type)));

  for (let i = 0; i < types.length; i++) {
    click(panelWin, findGraphNode(panelWin, nodeIds[i]));
    await waitForInspectorRender(panelWin, EVENTS);
    checkVariableView(gVars, 0, defaults[i], types[i]);
  }

  await teardown(target);
});
