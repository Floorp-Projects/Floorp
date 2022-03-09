/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests removing tabs with keyboard shortcuts

"use strict";

add_task(async function() {
  const dbg = await initDebugger(
    "doc-scripts.html",
    "simple1.js",
    "simple2.js"
  );

  await selectSource(dbg, "simple1.js");
  await selectSource(dbg, "simple2.js");
  is(countTabs(dbg), 2);

  pressKey(dbg, "close");
  waitForDispatch(dbg.store, "CLOSE_TAB");
  is(countTabs(dbg), 1);

  pressKey(dbg, "close");
  waitForDispatch(dbg.store, "CLOSE_TAB");
  is(countTabs(dbg), 0);
});
