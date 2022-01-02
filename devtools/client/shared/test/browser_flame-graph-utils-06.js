/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the text displayed is the function name, file name and line number
// if applicable and demangling.

const {
  FlameGraphUtils,
} = require("devtools/client/shared/widgets/FlameGraph");
const { PALLETTE_SIZE } = require("devtools/client/shared/widgets/FlameGraph");
const MANGLED_FN = "__Z3FooIiEvv";
const UNMANGLED_FN = "void Foo<int>()";

add_task(async function() {
  await addTab("about:blank");
  await performTest();
  gBrowser.removeCurrentTab();
});

function performTest() {
  const out = FlameGraphUtils.createFlameGraphDataFromThread(TEST_DATA, {
    flattenRecursion: true,
  });

  ok(out, "Some data was outputted properly");
  is(out.length, PALLETTE_SIZE, "The outputted length is correct.");

  info("Got flame graph data:\n" + JSON.stringify(out) + "\n");

  for (let i = 0; i < out.length; i++) {
    const found = out[i];
    const expected = EXPECTED_OUTPUT[i];

    is(
      found.blocks.length,
      expected.blocks.length,
      "The correct number of blocks were found in this bucket."
    );

    for (let j = 0; j < found.blocks.length; j++) {
      is(
        found.blocks[j].x,
        expected.blocks[j].x,
        "The expected block X position is correct for this frame."
      );
      is(
        found.blocks[j].y,
        expected.blocks[j].y,
        "The expected block Y position is correct for this frame."
      );
      is(
        found.blocks[j].width,
        expected.blocks[j].width,
        "The expected block width is correct for this frame."
      );
      is(
        found.blocks[j].height,
        expected.blocks[j].height,
        "The expected block height is correct for this frame."
      );
      is(
        found.blocks[j].text,
        expected.blocks[j].text,
        "The expected block text is correct for this frame."
      );
    }
  }
}

var TEST_DATA = synthesizeProfileForTest([
  {
    frames: [
      {
        location: "A (http://path/to/file.js:10:5)",
      },
      {
        location: `${MANGLED_FN} (http://path/to/file.js:100:5)`,
      },
    ],
    time: 50,
  },
]);

var EXPECTED_OUTPUT = [
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [
      {
        startTime: 0,
        frameKey: "A (http://path/to/file.js:10:5)",
        x: 0,
        y: 0,
        width: 50,
        height: 15,
        text: "A (file.js:10)",
      },
    ],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [
      {
        startTime: 0,
        frameKey: `${MANGLED_FN} (http://path/to/file.js:100:5)`,
        x: 0,
        y: 15,
        width: 50,
        height: 15,
        text: `${UNMANGLED_FN} (file.js:100)`,
      },
    ],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
  {
    blocks: [],
  },
];
