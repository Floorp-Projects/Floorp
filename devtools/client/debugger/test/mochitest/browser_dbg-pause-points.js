/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

requestLongerTimeout(2);

async function stepOvers(dbg, count, onStep = () => {}) {
  for (let i = 0; i < count; i++) {
    await dbg.actions.stepOver(getThreadContext(dbg));
    await waitForPaused(dbg);
    onStep();
  }
}
function formatSteps(steps) {
  return steps.map(loc => `(${loc.join(",")})`).join(", ");
}

async function testCase(dbg, { name, steps }) {
  invokeInTab(name);
  const locations = [];

  const {
    selectors: { getTopFrame, getCurrentThread }
  } = dbg;

  await stepOvers(dbg, steps.length, state => {
    const { line, column } = getTopFrame(getCurrentThread()).location;
    locations.push([line, column]);
  });

  is(formatSteps(locations), formatSteps(steps), name);
  await resume(dbg);
}

add_task(async function test() {
  const dbg = await initDebugger("doc-pause-points.html", "pause-points.js");

  await selectSource(dbg, "pause-points.js");
  await testCase(dbg, {
    name: "statements",
    steps: [
      [9, 2],
      [10, 4],
      [10, 13],
      [11, 2],
      [11, 21],
      [12, 2],
      [12, 12],
      [13, 0]
    ]
  });

  await testCase(dbg, {
    name: "expressions",
    steps: [[40, 2], [41, 2], [42, 12], [43, 0]]
  });

  await testCase(dbg, {
    name: "sequences",
    steps: [[23, 2], [25, 12], [29, 12], [34, 2], [37, 0]]
  });

  await testCase(dbg, {
    name: "flow",
    steps: [
      [16, 2],
      [17, 12],
      [18, 10],
      [19, 8],
      [19, 17],
      [19, 8],
      [19, 17],
      [19, 8]
    ]
  });
});
