/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TOOLBARID = "test-noncustomizable-toolbar-for-toggling";
function test() {
  let tb = document.createXULElement("toolbar");
  tb.id = TOOLBARID;
  gNavToolbox.appendChild(tb);
  try {
    CustomizableUI.setToolbarVisibility(TOOLBARID, false);
  } catch (ex) {
    ok(false, "Should not throw exceptions trying to set toolbar visibility.");
  }
  is(tb.getAttribute("collapsed"), "true", "Toolbar should be collapsed");
  try {
    CustomizableUI.setToolbarVisibility(TOOLBARID, true);
  } catch (ex) {
    ok(false, "Should not throw exceptions trying to set toolbar visibility.");
  }
  is(tb.getAttribute("collapsed"), "false", "Toolbar should be uncollapsed");
  tb.remove();
}
