/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#getParamFlags()
 */

add_task(async function() {
  let { target, front } = await initBackend(SIMPLE_NODES_URL);
  let [_, nodes] = await Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 15)
  ]);

  let allNodeParams = await Promise.all(nodes.map(node => node.getParams()));
  let nodeTypes = [
    "AudioDestinationNode",
    "AudioBufferSourceNode", "ScriptProcessorNode", "AnalyserNode", "GainNode",
    "DelayNode", "BiquadFilterNode", "WaveShaperNode", "PannerNode", "ConvolverNode",
    "ChannelSplitterNode", "ChannelMergerNode", "DynamicsCompressorNode", "OscillatorNode",
    "StereoPannerNode"
  ];

  // For some reason nodeTypes.forEach and params.forEach fail here so we use
  // simple for loops.
  for (let i = 0; i < nodeTypes.length; i++) {
    let type = nodeTypes[i];
    let params = allNodeParams[i];

    for (let {param, value, flags} of params) {
      let testFlags = await nodes[i].getParamFlags(param);
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
