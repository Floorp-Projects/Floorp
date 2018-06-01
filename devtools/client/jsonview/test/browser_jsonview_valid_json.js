/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "valid_json.json";

add_task(async function() {
  info("Test valid JSON started");

  const tab = await addJsonViewTab(TEST_JSON_URL);

  ok(tab.linkedBrowser.contentPrincipal.isNullPrincipal, "Should have null principal");

  is(await countRows(), 3, "There must be three rows");

  const objectCellCount = await getElementCount(
    ".jsonPanelBox .treeTable .objectCell");
  is(objectCellCount, 1, "There must be one object cell");

  const objectCellText = await getElementText(
    ".jsonPanelBox .treeTable .objectCell");
  is(objectCellText, "", "The summary is hidden when object is expanded");

  // Clicking the value does not collapse it (so that it can be selected and copied).
  await clickJsonNode(".jsonPanelBox .treeTable .treeValueCell");
  is(await countRows(), 3, "There must still be three rows");

  // Clicking the label collapses the auto-expanded node.
  await clickJsonNode(".jsonPanelBox .treeTable .treeLabel");
  is(await countRows(), 1, "There must be one row");

  // Collapsed nodes are preserved when switching panels.
  await selectJsonViewContentTab("headers");
  await selectJsonViewContentTab("json");
  is(await countRows(), 1, "There must still be one row");
});

function countRows() {
  return getElementCount(".jsonPanelBox .treeTable .treeRow");
}
