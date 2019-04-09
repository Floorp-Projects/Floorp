/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test can be really slow on debug platforms
requestLongerTimeout(5);

// Debugger operations may still be in progress when we step.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/Current thread has paused or resumed/);

add_task(async function test() {
  const dbg = await initDebugger("big-sourcemap.html", "big-sourcemap");
  invokeInTab("hitDebugStatement");
  await waitForPaused(dbg, "bundle.js");

  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);

  assertDebugLine(dbg, 42271);
  assertPausedLocation(dbg);
});
