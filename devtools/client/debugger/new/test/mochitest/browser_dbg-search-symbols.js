/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function openFunctionSearch(dbg) {
  synthesizeKeyShortcut("CmdOrCtrl+Shift+O");
  return new Promise(r => setTimeout(r, 200));
}

function resultCount(dbg) {
  return findAllElements(dbg, "resultItems").length;
}

// Testing function search
add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html", "switching-01");

  await selectSource(dbg, "switching-01");

  // test opening and closing
  await openFunctionSearch(dbg);
  is(dbg.selectors.getActiveSearch(dbg.getState()), "symbol");
  pressKey(dbg, "Escape");
  is(dbg.selectors.getActiveSearch(dbg.getState()), null);

  await openFunctionSearch(dbg);
  is(resultCount(dbg), 1);

  type(dbg, "x");
  is(resultCount(dbg), 0);
});
