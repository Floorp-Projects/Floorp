/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";


// Adding the character encoding menu to the panel, exiting customize mode,
// and moving it to the nav-bar should have it enabled, not disabled.
add_task(function() {
  yield startCustomizing();
  CustomizableUI.addWidgetToArea("characterencoding-button", "PanelUI-contents");
  yield endCustomizing();
  yield PanelUI.show();
  let panelHiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  yield panelHiddenPromise;
  CustomizableUI.addWidgetToArea("characterencoding-button", 'nav-bar');
  let button = document.getElementById("characterencoding-button");
  ok(!button.hasAttribute("disabled"), "Button shouldn't be disabled");
});

add_task(function asyncCleanup() {
  resetCustomization();
});

