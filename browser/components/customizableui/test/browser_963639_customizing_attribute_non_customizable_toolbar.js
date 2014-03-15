/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kToolbar = "test-toolbar-963639-non-customizable-customizing-attribute";

add_task(function() {
  info("Test for Bug 963639 - CustomizeMode _onToolbarVisibilityChange sets @customizing on non-customizable toolbars");

  let toolbar = document.createElement("toolbar");
  toolbar.id = kToolbar;
  gNavToolbox.appendChild(toolbar);

  let testToolbar = document.getElementById(kToolbar)
  ok(testToolbar, "Toolbar was created.");
  is(gNavToolbox.getElementsByAttribute("id", kToolbar).length, 1,
     "Toolbar was added to the navigator toolbox");

  toolbar.setAttribute("toolbarname", "NonCustomizableToolbarCustomizingAttribute");
  toolbar.setAttribute("collapsed", "true");

  yield startCustomizing();
  window.setToolbarVisibility(toolbar, "true");
  isnot(toolbar.getAttribute("customizing"), "true",
        "Toolbar doesn't have the customizing attribute");

  yield endCustomizing();
  gNavToolbox.removeChild(toolbar);

  is(gNavToolbox.getElementsByAttribute("id", kToolbar).length, 0,
     "Toolbar was removed from the navigator toolbox");
});
