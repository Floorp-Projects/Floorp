/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#getParams()
 */

add_task(async function() {
  const { target, front } = await initBackend(SIMPLE_NODES_URL);
  const [_, nodes] = await Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 15)
  ]);

  await loadFrameScriptUtils();

  const allNodeParams = await Promise.all(nodes.map(node => node.getParams()));
  const nodeTypes = [
    "AudioDestinationNode",
    "AudioBufferSourceNode", "ScriptProcessorNode", "AnalyserNode", "GainNode",
    "DelayNode", "BiquadFilterNode", "WaveShaperNode", "PannerNode", "ConvolverNode",
    "ChannelSplitterNode", "ChannelMergerNode", "DynamicsCompressorNode", "OscillatorNode",
    "StereoPannerNode"
  ];

  const defaults = await Promise.all(nodeTypes.map(type => nodeDefaultValues(type)));

  nodeTypes.map((type, i) => {
    const params = allNodeParams[i];

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
