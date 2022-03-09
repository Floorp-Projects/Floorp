/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that dynamically generated <script> elements which contain source maps
// will be shown in the debugger.

"use strict";

add_task(async function() {
  const dbg = await initDebugger("doc-react-jsx.html");

  invokeInTab("injectScript");
  await waitForSources(dbg, "doc-react-jsx.html", "main.js");
  await selectSource(dbg, "main.js");
  await addBreakpoint(dbg, "main.js", 3);
  invokeInTab("foo");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "main.js").id, 3);
});
