/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#getParamFlags()
 */

add_task(async function() {
  const { target, front } = await initBackend(SIMPLE_NODES_URL);
  const [_, nodes] = await Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 15)
  ]);

  const allNodeParams = await Promise.all(nodes.map(node => node.getParams()));
  const nodeTypes = [
    "AudioDestinationNode",
    "AudioBufferSourceNode", "ScriptProcessorNode", "AnalyserNode", "GainNode",
    "DelayNode", "BiquadFilterNode", "WaveShaperNode", "PannerNode", "ConvolverNode",
    "ChannelSplitterNode", "ChannelMergerNode", "DynamicsCompressorNode", "OscillatorNode",
    "StereoPannerNode"
  ];

  // For some reason nodeTypes.forEach and params.forEach fail here so we use
  // simple for loops.
  for (let i = 0; i < nodeTypes.length; i++) {
    const type = nodeTypes[i];
    const params = allNodeParams[i];

    for (const {param, value, flags} of params) {
      const testFlags = await nodes[i].getParamFlags(param);
      ok(typeof testFlags === "object", type + " has flags from #getParamFlags(" + param + ")");

      if (param === "buffer") {
        is(flags.Buffer, true, "`buffer` params have Buffer flag");
      } else if (param === "bufferSize" || param === "frequencyBinCount") {
        is(flags.readonly, true, param + " is readonly");
      } else if (param === "curve") {
        is(flags.Float32Array, true, "`curve` param has Float32Array flag");
      }
    }
  }

  await removeTab(target.tab);
});
