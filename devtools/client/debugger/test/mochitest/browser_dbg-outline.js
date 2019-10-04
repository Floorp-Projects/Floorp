/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function getItems(dbg) {
  return findAllElements(dbg, "outlineItems");
}

function getNthItem(dbg, index) {
  return findElement(dbg, "outlineItem", index);
}

// Tests that clicking a function in outline panel, the editor highlights the correct location.
// Tests that outline panel can sort functions alphabetically.
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1");
  const {
    selectors: { getSelectedSource },
    getState,
  } = dbg;

  await selectSource(dbg, "simple1", 1);

  findElementWithSelector(dbg, ".outline-tab").click();
  is(getItems(dbg).length, 5, "5 items in the list");

  info("Click an item in outline panel");
  const item = getNthItem(dbg, 3);
  is(item.innerText, "evaledFunc()", "got evaled func");
  item.click();
  assertHighlightLocation(dbg, "simple1", 15);
  ok(
    item.parentNode.classList.contains("focused"),
    "The clicked item li is focused"
  );

  info("Ensure main() is the first function listed");
  const firstFunction = findElementWithSelector(
    dbg,
    ".outline-list__element .function-signature"
  );
  is(
    firstFunction.innerText,
    "main()",
    "Natural first function is first listed"
  );

  info("Sort the list");
  findElementWithSelector(dbg, ".outline-footer button").click();
  // Button becomes active to show alphabetization
  is(
    findElementWithSelector(dbg, ".outline-footer button").className,
    "active",
    "Alphabetize button is highlighted when active"
  );

  info("Ensure doEval() is the first function listed after alphabetization");
  const firstAlphaFunction = findElementWithSelector(
    dbg,
    ".outline-list__element .function-signature"
  );
  is(
    firstAlphaFunction.innerText,
    "doEval()",
    "Alphabetized first function is correct"
  );
});
