/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "valid_json.json";

add_task(async function() {
  info("Test valid JSON started");

  await addJsonViewTab(TEST_JSON_URL);

  // Select the RawData tab
  await selectJsonViewContentTab("headers");

  // Check displayed headers
  const count = await getElementCount(".headersPanelBox .netHeadersGroup");
  is(count, 2, "There must be two header groups");

  const text = await getElementText(".headersPanelBox .netInfoHeadersTable");
  isnot(text, "", "Headers text must not be empty");

  const browser = gBrowser.selectedBrowser;

  // Verify JSON copy into the clipboard.
  await waitForClipboardPromise(function setup() {
    BrowserTestUtils.synthesizeMouseAtCenter(
      ".headersPanelBox .toolbar button.copy",
      {}, browser);
  }, function validator(value) {
    return value.indexOf("application/json") > 0;
  });
});
