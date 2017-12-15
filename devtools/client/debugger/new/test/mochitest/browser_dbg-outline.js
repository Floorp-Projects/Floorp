/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the editor highlights the correct location when the
// debugger pauses

function getItems(dbg) {
  return findAllElements(dbg, "outlineItems");
}

function getNthItem(dbg, index) {
  return findElement(dbg, "outlineItem", index);
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  const { selectors: { getSelectedSource }, getState } = dbg;

  await selectSource(dbg, "simple1", 1);
  await waitForLoadedSource(dbg, "simple1");

  findElementWithSelector(dbg, ".outline-tab").click();
  is(getItems(dbg).length, 5, "5 items in the list");

  // click on an element
  const item = getNthItem(dbg, 3);
  is(item.innerText, "evaledFunc()", "got evaled func");
  item.click();
  assertHighlightLocation(dbg, "simple1", 15);
});
