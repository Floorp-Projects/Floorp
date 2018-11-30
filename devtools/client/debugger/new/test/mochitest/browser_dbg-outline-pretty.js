/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function getItems(dbg) {
  return findAllElements(dbg, "outlineItems");
}

function getNthItem(dbg, index) {
  return findElement(dbg, "outlineItem", index);
}

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
