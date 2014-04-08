/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kToolbarID = "test-toolbar";

/**
 * Tests that customizable toolbars are forced to have their mode
 * attribute set to "icons".
 */
add_task(function* testAddingToolbar() {
  let toolbar = document.createElement("toolbar");
  toolbar.setAttribute("mode", "full");
  toolbar.setAttribute("customizable", "true");
  toolbar.setAttribute("id", kToolbarID);

  CustomizableUI.registerArea(kToolbarID, {
     type: CustomizableUI.TYPE_TOOLBAR,
     legacy: false,
  })

  gNavToolbox.appendChild(toolbar);

  is(toolbar.getAttribute("mode"), "icons",
     "Toolbar should have its mode attribute set to icons.")

  toolbar.remove();
  CustomizableUI.unregisterArea(kToolbarID);
});