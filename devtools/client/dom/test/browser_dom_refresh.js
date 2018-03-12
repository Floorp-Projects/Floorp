/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_basic.html";

/**
 * Basic test that checks the Refresh action in DOM panel.
 */
add_task(function* () {
  info("Test DOM panel basic started");

  let { panel } = yield addTestTab(TEST_PAGE_URL);

  // Create a new variable in the page scope and refresh the panel.
  yield evaluateJSAsync(panel, "var _b = 10");
  yield refreshPanel(panel);

  // Verify that the variable is displayed now.
  let row = getRowByLabel(panel, "_b");
  ok(row, "New variable must be displayed");
});
