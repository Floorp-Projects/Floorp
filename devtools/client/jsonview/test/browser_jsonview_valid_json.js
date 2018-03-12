/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "valid_json.json";

add_task(function* () {
  info("Test valid JSON started");

  let tab = yield addJsonViewTab(TEST_JSON_URL);

  ok(tab.linkedBrowser.contentPrincipal.isNullPrincipal, "Should have null principal");

  is(yield countRows(), 3, "There must be three rows");

  let objectCellCount = yield getElementCount(
    ".jsonPanelBox .treeTable .objectCell");
  is(objectCellCount, 1, "There must be one object cell");

  let objectCellText = yield getElementText(
    ".jsonPanelBox .treeTable .objectCell");
  is(objectCellText, "", "The summary is hidden when object is expanded");

  // Clicking the value does not collapse it (so that it can be selected and copied).
  yield clickJsonNode(".jsonPanelBox .treeTable .treeValueCell");
  is(yield countRows(), 3, "There must still be three rows");

  // Clicking the label collapses the auto-expanded node.
  yield clickJsonNode(".jsonPanelBox .treeTable .treeLabel");
  is(yield countRows(), 1, "There must be one row");

  // Collapsed nodes are preserved when switching panels.
  yield selectJsonViewContentTab("headers");
  yield selectJsonViewContentTab("json");
  is(yield countRows(), 1, "There must still be one row");
});

function countRows() {
  return getElementCount(".jsonPanelBox .treeTable .treeRow");
}
