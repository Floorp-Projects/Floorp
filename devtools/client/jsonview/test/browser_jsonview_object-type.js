/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {ELLIPSIS} = require("devtools/shared/l10n");

add_task(function* () {
  info("Test Object type property started");

  const TEST_JSON_URL = "data:application/json,{\"x\":{\"type\":\"string\"}}";
  yield addJsonViewTab(TEST_JSON_URL);

  let count = yield getElementCount(".jsonPanelBox .treeTable .treeRow");
  is(count, 2, "There must be two rows");

  // Collapse auto-expanded node.
  yield clickJsonNode(".jsonPanelBox .treeTable .treeLabel");

  count = yield getElementCount(".jsonPanelBox .treeTable .treeRow");
  is(count, 1, "There must be one row");

  let label = yield getElementText(".jsonPanelBox .treeTable .objectCell");
  is(label, `{${ELLIPSIS}}`, "The label must be indicating an object");
});
