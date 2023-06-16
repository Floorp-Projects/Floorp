/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests breakpoints syncing when reloading

"use strict";

requestLongerTimeout(3);

// Tests that a breakpoint set is correctly synced after reload
// and gets hit correctly.
add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js", "long.js");

  await selectSource(dbg, "simple1.js");

  // Setting 2 breakpoints, one on line 61 which is expected
  // to get hit, while the other on line 56 which is not expected
  // to get hit, but to assert that it correctly set after reload.
  await addBreakpointViaGutter(dbg, 61);
  await addBreakpointViaGutter(dbg, 56);

  await selectSource(dbg, "long.js");

  await addBreakpointViaGutter(dbg, 1);

  const onReloaded = reload(dbg);
  await waitForPaused(dbg);

  info("Assert that the source is not long.js");
  // Adding this is redundant but just to make it explicit that we
  // make sure long.js should not exist yet
  assertSourceDoesNotExist(dbg, "long.js");

  const source = findSource(dbg, "simple1.js");

  await assertPausedAtSourceAndLine(dbg, source.id, 61);

  info("The breakpoint for long.js does not exist yet");
  await waitForState(dbg, state => dbg.selectors.getBreakpointCount() == 2);

  // The breakpoints are available once their corresponding source
  // has been processed. Let's assert that all the breakpoints for
  // simple1.js have been restored.
  await assertBreakpoint(dbg, 56);
  await assertBreakpoint(dbg, 61);

  await resume(dbg);
  await waitForPaused(dbg);

  const source2 = findSource(dbg, "long.js");

  await assertPausedAtSourceAndLine(dbg, source2.id, 1);

  info("All 3 breakpoints from simple1.js and long.js still exist");
  await waitForState(dbg, state => dbg.selectors.getBreakpointCount() == 3);

  await assertBreakpoint(dbg, 1);

  await resume(dbg);

  info("Wait for reload to complete after resume");
  await onReloaded;

  // remove breakpoints so they do not affect other
  // tests.
  await removeBreakpoint(dbg, source.id, 56);
  await removeBreakpoint(dbg, source.id, 61);
  await removeBreakpoint(dbg, source2.id, 1);

  await dbg.toolbox.closeToolbox();
});

// Test that pending breakpoints are installed in inline scripts as they are
// sent to the client.
add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "doc-scripts.html");

  await selectSource(dbg, "doc-scripts.html");
  await addBreakpointViaGutter(dbg, 22);
  await addBreakpointViaGutter(dbg, 27);

  const onReloaded = reload(dbg, "doc-scripts.html");
  await waitForPaused(dbg);

  const source = findSource(dbg, "doc-scripts.html");

  await assertPausedAtSourceAndLine(dbg, source.id, 22);

  info("Only the breakpoint for the first inline script should exist");
  await waitForState(dbg, state => dbg.selectors.getBreakpointCount() == 1);

  await assertBreakpoint(dbg, 22);

  // The second breakpoint we added is in a later inline script, and won't
  // appear until after we have resumed from the first breakpoint and the
  // second inline script has been created.
  await assertNoBreakpoint(dbg, 27);

  await resume(dbg);
  await waitForPaused(dbg);

  info("All 2 breakpoints from both inline scripts still exist");
  await waitForState(dbg, state => dbg.selectors.getBreakpointCount() == 2);

  await assertPausedAtSourceAndLine(dbg, source.id, 27);
  await assertBreakpoint(dbg, 27);

  await resume(dbg);

  info("Wait for reload to complete after resume");
  await onReloaded;

  await removeBreakpoint(dbg, source.id, 22);
  await removeBreakpoint(dbg, source.id, 27);

  await dbg.toolbox.closeToolbox();
});

function assertSourceDoesNotExist(dbg, url) {
  const source = findSource(dbg, url, { silent: true });
  ok(!source, `Source for ${url} does not exist`);
}
