/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function* () {
  info("Test JSON row selection started");

  // Create a tall JSON so that there is a scrollbar.
  let numRows = 1e3;
  let json = JSON.stringify(Array(numRows).fill().map((_, i) => i));
  let tab = yield addJsonViewTab("data:application/json," + json);

  is(yield getElementCount(".treeRow"), numRows, "Got the expected number of rows.");
  yield assertRowSelected(null);
  yield evalInContent("var scroller = $('.jsonPanelBox .panelContent')");
  ok(yield evalInContent("scroller.clientHeight < scroller.scrollHeight"),
     "There is a scrollbar.");
  is(yield evalInContent("scroller.scrollTop"), 0, "Initially scrolled to the top.");

  // Click to select last row.
  yield evalInContent("$('.treeRow:last-child').click()");
  yield assertRowSelected(numRows);
  is(yield evalInContent("scroller.scrollTop + scroller.clientHeight"),
     yield evalInContent("scroller.scrollHeight"), "Scrolled to the bottom.");

  // Click to select 2nd row.
  yield evalInContent("$('.treeRow:nth-child(2)').click()");
  yield assertRowSelected(2);
  ok(yield evalInContent("scroller.scrollTop > 0"), "Not scrolled to the top.");

  // Synthetize up arrow key to select first row.
  yield evalInContent("$('.treeTable').focus()");
  yield BrowserTestUtils.synthesizeKey("VK_UP", {}, tab.linkedBrowser);
  yield assertRowSelected(1);
  is(yield evalInContent("scroller.scrollTop"), 0, "Scrolled to the top.");
});

async function assertRowSelected(rowNum) {
  let idx = evalInContent("[].indexOf.call($$('.treeRow'), $('.treeRow.selected'))");
  is(await idx + 1, +rowNum, `${rowNum ? "The row #" + rowNum : "No row"} is selected.`);
}
