/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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

  assertOutlineItems(dbg, [
    "λmain()",
    "λdoEval()",
    "λevaledFunc()",
    "λdoNamedEval()",
    // evaledFunc is set twice
    "λevaledFunc()",
    "class MyClass",
    "λconstructor(a, b)",
    "λtest()",
    "λ#privateFunc(a, b)",
    "class Klass",
    "λconstructor()",
    "λtest()",
  ]);

  info("Click an item in outline panel");
  const item = getNthItem(dbg, 3);
  item.click();
  assertHighlightLocation(dbg, "simple1", 15);
  ok(
    item.parentNode.classList.contains("focused"),
    "The clicked item li is focused"
  );

  info("Sort the list");
  findElementWithSelector(dbg, ".outline-footer button").click();
  // Button becomes active to show alphabetization
  is(
    findElementWithSelector(dbg, ".outline-footer button").className,
    "active",
    "Alphabetize button is highlighted when active"
  );

  info("Check that the list was sorted as expected");
  assertOutlineItems(dbg, [
    "λdoEval()",
    "λdoNamedEval()",
    // evaledFunc is set twice
    "λevaledFunc()",
    "λevaledFunc()",
    "λmain()",
    "class Klass",
    "λconstructor()",
    "λtest()",
    "class MyClass",
    "λ#privateFunc(a, b)",
    "λconstructor(a, b)",
    "λtest()",
  ]);
});

function assertOutlineItems(dbg, expectedItems) {
  SimpleTest.isDeeply(
    getItems(dbg).map(i => i.innerText.trim()),
    expectedItems,
    "The expected items are displayed in the outline panel"
  );
}

function getItems(dbg) {
  return Array.from(
    findAllElementsWithSelector(
      dbg,
      ".outline-list h2, .outline-list .outline-list__element"
    )
  );
}

function getNthItem(dbg, index) {
  return findElement(dbg, "outlineItem", index);
}
