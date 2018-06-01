/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "simple_json.json";

add_task(async function() {
  info("Test copy JSON started");

  await addJsonViewTab(TEST_JSON_URL);

  const countBefore = await getElementCount(".jsonPanelBox .treeTable .treeRow");
  ok(countBefore == 1, "There must be one row");

  const text = await getElementText(".jsonPanelBox .treeTable .treeRow");
  is(text, "name\"value\"", "There must be proper JSON displayed");

  // Verify JSON copy into the clipboard.
  const value = "{\"name\": \"value\"}\n";
  const browser = gBrowser.selectedBrowser;
  const selector = ".jsonPanelBox .toolbar button.copy";
  await waitForClipboardPromise(function setup() {
    BrowserTestUtils.synthesizeMouseAtCenter(selector, {}, browser);
  }, function validator(result) {
    const str = normalizeNewLines(result);
    return str == value;
  });
});
