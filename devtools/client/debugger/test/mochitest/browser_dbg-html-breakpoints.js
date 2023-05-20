/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-html-breakpoints.html");

  await selectSource(dbg, "doc-html-breakpoints.html");

  // Reload the page so that we know the debugger is already open and
  // functional before the page loads and we start getting notifications
  // about new actors being created in the page content.
  await reload(dbg, "doc-html-breakpoints.html");

  await waitForBreakableLine(dbg, "doc-html-breakpoints.html", 8);
  await addBreakpoint(dbg, "doc-html-breakpoints.html", 8);

  // Ensure that the breakpoints get added once the later scripts load.
  await waitForBreakableLine(dbg, "doc-html-breakpoints.html", 15);
  await addBreakpoint(dbg, "doc-html-breakpoints.html", 15);
  await waitForBreakableLine(dbg, "doc-html-breakpoints.html", 20);
  await addBreakpoint(dbg, "doc-html-breakpoints.html", 20);

  await reload(dbg, "doc-html-breakpoints.html", "simple2.js");

  invokeInTab("test1");
  await waitForPaused(dbg);

  const htmlSource = findSource(dbg, "doc-html-breakpoints.html");

  is(htmlSource.isHTML, true, "The html page is flagged as an html source");
  is(
    findSource(dbg, "simple2.js").isHTML,
    false,
    "The js source is not flagged as an html source"
  );

  assertPausedAtSourceAndLine(dbg, htmlSource.id, 8);
  await resume(dbg);

  await waitForBreakableLine(dbg, "doc-html-breakpoints.html", 15);
  invokeInTab("test3");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, htmlSource.id, 15);
  await resume(dbg);

  await waitForBreakableLine(dbg, "doc-html-breakpoints.html", 20);
  invokeInTab("test4");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, htmlSource.id, 20);
  await resume(dbg);
});
