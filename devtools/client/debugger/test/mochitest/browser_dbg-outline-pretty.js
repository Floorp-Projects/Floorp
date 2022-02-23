/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the length of outline functions for original and pretty printed source matches
add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple1");
  const {
    selectors: { getSelectedSource },
    getState
  } = dbg;

  await selectSource(dbg, "simple1");
  findElementWithSelector(dbg, ".outline-tab").click();
  const originalSource = getItems(dbg);

  clickElement(dbg, "prettyPrintButton");
  await waitForSource(dbg, "simple1.js:formatted");
  await waitForElementWithSelector(dbg, ".outline-list");
  const prettySource = getItems(dbg);

  is(originalSource.length, prettySource.length, "Length of outline functions for both prettyPrint and originalSource same");
});

function getItems(dbg) {
  return findAllElements(dbg, "outlineItems");
}

function getNthItem(dbg, index) {
  return findElement(dbg, "outlineItem", index);
}
