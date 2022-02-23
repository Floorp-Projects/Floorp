/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that after clicking a function in edtior, outline focuses that function

add_task(async function() {
  const dbg = await initDebugger("doc-sources.html", "long");

  await selectSource(dbg, "long", 1);
  findElementWithSelector(dbg, ".outline-tab").click();
  is(getItems(dbg).length, 9, "9 items in the outline list");

  info("Clicking inside a function in editor should focus the outline");
  clickAtPos(dbg, { line: 15, ch: 2 });
  await waitForElementWithSelector(dbg, ".outline-list__element.focused");
  ok(
    getFocusedFunction(dbg).includes("addTodo"),
    "The right function is focused"
  );

  info("Clicking an empty line in the editor should unfocus the outline");
  await clickAtPos(dbg, { line: 13, ch: 2 });
  is(getFocusedNode(dbg), null, "should not exist");
});

function getItems(dbg) {
  return findAllElements(dbg, "outlineItems");
}

function getFocusedNode(dbg) {
  return findElementWithSelector(dbg, ".outline-list__element.focused");
}

function getFocusedFunction(dbg) {
  return getFocusedNode(dbg).innerText;
}
