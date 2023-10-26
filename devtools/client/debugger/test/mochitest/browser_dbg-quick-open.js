/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Testing quick open

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-script-switching.html");

  // Inject lots of sources to go beyond the maximum limit of displayed sources (set to 100)
  const injectedSources = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      const sources = [];
      for (let i = 1; i <= 200; i++) {
        const value = String(i).padStart(3, "0");
        content.eval(
          `function evalSource() {}; //# sourceURL=eval-source-${value}.js`
        );
        sources.push(`eval-source-${value}.js`);
      }
      return sources;
    }
  );
  await waitForSources(dbg, ...injectedSources);

  info("Test opening and closing");
  await quickOpen(dbg, "");
  // Only the first 100 results are shown in the  quick open menu
  await waitForResults(dbg, injectedSources.slice(0, 100));
  is(resultCount(dbg), 100, "100 file results");
  pressKey(dbg, "Escape");
  assertQuickOpenDisabled(dbg);

  info("Testing the number of results for source search");
  await quickOpen(dbg, "sw");
  await waitForResults(dbg, [undefined, undefined]);
  is(resultCount(dbg), 2, "two file results");
  pressKey(dbg, "Escape");

  // We ensure that sources after maxResult limit are visible
  info("Test that first and last eval source are visible");
  await quickOpen(dbg, "eval-source-001.js");
  await waitForResults(dbg, ["eval-source-001.js"]);
  is(resultCount(dbg), 1, "one file result");
  pressKey(dbg, "Escape");
  await quickOpen(dbg, "eval-source-200.js");
  await waitForResults(dbg, ["eval-source-200.js"]);
  is(resultCount(dbg), 1, "one file result");
  pressKey(dbg, "Escape");

  info("Testing source search and check to see if source is selected");
  await waitForSource(dbg, "script-switching-01.js");
  await quickOpen(dbg, "sw1");
  await waitForResults(dbg, ["script-switching-01.js"]);
  is(resultCount(dbg), 1, "one file results");
  pressKey(dbg, "Enter");
  await waitForSelectedSource(dbg, "script-switching-01.js");

  info("Test that results show tab icons");
  await quickOpen(dbg, "sw1");
  await waitForResults(dbg, ["script-switching-01.js"]);
  await assertResultIsTab(dbg, 1);
  pressKey(dbg, "Tab");

  info(
    "Testing arrow keys in source search and check to see if source is selected"
  );
  await quickOpen(dbg, "sw2");
  await waitForResults(dbg, ["script-switching-02.js"]);
  is(resultCount(dbg), 1, "one file results");
  pressKey(dbg, "Down");
  pressKey(dbg, "Enter");
  await waitForSelectedSource(dbg, "script-switching-02.js");

  info("Testing tab closes the search");
  await quickOpen(dbg, "sw");
  await waitForResults(dbg, [undefined, undefined]);
  pressKey(dbg, "Tab");
  assertQuickOpenDisabled(dbg);

  info("Testing function search (anonymous fuctions should not display)");
  await quickOpen(dbg, "", "quickOpenFunc");
  await waitForResults(dbg, ["secondCall", "foo"]);
  is(resultCount(dbg), 2, "two function results");

  type(dbg, "@x");
  await waitForResults(dbg, []);
  is(resultCount(dbg), 0, "no functions with 'x' in name");

  pressKey(dbg, "Escape");
  assertQuickOpenDisabled(dbg);

  info("Testing goto line:column");
  assertLine(dbg, 0);
  assertColumn(dbg, null);
  await quickOpen(dbg, ":7:12");
  await waitForResults(dbg, [undefined, undefined]);
  pressKey(dbg, "Enter");
  await waitForSelectedSource(dbg, "script-switching-02.js");
  assertLine(dbg, 7);
  assertColumn(dbg, 12);

  info("Testing gotoSource");
  await quickOpen(dbg, "sw1:5");
  await waitForResults(dbg, ["script-switching-01.js"]);
  pressKey(dbg, "Enter");
  await waitForSelectedSource(dbg, "script-switching-01.js");
  assertLine(dbg, 5);
});

function assertQuickOpenEnabled(dbg) {
  is(dbg.selectors.getQuickOpenEnabled(), true, "quickOpen enabled");
}

function assertQuickOpenDisabled(dbg) {
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
  let value = dbg.selectors.getSelectedLocation().column;
  if (value === undefined) {
    value = null;
  } else {
    // column is 0-based, while we want to mention 1-based in the test.
    value++;
  }
  is(value, columnNumber, `goto column is ${columnNumber}`);
}

function resultCount(dbg) {
  return findAllElements(dbg, "resultItems").length;
}

async function quickOpen(dbg, query, shortcut = "quickOpen") {
  pressKey(dbg, shortcut);
  assertQuickOpenEnabled(dbg);
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
