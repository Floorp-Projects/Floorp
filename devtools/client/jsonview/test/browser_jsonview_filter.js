/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "array_json.json";

add_task(function* () {
  info("Test valid JSON started");

  let tab = yield addJsonViewTab(TEST_JSON_URL);

  let count = yield getElementCount(".jsonPanelBox .domTable .memberRow");
  is(count, 3, "There must be three rows");

  // XXX use proper shortcut to focus the filter box
  // as soon as bug Bug 1178771 is fixed.
  yield sendString("h", ".jsonPanelBox .searchBox");

  // The filtering is done asynchronously so, we need to wait.
  yield waitForFilter();

  let hiddenCount = yield getElementCount(".jsonPanelBox .domTable .memberRow.hidden");
  is(hiddenCount, 2, "There must be two hidden rows");
});
