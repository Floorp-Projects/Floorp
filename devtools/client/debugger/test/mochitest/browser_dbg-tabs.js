/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests adding and removing tabs

"use strict";

add_task(async function () {
  const dbg = await initDebugger(
    "doc-scripts.html",
    "simple1.js",
    "simple2.js"
  );

  await selectSource(dbg, "simple1.js");
  await selectSource(dbg, "simple2.js");
  is(countTabs(dbg), 2);

  info("Test reloading the debugger");
  await reload(dbg, "simple1.js", "simple2.js");
  await waitForSelectedSource(dbg, "simple2.js");
  is(countTabs(dbg), 2);

  info("Test reloading the debuggee a second time");
  await reload(dbg, "simple1.js", "simple2.js");
  await waitForSelectedSource(dbg, "simple2.js");
  is(countTabs(dbg), 2);
});

add_task(async function () {
  const dbg = await initDebugger(
    "doc-scripts.html",
    "simple1.js",
    "simple2.js"
  );

  await selectSource(dbg, "simple1.js");
  await selectSource(dbg, "simple2.js");
  await closeTab(dbg, "simple1.js");
  await closeTab(dbg, "simple2.js");

  info("Test reloading the debugger");
  await reload(dbg, "simple1.js", "simple2.js");
  is(countTabs(dbg), 0);
});
