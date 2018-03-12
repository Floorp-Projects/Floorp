/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "valid_json.json";

add_task(function* () {
  info("Test valid JSON started");

  yield addJsonViewTab(TEST_JSON_URL);

  // Select the RawData tab
  yield selectJsonViewContentTab("headers");

  // Check displayed headers
  let count = yield getElementCount(".headersPanelBox .netHeadersGroup");
  is(count, 2, "There must be two header groups");

  let text = yield getElementText(".headersPanelBox .netInfoHeadersTable");
  isnot(text, "", "Headers text must not be empty");

  let browser = gBrowser.selectedBrowser;

  // Verify JSON copy into the clipboard.
  yield waitForClipboardPromise(function setup() {
    BrowserTestUtils.synthesizeMouseAtCenter(
      ".headersPanelBox .toolbar button.copy",
      {}, browser);
  }, function validator(value) {
    return value.indexOf("application/json") > 0;
  });
});
