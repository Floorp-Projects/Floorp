/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests consecutive duplicate frames are removed from the flame graph data.

let {FlameGraphUtils} = Cu.import("resource:///modules/devtools/FlameGraph.jsm", {});

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let out = FlameGraphUtils.createFlameGraphDataFromSamples(TEST_DATA, {
    flattenRecursion: true
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
    location: "A"
  }, {
    location: "A"
  }, {
    location: "A"
  }, {
    location: "B",
  }, {
    location: "B",
  }, {
    location: "C"
  }],
  time: 50,
}];

let EXPECTED_OUTPUT = [{
  blocks: []
}, {
  blocks: []
}, {
  blocks: [{
    srcData: {
      startTime: 0,
      rawLocation: "A"
    },
    x: 0,
    y: 0,
    width: 50,
    height: 11,
    text: "A"
  }]
}, {
  blocks: [{
    srcData: {
      startTime: 0,
      rawLocation: "B"
    },
    x: 0,
    y: 11,
    width: 50,
    height: 11,
    text: "B"
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
