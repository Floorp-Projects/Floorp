/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that you can switch tools, without losing your editor position

"use strict";

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "long.js");

  await selectSource(dbg, "long.js");
  getCM(dbg).scrollTo(0, 284);

  pressKey(dbg, "inspector");
  pressKey(dbg, "debugger");

  is(getCM(dbg).getScrollInfo().top, 284);
});
