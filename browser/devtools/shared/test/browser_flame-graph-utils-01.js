/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that text metrics and data conversion from profiler samples
// widget work properly in the flame graph.

let {FlameGraphUtils} = Cu.import("resource:///modules/devtools/FlameGraph.jsm", {});

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let out = FlameGraphUtils.createFlameGraphDataFromSamples(TEST_DATA);

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
}];

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
    height: 11,
    text: "A"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 50,
      rawLocation: "B"
    },
    x: 50,
    y: 11,
    width: 160,
    height: 11,
    text: "B"
  }, {
    srcData: {
      startTime: 330,
      rawLocation: "B"
    },
    x: 330,
    y: 11,
    width: 130,
    height: 11,
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
    height: 11,
    text: "M"
  }, {
    srcData: {
      startTime: 50,
      rawLocation: "C"
    },
    x: 50,
    y: 22,
    width: 50,
    height: 11,
    text: "C"
  }, {
    srcData: {
      startTime: 330,
      rawLocation: "C"
    },
    x: 330,
    y: 22,
    width: 130,
    height: 11,
    text: "C"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 0,
      rawLocation: "N"
    },
    x: 0,
    y: 11,
    width: 50,
    height: 11,
    text: "N"
  }, {
    srcData: {
      startTime: 100,
      rawLocation: "D"
    },
    x: 100,
    y: 22,
    width: 110,
    height: 11,
    text: "D"
  }, {
    srcData: {
      startTime: 460,
      rawLocation: "X"
    },
    x: 460,
    y: 0,
    width: 40,
    height: 11,
    text: "X"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 210,
      rawLocation: "E"
    },
    x: 210,
    y: 11,
    width: 120,
    height: 11,
    text: "E"
  }, {
    srcData: {
      startTime: 460,
      rawLocation: "Y"
    },
    x: 460,
    y: 11,
    width: 40,
    height: 11,
    text: "Y"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 0,
      rawLocation: "P"
    },
    x: 0,
    y: 22,
    width: 50,
    height: 11,
    text: "P"
  }, {
    srcData: {
      startTime: 210,
      rawLocation: "F"
    },
    x: 210,
    y: 22,
    width: 120,
    height: 11,
    text: "F"
  }, {
    srcData: {
      startTime: 460,
      rawLocation: "Z"
    },
    x: 460,
    y: 22,
    width: 40,
    height: 11,
    text: "Z"
  }]
}, {
  blocks: []
}, {
  blocks: []
}];
