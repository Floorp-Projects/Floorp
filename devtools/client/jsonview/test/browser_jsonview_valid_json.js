/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "valid_json.json";

add_task(function* () {
  info("Test valid JSON started");

  let tab = yield addJsonViewTab(TEST_JSON_URL);

  let countBefore = yield getElementCount(".jsonPanelBox .domTable .memberRow");
  ok(countBefore == 1, "There must be one row");

  yield expandJsonNode(".jsonPanelBox .domTable .memberLabel");

  let countAfter = yield getElementCount(".jsonPanelBox .domTable .memberRow");
  ok(countAfter == 3, "There must be three rows");
});
