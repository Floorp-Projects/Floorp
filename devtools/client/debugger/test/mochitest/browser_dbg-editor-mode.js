/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the editor sets the correct mode for different file
// types
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");

  await selectSource(dbg, "simple1.js");
  is(dbg.getCM().getOption("mode").name, "javascript", "Mode is correct");

  await selectSource(dbg, "doc-scripts.html");
  is(dbg.getCM().getOption("mode").name, "htmlmixed", "Mode is correct");
});
