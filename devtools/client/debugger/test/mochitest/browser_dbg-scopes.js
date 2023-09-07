/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-script-switching.html");

  const ready = Promise.all([
    waitForPaused(dbg),
    waitForLoadedSource(dbg, "script-switching-02.js"),

    // MAP_FRAMES triggers a new Scopes panel render cycle, which introduces
    // a race condition with the click event on the foo node.
    waitForDispatch(dbg.store, "MAP_FRAMES"),
  ]);
  invokeInTab("firstCall");
  await ready;

  is(getScopeNodeLabel(dbg, 1), "secondCall");
  is(getScopeNodeLabel(dbg, 2), "<this>");
  is(getScopeNodeLabel(dbg, 4), "foo()");
  await toggleScopeNode(dbg, 4);
  is(getScopeNodeLabel(dbg, 5), "arguments");

  await stepOver(dbg);
  is(getScopeNodeLabel(dbg, 4), "foo()");
  is(getScopeNodeLabel(dbg, 5), "Window");
  is(getScopeNodeValue(dbg, 5), "Global");
});
