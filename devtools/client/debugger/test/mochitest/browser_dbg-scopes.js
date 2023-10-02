/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Test that the values of the scope nodes are displayed correctly.
add_task(async function testScopeNodes() {
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

  info("Resuming the thread");
  await resume(dbg);
});

// Test scope nodes for anonymous functions display correctly.
add_task(async function testAnonymousScopeNodes() {
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html;charset=utf8,<!DOCTYPE html><script>(function(){const x = 3; debugger;})()</script>"
  );

  info("Reload the page to hit the debugger statement while loading");
  const onReloaded = reload(dbg);
  await waitForPaused(dbg);
  ok(true, "We're paused");

  is(
    getScopeNodeLabel(dbg, 1),
    "<anonymous>",
    "The scope node for the anonymous function is displayed correctly"
  );

  info("Resuming the thread");
  await resume(dbg);
  await onReloaded;
});
