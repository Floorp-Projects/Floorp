/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test column sorting works and sorts dates correctly (including "session"
// cookies).

"use strict";

add_task(async function() {
  const TEST_URL = MAIN_DOMAIN + "storage-cookies-sort.html";
  await openTabAndSetupStorage(TEST_URL);
  showAllColumns(true);

  info("Sort on the expires column, ascending order");
  clickColumnHeader("expires");

  // Note that here we only specify `test_session` for `test_session1` and
  // `test_session2`. Since we sort on the "expires" column, there is no point
  // in asserting the order between those 2 items.
  checkCells([
    "test_session",
    "test_session",
    "test_hour",
    "test_day",
    "test_year",
  ]);

  info("Sort on the expires column, descending order");
  clickColumnHeader("expires");

  // Again, only assert `test_session` for `test_session1` and `test_session2`.
  checkCells([
    "test_year",
    "test_day",
    "test_hour",
    "test_session",
    "test_session",
  ]);

  info("Sort on the name column, ascending order");
  clickColumnHeader("name");
  checkCells([
    "test_day",
    "test_hour",
    "test_session1",
    "test_session2",
    "test_year",
  ]);
});

function checkCells(expected) {
  const cells = [
    ...gPanelWindow.document.querySelectorAll("#name .table-widget-cell"),
  ];
  cells.forEach(function(cell, i, arr) {
    // We use startsWith in order to avoid asserting the relative order of
    // "session" cookies when sorting on the "expires" column.
    ok(
      cell.value.startsWith(expected[i]),
      `Cell value starts with "${expected[i]}".`
    );
  });
}
