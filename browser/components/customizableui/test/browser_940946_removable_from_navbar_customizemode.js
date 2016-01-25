/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kTestBtnId = "test-removable-navbar-customize-mode";

// Items without the removable attribute in the navbar should be considered non-removable
add_task(function*() {
  let btn = createDummyXULButton(kTestBtnId, "Test removable in navbar in customize mode");
  document.getElementById("nav-bar").customizationTarget.appendChild(btn);
  yield startCustomizing();
  ok(!CustomizableUI.isWidgetRemovable(kTestBtnId), "Widget should not be considered removable");
  yield endCustomizing();
  document.getElementById(kTestBtnId).remove();
});

add_task(function* asyncCleanup() {
  yield endCustomizing();
  yield resetCustomization();
});
