/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that you can switch tools, without losing your editor position

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "long");

  await selectSource(dbg, "long");
  getCM(dbg).scrollTo(0, 284);

  pressKey(dbg, "inspector");
  pressKey(dbg, "debugger");

  is(getCM(dbg).getScrollInfo().top, 284);
});
