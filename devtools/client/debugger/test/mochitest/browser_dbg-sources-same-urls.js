/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the source tree with multiple worker threads which use
// different versions of the same url select source nodes correctly.

add_task(async function () {
  const dbg = await initDebugger(
    "doc-sources-multiple-workers.html",
    "shared-worker",
    "simple-worker",
    "simple-worker",
  );

  const {
    selectors: { getSelectedSource },
    getState,
  } = dbg;

  // Make sure all the sources are loaded
  /*
   * The output looks like the following:
   * =====================================
   *   ▼[Main Thread]
   *   | ▼[example.com]
   *   | |  ▶︎[browser/.../examples]
   *   ▶︎[shared-worker.js]
   *   ▶︎[simple-worker.js]
   *   ▶︎[simple-worker.js]
   */
  await assertSourceCount(dbg, 6);

  // Expand the shared worker source.
  await clickElement(dbg, "sourceDirectoryLabel", 4);
  await assertSourceCount(dbg, 8);
  await clickElement(dbg, "sourceDirectoryLabel", 6);
  /*
   * The output looks like the following:
   * =====================================
   *   ▼[Main Thread]
   *   | ▼[example.com]
   *   | |  ▶︎[browser/.../examples]
   *   ▶︎[shared-worker.js]
   *   | ▼[example.com]
   *   | |  ▶︎[browser/.../examples]
   *   | |  | [shared-worker.js]
   *   ▶︎[simple-worker.js]
   *   ▶︎[simple-worker.js]
   */
  await assertSourceCount(dbg, 9);

  // Expand the first simple worker source
  await clickElement(dbg, "sourceDirectoryLabel", 8);
  await assertSourceCount(dbg, 11);
  await clickElement(dbg, "sourceDirectoryLabel", 10);
  /*
   * The output looks like the following:
   * =====================================
   *   ▼[Main Thread]
   *   | ▼[example.com]
   *   | |  ▶︎[browser/.../examples]
   *   ▶︎[shared-worker.js]
   *   | ▼[example.com]
   *   | |  ▶︎[browser/.../examples]
   *   | |  | [shared-worker.js]
   *   ▶︎[simple-worker.js]
   *   | ▼[example.com]
   *   | |  ▶︎[browser/.../examples]
   *   | |  | [simple-worker.js]
   *   ▶︎[simple-worker.js]
   */
  await assertSourceCount(dbg, 12);

  // Expand the second simple worker source
  await clickElement(dbg, "sourceDirectoryLabel", 12);
  await assertSourceCount(dbg, 14);
  await clickElement(dbg, "sourceDirectoryLabel", 14);
  /*
   * The output looks like the following:
   * =====================================
   *   ▼[Main Thread]
   *   | ▼[example.com]
   *   | |  ▶︎[browser/.../examples]
   *   ▶︎[shared-worker.js]
   *   | ▼[example.com]
   *   | |  ▶︎[browser/.../examples]
   *   | |  | [shared-worker.js]
   *   ▶︎[simple-worker.js]
   *   | ▼[example.com]
   *   | |  ▶︎[browser/.../examples]
   *   | |  | [simple-worker.js]
   *   ▶︎[simple-worker.js]
   *   | ▼[example.com]
   *   | |  ▶︎[browser/.../examples]
   *   | |  | [simple-worker.js]
   */
  await assertSourceCount(dbg, 15);

  // Select the shared worker source
  const selectedSharedWorkerSource = waitForDispatch(dbg, "SET_FOCUSED_SOURCE_ITEM");
  await clickElement(dbg, "sourceNode", 7);
  await selectedSharedWorkerSource;
  const sharedWorkerSource = getSelectedSource().url;
  ok(sharedWorkerSource.includes("shared-worker.js"), "shared worker source is selected");

  // Select the second simple worker source
  const selectedSimpleWorkerSource = waitForDispatch(dbg, "SET_FOCUSED_SOURCE_ITEM");
  await clickElement(dbg, "sourceNode", 15);
  await selectedSimpleWorkerSource;

  // Assert that the correct simple worker source is selected
  const firstSimpleWorkerSourceNode = findElement(dbg, "sourceNode", 11);
  ok(!firstSimpleWorkerSourceNode.classList.contains("focused"),
    `The first simple worker source is not selected`);

  const secondSimpleWorkerSourceNode = findElement(dbg, "sourceNode", 15);
  ok(secondSimpleWorkerSourceNode.classList.contains("focused"),
    `The second simple worker source is selected`);
});
