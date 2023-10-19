/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that after clicking a function in edtior, outline focuses that function

"use strict";

// Tests that after clicking a function in edtior, outline focuses that function
add_task(async function () {
  const dbg = await initDebugger("doc-sources.html", "long.js");

  await selectSource(dbg, "long.js", 1);
  findElementWithSelector(dbg, ".outline-tab").click();
  await waitForElementWithSelector(dbg, ".outline-list");

  is(
    findAllElements(dbg, "outlineItems").length,
    9,
    "9 items in the outline list"
  );

  info("Clicking inside a function in editor should focus the outline");
  await clickAtPos(dbg, { line: 15, column: 3 });
  await waitForElementWithSelector(dbg, ".outline-list__element.focused");
  ok(
    getFocusedFunction(dbg).includes("addTodo"),
    "The right function is focused"
  );

  info("Clicking an empty line in the editor should unfocus the outline");
  await clickAtPos(dbg, { line: 13, column: 3 });
  is(getFocusedNode(dbg), null, "should not exist");
});

// Tests that clicking a function in outline panel, the editor highlights the correct location.
add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");

  await selectSource(dbg, "simple1.js", 1);

  findElementWithSelector(dbg, ".outline-tab").click();
  await waitForElementWithSelector(dbg, ".outline-list");

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
  await waitForLoadedSource(dbg, "simple1.js");
  assertHighlightLocation(dbg, "simple1.js", 15);
  ok(
    item.parentNode.classList.contains("focused"),
    "The clicked item li is focused"
  );
});

// Clicking on a class heading  select the correct class line

function getFocusedNode(dbg) {
  return findElementWithSelector(dbg, ".outline-list__element.focused");
}

function getFocusedFunction(dbg) {
  return getFocusedNode(dbg).innerText;
}

function getNthItem(dbg, index) {
  return findElement(dbg, "outlineItem", index);
}
