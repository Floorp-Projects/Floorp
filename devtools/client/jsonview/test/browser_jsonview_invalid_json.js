/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "invalid_json.json";

add_task(function* () {
  info("Test invalid JSON started");

  let tab = yield addJsonViewTab(TEST_JSON_URL);

  let count = yield getElementCount(".jsonPanelBox .domTable .memberRow");
  ok(count == 0, "There must be no row");

  let text = yield getElementText(".jsonPanelBox .jsonParseError");
  ok(text, "There must be an error description");
});
