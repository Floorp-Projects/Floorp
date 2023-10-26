/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that breakpoints work when set in inline scripts that do not start at column 0.

"use strict";

const TEST_PAGE = "doc-inline-script-offset.html";

add_task(async function () {
  const dbg = await initDebugger(TEST_PAGE);
  await selectSource(dbg, TEST_PAGE);

  // Ensure that breakable lines are correct when loading against an already loaded page
  await assertBreakableLines(dbg, TEST_PAGE, 16, [
    ...getRange(3, 5),
    ...getRange(11, 13),
    15,
  ]);

  await reload(dbg, TEST_PAGE);

  // Also verify they are fine after reload
  await assertBreakableLines(dbg, TEST_PAGE, 16, [
    ...getRange(3, 5),
    ...getRange(11, 13),
    15,
  ]);

  await addBreakpoint(dbg, "doc-inline-script-offset.html", 15, 67);

  const onReloaded = reload(dbg);
  await waitForPaused(dbg);
  ok(true, "paused after reloading at column breakpoint");

  await resume(dbg);
  info("Wait for reload to complete after resume");
  await onReloaded;
});
