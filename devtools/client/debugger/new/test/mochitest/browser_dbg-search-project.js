/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function openProjectSearch(dbg) {
  synthesizeKeyShortcut("CmdOrCtrl+Shift+F");
  return waitForState(
    dbg,
    state => dbg.selectors.getActiveSearch(state) === "project"
  );
}

function closeProjectSearch(dbg) {
  pressKey(dbg, "Escape");
  return waitForState(dbg, state => !dbg.selectors.getActiveSearch(state));
}

async function selectResult(dbg) {
  const select = waitForState(
    dbg,
    () => !dbg.selectors.getActiveSearch(dbg.getState())
  );
  await clickElement(dbg, "fileMatch");
  return select;
}

function getResultsCount(dbg) {
  const matches = dbg.selectors
    .getTextSearchResults(dbg.getState())
    .valueSeq()
    .map(file => file.matches)
    .toJS();

  return [...matches].length;
}

// Testing project search
add_task(async function() {
  await pushPref("devtools.debugger.project-text-search-enabled", true);

  const dbg = await initDebugger("doc-script-switching.html", "switching-01");

  await selectSource(dbg, "switching-01");

  // test opening and closing
  await openProjectSearch(dbg);
  await closeProjectSearch(dbg);

  await openProjectSearch(dbg);
  type(dbg, "first");
  pressKey(dbg, "Enter");

  await waitForState(dbg, () => getResultsCount(dbg) === 1);

  await selectResult(dbg);

  is(dbg.selectors.getActiveSearch(dbg.getState()), null);

  const selectedSource = dbg.selectors.getSelectedSource(dbg.getState());
  ok(selectedSource.url.includes("switching-01"));
});
