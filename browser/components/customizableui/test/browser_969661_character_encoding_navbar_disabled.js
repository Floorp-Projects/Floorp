/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Adding the character encoding menu to the panel, exiting customize mode,
// and moving it to the nav-bar should have it enabled, not disabled.
add_task(async function() {
  await startCustomizing();
  CustomizableUI.addWidgetToArea(
    "characterencoding-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  await endCustomizing();
  await document.getElementById("nav-bar").overflowable.show();
  let panelHiddenPromise = promiseOverflowHidden(window);
  PanelUI.overflowPanel.hidePopup();
  await panelHiddenPromise;
  CustomizableUI.addWidgetToArea("characterencoding-button", "nav-bar");
  let button = document.getElementById("characterencoding-button");
  ok(!button.hasAttribute("disabled"), "Button shouldn't be disabled");
});

add_task(function asyncCleanup() {
  resetCustomization();
});
