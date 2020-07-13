/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Testing quick open
add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  info("test opening and closing");
  await quickOpen(dbg, "");
  pressKey(dbg, "Escape");
  assertDisabled(dbg);

  info("Testing the number of results for source search");
  await quickOpen(dbg, "sw");
  await waitForResults(dbg, [undefined, undefined]);
  is(resultCount(dbg), 2, "two file results");
  pressKey(dbg, "Escape");

  info("Testing source search and check to see if source is selected");
  await waitForSource(dbg, "switching-01");
  await quickOpen(dbg, "sw1");
  await waitForResults(dbg, ["switching-01.js"]);
  is(resultCount(dbg), 1, "one file results");
  pressKey(dbg, "Enter");
  await waitForSelectedSource(dbg, "switching-01");

  info("Test that results show tab icons");
  await quickOpen(dbg, "sw1");
  await waitForResults(dbg, ["switching-01.js"]);
  await assertResultIsTab(dbg, 1);
  pressKey(dbg, "Tab");

  info(
    "Testing arrow keys in source search and check to see if source is selected"
  );
  await quickOpen(dbg, "sw2");
  await waitForResults(dbg, ["switching-02.js"]);
  is(resultCount(dbg), 1, "one file results");
  pressKey(dbg, "Down");
  pressKey(dbg, "Enter");
  await waitForSelectedSource(dbg, "switching-02");

  info("Testing tab closes the search");
  await quickOpen(dbg, "sw");
  await waitForResults(dbg, [undefined, undefined]);
  pressKey(dbg, "Tab");
  assertDisabled(dbg);

  info("Testing function search");
  await quickOpen(dbg, "", "quickOpenFunc");
  await waitForResults(dbg, ["secondCall", "foo"]);
  is(resultCount(dbg), 2, "two function results");

  type(dbg, "@x");
  await waitForResults(dbg, []);
  is(resultCount(dbg), 0, "no functions with 'x' in name");

  pressKey(dbg, "Escape");
  assertDisabled(dbg);

  info("Testing goto line:column");
  assertLine(dbg, 0);
  assertColumn(dbg, null);
  await quickOpen(dbg, ":7:12");
  await waitForResults(dbg, [undefined, undefined]);
  pressKey(dbg, "Enter");
  assertLine(dbg, 7);
  assertColumn(dbg, 12);

  info("Testing gotoSource");
  await quickOpen(dbg, "sw1:5");
  await waitForResults(dbg, ["switching-01.js"]);
  pressKey(dbg, "Enter");
  await waitForSelectedSource(dbg, "switching-01");
  assertLine(dbg, 5);
});

function assertEnabled(dbg) {
  is(dbg.selectors.getQuickOpenEnabled(), true, "quickOpen enabled");
}

function assertDisabled(dbg) {
  is(dbg.selectors.getQuickOpenEnabled(), false, "quickOpen disabled");
}

function assertLine(dbg, lineNumber) {
  is(
    dbg.selectors.getSelectedLocation().line,
    lineNumber,
    `goto line is ${lineNumber}`
  );
}

function assertColumn(dbg, columnNumber) {
  is(
    dbg.selectors.getSelectedLocation().column,
    columnNumber,
    `goto column is ${columnNumber}`
  );
}

function waitForSymbols(dbg, url) {
  const source = findSource(dbg, url);
  return waitForState(dbg, state => dbg.selectors.getSymbols(state, source.id));
}

function resultCount(dbg) {
  return findAllElements(dbg, "resultItems").length;
}

async function quickOpen(dbg, query, shortcut = "quickOpen") {
  pressKey(dbg, shortcut);
  assertEnabled(dbg);
  query !== "" && type(dbg, query);
}

async function waitForResults(dbg, results) {
  await waitForAllElements(dbg, "resultItems", results.length, true);

  for (let i = 0; i < results.length; ++i) {
    if (results[i] !== undefined) {
      await waitForElement(dbg, "resultItemName", results[i], i + 1);
    }
  }
}

function findResultEl(dbg, index = 1) {
  return waitForElementWithSelector(dbg, `.result-item:nth-child(${index})`);
}

async function assertResultIsTab(dbg, index) {
  const el = await findResultEl(dbg, index);
  ok(
    el && !!el.querySelector(".tab.result-item-icon"),
    "Result should be a tab"
  );
}