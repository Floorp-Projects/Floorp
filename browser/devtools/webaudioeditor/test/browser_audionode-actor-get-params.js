/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test AudioNode#getParams()
 */

function spawnTest () {
  let [target, debuggee, front] = yield initBackend(SIMPLE_NODES_URL);
  let [_, nodes] = yield Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 14)
  ]);

  let allNodeParams = yield Promise.all(nodes.map(node => node.getParams()));
  let nodeTypes = [
    "AudioDestinationNode",
    "AudioBufferSourceNode", "ScriptProcessorNode", "AnalyserNode", "GainNode",
    "DelayNode", "BiquadFilterNode", "WaveShaperNode", "PannerNode", "ConvolverNode",
    "ChannelSplitterNode", "ChannelMergerNode", "DynamicsCompressorNode", "OscillatorNode"
  ];

  nodeTypes.forEach((type, i) => {
    let params = allNodeParams[i];
    params.forEach(({param, value, flags}) => {
      ok(~NODE_PROPERTIES[type].indexOf(param), "expected parameter for " + type);

      // Skip over some properties that are undefined by default
      if (!/buffer|loop|smoothing|curve|cone/.test(param)) {
        ok(value != undefined, param + " is not undefined");
      }

      ok(typeof flags === "object", type + " has a flags object");

      if (param === "buffer") {
        is(flags.Buffer, true, "`buffer` params have Buffer flag");
      }
      else if (param === "bufferSize" || param === "frequencyBinCount") {
        is(flags.readonly, true, param + " is readonly");
      }
      else if (param === "curve") {
        is(flags["Float32Array"], true, "`curve` param has Float32Array flag");
      } else {
        is(Object.keys(flags), 0, type + "-" + param + " has no flags set")
      }
    });
  });

  yield removeTab(target.tab);
  finish();
}
