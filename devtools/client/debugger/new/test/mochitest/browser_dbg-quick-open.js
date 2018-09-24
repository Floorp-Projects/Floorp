/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function assertEnabled(dbg) {
  is(
    dbg.selectors.getQuickOpenEnabled(dbg.getState()),
    true,
    "quickOpen enabled"
  );
}

function assertDisabled(dbg) {
  is(
    dbg.selectors.getQuickOpenEnabled(dbg.getState()),
    false,
    "quickOpen disabled"
  );
}

function assertLine(dbg, lineNumber) {
  is(
    dbg.selectors.getSelectedLocation(dbg.getState()).line,
    lineNumber,
    `goto line is ${lineNumber}`
  );
}

function assertColumn(dbg, columnNumber) {
  is(
    dbg.selectors.getSelectedLocation(dbg.getState()).column,
    columnNumber,
    `goto column is ${columnNumber}`
  );
}

function waitForSymbols(dbg, url) {
  const source = findSource(dbg, url);
  return waitForState(dbg, state => dbg.selectors.getSymbols(state, source.id));
}

async function waitToClose(dbg) {
  pressKey(dbg, "Escape");
  return new Promise(r => setTimeout(r, 200));
}

function resultCount(dbg) {
  return findAllElements(dbg, "resultItems").length;
}

function quickOpen(dbg, query, shortcut = "quickOpen") {
  pressKey(dbg, shortcut);
  assertEnabled(dbg);
  query !== "" && type(dbg, query);
}

// Testing quick open
add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  info("test opening and closing");
  quickOpen(dbg, "");
  pressKey(dbg, "Escape");
  assertDisabled(dbg);

  info("Testing the number of results for source search");
  quickOpen(dbg, "sw");
  is(resultCount(dbg), 2, "two file results");
  pressKey(dbg, "Escape");

  info("Testing source search and check to see if source is selected");
  await waitForSource(dbg, "switching-01");
  quickOpen(dbg, "sw1");
  is(resultCount(dbg), 1, "one file results");
  pressKey(dbg, "Enter");
  await waitForSelectedSource(dbg, "switching-01");

  info("Testing arrow keys in source search and check to see if source is selected");
  quickOpen(dbg, "sw2");
  is(resultCount(dbg), 1, "one file results");
  pressKey(dbg, "Down");
  pressKey(dbg, "Enter");
  await waitForSelectedSource(dbg, "switching-02");

  info("Testing tab closes the search");
  quickOpen(dbg, "sw");
  pressKey(dbg, "Tab");
  assertDisabled(dbg);

  info("Testing function search");
  quickOpen(dbg, "", "quickOpenFunc");
  is(resultCount(dbg), 2, "two function results");

  type(dbg, "@x");
  is(resultCount(dbg), 0, "no functions with 'x' in name");

  pressKey(dbg, "Escape");
  assertDisabled(dbg);

  info("Testing variable search");
  quickOpen(dbg, "sw2");
  pressKey(dbg, "Enter");

  quickOpen(dbg, "#");
  is(resultCount(dbg), 1, "one variable result");
  const results = findAllElements(dbg, "resultItems");
  results.forEach(result => is(result.textContent, "x13"));
  await waitToClose(dbg);

  info("Testing goto line:column");
  assertLine(dbg, 0);
  assertColumn(dbg, null);
  quickOpen(dbg, ":7:12");
  pressKey(dbg, "Enter");
  assertLine(dbg, 7);
  assertColumn(dbg, 12);

  info("Testing gotoSource");
  quickOpen(dbg, "sw1:5");
  pressKey(dbg, "Enter");
  await waitForSelectedSource(dbg, "switching-01");
  assertLine(dbg, 5);
});
