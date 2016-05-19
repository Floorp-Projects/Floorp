/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that text metrics and data conversion from profiler samples
// widget work properly in the flame graph.

var {FlameGraphUtils} = require("devtools/client/shared/widgets/FlameGraph");
var {PALLETTE_SIZE} = require("devtools/client/shared/widgets/FlameGraph");

add_task(function* () {
  yield addTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let out = FlameGraphUtils.createFlameGraphDataFromThread(TEST_DATA);

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

var EXPECTED_OUTPUT = [{
  blocks: []
}, {
  blocks: []
}, {
  blocks: [{
    startTime: 50,
    frameKey: "A",
    x: 50,
    y: 0,
    width: 410,
    height: 15,
    text: "A"
  }]
}, {
  blocks: [{
    startTime: 50,
    frameKey: "B",
    x: 50,
    y: 15,
    width: 160,
    height: 15,
    text: "B"
  }, {
    startTime: 330,
    frameKey: "B",
    x: 330,
    y: 15,
    width: 130,
    height: 15,
    text: "B"
  }]
}, {
  blocks: [{
    startTime: 50,
    frameKey: "C",
    x: 50,
    y: 30,
    width: 50,
    height: 15,
    text: "C"
  }, {
    startTime: 330,
    frameKey: "C",
    x: 330,
    y: 30,
    width: 130,
    height: 15,
    text: "C"
  }]
}, {
  blocks: [{
    startTime: 100,
    frameKey: "D",
    x: 100,
    y: 30,
    width: 110,
    height: 15,
    text: "D"
  }, {
    startTime: 460,
    frameKey: "X",
    x: 460,
    y: 0,
    width: 40,
    height: 15,
    text: "X"
  }]
}, {
  blocks: [{
    startTime: 210,
    frameKey: "E",
    x: 210,
    y: 15,
    width: 120,
    height: 15,
    text: "E"
  }, {
    startTime: 460,
    frameKey: "Y",
    x: 460,
    y: 15,
    width: 40,
    height: 15,
    text: "Y"
  }]
}, {
  blocks: [{
    startTime: 210,
    frameKey: "F",
    x: 210,
    y: 30,
    width: 120,
    height: 15,
    text: "F"
  }, {
    startTime: 460,
    frameKey: "Z",
    x: 460,
    y: 30,
    width: 40,
    height: 15,
    text: "Z"
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
}, {
  blocks: [{
    startTime: 0,
    frameKey: "M",
    x: 0,
    y: 0,
    width: 50,
    height: 15,
    text: "M"
  }]
}, {
  blocks: [{
    startTime: 0,
    frameKey: "N",
    x: 0,
    y: 15,
    width: 50,
    height: 15,
    text: "N"
  }]
}, {
  blocks: []
}, {
  blocks: [{
    startTime: 0,
    frameKey: "P",
    x: 0,
    y: 30,
    width: 50,
    height: 15,
    text: "P"
  }]
}, {
  blocks: []
}, {
  blocks: []
}];
