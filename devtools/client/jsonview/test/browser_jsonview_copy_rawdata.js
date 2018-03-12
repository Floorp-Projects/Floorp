/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "simple_json.json";

let jsonText = "{\"name\": \"value\"}\n";
let prettyJson = "{\n  \"name\": \"value\"\n}";

add_task(function* () {
  info("Test copy raw data started");

  yield addJsonViewTab(TEST_JSON_URL);

  // Select the RawData tab
  yield selectJsonViewContentTab("rawdata");

  // Check displayed JSON
  let text = yield getElementText(".textPanelBox .data");
  is(text, jsonText, "Proper JSON must be displayed in DOM");

  let browser = gBrowser.selectedBrowser;

  // Verify JSON copy into the clipboard.
  yield waitForClipboardPromise(function setup() {
    BrowserTestUtils.synthesizeMouseAtCenter(
      ".textPanelBox .toolbar button.copy",
      {}, browser);
  }, jsonText);

  // Click 'Pretty Print' button
  yield BrowserTestUtils.synthesizeMouseAtCenter(
    ".textPanelBox .toolbar button.prettyprint",
    {}, browser);

  let prettyText = yield getElementText(".textPanelBox .data");
  prettyText = normalizeNewLines(prettyText);
  ok(prettyText.startsWith(prettyJson),
    "Pretty printed JSON must be displayed");

  // Verify JSON copy into the clipboard.
  yield waitForClipboardPromise(function setup() {
    BrowserTestUtils.synthesizeMouseAtCenter(
      ".textPanelBox .toolbar button.copy",
      {}, browser);
  }, function validator(value) {
    let str = normalizeNewLines(value);
    return str == prettyJson;
  });
});
