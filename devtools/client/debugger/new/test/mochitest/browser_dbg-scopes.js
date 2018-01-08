/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function toggleNode(dbg, index) {
  clickElement(dbg, "scopeNode", index);
}

function getLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}

add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  invokeInTab("firstCall");
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "switching-02");

  is(getLabel(dbg, 1), "secondCall");
  is(getLabel(dbg, 2), "<this>");
  is(getLabel(dbg, 4), "foo()");

  toggleNode(dbg, 4);
  await waitForDispatch(dbg, "LOAD_OBJECT_PROPERTIES");
  is(getLabel(dbg, 5), "arguments");

  await stepOver(dbg);
  is(getLabel(dbg, 4), "foo()");
  is(getLabel(dbg, 5), "Window");
});
