/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check that toggleable toolbars dropdown in always shown");

  info("Remove all possible custom toolbars");
  yield removeCustomToolbars();

  info("Enter customization mode");
  yield startCustomizing();

  let toolbarsToggle = document.getElementById("customization-toolbar-visibility-button");
  ok(toolbarsToggle, "The toolbars toggle dropdown exists");
  ok(!toolbarsToggle.hasAttribute("hidden"),
     "The toolbars toggle dropdown is displayed");
});

add_task(function asyncCleanup() {
  info("Exit customization mode");
  yield endCustomizing();
});
