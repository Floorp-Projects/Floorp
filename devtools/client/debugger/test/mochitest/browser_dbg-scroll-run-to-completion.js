/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scroll-run-to-completion.html");
  invokeInTab("pauseOnce", "doc-scroll-run-to-completion.html");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-scroll-run-to-completion.html").id,
    20
  );

  await checkEvaluateInTopFrame(dbg, "window.scrollBy(0, 10);", undefined);

  // checkEvaluateInTopFrame does an implicit resume for some reason.
  await waitForPaused(dbg);

  const onTestPassed = once(Services.ppmm, "test passed");
  await resume(dbg);
  await onTestPassed;
});
