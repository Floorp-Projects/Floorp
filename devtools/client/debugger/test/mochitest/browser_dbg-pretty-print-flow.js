/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that loader and new tab appear when pretty printing,
// and the selected location is mapped afterwards

add_task(async function() {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await selectSource(dbg, "pretty.js", 4, 8);

  prettyPrint(dbg);
  await waitForTabCounts(dbg, 2);
  await waitForElementWithSelector(dbg, selectors.prettyPrintLoader);
  await waitForSelectedLocation(dbg, 5);
});

function waitForTabCounts(dbg, counts) {
  return waitForState(dbg, state => {
    const tabCounts = countTabs(dbg);

    return tabCounts == counts;
  });
}
