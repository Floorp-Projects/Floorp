/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that text metrics and data conversion from profiler samples
// widget work properly in the flame graph.

let {FlameGraphUtils, FLAME_GRAPH_BLOCK_HEIGHT} = devtools.require("devtools/shared/widgets/FlameGraph");
let { DevToolsUtils } = Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {});

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let out = FlameGraphUtils.createFlameGraphDataFromThread(TEST_DATA);

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

let TEST_DATA = synthesizeProfileForTest([{
  frames: [{
    location: "M"
  }, {
    location: "N",
  }, {
    location: "P"
  }],
  time: 50,
}, {
  frames: [{
    location: "A"
  }, {
    location: "B",
  }, {
    location: "C"
  }],
  time: 100,
}, {
  frames: [{
    location: "A"
  }, {
    location: "B",
  }, {
    location: "D"
  }],
  time: 210,
}, {
  frames: [{
    location: "A"
  }, {
    location: "E",
  }, {
    location: "F"
  }],
  time: 330,
}, {
  frames: [{
    location: "A"
  }, {
    location: "B",
  }, {
    location: "C"
  }],
  time: 460,
}, {
  frames: [{
    location: "X"
  }, {
    location: "Y",
  }, {
    location: "Z"
  }],
  time: 500
}]);

let EXPECTED_OUTPUT = [{
  blocks: []
}, {
  blocks: []
}, {
  blocks: [{
    srcData: {
      startTime: 50,
      rawLocation: "A"
    },
    x: 50,
    y: 0,
    width: 410,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "A"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 50,
      rawLocation: "B"
    },
    x: 50,
    y: FLAME_GRAPH_BLOCK_HEIGHT,
    width: 160,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "B"
  }, {
    srcData: {
      startTime: 330,
      rawLocation: "B"
    },
    x: 330,
    y: FLAME_GRAPH_BLOCK_HEIGHT,
    width: 130,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "B"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 0,
      rawLocation: "M"
    },
    x: 0,
    y: 0,
    width: 50,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "M"
  }, {
    srcData: {
      startTime: 50,
      rawLocation: "C"
    },
    x: 50,
    y: FLAME_GRAPH_BLOCK_HEIGHT * 2,
    width: 50,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "C"
  }, {
    srcData: {
      startTime: 330,
      rawLocation: "C"
    },
    x: 330,
    y: FLAME_GRAPH_BLOCK_HEIGHT * 2,
    width: 130,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "C"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 0,
      rawLocation: "N"
    },
    x: 0,
    y: FLAME_GRAPH_BLOCK_HEIGHT,
    width: 50,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "N"
  }, {
    srcData: {
      startTime: 100,
      rawLocation: "D"
    },
    x: 100,
    y: FLAME_GRAPH_BLOCK_HEIGHT * 2,
    width: 110,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "D"
  }, {
    srcData: {
      startTime: 460,
      rawLocation: "X"
    },
    x: 460,
    y: 0,
    width: 40,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "X"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 210,
      rawLocation: "E"
    },
    x: 210,
    y: FLAME_GRAPH_BLOCK_HEIGHT,
    width: 120,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "E"
  }, {
    srcData: {
      startTime: 460,
      rawLocation: "Y"
    },
    x: 460,
    y: FLAME_GRAPH_BLOCK_HEIGHT,
    width: 40,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "Y"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 0,
      rawLocation: "P"
    },
    x: 0,
    y: FLAME_GRAPH_BLOCK_HEIGHT * 2,
    width: 50,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "P"
  }, {
    srcData: {
      startTime: 210,
      rawLocation: "F"
    },
    x: 210,
    y: FLAME_GRAPH_BLOCK_HEIGHT * 2,
    width: 120,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "F"
  }, {
    srcData: {
      startTime: 460,
      rawLocation: "Z"
    },
    x: 460,
    y: FLAME_GRAPH_BLOCK_HEIGHT * 2,
    width: 40,
    height: FLAME_GRAPH_BLOCK_HEIGHT,
    text: "Z"
  }]
}, {
  blocks: []
}, {
  blocks: []
}];
