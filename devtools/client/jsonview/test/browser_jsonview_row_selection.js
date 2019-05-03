/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  info("Test 1 JSON row selection started");

  // Create a tall JSON so that there is a scrollbar.
  const numRows = 1e3;
  const json = JSON.stringify(Array(numRows).fill().map((_, i) => i));
  const tab = await addJsonViewTab("data:application/json," + json);

  is(await getElementCount(".treeRow"), numRows, "Got the expected number of rows.");
  await assertRowSelected(null);

  // Focus the tree and select first row.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const tree = content.document.querySelector(".treeTable");
    tree.focus();
    is(tree, content.document.activeElement, "Tree should be focused");
    content.document.querySelector(".treeRow:nth-child(1)").click();
  });
  await assertRowSelected(1);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    ok(scroller.clientHeight < scroller.scrollHeight, "There is a scrollbar.");
    is(scroller.scrollTop, 0, "Initially scrolled to the top.");
  });

  // Select last row.
  await BrowserTestUtils.synthesizeKey("VK_END", {}, tab.linkedBrowser);
  await assertRowSelected(numRows);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    is(scroller.scrollTop + scroller.clientHeight, scroller.scrollHeight,
       "Scrolled to the bottom.");
    // Click to select 2nd row.
    content.document.querySelector(".treeRow:nth-child(2)").click();
  });
  await assertRowSelected(2);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    ok(scroller.scrollTop > 0, "Not scrolled to the top.");
    // Synthetize up arrow key to select first row.
    content.document.querySelector(".treeTable").focus();
  });
  await BrowserTestUtils.synthesizeKey("VK_UP", {}, tab.linkedBrowser);
  await assertRowSelected(1);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    is(scroller.scrollTop, 0, "Scrolled to the top.");
  });
});

add_task(async function() {
  info("Test 2 JSON row selection started");

  const numRows = 4;
  const tab = await addJsonViewTab("data:application/json,[0,1,2,3]");

  is(await getElementCount(".treeRow"), numRows, "Got the expected number of rows.");
  await assertRowSelected(null);

  // Click to select first row.
  await clickJsonNode(".treeRow:first-child");
  await assertRowSelected(1);

  // Synthetize multiple down arrow keydowns to select following rows.
  for (let i = 2; i < numRows; ++i) {
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {type: "keydown"}, tab.linkedBrowser);
    await assertRowSelected(i);
  }

  // Now synthetize the keyup, this shouldn't change selected row.
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {type: "keyup"}, tab.linkedBrowser);
  await assertRowSelected(numRows - 1);
});

add_task(async function() {
  info("Test 3 JSON row selection started");

  // Create a JSON with a row taller than the panel.
  const json = JSON.stringify([0, "a ".repeat(1e4), 1]);
  const tab = await addJsonViewTab("data:application/json," + encodeURI(json));

  is(await getElementCount(".treeRow"), 3, "Got the expected number of rows.");
  await assertRowSelected(null);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    const row = content.document.querySelector(".treeRow:nth-child(2)");
    ok(scroller.clientHeight < row.clientHeight, "The row is taller than the scroller.");
    is(scroller.scrollTop, 0, "Initially scrolled to the top.");

    // Select the tall row.
    content.document.querySelector(".treeTable").focus();
    row.click();
  });
  await assertRowSelected(2);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    is(scroller.scrollTop, 0, "When the row is visible, do not scroll on click.");
  });

  // Select the last row.
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, tab.linkedBrowser);
  await assertRowSelected(3);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    is(scroller.scrollTop + scroller.offsetHeight,
       scroller.scrollHeight, "Scrolled to the bottom.");

    // Select the tall row.
    const row = content.document.querySelector(".treeRow:nth-child(2)");
    row.click();
  });

  await assertRowSelected(2);
  const scroll = await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    const row = content.document.querySelector(".treeRow:nth-child(2)");
    is(scroller.scrollTop + scroller.offsetHeight, scroller.scrollHeight,
      "Scrolled to the bottom. When the row is visible, do not scroll on click.");

    // Scroll up a bit, so that both the top and bottom of the row are not visible.
    const scrollPos =
      scroller.scrollTop = Math.ceil((scroller.scrollTop + row.offsetTop) / 2);
    ok(scroller.scrollTop > row.offsetTop,
       "The top of the row is not visible.");
    ok(scroller.scrollTop + scroller.offsetHeight < row.offsetTop + row.offsetHeight,
       "The bottom of the row is not visible.");

    // Select the tall row.
    row.click();
    return scrollPos;
  });

  await assertRowSelected(2);
  await ContentTask.spawn(gBrowser.selectedBrowser, scroll, function(scrollPos) {
    const scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    is(scroller.scrollTop, scrollPos, "Scroll did not change");
  });
});

async function assertRowSelected(rowNum) {
  const idx = await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    return [].indexOf.call(content.document.querySelectorAll(".treeRow"),
      content.document.querySelector(".treeRow.selected"));
  });
  is(idx + 1, +rowNum, `${rowNum ? "The row #" + rowNum : "No row"} is selected.`);
}
