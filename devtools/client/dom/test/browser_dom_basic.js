/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_basic.html";

/**
 * Basic test that checks content of the DOM panel.
 */
add_task(async function() {
  info("Test DOM panel basic started");

  const { panel } = await addTestTab(TEST_PAGE_URL);

  // Expand specified row and wait till children are displayed.
  await expandRow(panel, "_a");

  // Verify that child is displayed now.
  const childRow = getRowByLabel(panel, "_data");
  ok(childRow, "Child row must exist");
});
