/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests if (idle) nodes are added when necessary in the flame graph data.

var {FlameGraphUtils} = require("devtools/client/shared/widgets/FlameGraph");
var {PALLETTE_SIZE} = require("devtools/client/shared/widgets/FlameGraph");
var {FrameNode} = require("devtools/client/performance/modules/logic/tree-model");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let out = FlameGraphUtils.createFlameGraphDataFromThread(TEST_DATA, {
    flattenRecursion: true,
    contentOnly: true,
    showIdleBlocks: '\m/'
  });

  ok(out, "Some data was outputted properly");
  is(out.length, PALLETTE_SIZE, "The outputted length is correct.");

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

var TEST_DATA = synthesizeProfileForTest([{
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
  frames: [],
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
}]);

var EXPECTED_OUTPUT = [{
  blocks: []
}, {
  blocks: []
}, {
  blocks: [{
    startTime: 0,
    frameKey: "http://A",
    x: 0,
    y: 0,
    width: 50,
    height: 15,
    text: "http://A"
  }, {
    startTime: 100,
    frameKey: "http://A",
    x: 100,
    y: 0,
    width: 50,
    height: 15,
    text: "http://A"
  }]
}, {
  blocks: []
}, {
  blocks: []
}, {
  blocks: []
}, {
  blocks: [{
    startTime: 0,
    frameKey: "Gecko",
    x: 0,
    y: 45,
    width: 50,
    height: 15,
    text: "Gecko"
  }]
}, {
  blocks: []
}, {
  blocks: [{
    startTime: 0,
    frameKey: "https://B",
    x: 0,
    y: 15,
    width: 50,
    height: 15,
    text: "https://B"
  }, {
    startTime: 100,
    frameKey: "https://B",
    x: 100,
    y: 15,
    width: 50,
    height: 15,
    text: "https://B"
  }]
}, {
  blocks: []
}, {
  blocks: []
}, {
  blocks: []
}, {
  blocks: [{
    startTime: 0,
    frameKey: "file://C",
    x: 0,
    y: 30,
    width: 50,
    height: 15,
    text: "file://C"
  }, {
    startTime: 100,
    frameKey: "file://C",
    x: 100,
    y: 30,
    width: 50,
    height: 15,
    text: "file://C"
  }]
}, {
  blocks: [{
    startTime: 50,
    frameKey: "m/",
    x: 50,
    y: 0,
    width: 50,
    height: 15,
    text: "m/"
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
  blocks: []
}, {
  blocks: []
}];
