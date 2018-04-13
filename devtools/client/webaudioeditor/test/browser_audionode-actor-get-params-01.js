/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#getParams()
 */

add_task(async function() {
  let { target, front } = await initBackend(SIMPLE_NODES_URL);
  let [_, nodes] = await Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 15)
  ]);

  await loadFrameScriptUtils();

  let allNodeParams = await Promise.all(nodes.map(node => node.getParams()));
  let nodeTypes = [
    "AudioDestinationNode",
    "AudioBufferSourceNode", "ScriptProcessorNode", "AnalyserNode", "GainNode",
    "DelayNode", "BiquadFilterNode", "WaveShaperNode", "PannerNode", "ConvolverNode",
    "ChannelSplitterNode", "ChannelMergerNode", "DynamicsCompressorNode", "OscillatorNode",
    "StereoPannerNode"
  ];

  let defaults = await Promise.all(nodeTypes.map(type => nodeDefaultValues(type)));

  nodeTypes.map((type, i) => {
    let params = allNodeParams[i];

    params.forEach(({param, value, flags}) => {
      ok(param in defaults[i], "expected parameter for " + type);

      ok(typeof flags === "object", type + " has a flags object");

      if (param === "buffer") {
        is(flags.Buffer, true, "`buffer` params have Buffer flag");
      } else if (param === "bufferSize" || param === "frequencyBinCount") {
        is(flags.readonly, true, param + " is readonly");
      } else if (param === "curve") {
        is(flags.Float32Array, true, "`curve` param has Float32Array flag");
      }
    });
  });

  await removeTab(target.tab);
});
