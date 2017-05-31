/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function* () {
  info("Test UTF-8 JSON started");

  const encodedChar = "%E2%9D%A4"; // In UTF-8 this is a heavy black heart
  const TEST_JSON_URL = "data:application/json;charset=ANSI,[\"" + encodedChar + "\"]";

  yield addJsonViewTab(TEST_JSON_URL);

  let countBefore = yield getElementCount(".jsonPanelBox .treeTable .treeRow");
  is(countBefore, 1, "There must be one row.");

  let objectCellCount = yield getElementCount(
    ".jsonPanelBox .treeTable .stringCell");
  is(objectCellCount, 1, "There must be one string cell.");

  let objectCellText = yield getElementText(
    ".jsonPanelBox .treeTable .stringCell");
  is(objectCellText, JSON.stringify(decodeURIComponent(encodedChar)),
     "The source has been parsed as UTF-8, ignoring the charset parameter.");
});
