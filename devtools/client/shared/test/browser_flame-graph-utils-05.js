/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that flame graph data is cached, and that the cache may be cleared.

const {
  FlameGraphUtils,
} = require("devtools/client/shared/widgets/FlameGraph");

add_task(async function() {
  await addTab("about:blank");
  await performTest();
  gBrowser.removeCurrentTab();
});

function performTest() {
  const out1 = FlameGraphUtils.createFlameGraphDataFromThread(TEST_DATA);
  const out2 = FlameGraphUtils.createFlameGraphDataFromThread(TEST_DATA);
  is(out1, out2, "The outputted data is identical.");

  const out3 = FlameGraphUtils.createFlameGraphDataFromThread(TEST_DATA, {
    flattenRecursion: true,
  });
  is(out2, out3, "The outputted data is still identical.");

  FlameGraphUtils.removeFromCache(TEST_DATA);
  const out4 = FlameGraphUtils.createFlameGraphDataFromThread(TEST_DATA, {
    flattenRecursion: true,
  });
  isnot(out3, out4, "The outputted data is not identical anymore.");
}

var TEST_DATA = synthesizeProfileForTest([
  {
    frames: [
      {
        location: "A",
      },
      {
        location: "A",
      },
      {
        location: "A",
      },
      {
        location: "B",
      },
      {
        location: "B",
      },
      {
        location: "C",
      },
    ],
    time: 50,
  },
]);
