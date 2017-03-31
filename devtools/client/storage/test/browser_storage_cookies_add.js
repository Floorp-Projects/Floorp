/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the adding of cookies.

"use strict";

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies.html");
  showAllColumns(true);

  yield performAdd(["cookies", "http://test1.example.org"]);
  yield performAdd(["cookies", "http://test1.example.org"]);
  yield performAdd(["cookies", "http://test1.example.org"]);
  yield performAdd(["cookies", "http://test1.example.org"]);
  yield performAdd(["cookies", "http://test1.example.org"]);

  yield finishTests();
});
