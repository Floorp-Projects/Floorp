/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_basic.html";

/**
 * Basic test that checks content of the DOM panel.
 */
add_task(function* () {
  info("Test DOM panel basic started");

  let { panel } = yield addTestTab(TEST_PAGE_URL);

  // Expand specified row and wait till children are displayed.
  yield expandRow(panel, "_a");

  // Verify that child is displayed now.
  let childRow = getRowByLabel(panel, "_data");
  ok(childRow, "Child row must exist");
});
