/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#type
 */

add_task(async function() {
  let { target, front } = await initBackend(SIMPLE_NODES_URL);
  let [_, nodes] = await Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 14)
  ]);

  let actualTypes = nodes.map(node => node.type);
  let expectedTypes = [
    "AudioDestinationNode",
    "AudioBufferSourceNode", "ScriptProcessorNode", "AnalyserNode", "GainNode",
    "DelayNode", "BiquadFilterNode", "WaveShaperNode", "PannerNode", "ConvolverNode",
    "ChannelSplitterNode", "ChannelMergerNode", "DynamicsCompressorNode", "OscillatorNode"
  ];

  expectedTypes.forEach((type, i) => {
    is(actualTypes[i], type, type + " successfully created with correct type");
  });

  await removeTab(target.tab);
});
