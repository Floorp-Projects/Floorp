/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

add_task(async function() {
  const dbg = await initDebugger("doc-scroll-run-to-completion.html");
  invokeInTab("pauseOnce", "doc-scroll-run-to-completion.html");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  await checkEvaluateInTopFrame(dbg, 'window.scrollBy(0, 10);', undefined);

  // checkEvaluateInTopFrame does an implicit resume for some reason.
  await waitForPaused(dbg);

  resume(dbg);
  await once(Services.ppmm, "test passed");
});
