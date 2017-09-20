/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const VALID_JSON_URL = URL_ROOT + "valid_json.json";
const INVALID_JSON_URL = URL_ROOT + "invalid_json.json";
const prettyPrintButtonClass = ".textPanelBox .toolbar button.prettyprint";

add_task(function* () {
  info("Test 'Pretty Print' button disappears on parsing invalid JSON");

  let count = yield testPrettyPrintButton(INVALID_JSON_URL);
  is(count, 0, "There must be no pretty-print button for invalid json");
});

add_task(function* () {
  info("Test 'Pretty Print' button is present on parsing valid JSON");

  let count = yield testPrettyPrintButton(VALID_JSON_URL);
  is(count, 1, "There must be pretty-print button for valid json");
});

function* testPrettyPrintButton(url) {
  yield addJsonViewTab(url);

  yield selectJsonViewContentTab("rawdata");
  info("Switched to Raw Data tab.");

  let count = yield getElementCount(prettyPrintButtonClass);
  return count;
}
