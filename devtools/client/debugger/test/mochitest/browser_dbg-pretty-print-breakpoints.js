/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that breakpoints appear when you reload a page
// with pretty-printed files.
add_task(async function() {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await selectSource(dbg, "pretty.js");
  await prettyPrint(dbg);

  await addBreakpoint(dbg, "pretty.js:formatted", 5);
  await closeTab(dbg, "pretty.js:formatted");

  await reload(dbg, "pretty.js");
  invokeInTab("stuff");

  await waitForPaused(dbg);
  await assertEditorBreakpoint(dbg, 4, true);
});
