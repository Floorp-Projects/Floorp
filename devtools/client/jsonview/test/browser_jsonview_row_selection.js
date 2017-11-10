/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  info("Test 1 JSON row selection started");

  // Create a tall JSON so that there is a scrollbar.
  let numRows = 1e3;
  let json = JSON.stringify(Array(numRows).fill().map((_, i) => i));
  let tab = await addJsonViewTab("data:application/json," + json);

  is(await getElementCount(".treeRow"), numRows, "Got the expected number of rows.");
  await assertRowSelected(null);
  await evalInContent("var scroller = $('.jsonPanelBox .panelContent')");
  ok(await evalInContent("scroller.clientHeight < scroller.scrollHeight"),
     "There is a scrollbar.");
  is(await evalInContent("scroller.scrollTop"), 0, "Initially scrolled to the top.");

  // Click to select last row.
  await evalInContent("$('.treeRow:last-child').click()");
  await assertRowSelected(numRows);
  is(await evalInContent("scroller.scrollTop + scroller.clientHeight"),
     await evalInContent("scroller.scrollHeight"), "Scrolled to the bottom.");

  // Click to select 2nd row.
  await evalInContent("$('.treeRow:nth-child(2)').click()");
  await assertRowSelected(2);
  ok(await evalInContent("scroller.scrollTop > 0"), "Not scrolled to the top.");

  // Synthetize up arrow key to select first row.
  await evalInContent("$('.treeTable').focus()");
  await BrowserTestUtils.synthesizeKey("VK_UP", {}, tab.linkedBrowser);
  await assertRowSelected(1);
  is(await evalInContent("scroller.scrollTop"), 0, "Scrolled to the top.");
});

add_task(async function () {
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

async function assertRowSelected(rowNum) {
  let idx = evalInContent("[].indexOf.call($$('.treeRow'), $('.treeRow.selected'))");
  is(await idx + 1, +rowNum, `${rowNum ? "The row #" + rowNum : "No row"} is selected.`);
}
