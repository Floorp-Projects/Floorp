/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Test that breakpoints work when set in inline scripts that do not start at column 0.

// Debugger operations may still be in progress when we navigate.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/Page has navigated/);

add_task(async function() {
  const dbg = await initDebugger("doc-inline-script-offset.html");
  await addBreakpoint(dbg, "doc-inline-script-offset.html", 6, 66);
  await reload(dbg);
  await waitForPaused(dbg);
  ok(true, "paused after reloading at column breakpoint");
});
