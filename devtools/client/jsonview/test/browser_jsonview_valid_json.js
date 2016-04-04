/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "valid_json.json";

add_task(function* () {
  info("Test valid JSON started");

  yield addJsonViewTab(TEST_JSON_URL);

  let countBefore = yield getElementCount(".jsonPanelBox .treeTable .treeRow");
  ok(countBefore == 1, "There must be one row");

  yield expandJsonNode(".jsonPanelBox .treeTable .treeLabel");

  let countAfter = yield getElementCount(".jsonPanelBox .treeTable .treeRow");
  ok(countAfter == 3, "There must be three rows");

  let objectCellCount = yield getElementCount(
    ".jsonPanelBox .treeTable .objectCell");
  ok(objectCellCount == 1, "There must be one object cell");

  let objectCellText = yield getElementText(
    ".jsonPanelBox .treeTable .objectCell");
  ok(objectCellText == "", "The summary is hidden when object is expanded");
});
