/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests if (idle) nodes are added when necessary in the flame graph data.

let {FlameGraphUtils} = Cu.import("resource:///modules/devtools/FlameGraph.jsm", {});
let {FrameNode} = devtools.require("devtools/shared/profiler/tree-model");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let out = FlameGraphUtils.createFlameGraphDataFromSamples(TEST_DATA, {
    flattenRecursion: true,
    filterFrames: FrameNode.isContent,
    showIdleBlocks: "\m/"
  });

  ok(out, "Some data was outputted properly");
  is(out.length, 10, "The outputted length is correct.");

  info("Got flame graph data:\n" + out.toSource() + "\n");

  for (let i = 0; i < out.length; i++) {
    let found = out[i];
    let expected = EXPECTED_OUTPUT[i];

    is(found.blocks.length, expected.blocks.length,
      "The correct number of blocks were found in this bucket.");

    for (let j = 0; j < found.blocks.length; j++) {
      is(found.blocks[j].x, expected.blocks[j].x,
        "The expected block X position is correct for this frame.");
      is(found.blocks[j].y, expected.blocks[j].y,
        "The expected block Y position is correct for this frame.");
      is(found.blocks[j].width, expected.blocks[j].width,
        "The expected block width is correct for this frame.");
      is(found.blocks[j].height, expected.blocks[j].height,
        "The expected block height is correct for this frame.");
      is(found.blocks[j].text, expected.blocks[j].text,
        "The expected block text is correct for this frame.");
    }
  }
}

let TEST_DATA = [{
  frames: [{
    location: "http://A"
  }, {
    location: "http://A"
  }, {
    location: "http://A"
  }, {
    location: "https://B"
  }, {
    location: "https://B"
  }, {
    location: "file://C",
  }, {
    location: "chrome://D"
  }, {
    location: "resource://E"
  }],
  time: 50
}, {
  frames: [{
    location: "chrome://D"
  }, {
    location: "resource://E"
  }],
  time: 100
}, {
  frames: [{
    location: "http://A"
  }, {
    location: "https://B"
  }, {
    location: "file://C",
  }],
  time: 150
}];

let EXPECTED_OUTPUT = [{
  blocks: []
}, {
  blocks: []
}, {
  blocks: [{
    srcData: {
      startTime: 0,
      rawLocation: "http://A"
    },
    x: 0,
    y: 0,
    width: 50,
    height: 11,
    text: "http://A"
  }, {
    srcData: {
      startTime: 0,
      rawLocation: "file://C"
    },
    x: 0,
    y: 22,
    width: 50,
    height: 11,
    text: "file://C"
  }, {
    srcData: {
      startTime: 100,
      rawLocation: "http://A"
    },
    x: 100,
    y: 0,
    width: 50,
    height: 11,
    text: "http://A"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 50,
      rawLocation: "\m/"
    },
    x: 50,
    y: 0,
    width: 50,
    height: 11,
    text: "\m/"
  }]
}, {
  blocks: []
}, {
  blocks: []
}, {
  blocks: []
}, {
  blocks: []
}, {
  blocks: [{
    srcData: {
      startTime: 0,
      rawLocation: "https://B"
    },
    x: 0,
    y: 11,
    width: 50,
    height: 11,
    text: "https://B"
  }, {
    srcData: {
      startTime: 100,
      rawLocation: "https://B"
    },
    x: 100,
    y: 11,
    width: 50,
    height: 11,
    text: "https://B"
  }]
}, {
  blocks: []
}];
