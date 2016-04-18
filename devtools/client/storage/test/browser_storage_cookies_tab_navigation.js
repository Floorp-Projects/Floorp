/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check cookie table tab navigation.

"use strict";

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies.html");
  showAllColumns(true);

  yield startCellEdit("test1", "name");

  PressKeyXTimes("VK_TAB", 18);
  is(getCurrentEditorValue(), "value3",
     "We have tabbed to the correct cell.");

  PressKeyXTimes("VK_TAB", 18, {shiftKey: true});
  is(getCurrentEditorValue(), "test1",
     "We have shift-tabbed to the correct cell.");

  yield finishTests();
});
