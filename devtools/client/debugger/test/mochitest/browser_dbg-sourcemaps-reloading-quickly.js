/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Test reloading an original file while the sourcemap is loading.
 * The test passes when the selected source is visible after two reloads.
 */
add_task(async function() {
  const dbg = await initDebugger("doc-sourcemaps.html");

  await waitForSources(dbg, "entry.js");
  await selectSource(dbg, "entry.js");

  await reload(dbg);
  await waitForSources(dbg, "bundle.js");

  await reload(dbg);
  await waitForLoadedSource(dbg, "entry.js");

  ok(
    getCM(dbg)
      .getValue()
      .includes("window.keepMeAlive"),
    "Original source text loaded correctly"
  );
});
