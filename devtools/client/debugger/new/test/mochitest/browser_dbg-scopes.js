/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function getLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}

add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  invokeInTab("firstCall");
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "switching-02");

  // MAP_FRAMES triggers a new Scopes panel render cycle, which introduces
  // a race condition with the click event on the foo node.
  await waitForDispatch(dbg, "MAP_FRAMES");

  is(getLabel(dbg, 1), "secondCall");
  is(getLabel(dbg, 2), "<this>");
  is(getLabel(dbg, 4), "foo()");
  await toggleScopeNode(dbg, 4);
  is(getLabel(dbg, 5), "arguments");

  await stepOver(dbg);
  is(getLabel(dbg, 4), "foo()");
  is(getLabel(dbg, 5), "Window");
});
