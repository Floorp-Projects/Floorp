/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

requestLongerTimeout(2);

async function testCase(dbg, { name, steps }) {
  info(` ### Execute testCase "${name}"`);

  const {
    selectors: { getTopFrame, getCurrentThread },
  } = dbg;
  const locations = [];

  const recordFrame = state => {
    const { line, column } = getTopFrame(getCurrentThread()).location;
    locations.push([line, column]);
    info(`Break on ${line}:${column}`);
  };

  info("Trigger the expected debugger statement");
  const onPaused = waitForPaused(dbg);
  invokeInTab(name);
  await onPaused;
  recordFrame();

  info("Start stepping over");
  for (let i = 0; i < steps.length - 1; i++) {
    await dbg.actions.stepOver();
    await waitForPaused(dbg);
    recordFrame();
  }

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
      [13, 0],
    ],
  });

  await testCase(dbg, {
    name: "expressions",
    steps: [
      [40, 2],
      [41, 2],
      [42, 12],
      [43, 0],
    ],
  });

  await testCase(dbg, {
    name: "sequences",
    steps: [
      [23, 2],
      [25, 12],
      [29, 12],
      [34, 2],
      [37, 0],
    ],
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
      [19, 8],
    ],
  });
});

function formatSteps(steps) {
  return steps.map(loc => `(${loc.join(",")})`).join(", ");
}
