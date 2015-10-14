/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "simple_json.json";

add_task(function* () {
  info("Test copy JSON started");

  let tab = yield addJsonViewTab(TEST_JSON_URL);

  let countBefore = yield getElementCount(".jsonPanelBox .domTable .memberRow");
  ok(countBefore == 1, "There must be one row");

  let text = yield getElementText(".jsonPanelBox .domTable .memberRow");
  is(text, "name\"value\"", "There must be proper JSON displayed");

  // Verify JSON copy into the clipboard.
  let value = "{\"name\": \"value\"}\n";
  let browser = gBrowser.selectedBrowser
  let selector = ".jsonPanelBox .toolbar button.copy";
  yield waitForClipboardPromise(function setup() {
    BrowserTestUtils.synthesizeMouseAtCenter(selector, {}, browser);
  }, function validator(result) {
    let str = normalizeNewLines(result);
    return str == value;
  });
});
