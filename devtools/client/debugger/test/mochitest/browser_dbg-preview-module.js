/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test hovering in a script that is paused on load
// and doesn't have functions.
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");

  navigate(dbg, "doc-on-load.html");

  // wait for `top-level.js` to load and to pause at a debugger statement
  await waitForSelectedSource(dbg, "top-level.js");
  await waitForPaused(dbg);

  await assertPreviews(dbg, [
    {
      line: 1,
      column: 6,
      expression: "obj",
      fields: [["foo", "1"], ["bar", "2"]]
    }
  ]);

  await assertPreviewTooltip(dbg, 2, 7, { result: "3", expression: "func" });
});
