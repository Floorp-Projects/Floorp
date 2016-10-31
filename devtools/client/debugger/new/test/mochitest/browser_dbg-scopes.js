/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function toggleNode(dbg, index) {
  clickElement(dbg, "scopeNode", index);
}

function getLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}

add_task(function* () {
  const dbg = yield initDebugger("doc-script-switching.html");

  toggleScopes(dbg);

  invokeInTab("firstCall");
  yield waitForPaused(dbg);

  toggleNode(dbg, 1);
  toggleNode(dbg, 2);

  yield waitForDispatch(dbg, "LOAD_OBJECT_PROPERTIES");

  is(getLabel(dbg, 1), "secondCall");
  is(getLabel(dbg, 2), "<this>");
});
