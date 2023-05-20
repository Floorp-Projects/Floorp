/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that XHR handlers are not called when pausing in the debugger.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-xhr-run-to-completion.html");
  invokeInTab("singleRequest", "doc-xhr-run-to-completion.html");
  await waitForPaused(dbg);
  await waitForSelectedLocation(dbg, 23);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-xhr-run-to-completion.html").id,
    23
  );

  const onTestPassed = once(Services.ppmm, "test passed");
  await resume(dbg);
  await onTestPassed;
});

// Test that XHR handlers are not called when pausing in the debugger,
// including when there are multiple XHRs and multiple times we pause before
// they can be processed.
add_task(async function () {
  const dbg = await initDebugger("doc-xhr-run-to-completion.html");
  invokeInTab("multipleRequests", "doc-xhr-run-to-completion.html");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-xhr-run-to-completion.html").id,
    31
  );
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-xhr-run-to-completion.html").id,
    33
  );
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-xhr-run-to-completion.html").id,
    34
  );
  const onTestPassed = once(Services.ppmm, "test passed");
  await resume(dbg);
  await onTestPassed;
});
