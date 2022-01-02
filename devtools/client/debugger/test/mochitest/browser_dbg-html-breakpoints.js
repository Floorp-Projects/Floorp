/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

add_task(async function() {
  const dbg = await initDebugger("doc-html-breakpoints.html");

  await selectSource(dbg, "doc-html-breakpoints.html");

  // Reload the page so that we know the debugger is already open and
  // functional before the page loads and we start getting notifications
  // about new actors being created in the page content.
  await reload(dbg, "doc-html-breakpoints.html");

  await waitForBreakableLine(dbg, "doc-html-breakpoints.html", 8);
  await addBreakpoint(dbg, "doc-html-breakpoints.html", 8);

  // Ensure that the breakpoints get added once the later scripts load.
  await waitForBreakableLine(dbg, "doc-html-breakpoints.html", 14);
  await addBreakpoint(dbg, "doc-html-breakpoints.html", 14);
  await waitForBreakableLine(dbg, "doc-html-breakpoints.html", 20);
  await addBreakpoint(dbg, "doc-html-breakpoints.html", 20);

  await reload(dbg, "doc-html-breakpoints.html");

  invokeInTab("test1");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  await resume(dbg);

  await waitForBreakableLine(dbg, "doc-html-breakpoints.html", 14);
  invokeInTab("test3");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  await resume(dbg);

  await waitForBreakableLine(dbg, "doc-html-breakpoints.html", 20);
  invokeInTab("test4");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  await resume(dbg);
});
