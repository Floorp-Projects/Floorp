/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of cookies with the keyboard.

"use strict";

add_task(function*() {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies.html");
  yield gUI.table.once(TableWidget.EVENTS.FIELDS_EDITABLE);

  showAllColumns(true);

  yield startCellEdit("test4", "name");
  yield typeWithTerminator("test6", "VK_TAB");
  yield typeWithTerminator("/", "VK_TAB");
  yield typeWithTerminator(".example.org", "VK_TAB");
  yield typeWithTerminator("Tue, 25 Dec 2040 12:00:00 GMT", "VK_TAB");
  yield typeWithTerminator("test6value", "VK_TAB");
  yield typeWithTerminator("false", "VK_TAB");
  yield typeWithTerminator("false", "VK_TAB");

  yield finishTests();
});
