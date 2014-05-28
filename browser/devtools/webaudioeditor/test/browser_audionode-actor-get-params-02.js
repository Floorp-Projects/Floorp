/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that default properties are returned with the correct type
 * from the AudioNode actors.
 */

function spawnTest() {
  let [target, debuggee, front] = yield initBackend(SIMPLE_NODES_URL);
  let [_, nodes] = yield Promise.all([
    front.setup({ reload: true }),
    getN(front, "create-node", 14)
  ]);

  let allParams = yield Promise.all(nodes.map(node => node.getParams()));
  let types = [
    "AudioDestinationNode", "AudioBufferSourceNode", "ScriptProcessorNode",
    "AnalyserNode", "GainNode", "DelayNode", "BiquadFilterNode", "WaveShaperNode",
    "PannerNode", "ConvolverNode", "ChannelSplitterNode", "ChannelMergerNode",
    "DynamicsCompressorNode", "OscillatorNode"
  ];

  allParams.forEach((params, i) => {
    compare(params, NODE_DEFAULT_VALUES[types[i]], types[i]);
  });

  yield removeTab(target.tab);
  finish();
}

function compare (actual, expected, type) {
  actual.forEach(({ value, param }) => {
    value = getGripValue(value);
    if (typeof expected[param] === "function") {
      ok(expected[param](value), type + " has a passing value for " + param);
    }
    else {
      ise(value, expected[param], type + " has correct default value and type for " + param);
    }
  });

  is(actual.length, Object.keys(expected).length,
    type + " has correct amount of properties.");
}
