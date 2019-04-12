/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that when  pause on next is selected, we  pause on the next execution

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  const {
    selectors: { getIsWaitingOnBreak, getCurrentThread }
  } = dbg;

  clickElement(dbg, "pause");
  await waitForState(dbg, state => getIsWaitingOnBreak(getCurrentThread()));
  invokeInTab("simple");

  await waitForPaused(dbg, "simple3");
  assertPaused(dbg);
});
