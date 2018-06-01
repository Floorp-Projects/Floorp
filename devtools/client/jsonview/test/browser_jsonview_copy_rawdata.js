/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "simple_json.json";

const jsonText = "{\"name\": \"value\"}\n";
const prettyJson = "{\n  \"name\": \"value\"\n}";

add_task(async function() {
  info("Test copy raw data started");

  await addJsonViewTab(TEST_JSON_URL);

  // Select the RawData tab
  await selectJsonViewContentTab("rawdata");

  // Check displayed JSON
  const text = await getElementText(".textPanelBox .data");
  is(text, jsonText, "Proper JSON must be displayed in DOM");

  const browser = gBrowser.selectedBrowser;

  // Verify JSON copy into the clipboard.
  await waitForClipboardPromise(function setup() {
    BrowserTestUtils.synthesizeMouseAtCenter(
      ".textPanelBox .toolbar button.copy",
      {}, browser);
  }, jsonText);

  // Click 'Pretty Print' button
  await BrowserTestUtils.synthesizeMouseAtCenter(
    ".textPanelBox .toolbar button.prettyprint",
    {}, browser);

  let prettyText = await getElementText(".textPanelBox .data");
  prettyText = normalizeNewLines(prettyText);
  ok(prettyText.startsWith(prettyJson),
    "Pretty printed JSON must be displayed");

  // Verify JSON copy into the clipboard.
  await waitForClipboardPromise(function setup() {
    BrowserTestUtils.synthesizeMouseAtCenter(
      ".textPanelBox .toolbar button.copy",
      {}, browser);
  }, function validator(value) {
    const str = normalizeNewLines(value);
    return str == prettyJson;
  });
});
