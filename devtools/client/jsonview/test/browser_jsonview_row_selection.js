/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  info("Test 1 JSON row selection started");

  // Create a tall JSON so that there is a scrollbar.
  let numRows = 1e3;
  let json = JSON.stringify(Array(numRows).fill().map((_, i) => i));
  let tab = await addJsonViewTab("data:application/json," + json);

  is(await getElementCount(".treeRow"), numRows, "Got the expected number of rows.");
  await assertRowSelected(null);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    ok(scroller.clientHeight < scroller.scrollHeight, "There is a scrollbar.");
    is(scroller.scrollTop, 0, "Initially scrolled to the top.");

    // Click to select last row.
    content.document.querySelector(".treeRow:last-child").click();
  });
  await assertRowSelected(numRows);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    is(scroller.scrollTop + scroller.clientHeight, scroller.scrollHeight,
       "Scrolled to the bottom.");
    // Click to select 2nd row.
    content.document.querySelector(".treeRow:nth-child(2)").click();
  });
  await assertRowSelected(2);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    ok(scroller.scrollTop > 0, "Not scrolled to the top.");
    // Synthetize up arrow key to select first row.
    content.document.querySelector(".treeTable").focus();
  });
  await BrowserTestUtils.synthesizeKey("VK_UP", {}, tab.linkedBrowser);
  await assertRowSelected(1);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    is(scroller.scrollTop, 0, "Scrolled to the top.");
  });
});

add_task(async function() {
  info("Test 2 JSON row selection started");

  let numRows = 4;
  let tab = await addJsonViewTab("data:application/json,[0,1,2,3]");

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
  let json = JSON.stringify([0, "a ".repeat(1e4), 1]);
  await addJsonViewTab("data:application/json," + encodeURI(json));

  is(await getElementCount(".treeRow"), 3, "Got the expected number of rows.");
  await assertRowSelected(null);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    let row = content.document.querySelector(".treeRow:nth-child(2)");
    ok(scroller.clientHeight < row.clientHeight, "The row is taller than the scroller.");
    is(scroller.scrollTop, 0, "Initially scrolled to the top.");

    // Select the tall row.
    row.click();
  });
  await assertRowSelected(2);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    let row = content.document.querySelector(".treeRow:nth-child(2)");
    is(scroller.scrollTop, row.offsetTop,
       "Scrolled to the top of the row.");
  });

  // Select the last row.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.document.querySelector(".treeRow:last-child").click();
  });
  await assertRowSelected(3);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    is(scroller.scrollTop + scroller.offsetHeight,
       scroller.scrollHeight, "Scrolled to the bottom.");

    // Select the tall row.
    let row = content.document.querySelector(".treeRow:nth-child(2)");
    row.click();
  });

  await assertRowSelected(2);
  let scroll = await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    let row = content.document.querySelector(".treeRow:nth-child(2)");
    is(scroller.scrollTop + scroller.offsetHeight, row.offsetTop + row.offsetHeight,
       "Scrolled to the bottom of the row.");

    // Scroll up a bit, so that both the top and bottom of the row are not visible.
    let scrollPos =
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
    let scroller = content.document.querySelector(".jsonPanelBox .panelContent");
    is(scroller.scrollTop, scrollPos, "Scroll did not change");
  });
});

async function assertRowSelected(rowNum) {
  let idx = await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    return [].indexOf.call(content.document.querySelectorAll(".treeRow"),
      content.document.querySelector(".treeRow.selected"));
  });
  is(idx + 1, +rowNum, `${rowNum ? "The row #" + rowNum : "No row"} is selected.`);
}
