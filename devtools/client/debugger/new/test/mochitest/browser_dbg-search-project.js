/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function openProjectSearch(dbg) {
  synthesizeKeyShortcut("CmdOrCtrl+Shift+F");
  return waitForState(
    dbg,
    state => dbg.selectors.getActiveSearchState(state) === "project"
  );
}

function closeProjectSearch(dbg) {
  pressKey(dbg, "Escape");
  return waitForState(dbg, state => !dbg.selectors.getActiveSearchState(state));
}

function getResult(dbg) {
  return findElementWithSelector(dbg, ".managed-tree .result");
}

async function selectResult(dbg) {
  const item = getResult(dbg);
  const select = waitForDispatch(dbg, "SELECT_SOURCE");
  item.click();
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
add_task(function*() {
  Services.prefs.setBoolPref(
    "devtools.debugger.project-text-search-enabled",
    true
  );

  const dbg = yield initDebugger("doc-script-switching.html", "switching-01");

  yield selectSource(dbg, "switching-01");

  // test opening and closing
  yield openProjectSearch(dbg);
  yield closeProjectSearch(dbg);

  yield openProjectSearch(dbg);
  type(dbg, "first");
  pressKey(dbg, "Enter");

  yield waitForState(dbg, () => getResultsCount(dbg) === 1);

  yield selectResult(dbg);
  is(dbg.selectors.getActiveSearchState(dbg.getState()), null);

  const selectedSource = dbg.selectors.getSelectedSource(dbg.getState());
  ok(selectedSource.get("url").includes("switching-01"));
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(
    "devtools.debugger.project-text-search-enabled",
    false
  );
});
