/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  const ready = Promise.all([
    waitForPaused(dbg),
    waitForLoadedSource(dbg, "switching-02"),

    // MAP_FRAMES triggers a new Scopes panel render cycle, which introduces
    // a race condition with the click event on the foo node.
    waitForDispatch(dbg, "MAP_FRAMES"),
  ]);
  invokeInTab("firstCall");
  await ready;

  is(getLabel(dbg, 1), "secondCall");
  is(getLabel(dbg, 2), "<this>");
  is(getLabel(dbg, 4), "foo()");
  await toggleScopeNode(dbg, 4);
  is(getLabel(dbg, 5), "arguments");

  await stepOver(dbg);
  is(getLabel(dbg, 4), "foo()");
  is(getLabel(dbg, 5), "Window");
});

function getLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}
