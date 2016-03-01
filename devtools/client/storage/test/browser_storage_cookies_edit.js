/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of cookies.

"use strict";

add_task(function*() {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies.html");
  yield gUI.table.once(TableWidget.EVENTS.FIELDS_EDITABLE);

  showAllColumns(true);

  yield editCell("test3", "name", "newTest3");
  yield editCell("newTest3", "path", "/");
  yield editCell("newTest3", "host", "test1.example.org");
  yield editCell("newTest3", "expires", "Tue, 14 Feb 2040 17:41:14 GMT");
  yield editCell("newTest3", "value", "newValue3");
  yield editCell("newTest3", "isSecure", "true");
  yield editCell("newTest3", "isHttpOnly", "true");

  yield finishTests();
});
